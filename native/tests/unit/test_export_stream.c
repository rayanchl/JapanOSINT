/* test_export_stream.c — an off-loop export is streamed, not buffered.
 *
 * The export worker used to build the whole body in memory and the loop then
 * copied it once more into the connection's send buffer: measured 790 MB peak
 * footprint for a 347 MB export, 1.49 GB for a 695 MB one. It now hands the
 * loop one framed batch at a time through an xstream (core/httpd.c). This
 * drives that state machine with the real xs_write / xs_pump / xs_on_close on
 * an in-memory mongoose connection (mg_send on a TCP connection only appends
 * to c->send, so no socket is needed).
 *
 * Holds:
 *   - nothing is committed until a full batch is held or the run is done, so
 *     a refusal is answered with its real status and its own body;
 *   - a committed stream is chunk-framed and ends with the terminator;
 *   - a run that stopped early after the commit ends WITHOUT the terminator
 *     and closes (a truncated transfer, never a complete-looking file);
 *   - the worker blocks at the high-water mark: queued + unsent never grows
 *     with the size of the export;
 *   - closing the connection unblocks a waiting worker and stops its walk;
 *   - with the shutdown door shut, a worker never touches the manager (every
 *     wakeup here goes through that closed door; mgr is NULL). */
#include "../../core/httpd.c"

#include <assert.h>

static struct mg_connection *fake_conn(unsigned long id) {
  struct mg_connection *c = calloc(1, sizeof *c);
  assert(c);
  c->id = id;
  return c;
}
static void drop_conn(struct mg_connection *c) { mg_iobuf_free(&c->send); free(c); }

static xstream *new_stream(struct mg_connection *c) {
  xstream *x = calloc(1, sizeof *x);
  assert(x);
  pthread_mutex_init(&x->mu, NULL);
  pthread_cond_init(&x->cv, NULL);
  x->refs = 2;                           /* registry + worker, as export_offload */
  x->cid = c->id;
  x->status = 200;
  snprintf(x->hdr, sizeof x->hdr, "Content-Type: text/csv\r\n");
  x->next_live = g_xs_live;
  g_xs_live = x;
  return x;
}
/* What export_thread does once the run returns. */
static void finish(xstream *x, int status, int rc) {
  pthread_mutex_lock(&x->mu);
  x->done = 1;
  x->status = status;
  x->incomplete = (status == 200 && rc != 0);
  pthread_mutex_unlock(&x->mu);
  xs_release(x);
}
static int has(struct mg_connection *c, const char *needle) {
  size_t n = strlen(needle);
  if (c->send.len < n) return 0;
  for (size_t i = 0; i + n <= c->send.len; i++)
    if (memcmp(c->send.buf + i, needle, n) == 0) return 1;
  return 0;
}
static int ends_with_terminator(struct mg_connection *c) {
  return c->send.len >= 5 && memcmp(c->send.buf + c->send.len - 5, "0\r\n\r\n", 5) == 0;
}

static char batch[65536];

typedef struct { xstream *x; int batches; int rc; } wk;
static void *writer(void *p) {
  wk *w = p;
  for (int i = 0; i < w->batches; i++)
    if (xs_write(w->x, batch, sizeof batch)) { w->rc = 1; return NULL; }
  return NULL;
}

