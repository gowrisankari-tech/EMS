#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "reports.h"

#define EMP_FILE DATA_DIR "employees.txt"
#define DEPT_FILE DATA_DIR "departments.txt"
#define ATT_FILE DATA_DIR "attendance.txt"
#define PAY_FILE DATA_DIR "payroll.txt"

#define MAX_DEPTS 100

/* Aggregation: COUNT(*) GROUP BY department_id, done manually over the file. */
void report_headcount_by_department(void) {
    FILE *dfp = fopen(DEPT_FILE, "r");
    FILE *efp = fopen(EMP_FILE, "r");
    if (!dfp || !efp) { printf("Missing department/employee data.\n"); return; }

    int dept_ids[MAX_DEPTS], counts[MAX_DEPTS];
    char dept_names[MAX_DEPTS][MAX_NAME];
    int n_depts = 0;

    char line[MAX_LINE];
    int id, head;
    char name[MAX_NAME];
    while (fgets(line, sizeof(line), dfp) && n_depts < MAX_DEPTS) {
        if (sscanf(line, "%d|%49[^|]|%d", &id, name, &head) == 3) {
            dept_ids[n_depts] = id;
            strcpy(dept_names[n_depts], name);
            counts[n_depts] = 0;
            n_depts++;
        }
    }
    fclose(dfp);

    int eid, dept_id;
    char fn[MAX_NAME], ln[MAX_NAME], em[MAX_NAME], ph[15], desg[MAX_NAME], doj[MAX_DATE];
    while (fgets(line, sizeof(line), efp)) {
        if (sscanf(line, "%d|%49[^|]|%49[^|]|%49[^|]|%14[^|]|%d|%49[^|]|%10[^\n]",
                   &eid, fn, ln, em, ph, &dept_id, desg, doj) == 8) {
            for (int i = 0; i < n_depts; i++)
                if (dept_ids[i] == dept_id) { counts[i]++; break; }
        }
    }
    fclose(efp);

    printf("%-20s %-10s\n", "Department", "Headcount");
    printf("------------------------------\n");
    for (int i = 0; i < n_depts; i++)
        printf("%-20s %-10d\n", dept_names[i], counts[i]);
}

/* Aggregation: COUNT(*) GROUP BY status over attendance.txt. */
void report_attendance_summary(void) {
    FILE *fp = fopen(ATT_FILE, "r");
    if (!fp) { printf("No attendance data.\n"); return; }

    int present = 0, late = 0, absent = 0, total = 0;
    char line[MAX_LINE];
    int id, emp_id;
    char date[MAX_DATE], cin[MAX_TIME], cout[MAX_TIME], status[MAX_STATUS];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d|%d|%10[^|]|%5[^|]|%5[^|]|%14[^\n]",
                   &id, &emp_id, date, cin, cout, status) == 6) {
            total++;
            if (strcmp(status, "Present") == 0) present++;
            else if (strcmp(status, "Late") == 0) late++;
            else if (strcmp(status, "Absent") == 0) absent++;
        }
    }
    fclose(fp);

    printf("Total records: %d\n", total);
    printf("Present: %d (%.1f%%)\n", present, total ? present * 100.0 / total : 0);
    printf("Late:    %d (%.1f%%)\n", late, total ? late * 100.0 / total : 0);
    printf("Absent:  %d (%.1f%%)\n", absent, total ? absent * 100.0 / total : 0);
}

/* Aggregation: SUM(net_pay) for a given month over payroll.txt. */
void report_payroll_summary(const char *month) {
    FILE *fp = fopen(PAY_FILE, "r");
    if (!fp) { printf("No payroll data.\n"); return; }

    char line[MAX_LINE];
    int id, emp_id;
    char m[8];
    float basic, ded, net;
    float total_net = 0;
    int count = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d|%d|%7[^|]|%f|%f|%f", &id, &emp_id, m, &basic, &ded, &net) == 6) {
            if (strcmp(m, month) == 0) {
                total_net += net;
                count++;
            }
        }
    }
    fclose(fp);

    printf("Payroll summary for %s: %d employee(s) paid, total net pay Rs.%.2f\n", month, count, total_net);
}
