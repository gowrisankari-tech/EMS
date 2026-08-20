/*
 * Integration test suite for the EMS C console app.
 *
 * Each test runs the compiled ../ems binary in a fresh temp working
 * directory (so data files start empty every time), feeds it a scripted
 * sequence of stdin lines exactly as a user would type them (including a
 * blank line for every "Press Enter to continue..." prompt), and asserts
 * that specific substrings appear (or don't appear) in stdout.
 *
 * Build/run:
 *   make test-integration
 * or directly:
 *   gcc -Wall -Wextra -g -o tests/test_ems_integration tests/test_ems_integration.c
 *   ./tests/test_ems_integration
 * (run `make` first so ../ems exists)
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

static char ROOT[PATH_MAX];
static char BINARY[PATH_MAX + 16];

static int passed = 0;
static int failed = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Runs ../ems in `workdir`, feeding it session_lines (NULL-terminated
   array of strings) as stdin, one per line. Returns a malloc'd buffer
   containing everything the process wrote to stdout; caller frees it. */
static char *run_session(const char *session_lines[], const char *workdir) {
    char stdin_path[PATH_MAX];
    snprintf(stdin_path, sizeof(stdin_path), "%s/.stdin_data", workdir);

    FILE *f = fopen(stdin_path, "w");
    if (!f) { perror("fopen stdin_path"); exit(1); }
    for (int i = 0; session_lines[i] != NULL; i++)
        fprintf(f, "%s\n", session_lines[i]);
    fclose(f);

    char cmd[PATH_MAX * 4];
    snprintf(cmd, sizeof(cmd), "cd '%s' && '%s' < '%s' 2>/dev/null",
             workdir, BINARY, stdin_path);

    FILE *p = popen(cmd, "r");
    if (!p) { perror("popen"); exit(1); }

    size_t cap = 65536, len = 0;
    char *buf = malloc(cap);
    buf[0] = '\0';
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), p)) > 0) {
        if (len + n + 1 > cap) {
            cap *= 2;
            buf = realloc(buf, cap);
        }
        memcpy(buf + len, chunk, n);
        len += n;
        buf[len] = '\0';
    }
    pclose(p);
    return buf;
}

static char *fresh_dir(void) {
    static char template[] = "/tmp/ems_test_XXXXXX";
    char *tmpl = strdup(template);
    char *d = mkdtemp(tmpl);
    if (!d) { perror("mkdtemp"); exit(1); }
    return d; /* caller must free */
}

static void rmtree(const char *dir) {
    char cmd[PATH_MAX + 16];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", dir);
    system(cmd);
}

/* Checks that every string in must_contain appears in output, and none
   of must_not_contain does. Either array may be NULL-terminated and
   empty (single NULL element). */
static void check(const char *name, const char *output,
                   const char *must_contain[], const char *must_not_contain[]) {
    int ok = 1;
    for (int i = 0; must_contain && must_contain[i]; i++) {
        if (!strstr(output, must_contain[i])) {
            printf("        expected to find: \"%s\"\n", must_contain[i]);
            ok = 0;
        }
    }
    for (int i = 0; must_not_contain && must_not_contain[i]; i++) {
        if (strstr(output, must_not_contain[i])) {
            printf("        expected NOT to find: \"%s\"\n", must_not_contain[i]);
            ok = 0;
        }
    }
    if (ok) { passed++; printf("[PASS] %s\n", name); }
    else    { failed++; printf("[FAIL] %s\n", name); }
}

/* Creates Department 1 (Engineering), Employee 1 (John Doe), and an
   EMPLOYEE-role login 'jdoe' linked to Employee 1. */
