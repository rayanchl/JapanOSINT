/* tests/unit/test_hostgate.c — the per-host politeness table and the camera
 * destination policy in core/hostgate.c. No network (one .invalid lookup).
 *
 * WHAT IS BEING PROTECTED.
 *   1. Capacity. The table held 1,024 hosts and never evicted; past that every
 *      new host was fetched UNGATED and the heartbeat said nothing. The fleet
 *      names ~6,500 hosts. Now >20,000 distinct hosts are all gated, with idle
 *      slots reclaimed.
 *   2. Reclaim safety. acquire() keeps a pointer to its slot across cond
 *      waits, so a slot with a request in flight or a thread parked on it must
 *      never be handed to another host.
 *   3. Fail-open is COUNTED. A table genuinely full of live hosts still fails
 *      open — and shows up in hostgate_counters()' over_budget.
 *   4. release() of a host with no slot does not claim one.
 *   5. hostgate_penalize() delays the next start, capped at 60 s.
 *   6. The camera policy: strict unless JO_CAMERA_ALLOW_LAN=1; metadata and
 *      link-local refused regardless; JO_HTTP_BLOCK_PRIVATE=1 wins.
 *
 * Includes hostgate.c to reach the table and the test clock. */

#include "../../core/hostgate.c"

#include <assert.h>

static void reset_table(void) {
  pthread_mutex_lock(&g_mu);
  memset(g_slots, 0, sizeof g_slots);
  memset(g_bucket, 0, sizeof g_bucket);
  g_used = 0; g_hand = 0; g_full_until_ms = 0;
  g_waits = g_timeouts = g_inflight = 0;
  g_table_full = g_evictions = g_penalties = g_hosts = 0;
  pthread_mutex_unlock(&g_mu);
}

static void host_url(char *out, size_t cap, const char *tag, int i) {
  snprintf(out, cap, "https://h%d.%s.example/path?q=1", i, tag);
}

static hg_slot *lookup(const char *host) {
  pthread_mutex_lock(&g_mu);
  hg_slot *s = slot_lookup(host);
  pthread_mutex_unlock(&g_mu);
  return s;
}

/* 1. 20,000+ distinct hosts, each acquired and released, the clock moving
 * 10 ms per host: every single one is gated, and slots get reclaimed. */
static void test_many_hosts_all_gated(void) {
  reset_table();
  char u[256];
  const int N = 40000;
  for (int i = 0; i < N; i++) {
    host_url(u, sizeof u, "many", i);
    int got = hostgate_acquire(u, 1000);
    assert(got == 1);
    hostgate_release(u);
    g_clock_skew_ms += 10;
  }
  hostgate_stats_t st;
  hostgate_stats(&st);
  assert(st.table_full == 0);
  assert(st.timeouts == 0);
  assert(st.inflight == 0);
  assert(st.capacity == HG_SLOTS);
  assert(st.hosts <= HG_SLOTS);
  assert(st.evictions >= N - HG_SLOTS);
  /* The most recent host is still findable and its slot intact. */
  host_url(u, sizeof u, "many", N - 1);
  char h[HG_HOSTLEN];
  assert(hostgate_url_host(u, h, sizeof h));
  assert(lookup(h) != NULL);
  printf("  %d distinct hosts gated, %ld evictions, %ld resident\n",
         N, st.evictions, st.hosts);
}

