#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "attendance.h"
#include "employee.h"

#define ATT_FILE DATA_DIR "attendance.txt"
#define TMP_FILE DATA_DIR "attendance.tmp"

#define SHIFT_START_HOUR 9
#define SHIFT_START_MIN  30   /* Check-in after 09:30 => Late */

static int parse_attendance(const char *line, Attendance *a) {
    return sscanf(line, "%d|%d|%10[^|]|%5[^|]|%5[^|]|%14[^\n]",
                  &a->id, &a->employee_id, a->date, a->check_in, a->check_out, a->status) == 6;
}

static void get_today(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d", tm_info);
}

static void get_now_time(char *buf, int size, int *hour, int *min) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%H:%M", tm_info);
    *hour = tm_info->tm_hour;
    *min = tm_info->tm_min;
}

/* Rule-based classification: deterministic threshold check, not ML. */
static const char *classify_status(int hour, int min) {
    if (hour > SHIFT_START_HOUR || (hour == SHIFT_START_HOUR && min > SHIFT_START_MIN))
        return "Late";
    return "Present";
}

/* True if this employee already has ANY attendance row for `date`
   (regardless of status) — used to block double check-in and to skip
   already-recorded employees during absentee marking. */
static int has_attendance_for_date(int employee_id, const char *date) {
    FILE *fp = fopen(ATT_FILE, "r");
    if (!fp) return 0;
    char line[MAX_LINE];
    Attendance a;
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_attendance(line, &a) && a.employee_id == employee_id && strcmp(a.date, date) == 0) {
            found = 1;
            break;
        }
    }
    fclose(fp);
    return found;
}

void mark_checkin(int employee_id) {
    if (!employee_exists(employee_id)) {
        printf("Employee ID %d does not exist.\n", employee_id);
        return;
    }

    char today[MAX_DATE], now_str[MAX_TIME];
    int hour, min;
    get_today(today, sizeof(today));

    if (has_attendance_for_date(employee_id, today)) {
        printf("You already have an attendance record for today.\n");
        return;
    }

    get_now_time(now_str, sizeof(now_str), &hour, &min);
    const char *status = classify_status(hour, min);

    int id = next_id(ATT_FILE);
    FILE *fp = fopen(ATT_FILE, "a");
    if (!fp) { printf("Could not open attendance.txt\n"); return; }
    /* check_out left as "--:--" until checkout is marked */
    fprintf(fp, "%d|%d|%s|%s|--:--|%s\n", id, employee_id, today, now_str, status);
    fclose(fp);

    printf("Checked in at %s -> Status: %s\n", now_str, status);
}

void mark_checkout(int employee_id) {
    char today[MAX_DATE], now_str[MAX_TIME];
    int hour, min;
    get_today(today, sizeof(today));
    get_now_time(now_str, sizeof(now_str), &hour, &min);

    FILE *fp = fopen(ATT_FILE, "r");
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!fp || !tmp) { printf("Could not open attendance.txt\n"); return; }

    char line[MAX_LINE];
    Attendance a;
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (!found && parse_attendance(line, &a) &&
            a.employee_id == employee_id && strcmp(a.date, today) == 0 &&
            strcmp(a.check_out, "--:--") == 0) {
            strcpy(a.check_out, now_str);
            found = 1;
            fprintf(tmp, "%d|%d|%s|%s|%s|%s\n", a.id, a.employee_id, a.date, a.check_in, a.check_out, a.status);
        } else {
            fputs(line, tmp);
        }
    }
    fclose(fp);
    fclose(tmp);
    remove(ATT_FILE);
    rename(TMP_FILE, ATT_FILE);

    if (found) printf("Checked out at %s.\n", now_str);
    else printf("No open check-in found for you today.\n");
}

/* employee_id = 0 means show every employee's attendance (Admin/HR view). */
void view_attendance_history(int employee_id) {
    FILE *fp = fopen(ATT_FILE, "r");
    if (!fp) { printf("No attendance records found.\n"); return; }

    char line[MAX_LINE];
    Attendance a;
    printf("%-4s %-6s %-11s %-8s %-8s %-8s\n", "ID", "EmpID", "Date", "In", "Out", "Status");
    printf("------------------------------------------------------\n");
    int any = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_attendance(line, &a) && (employee_id == 0 || a.employee_id == employee_id)) {
            printf("%-4d %-6d %-11s %-8s %-8s %-8s\n",
                   a.id, a.employee_id, a.date, a.check_in, a.check_out, a.status);
            any = 1;
        }
    }
    fclose(fp);
    if (!any) printf("(no records)\n");
}

void mark_absentees_for_date(const char *date) {
    if (!valid_date(date)) {
        printf("Invalid date format.\n");
        return;
    }

    int ids[1000];
    int n = get_all_employee_ids(ids, 1000);
    if (n == 0) { printf("No employees on file.\n"); return; }

    int marked = 0;
    for (int i = 0; i < n; i++) {
        if (has_attendance_for_date(ids[i], date)) continue; /* already Present/Late/Absent */

        int id = next_id(ATT_FILE);
        FILE *fp = fopen(ATT_FILE, "a");
        if (!fp) { printf("Could not open attendance.txt\n"); return; }
        fprintf(fp, "%d|%d|%s|--:--|--:--|Absent\n", id, ids[i], date);
        fclose(fp);
        marked++;
    }
    printf("Marked %d employee(s) Absent for %s (skipped those with an existing record).\n", marked, date);
}

void delete_attendance_for_employee(int employee_id) {
    FILE *fp = fopen(ATT_FILE, "r");
    if (!fp) return;
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!tmp) { fclose(fp); return; }

    char line[MAX_LINE];
    Attendance a;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_attendance(line, &a) && a.employee_id == employee_id) continue; /* drop */
        fputs(line, tmp);
    }
    fclose(fp);
    fclose(tmp);
    remove(ATT_FILE);
    rename(TMP_FILE, ATT_FILE);
}