static void seed_base(const char *workdir) {
    const char *lines[] = {
        "admin", "admin123",
        "6", "Engineering", "",
        "1", "John", "Doe", "john@example.com", "9876543210", "1", "Engineer", "2024-01-15", "",
        "23", "jdoe", "pass1234", "EMPLOYEE", "1", "",
        "0", NULL
    };
    char *out = run_session(lines, workdir);
    free(out);
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

static void test_login(void) {
    char *d = fresh_dir();
    const char *lines[] = { "admin", "admin123", "0", NULL };
    char *out = run_session(lines, d);
    const char *mc[] = { "Welcome, admin! Role: Admin", NULL };
    check("default admin login succeeds", out, mc, NULL);
    free(out); rmtree(d); free(d);

    d = fresh_dir();
    const char *lines2[] = { "admin", "wrong1", "admin", "wrong2", "admin", "wrong3", NULL };
    out = run_session(lines2, d);
    const char *mc2[] = { "Too many failed attempts. Exiting.", NULL };
    check("3 failed logins lock out", out, mc2, NULL);
    free(out); rmtree(d); free(d);
}

static void test_employee_validation(void) {
    char *d = fresh_dir();
    const char *lines[] = {
        "admin", "admin123",
        "1", "Alice", "Smith", "not-an-email", "alice@example.com",
        "notaphone", "5551234567", "0", "Analyst", "2024-02-01", "",
        "0", NULL
    };
    char *out = run_session(lines, d);
    const char *mc[] = { "Enter a valid email address", "Employee added with ID 1.", NULL };
    check("invalid email is rejected and re-prompted", out, mc, NULL);
    free(out); rmtree(d); free(d);
}

static void test_duplicate_department(void) {
    char *d = fresh_dir();
    const char *lines[] = {
        "admin", "admin123",
        "6", "Engineering", "",
        "6", "engineering", "Sales", "",
        "0", NULL
    };
    char *out = run_session(lines, d);
    const char *mc[] = { "already exists", "Department added with ID 2.", NULL };
    check("duplicate department name (case-insensitive) rejected", out, mc, NULL);
    free(out); rmtree(d); free(d);
}

static void test_rbac_scoping(void) {
    char *d = fresh_dir();
    seed_base(d);
    const char *lines[] = {
        "jdoe", "pass1234",
        "1", "",
        "1", "",
        "0", NULL
    };
    char *out = run_session(lines, d);
    const char *mc[] = { "Checked in at", "already have an attendance record for today", NULL };
    check("employee check-in is session-scoped (no ID prompt) and blocks duplicates", out, mc, NULL);
    const char *mnc[] = { "Employee ID:", NULL };
    check("employee menu never asks 'Employee ID:'", out, NULL, mnc);
    free(out); rmtree(d); free(d);
}

static void test_leave(void) {
    char *d = fresh_dir();
    seed_base(d);
    const char *lines[] = {
        "jdoe", "pass1234",
        "4", "Vacation", "Sick", "2024-03-40", "2024-03-05", "2024-03-10", "",
        "6", "",
        "0", NULL
    };
    char *out = run_session(lines, d);
    const char *mc[] = {
        "Invalid leave type", "Invalid date format",
        "submitted (6 day(s)), status: Pending",
        "Remaining leave balance: 20 day(s)", NULL
    };
    check("invalid leave type rejected, invalid date rejected, valid leave applied", out, mc, NULL);
    free(out); rmtree(d); free(d);

    d = fresh_dir();
    seed_base(d);
    const char *apply_lines[] = {
        "jdoe", "pass1234", "4", "Sick", "2024-03-05", "2024-03-10", "", "0", NULL
    };
    char *tmp = run_session(apply_lines, d);
    free(tmp);

    const char *approve_lines[] = { "admin", "admin123", "11", "1", "approved", "", "0", NULL };
    out = run_session(approve_lines, d);
    const char *mc2[] = { "Leave request 1 marked as Approved", NULL };
    check("lowercase leave decision normalizes to Approved", out, mc2, NULL);
    free(out);

    const char *balance_lines[] = { "jdoe", "pass1234", "6", "", "0", NULL };
    char *out2 = run_session(balance_lines, d);
    const char *mc3[] = { "Remaining leave balance: 14 day(s)", NULL };
    check("approved leave reduces balance by inclusive day count (6 days)", out2, mc3, NULL);
    free(out2); rmtree(d); free(d);
}

static void test_duplicate_username(void) {
    char *d = fresh_dir();
    const char *lines[] = {
        "admin", "admin123",
        "23", "bob", "secret1", "HR", "0", "",
        "23", "bob", "bob2", "secret2", "EMPLOYEE", "0", "",
        "0", NULL
    };
    char *out = run_session(lines, d);
    const char *mc[] = { "already taken", "User 'bob2' created with ID 3", NULL };
    check("duplicate username rejected on registration", out, mc, NULL);
    free(out); rmtree(d); free(d);
}

static void test_payroll(void) {
    char *d = fresh_dir();
    seed_base(d);
    const char *lines[] = {
        "admin", "admin123",
        "22", "2024-04-01", "",
        "12", "1", "2024-04", "3000", "",
        "0", NULL
    };
    char *out = run_session(lines, d);
    const char *mc[] = {
        "Marked 1 employee(s) Absent for 2024-04-01",
        "1 absent day(s) -> deduction Rs.100.00 -> net pay Rs.2900.00", NULL
    };
    check("absentee marking + payroll deduction works end to end", out, mc, NULL);
    free(out); rmtree(d); free(d);

    d = fresh_dir();
    seed_base(d);
    const char *lines2[] = {
        "admin", "admin123",
        "12", "1", "2024-04", "3000", "",
        "12", "1", "2024-04", "4000", "",
        "13", "",
        "0", NULL
    };
    out = run_session(lines2, d);
    const char *mc2[] = { "already existed — regenerating it", NULL };
    check("regenerating payroll for same employee+month replaces, not duplicates", out, mc2, NULL);
    const char *mc3[] = { "4000.00", NULL };
    check("only one payroll row remains for employee 1 after regeneration", out, mc3, NULL);
    free(out); rmtree(d); free(d);
}

static void test_cascade_delete(void) {
    char *d = fresh_dir();
    seed_base(d);
    const char *setup1[] = {
        "jdoe", "pass1234", "1", "", "4", "Sick", "2024-03-05", "2024-03-05", "", "0", NULL
    };
    char *tmp = run_session(setup1, d); free(tmp);

    const char *setup2[] = { "admin", "admin123", "12", "1", "2024-03", "1000", "", "0", NULL };
    tmp = run_session(setup2, d); free(tmp);

    const char *lines[] = {
        "admin", "admin123",
        "4", "1", "y", "",
        "9", "",
        "10", "",
        "13", "",
        "0", NULL
    };
    char *out = run_session(lines, d);
    const char *mc[] = {
        "Employee 1 and all related records deleted",
        "(no records)", "(no leave requests)", "(no payslips)", NULL
    };
    check("deleting an employee cascades and removes dependent records", out, mc, NULL);
    free(out); rmtree(d); free(d);
}

static void test_notification_ownership(void) {
    char *d = fresh_dir();
    seed_base(d);
    const char *setup1[] = {
        "admin", "admin123",
        "1", "Bob", "Lee", "bob@example.com", "5551234567", "1", "QA", "2024-01-01", "",
        "23", "blee", "pass1234", "EMPLOYEE", "2", "",
        "0", NULL
    };
    char *tmp = run_session(setup1, d); free(tmp);

    const char *setup2[] = { "jdoe", "pass1234", "4", "Sick", "2024-03-05", "2024-03-05", "", "0", NULL };
    tmp = run_session(setup2, d); free(tmp);

    const char *setup3[] = { "admin", "admin123", "11", "1", "Approved", "", "0", NULL };
    tmp = run_session(setup3, d); free(tmp);

    const char *lines[] = { "blee", "pass1234", "11", "1", "", "0", NULL };
    char *out = run_session(lines, d);
    const char *mc[] = { "Notification ID 1 not found", NULL };
    check("an employee cannot mark another employee's notification as read", out, mc, NULL);
    free(out); rmtree(d); free(d);
}

static void test_document_upload_sanitization(void) {
    char *d = fresh_dir();
    seed_base(d);

    char src[PATH_MAX];
    snprintf(src, sizeof(src), "%s/resume.txt", d);
    FILE *f = fopen(src, "w");
    fprintf(f, "dummy resume contents");
    fclose(f);

    const char *lines[] = { "jdoe", "pass1234", "9", src, "../../../etc/passwd", "0", NULL };
    char *out = run_session(lines, d);
    const char *mc[] = { "Document uploaded and stored at:", NULL };
    const char *mnc[] = { "etc/passwd", NULL };
    check("path-traversal filename is sanitized before use", out, mc, mnc);

    char escaped_path[PATH_MAX];
    snprintf(escaped_path, sizeof(escaped_path), "%s/etc/passwd", d);
    int escaped = (access(escaped_path, F_OK) == 0);
    check("no file was actually written outside data/docs/",
          escaped ? "escaped=True" : "escaped=False",
          (const char*[]){ "escaped=False", NULL }, NULL);

    free(out); rmtree(d); free(d);
}

static void test_menu_robustness(void) {
    char *d = fresh_dir();
    const char *lines[] = { "admin", "admin123", "banana", "17", "0", NULL };
    char *out = run_session(lines, d);
    const char *mc[] = { "Invalid number, try again.", "DASHBOARD", NULL };
    check("non-numeric menu choice is rejected cleanly, not undefined behaviour", out, mc, NULL);
    free(out); rmtree(d); free(d);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(void) {
    /* Resolve ROOT (project root, one level up from tests/) and BINARY
       (../ems) the same way the Python version derives them. */
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    char tests_dir[PATH_MAX];
    if (len != -1) {
        exe_path[len] = '\0';
        strncpy(tests_dir, exe_path, sizeof(tests_dir));
        char *slash = strrchr(tests_dir, '/');
        if (slash) *slash = '\0';
    } else {
        /* Fallback: assume run from the tests/ directory. */
        getcwd(tests_dir, sizeof(tests_dir));
    }
    char cmd[PATH_MAX * 3];
    snprintf(cmd, sizeof(cmd), "cd '%s/..' && pwd", tests_dir);
    FILE *p = popen(cmd, "r");
    if (p && fgets(ROOT, sizeof(ROOT), p)) {
        ROOT[strcspn(ROOT, "\n")] = '\0';
    }
    if (p) pclose(p);
    snprintf(BINARY, sizeof(BINARY), "%s/ems", ROOT);

    if (access(BINARY, X_OK) != 0) {
        fprintf(stderr, "Cannot find or execute %s — run `make` first.\n", BINARY);
        return 1;
    }

    test_login();
    test_employee_validation();
    test_duplicate_department();
    test_rbac_scoping();
    test_leave();
    test_duplicate_username();
    test_payroll();
    test_cascade_delete();
    test_notification_ownership();
    test_document_upload_sanitization();
    test_menu_robustness();

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
