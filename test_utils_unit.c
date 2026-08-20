/*
 * Unit tests for pure/isolatable logic in the EMS C project.
 *
 * Unlike tests/test_ems_integration.c (which drives the compiled ./ems
 * binary through stdin like a user), this file links directly against
 * the project's own .o files and calls functions in-process:
 *   - utils.c   : valid_email, valid_date, str_ieq, sanitize_filename,
 *                 hash_password, next_id, file_exists
 *   - leave.c   : leave_days_between
 *
 * Build/run:
 *   make test-unit
 * (the Makefile target builds every src object except main.o, then links
 *  this file against them and runs the resulting binary)
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../src/common.h"
#include "../src/leave.h"

static int passed = 0;
static int failed = 0;

#define CHECK(name, cond) do { \
    if (cond) { passed++; printf("[PASS] %s\n", name); } \
    else      { failed++; printf("[FAIL] %s\n", name); } \
} while (0)

#define CHECK_INT_EQ(name, actual, expected) do { \
    int _a = (actual), _e = (expected); \
    if (_a == _e) { passed++; printf("[PASS] %s\n", name); } \
    else { failed++; printf("[FAIL] %s (got %d, expected %d)\n", name, _a, _e); } \
} while (0)

#define CHECK_STR_EQ(name, actual, expected) do { \
    const char *_a = (actual), *_e = (expected); \
    if (strcmp(_a, _e) == 0) { passed++; printf("[PASS] %s\n", name); } \
    else { failed++; printf("[FAIL] %s (got %s, expected %s)\n", name, _a, _e); } \
} while (0)

/* ------------------------------------------------------------------ */
/* valid_email                                                        */
/* ------------------------------------------------------------------ */
static void test_valid_email(void) {
    CHECK("valid_email accepts a normal address", valid_email("john@example.com"));
    CHECK("valid_email rejects missing @", !valid_email("not-an-email"));
    CHECK("valid_email rejects @ as first character", !valid_email("@example.com"));
    CHECK("valid_email rejects missing dot after @", !valid_email("john@examplecom"));
    CHECK("valid_email rejects dot immediately after @", !valid_email("john@.com"));
    CHECK("valid_email rejects nothing after the dot", !valid_email("john@example."));
}

/* ------------------------------------------------------------------ */
/* valid_date                                                         */
/* ------------------------------------------------------------------ */
static void test_valid_date(void) {
    CHECK("valid_date accepts a well-formed date", valid_date("2024-03-05"));
    CHECK("valid_date rejects wrong length", !valid_date("2024-3-5"));
    CHECK("valid_date rejects missing dashes", !valid_date("20240305!!"));
    CHECK("valid_date rejects non-digit characters", !valid_date("202a-03-05"));
    CHECK("valid_date rejects month 00", !valid_date("2024-00-05"));
    CHECK("valid_date rejects month 13", !valid_date("2024-13-05"));
    CHECK("valid_date rejects day 00", !valid_date("2024-03-00"));
    CHECK("valid_date rejects day 32", !valid_date("2024-03-32"));
    CHECK("valid_date rejects year below 1900", !valid_date("1899-03-05"));
    CHECK("valid_date rejects null input", !valid_date(NULL));
}

/* ------------------------------------------------------------------ */
/* str_ieq (case-insensitive equality)                                 */
/* ------------------------------------------------------------------ */
static void test_str_ieq(void) {
    CHECK("str_ieq matches identical strings", str_ieq("Sick", "Sick"));
    CHECK("str_ieq matches different case", str_ieq("engineering", "Engineering"));
    CHECK("str_ieq rejects different content", !str_ieq("Sick", "Casual"));
    CHECK("str_ieq rejects a differing-length prefix match", !str_ieq("Sick", "Sickly"));
}

/* ------------------------------------------------------------------ */
/* sanitize_filename (path traversal defense)                          */
/* ------------------------------------------------------------------ */
static void test_sanitize_filename(void) {
    char buf[256];

    strcpy(buf, "../../../etc/passwd");
    sanitize_filename(buf);
    CHECK("sanitize_filename strips traversal to a plain filename", strstr(buf, "..") == NULL);
    CHECK("sanitize_filename strips traversal to a plain filename (no slash)", strchr(buf, '/') == NULL);

    strcpy(buf, "resume.txt");
    sanitize_filename(buf);
    CHECK_STR_EQ("sanitize_filename leaves an already-safe name untouched", buf, "resume.txt");

    strcpy(buf, "..");
    sanitize_filename(buf);
    CHECK_STR_EQ("sanitize_filename falls back to 'file' when nothing survives", buf, "file");

    strcpy(buf, "a\\b/c.txt");
    sanitize_filename(buf);
    CHECK("sanitize_filename removes backslashes and forward slashes",
          strchr(buf, '/') == NULL && strchr(buf, '\\') == NULL);
}

/* ------------------------------------------------------------------ */
/* hash_password                                                       */
/* ------------------------------------------------------------------ */
static void test_hash_password(void) {
    CHECK_INT_EQ("hash_password is deterministic for the same input",
                 (int)hash_password("pass1234"), (int)hash_password("pass1234"));
    CHECK("hash_password differs for different input",
          hash_password("pass1234") != hash_password("pass12345"));
}

/* ------------------------------------------------------------------ */
/* leave_days_between (inclusive day count)                            */
/* ------------------------------------------------------------------ */
static void test_leave_days_between(void) {
    CHECK_INT_EQ("leave_days_between same day counts as 1",
                 leave_days_between("2024-03-05", "2024-03-05"), 1);
    CHECK_INT_EQ("leave_days_between is inclusive of both endpoints",
                 leave_days_between("2024-03-05", "2024-03-10"), 6);
    CHECK_INT_EQ("leave_days_between spans a month boundary correctly",
                 leave_days_between("2024-01-30", "2024-02-02"), 4);
}

/* ------------------------------------------------------------------ */
/* next_id / file_exists (small file-backed helpers)                   */
/* ------------------------------------------------------------------ */
static void test_next_id_and_file_exists(void) {
    const char *path = "unit_test_scratch.txt";
    remove(path);

    CHECK("file_exists is false for a file that hasn't been created", !file_exists(path));
    CHECK_INT_EQ("next_id starts at 1 for a missing file", next_id(path), 1);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "1|John|Doe\n3|Bob|Lee\n2|Alice|Smith\n");
    fclose(fp);

    CHECK("file_exists is true once the file has been written", file_exists(path));
    CHECK_INT_EQ("next_id returns max(id)+1 regardless of row order", next_id(path), 4);

    remove(path);
}

int main(void) {
    /* Run from a scratch directory so file-backed tests never touch the
       real data/ directory or leave stray files behind in the repo. */
    system("rm -rf /tmp/ems_unit_test_scratch && mkdir -p /tmp/ems_unit_test_scratch");
    if (chdir("/tmp/ems_unit_test_scratch") != 0) {
        perror("chdir");
        return 1;
    }

    test_valid_email();
    test_valid_date();
    test_str_ieq();
    test_sanitize_filename();
    test_hash_password();
    test_leave_days_between();
    test_next_id_and_file_exists();

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
