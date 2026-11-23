/* tests/unit/test_joenv.c — lib/joenv.c, the one typed environment reader.
 *
 * Pins the contract every converted call site relies on: unset/empty -> the
 * default; garbage or trailing garbage -> the default; out of range -> clamped
 * (or the default for the _strict forms); bool spellings; and that the stderr
 * complaint is ONE line per variable name, not one per call. */
#include "../../lib/joenv.c"
#include <assert.h>
#include <unistd.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

static char g_errpath[256];

/* Number of stderr lines mentioning `name` so far. */
static int lines_for(const char *name) {
  fflush(stderr);
  FILE *f = fopen(g_errpath, "r");
  if (!f) return -1;
  char ln[512];
  int n = 0;
  while (fgets(ln, sizeof ln, f)) if (strstr(ln, name)) n++;
  fclose(f);
  return n;
}

int main(void) {
  snprintf(g_errpath, sizeof g_errpath, "%s/jo-joenv-%d.err",
           getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
  assert(freopen(g_errpath, "w", stderr));

  printf("-- integers\n");
  unsetenv("JOT_A");
  ok(jo_env_long("JOT_A", 7, 1, 100) == 7, "unset -> default");
  ok(lines_for("JOT_A") == 0, "unset is silent");
  setenv("JOT_A", "", 1);
  ok(jo_env_long("JOT_A", 7, 1, 100) == 7, "empty -> default");
  ok(lines_for("JOT_A") == 0, "empty is silent");
  setenv("JOT_A", "42", 1);
  ok(jo_env_long("JOT_A", 7, 1, 100) == 42, "in range -> value");
  setenv("JOT_A", "43", 1);
  ok(jo_env_long("JOT_A", 7, 1, 100) == 43, "setenv between calls is seen (no cache)");
  setenv("JOT_A", " 44\n", 1);
  ok(jo_env_long("JOT_A", 7, 1, 100) == 44, "trailing whitespace tolerated");
  ok(lines_for("JOT_A") == 0, "valid values are silent");

  setenv("JOT_B", "abc", 1);
  ok(jo_env_long("JOT_B", 7, 1, 100) == 7, "garbage -> default");
  ok(jo_env_long("JOT_B", 7, 1, 100) == 7, "garbage again -> default");
  ok(lines_for("JOT_B") == 1, "garbage complains ONCE per name, not per call");
  setenv("JOT_C", "12abc", 1);
  ok(jo_env_int("JOT_C", 5, 0, 100) == 5, "trailing garbage -> default (atoi would say 12)");
  ok(lines_for("JOT_C") == 1, "trailing garbage complains once");
  setenv("JOT_C2", "1.5", 1);
  ok(jo_env_int("JOT_C2", 5, 0, 100) == 5, "a fraction is not an integer");
  setenv("JOT_C3", "-", 1);
  ok(jo_env_int("JOT_C3", 5, -9, 9) == 5, "a bare sign is garbage");

  setenv("JOT_D", "-5", 1);
  ok(jo_env_long("JOT_D", 7, 1, 100) == 1, "below min -> clamped to min");
  ok(lines_for("JOT_D") == 1, "clamp complains once");
  ok(jo_env_long("JOT_D", 7, 1, 100) == 1, "clamped again");
  ok(lines_for("JOT_D") == 1, "...still one line");
  setenv("JOT_E", "5000", 1);
  ok(jo_env_long("JOT_E", 7, 1, 100) == 100, "above max -> clamped to max");
  ok(jo_env_int("JOT_E", 7, 1, 100) == 100, "int form clamps too");
  setenv("JOT_F", "99999999999999999999999", 1);
  ok(jo_env_long("JOT_F", 7, 1, 100) == 100, "overflow of the type -> clamped to max");
  setenv("JOT_F2", "-99999999999999999999999", 1);
  ok(jo_env_long("JOT_F2", 7, -50, 100) == -50, "negative overflow -> clamped to min");
  setenv("JOT_G", "0", 1);
  ok(jo_env_long_strict("JOT_G", 7, 1, 100) == 7, "_strict: below min -> default, not the bound");
  setenv("JOT_G2", "101", 1);
  ok(jo_env_int_strict("JOT_G2", 7, 1, 100) == 7, "_strict: above max -> default");
  setenv("JOT_G3", "100", 1);
  ok(jo_env_int_strict("JOT_G3", 7, 1, 100) == 100, "_strict: the bound itself is in range");
  setenv("JOT_H", "9000000000", 1);
  ok(jo_env_ll("JOT_H", 0, 0, LLONG_MAX) == 9000000000LL, "long long carries values past int");
  ok(jo_env_int("JOT_H", 3, 0, INT_MAX) == INT_MAX, "int form clamps a value past int");

  printf("-- doubles\n");
  unsetenv("JOT_X");
  ok(jo_env_double("JOT_X", 0.5, 0.0, 1.0) == 0.5, "double unset -> default");
  setenv("JOT_X", "0.25", 1);
  ok(jo_env_double("JOT_X", 0.5, 0.0, 1.0) == 0.25, "double in range");
  setenv("JOT_X", "2.5", 1);
  ok(jo_env_double("JOT_X", 0.5, 0.0, 1.0) == 1.0, "double above max -> clamped");
  ok(jo_env_double_strict("JOT_X", 0.5, 0.0, 1.0) == 0.5, "double _strict above max -> default");
  setenv("JOT_Y", "0.7x", 1);
  ok(jo_env_double("JOT_Y", 0.5, 0.0, 1.0) == 0.5, "double trailing garbage -> default");
  setenv("JOT_Z", "nan", 1);
  ok(jo_env_double("JOT_Z", 0.5, -DBL_MAX, DBL_MAX) == 0.5, "nan is not a number here");
  setenv("JOT_Z2", "inf", 1);
  ok(jo_env_double("JOT_Z2", 0.5, -DBL_MAX, DBL_MAX) == 0.5, "inf is not a number here");

  printf("-- bool\n");
  static const char *yes[] = {"1", "true", "TRUE", "Yes", "on", "y", "T", NULL};
  static const char *no[]  = {"0", "false", "False", "NO", "off", "n", "f", NULL};
  int yes_ok = 1, no_ok = 1;
  for (int i = 0; yes[i]; i++) { setenv("JOT_BOOL", yes[i], 1); if (jo_env_bool("JOT_BOOL", 0) != 1) yes_ok = 0; }
  for (int i = 0; no[i]; i++)  { setenv("JOT_BOOL", no[i], 1);  if (jo_env_bool("JOT_BOOL", 1) != 0) no_ok = 0; }
  ok(yes_ok, "1/true/TRUE/Yes/on/y/T read as true");
  ok(no_ok, "0/false/False/NO/off/n/f read as false");
  ok(lines_for("JOT_BOOL") == 0, "recognised spellings are silent");
  unsetenv("JOT_BOOL");
  ok(jo_env_bool("JOT_BOOL", 1) == 1 && jo_env_bool("JOT_BOOL", 0) == 0, "bool unset -> default");
  setenv("JOT_BOOL", "", 1);
  ok(jo_env_bool("JOT_BOOL", 1) == 1, "bool empty -> default");
  setenv("JOT_BOOL2", "maybe", 1);
  ok(jo_env_bool("JOT_BOOL2", 1) == 1 && jo_env_bool("JOT_BOOL2", 0) == 0, "bool garbage -> default");
  ok(lines_for("JOT_BOOL2") == 1, "bool garbage complains once");
  setenv("JOT_BOOL3", "  yes \n", 1);
  ok(jo_env_bool("JOT_BOOL3", 0) == 1, "bool tolerates surrounding whitespace");

  printf("-- string\n");
  unsetenv("JOT_S");
  ok(jo_env_str("JOT_S", NULL) == NULL, "str unset -> NULL default");
  ok(!strcmp(jo_env_str("JOT_S", "dflt"), "dflt"), "str unset -> default");
  setenv("JOT_S", "", 1);
  ok(!strcmp(jo_env_str("JOT_S", "dflt"), "dflt"), "str empty -> default");
  setenv("JOT_S", "val", 1);
  ok(!strcmp(jo_env_str("JOT_S", "dflt"), "val"), "str set -> value");

  printf("%s\n", g_fail ? "FAILED" : "all passed");
  unlink(g_errpath);
  return g_fail ? 1 : 0;
}