/* 2. In-flight and waited-on slots survive any amount of churn. */
static void test_reclaim_never_takes_live_slot(void) {
  reset_table();
  const char *held = "https://held.example/x";
  const char *parked = "https://parked.example/x";
  assert(hostgate_acquire(held, 1000) == 1);              /* stays in flight */
  assert(hostgate_acquire(parked, 1000) == 1);
  hostgate_release(parked);
  hg_slot *ps = lookup("parked.example");
  assert(ps);
  pthread_mutex_lock(&g_mu); ps->waiters = 1; pthread_mutex_unlock(&g_mu);
  hg_slot *hs = lookup("held.example");
  assert(hs && hs->in_flight == 1);

  char u[256];
  for (int i = 0; i < 3 * HG_SLOTS; i++) {
    host_url(u, sizeof u, "churn", i);
    assert(hostgate_acquire(u, 1000) == 1);
    hostgate_release(u);
    g_clock_skew_ms += 10;
  }
  /* Same slot, same host, still in flight / still pinned. */
  assert(lookup("held.example") == hs);
  assert(strcmp(hs->host, "held.example") == 0 && hs->in_flight == 1);
  assert(lookup("parked.example") == ps);
  assert(strcmp(ps->host, "parked.example") == 0 && ps->waiters == 1);
  pthread_mutex_lock(&g_mu); ps->waiters = 0; pthread_mutex_unlock(&g_mu);
  hostgate_release(held);
  assert(hs->in_flight == 0);
}

/* 3. A table full of LIVE hosts fails open — counted, and cheaply. */
static void test_full_table_fail_open_counted(void) {
  reset_table();
  char u[256];
  for (int i = 0; i < HG_SLOTS; i++) {
    host_url(u, sizeof u, "live", i);
    assert(hostgate_acquire(u, 1000) == 1);               /* never released */
  }
  long w0, t0, f0;
  hostgate_counters(&w0, &t0, &f0);
  assert(t0 == 0 && f0 == HG_SLOTS);

  assert(hostgate_acquire("https://one-too-many.example/", 1000) == 0);
  assert(hostgate_acquire("https://two-too-many.example/", 1000) == 0);
  hostgate_stats_t st;
  hostgate_stats(&st);
  assert(st.table_full == 2);
  long w, t, f;
  hostgate_counters(&w, &t, &f);
  assert(t == 2);                     /* over_budget on the heartbeat sees it */
  /* The second refusal did not sweep: the rescan holdoff was armed. */
  assert(g_full_until_ms > mono_ms());

  /* 4. release() for a host that never got a slot must not claim one. */
  long hosts_before = st.hosts;
  hostgate_release("https://one-too-many.example/");
  hostgate_stats(&st);
  assert(st.hosts == hosts_before);
  assert(lookup("one-too-many.example") == NULL);

  for (int i = 0; i < HG_SLOTS; i++) {
    host_url(u, sizeof u, "live", i);
    hostgate_release(u);
  }
  hostgate_stats(&st);
  assert(st.inflight == 0);
}