int main(void) {
  g_wake_closed = 1;                     /* the door is shut: mgr is never touched */
  memset(batch, 'x', sizeof batch);

  /* 1. commit threshold, framing, terminator */
  { struct mg_connection *c = fake_conn(1);
    xstream *x = new_stream(c);
    assert(xs_write(x, "a,b\r\n", 5) == 0);
    xs_on_io(c);
    assert(c->send.len == 0 && "under one batch and not done: nothing committed");
    assert(xs_write(x, batch, sizeof batch) == 0);
    xs_on_io(c);
    assert(has(c, "HTTP/1.1 200 OK\r\nContent-Type: text/csv\r\nTransfer-Encoding: chunked\r\n\r\n"));
    assert(has(c, "5\r\na,b\r\n\r\n") && has(c, "10000\r\nxxxx"));
    assert(!ends_with_terminator(c));
    finish(x, 200, 0);
    xs_on_io(c);
    assert(ends_with_terminator(c) && !c->is_draining);
    assert(g_xs_live == NULL && "a finished stream leaves the registry");
    drop_conn(c);
    printf("  commit after a full batch, chunk framing, terminator: ok\n"); }

  /* 2. a refusal before the commit keeps its status and its body */
  { struct mg_connection *c = fake_conn(2);
    xstream *x = new_stream(c);
    const char *msg = "{\"error\":\"sort_not_honoured\"}\n";
    assert(xs_write(x, msg, strlen(msg)) == 0);
    finish(x, 400, 1);
    xs_on_io(c);
    assert(has(c, "HTTP/1.1 400") && has(c, "sort_not_honoured"));
    assert(!has(c, "200 OK") && !has(c, "chunked"));
    assert(g_xs_live == NULL);
    drop_conn(c);
    printf("  refusal answered with its own status and body, never a 200: ok\n"); }

  /* 3. stopped early after the commit: no terminator, connection closes */
  { struct mg_connection *c = fake_conn(3);
    xstream *x = new_stream(c);
    assert(xs_write(x, batch, sizeof batch) == 0);
    xs_on_io(c);
    assert(has(c, "200 OK"));
    finish(x, 200, 1);
    xs_on_io(c);
    assert(!ends_with_terminator(c) && c->is_draining);
    drop_conn(c);
    printf("  short walk after the commit ends without the terminator: ok\n"); }

  /* 4. backpressure: a 6 MB export never holds more than the high-water mark */
  { struct mg_connection *c = fake_conn(4);
    xstream *x = new_stream(c);
    wk w = { x, 96, 0 };                 /* 96 x 64 KB = 6 MB */
    pthread_t th;
    assert(pthread_create(&th, NULL, writer, &w) == 0);
    size_t peak = 0, drained = 0;
    for (int spins = 0; spins < 200000; spins++) {
      xs_on_io(c);
      pthread_mutex_lock(&x->mu);
      size_t held = x->queued + c->send.len;
      int q = x->queued != 0;
      pthread_mutex_unlock(&x->mu);
      if (held > peak) peak = held;
      if (c->send.len) {                 /* the socket takes up to 256 KB */
        size_t n = c->send.len < 262144 ? c->send.len : 262144;
        mg_iobuf_del(&c->send, 0, n);
        drained += n;
      }
      if (!q && c->send.len == 0) {
        struct timespec ts = { 0, 1000000 };
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&x->mu);
        int idle = x->queued == 0;
        pthread_mutex_unlock(&x->mu);
        if (idle && drained >= 96u * sizeof batch) break;
      }
    }
    pthread_join(th, NULL);
    assert(w.rc == 0);
    assert(drained >= 96u * sizeof batch && "every byte reached the socket");
    assert(peak <= XS_HIGH_WATER + 2 * (sizeof batch + 16) &&
           "held bytes are bounded by the high-water mark, not the export");
    finish(x, 200, 0);
    xs_on_io(c);
    assert(ends_with_terminator(c));
    drop_conn(c);
    printf("  6 MB through a %u KB high-water mark: peak held %zu KB: ok\n",
           XS_HIGH_WATER / 1024, peak / 1024); }

  /* 5. hang-up while the worker waits on the high-water mark */
  { struct mg_connection *c = fake_conn(5);
    xstream *x = new_stream(c);
    wk w = { x, 1000, 0 };               /* far more than ever fits */
    pthread_t th;
    assert(pthread_create(&th, NULL, writer, &w) == 0);
    for (int i = 0; i < 2000; i++) {     /* pump, never drain: the peer stopped */
      xs_on_io(c);
      struct timespec ts = { 0, 1000000 };
      nanosleep(&ts, NULL);
    }
    assert(c->send.len <= XS_HIGH_WATER + 2 * (sizeof batch + 16));
    xs_on_close(c->id);                  /* MG_EV_CLOSE */
    pthread_join(th, NULL);              /* returns: the wait was broken */
    assert(w.rc == 1 && "the writer told the walk to stop");
    assert(g_xs_live == NULL);
    finish(x, 200, 1);                   /* the worker's share; frees the stream */
    drop_conn(c);
    printf("  hang-up unblocks a worker waiting on the high-water mark: ok\n"); }

  printf("\nall passed\n");
  return 0;
}
