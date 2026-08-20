#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "leave.h"
#include "employee.h"
#include "notifications.h"

#define LEAVE_FILE DATA_DIR "leave.txt"
#define TMP_FILE DATA_DIR "leave.tmp"
#define ANNUAL_LEAVE_QUOTA 20

static int parse_leave(const char *line, LeaveRequest *l) {
    return sscanf(line, "%d|%d|%14[^|]|%10[^|]|%10[^|]|%14[^\n]",
                  &l->id, &l->employee_id, l->leave_type, l->from_date, l->to_date, l->status) == 6;
}

/* Inclusive day count between two YYYY-MM-DD dates. Exposed via leave.h
   (as leave_days_between) so it can be exercised directly by unit tests
   without going through the interactive apply_leave() flow. */
int leave_days_between(const char *from, const char *to) {
    struct tm tm_from = {0}, tm_to = {0};
    sscanf(from, "%d-%d-%d", &tm_from.tm_year, &tm_from.tm_mon, &tm_from.tm_mday);
    sscanf(to, "%d-%d-%d", &tm_to.tm_year, &tm_to.tm_mon, &tm_to.tm_mday);
    tm_from.tm_year -= 1900; tm_from.tm_mon -= 1;
    tm_to.tm_year -= 1900;   tm_to.tm_mon -= 1;

    time_t t_from = mktime(&tm_from);
    time_t t_to = mktime(&tm_to);
    double diff_seconds = difftime(t_to, t_from);
    int days = (int)(diff_seconds / (60 * 60 * 24)) + 1; /* inclusive */
    return days > 0 ? days : 1;
}

static void read_valid_leave_type(char *out, int size) {
    char buf[15];
    for (;;) {
        read_nonempty_line("Leave type (Sick / Casual / Earned): ", buf, sizeof(buf));
        if (str_ieq(buf, "Sick")) { snprintf(out, size, "Sick"); return; }
        if (str_ieq(buf, "Casual")) { snprintf(out, size, "Casual"); return; }
        if (str_ieq(buf, "Earned")) { snprintf(out, size, "Earned"); return; }
        printf("Invalid leave type. Please type Sick, Casual, or Earned.\n");
    }
}

static void read_valid_date_range(char *from, int from_size, char *to, int to_size) {
    for (;;) {
        read_nonempty_line("From date (YYYY-MM-DD): ", from, from_size);
        if (!valid_date(from)) { printf("Invalid date format.\n"); continue; }
        break;
    }
    for (;;) {
        read_nonempty_line("To date (YYYY-MM-DD): ", to, to_size);
        if (!valid_date(to)) { printf("Invalid date format.\n"); continue; }
        /* YYYY-MM-DD strings sort lexically the same as chronologically,
           so a plain strcmp is a safe and cheap ordering check here. */
        if (strcmp(to, from) < 0) {
            printf("To date cannot be before the From date.\n");
            continue;
        }
        break;
    }
}

void apply_leave(int employee_id) {
    if (!employee_exists(employee_id)) {
        printf("Employee ID %d does not exist.\n", employee_id);
        return;
    }

    LeaveRequest l;
    l.employee_id = employee_id;
    read_valid_leave_type(l.leave_type, sizeof(l.leave_type));
    read_valid_date_range(l.from_date, sizeof(l.from_date), l.to_date, sizeof(l.to_date));

    int requested_days = leave_days_between(l.from_date, l.to_date);
    int remaining = leave_balance(l.employee_id);
    if (requested_days > remaining) {
        printf("Cannot apply: requested %d day(s) but only %d remaining out of %d annual quota.\n",
               requested_days, remaining, ANNUAL_LEAVE_QUOTA);
        return;
    }

    l.id = next_id(LEAVE_FILE);
    strcpy(l.status, "Pending"); /* state machine starts here */

    FILE *fp = fopen(LEAVE_FILE, "a");
    if (!fp) { printf("Could not open leave.txt\n"); return; }
    fprintf(fp, "%d|%d|%s|%s|%s|%s\n", l.id, l.employee_id, l.leave_type, l.from_date, l.to_date, l.status);
    fclose(fp);

    printf("Leave request %d submitted (%d day(s)), status: Pending.\n", l.id, requested_days);
}