/* 5. Penalties delay the next start and are capped. */
static void test_penalize(void) {
  reset_table();
  const char *u = "https://busy.example/api";
  hostgate_penalize(u, 200);
  struct timespec a, b;
  clock_gettime(CLOCK_MONOTONIC, &a);
  assert(hostgate_acquire(u, 50) == 1);   /* waits the penalty, not 50 ms   */
  clock_gettime(CLOCK_MONOTONIC, &b);
  long el = (long)((b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000);
  assert(el >= 150);
  hostgate_release(u);

  hostgate_penalize(u, 3600L * 1000L);    /* an hour asked → a minute kept  */
  hg_slot *s = lookup("busy.example");
  assert(s);
  long long left = s->penalty_until_ms - mono_ms();
  assert(left > 0 && left <= HG_PENALTY_MAX_MS);
  /* A penalised slot is not reclaimable while the penalty runs. */
  assert(!slot_evictable(s, mono_ms() + HG_PENALTY_MAX_MS));
  hostgate_stats_t st;
  hostgate_stats(&st);
  assert(st.penalties == 2);
  /* Loopback is exempt from the gate, so it is never penalised either. */
  hostgate_penalize("http://127.0.0.1:8080/v1", 1000);
  hostgate_stats(&st);
  assert(st.penalties == 2);
}

/* 6. Camera destination policy. */
static void test_camera_policy(void) {
  unsetenv("JO_HTTP_BLOCK_PRIVATE");
  unsetenv("JO_CAMERA_ALLOW_LAN");
  assert(!hostgate_camera_lan_allowed());
  assert(hostgate_camera_url_check("rtsp://127.0.0.1:554/live", 0) == HG_URL_PRIVATE);
  assert(hostgate_camera_url_check("http://192.168.1.10/snap.jpg", 0) == HG_URL_PRIVATE);
  assert(hostgate_camera_url_check("http://10.0.0.5:8080/", 0) == HG_URL_PRIVATE);
  assert(hostgate_camera_url_check("http://[::1]/", 0) == HG_URL_PRIVATE);
  assert(hostgate_camera_url_check("http://localhost/", 0) == HG_URL_PRIVATE);
  assert(hostgate_camera_url_check("http://cam.localhost/", 0) == HG_URL_PRIVATE);
  assert(hostgate_camera_url_check("http://169.254.169.254/latest/", 0) == HG_URL_PRIVATE);
  assert(hostgate_camera_url_check("http://metadata.google.internal/", 0) == HG_URL_PRIVATE);
  assert(hostgate_camera_url_check("http://8.8.8.8/x.jpg", 0) == HG_URL_OK);
  assert(hostgate_camera_url_check("rtsps://93.184.216.34/s", 0) == HG_URL_OK);
  assert(hostgate_camera_url_check("8.8.8.8/x.jpg", 0) == HG_URL_BAD_SCHEME);
  assert(hostgate_camera_url_check("ht tp://8.8.8.8/", 0) == HG_URL_BAD_SCHEME);
  assert(hostgate_camera_url_check("http:///nohost", 0) == HG_URL_BAD_HOST);
  /* resolve=1: a name that does not resolve is refused, never waved through */
  assert(hostgate_camera_url_check("rtsp://cam.invalid/s", 1) == HG_URL_BAD_HOST);
  assert(hostgate_camera_addr_check("10.1.2.3") == HG_URL_PRIVATE);
  assert(hostgate_camera_addr_check("127.0.0.1") == HG_URL_PRIVATE);
  assert(hostgate_camera_addr_check("8.8.8.8") == HG_URL_OK);
  /* The general floor is unchanged: llama-server on loopback still works. */
  assert(hostgate_url_check("http://127.0.0.1:8080/v1") == HG_URL_OK);

  setenv("JO_CAMERA_ALLOW_LAN", "1", 1);
  assert(hostgate_camera_lan_allowed());
  assert(hostgate_camera_url_check("http://192.168.1.10/snap.jpg", 0) == HG_URL_OK);
  assert(hostgate_camera_url_check("rtsp://127.0.0.1:554/live", 0) == HG_URL_OK);
  assert(hostgate_camera_addr_check("10.1.2.3") == HG_URL_OK);
  /* opt-in is LAN, never metadata / link-local */
  assert(hostgate_camera_url_check("http://169.254.169.254/", 0) == HG_URL_PRIVATE);
  assert(hostgate_camera_url_check("http://metadata.google.internal/", 0) == HG_URL_PRIVATE);
  assert(hostgate_camera_addr_check("169.254.169.254") == HG_URL_PRIVATE);
  assert(hostgate_camera_addr_check("fe80::1") == HG_URL_PRIVATE);

  setenv("JO_CAMERA_ALLOW_LAN", "0", 1);
  assert(!hostgate_camera_lan_allowed());

  /* JO_HTTP_BLOCK_PRIVATE=1 beats the camera opt-in. */
  setenv("JO_CAMERA_ALLOW_LAN", "1", 1);
  setenv("JO_HTTP_BLOCK_PRIVATE", "1", 1);
  assert(hostgate_camera_url_check("http://192.168.1.10/", 0) == HG_URL_PRIVATE);
  assert(hostgate_camera_addr_check("192.168.1.10") == HG_URL_PRIVATE);
  unsetenv("JO_HTTP_BLOCK_PRIVATE");
  unsetenv("JO_CAMERA_ALLOW_LAN");
}

int main(void) {
  /* Shrink nothing: the real TTL and the real capacity, with a fake clock. */
  test_many_hosts_all_gated();
  test_reclaim_never_takes_live_slot();
  test_full_table_fail_open_counted();
  test_penalize();
  test_camera_policy();
  printf("test_hostgate: OK\n");
  return 0;
}
