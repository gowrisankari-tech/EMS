#include <stdio.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "dashboard.h"

#define EMP_FILE DATA_DIR "employees.txt"
#define DEPT_FILE DATA_DIR "departments.txt"
#define ATT_FILE DATA_DIR "attendance.txt"
#define LEAVE_FILE DATA_DIR "leave.txt"
#define PAY_FILE DATA_DIR "payroll.txt"

static int count_lines(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    int count = 0;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] != '\n') count++;
    }
    fclose(fp);
    return count;
}

static void get_today(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d", tm_info);
}

void show_dashboard(void) {
    char today[MAX_DATE];
    get_today(today, sizeof(today));

    int total_employees = count_lines(EMP_FILE);
    int total_departments = count_lines(DEPT_FILE);

    /* Present/Late/Absent today, and Pending leave count — scanned directly. */
    int present_today = 0, absent_today = 0, pending_leave = 0;
    char line[MAX_LINE];

    FILE *afp = fopen(ATT_FILE, "r");
    if (afp) {
        int id, emp_id;
        char date[MAX_DATE], cin[MAX_TIME], cout[MAX_TIME], status[MAX_STATUS];
        while (fgets(line, sizeof(line), afp)) {
            if (sscanf(line, "%d|%d|%10[^|]|%5[^|]|%5[^|]|%14[^\n]",
                       &id, &emp_id, date, cin, cout, status) == 6) {
                if (strcmp(date, today) == 0) {
                    if (strcmp(status, "Present") == 0 || strcmp(status, "Late") == 0)
                        present_today++;
                    else if (strcmp(status, "Absent") == 0)
                        absent_today++;
                }
            }
        }
        fclose(afp);
    }

    FILE *lfp = fopen(LEAVE_FILE, "r");
    if (lfp) {
        int id, emp_id;
        char type[15], from[MAX_DATE], to[MAX_DATE], status[15];
        while (fgets(line, sizeof(line), lfp)) {
            if (sscanf(line, "%d|%d|%14[^|]|%10[^|]|%10[^|]|%14[^\n]",
                       &id, &emp_id, type, from, to, status) == 6) {
                if (strcmp(status, "Pending") == 0) pending_leave++;
            }
        }
        fclose(lfp);
    }

    int payroll_records = count_lines(PAY_FILE);

    printf("\n==================== DASHBOARD (%s) ====================\n", today);
    printf(" Total Employees        : %d\n", total_employees);
    printf(" Total Departments      : %d\n", total_departments);
    printf(" Present/Late Today     : %d\n", present_today);
    printf(" Absent Today           : %d\n", absent_today);
    printf(" Pending Leave Requests : %d\n", pending_leave);
    printf(" Payroll Records Logged : %d\n", payroll_records);
    printf("==========================================================\n");
}