void view_leave_requests(int employee_id) {
    FILE *fp = fopen(LEAVE_FILE, "r");
    if (!fp) { printf("No leave requests found.\n"); return; }

    char line[MAX_LINE];
    LeaveRequest l;
    printf("%-4s %-6s %-8s %-11s %-11s %-10s\n", "ID", "EmpID", "Type", "From", "To", "Status");
    printf("--------------------------------------------------------------\n");
    int any = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_leave(line, &l) && (employee_id == 0 || l.employee_id == employee_id)) {
            printf("%-4d %-6d %-8s %-11s %-11s %-10s\n",
                   l.id, l.employee_id, l.leave_type, l.from_date, l.to_date, l.status);
            any = 1;
        }
    }
    fclose(fp);
    if (!any) printf("(no leave requests)\n");
}

/* State machine transition: Pending -> Approved | Rejected (no reverse
   transitions). Decision input is validated/normalized case-insensitively
   so "approved", "APPROVED", etc. all resolve correctly instead of
   silently writing an unrecognized string that would break the exact
   strcmp() in leave_balance(). */
void approve_or_reject_leave(void) {
    int id = read_int("Leave request ID: ");

    char decision[15];
    for (;;) {
        read_nonempty_line("Decision (Approved / Rejected): ", decision, sizeof(decision));
        if (str_ieq(decision, "Approved")) { strcpy(decision, "Approved"); break; }
        if (str_ieq(decision, "Rejected")) { strcpy(decision, "Rejected"); break; }
        printf("Please type Approved or Rejected.\n");
    }

    FILE *fp = fopen(LEAVE_FILE, "r");
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!fp || !tmp) { printf("Could not open leave.txt\n"); return; }

    char line[MAX_LINE];
    LeaveRequest l;
    int found = 0;
    int notify_emp_id = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_leave(line, &l) && l.id == id) {
            if (strcmp(l.status, "Pending") != 0) {
                printf("Leave %d is already '%s' — cannot transition again.\n", id, l.status);
                fputs(line, tmp);
                continue;
            }
            strcpy(l.status, decision);
            found = 1;
            notify_emp_id = l.employee_id;
            fprintf(tmp, "%d|%d|%s|%s|%s|%s\n", l.id, l.employee_id, l.leave_type, l.from_date, l.to_date, l.status);
        } else {
            fputs(line, tmp);
        }
    }
    fclose(fp);
    fclose(tmp);
    remove(LEAVE_FILE);
    rename(TMP_FILE, LEAVE_FILE);

    if (found) {
        printf("Leave request %d marked as %s.\n", id, decision);
        char msg[100];
        snprintf(msg, sizeof(msg), "Your leave request #%d was %s.", id, decision);
        push_notification(notify_emp_id, msg);
    } else {
        printf("Leave request ID %d not found or already processed.\n", id);
    }
}

/* Accrual/deduction algorithm: balance = quota - sum(days of Approved leaves). */
int leave_balance(int employee_id) {
    FILE *fp = fopen(LEAVE_FILE, "r");
    if (!fp) return ANNUAL_LEAVE_QUOTA;

    char line[MAX_LINE];
    LeaveRequest l;
    int used = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_leave(line, &l) && l.employee_id == employee_id && strcmp(l.status, "Approved") == 0) {
            used += leave_days_between(l.from_date, l.to_date);
        }
    }
    fclose(fp);
    int remaining = ANNUAL_LEAVE_QUOTA - used;
    return remaining > 0 ? remaining : 0;
}

void delete_leave_for_employee(int employee_id) {
    FILE *fp = fopen(LEAVE_FILE, "r");
    if (!fp) return;
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!tmp) { fclose(fp); return; }

    char line[MAX_LINE];
    LeaveRequest l;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_leave(line, &l) && l.employee_id == employee_id) continue; /* drop */
        fputs(line, tmp);
    }
    fclose(fp);
    fclose(tmp);
    remove(LEAVE_FILE);
    rename(TMP_FILE, LEAVE_FILE);
}
