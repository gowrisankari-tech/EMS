#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "auth.h"
#include "employee.h"

#define USERS_FILE DATA_DIR "users.txt"

static Role role_from_string(const char *s) {
    if (strcmp(s, "ADMIN") == 0) return ROLE_ADMIN;
    if (strcmp(s, "HR") == 0) return ROLE_HR;
    return ROLE_EMPLOYEE;
}

static int username_exists(const char *username) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) return 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        int id, employee_id;
        char uname[MAX_NAME], role_str[15];
        unsigned long hash;
        if (sscanf(line, "%d|%49[^|]|%lu|%14[^|]|%d", &id, uname, &hash, role_str, &employee_id) == 5) {
            if (str_ieq(uname, username)) { fclose(fp); return 1; }
        }
    }
    fclose(fp);
    return 0;
}

void ensure_default_admin(void) {
    if (file_exists(USERS_FILE)) return;

    FILE *fp = fopen(USERS_FILE, "w");
    if (!fp) { printf("Could not initialize users.txt\n"); return; }

    /* id|username|password_hash|role|employee_id */
    fprintf(fp, "1|admin|%lu|ADMIN|0\n", hash_password("admin123"));
    fclose(fp);
    printf("First run: created default login -> username: admin  password: admin123\n");
    printf("Please change this after logging in.\n");
}

int login(User *out_user) {
    char username[MAX_NAME], password[MAX_NAME];
    printf("Username: ");
    read_line(username, sizeof(username));
    printf("Password: ");
    read_line(password, sizeof(password));

    unsigned long entered_hash = hash_password(password);

    FILE *fp = fopen(USERS_FILE, "r");
    if (!fp) { printf("No users found. Contact Admin.\n"); return 0; }

    char line[MAX_LINE];
    char role_str[15];
    while (fgets(line, sizeof(line), fp)) {
        User u;
        unsigned long stored_hash;
        /* id|username|password_hash|role|employee_id */
        int n = sscanf(line, "%d|%49[^|]|%lu|%14[^|]|%d",
                        &u.id, u.username, &stored_hash, role_str, &u.employee_id);
        if (n != 5) continue;

        if (strcmp(u.username, username) == 0 && stored_hash == entered_hash) {
            u.password_hash = stored_hash;
            u.role = role_from_string(role_str);
            *out_user = u;
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

/* Validates a role string case-insensitively and normalizes it to the
   canonical uppercase form stored on disk. Loops until the user gives one
   of ADMIN/HR/EMPLOYEE instead of silently defaulting unknown input to
   EMPLOYEE (the previous behaviour). */
static void read_valid_role(char *out, int size) {
    char buf[15];
    for (;;) {
        read_nonempty_line("Role (ADMIN / HR / EMPLOYEE): ", buf, sizeof(buf));
        if (str_ieq(buf, "ADMIN")) { snprintf(out, size, "ADMIN"); return; }
        if (str_ieq(buf, "HR")) { snprintf(out, size, "HR"); return; }
        if (str_ieq(buf, "EMPLOYEE")) { snprintf(out, size, "EMPLOYEE"); return; }
        printf("Invalid role. Please type ADMIN, HR, or EMPLOYEE.\n");
    }
}

void register_user(void) {
    char username[MAX_NAME], password[MAX_NAME], role_str[15];

    for (;;) {
        read_nonempty_line("New username: ", username, sizeof(username));
        if (username_exists(username)) {
            printf("Username '%s' is already taken. Choose another.\n", username);
            continue;
        }
        break;
    }

    for (;;) {
        read_nonempty_line("New password: ", password, sizeof(password));
        if (strlen(password) < 4) {
            printf("Password must be at least 4 characters.\n");
            continue;
        }
        break;
    }

    read_valid_role(role_str, sizeof(role_str));

    int employee_id;
    for (;;) {
        employee_id = read_int("Linked Employee ID (0 if none): ");
        if (employee_id == 0 || employee_exists(employee_id)) break;
        printf("Employee ID %d does not exist. Enter 0 if this user has no linked employee record.\n", employee_id);
    }

    int id = next_id(USERS_FILE);
    FILE *fp = fopen(USERS_FILE, "a");
    if (!fp) { printf("Could not open users.txt\n"); return; }
    fprintf(fp, "%d|%s|%lu|%s|%d\n", id, username, hash_password(password), role_str, employee_id);
    fclose(fp);

    printf("User '%s' created with ID %d (role: %s).\n", username, id, role_str);
}
