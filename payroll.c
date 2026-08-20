#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "payroll.h"
#include "employee.h"
#include "attendance.h"
#include "notifications.h"

#define PAY_FILE DATA_DIR "payroll.txt"
#define ATT_FILE DATA_DIR "attendance.txt"
#define TMP_FILE DATA_DIR "payroll.tmp"

static int parse_payroll(const char *line, Payroll *p) {
    return sscanf(line, "%d|%d|%7[^|]|%f|%f|%f",
                  &p->id, &p->employee_id, p->month, &p->basic_salary, &p->deductions, &p->net_pay) == 6;
}

/* Counts "Absent" days for an employee in a given YYYY-MM by scanning
   attendance.txt. This only returns non-zero once attendance rows are
   actually marked Absent — see attendance.c: mark_absentees_for_date(). */
static int count_absent_days(int employee_id, const char *month) {
    FILE *fp = fopen(ATT_FILE, "r");
    if (!fp) return 0;

    char line[MAX_LINE];
    int id, emp_id;
    char date[MAX_DATE], check_in[MAX_TIME], check_out[MAX_TIME], status[MAX_STATUS];
    int absent = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d|%d|%10[^|]|%5[^|]|%5[^|]|%14[^\n]",
                   &id, &emp_id, date, check_in, check_out, status) == 6) {
            if (emp_id == employee_id && strncmp(date, month, 7) == 0 && strcmp(status, "Absent") == 0)
                absent++;
        }
    }
    fclose(fp);
    return absent;
}

static int remove_existing_payroll(int employee_id, const char *month) {
    FILE *fp = fopen(PAY_FILE, "r");
    if (!fp) return 0;
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!tmp) { fclose(fp); return 0; }

    char line[MAX_LINE];
    Payroll p;
    int removed = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_payroll(line, &p) && p.employee_id == employee_id && strcmp(p.month, month) == 0) {
            removed = 1;
            continue;
        }
        fputs(line, tmp);
    }
    fclose(fp);
    fclose(tmp);
    remove(PAY_FILE);
    rename(TMP_FILE, PAY_FILE);
    return removed;
}

static void read_valid_month(char *buf, int size) {
    for (;;) {
        read_nonempty_line("Month (YYYY-MM): ", buf, size);
        if (strlen(buf) != 7 || buf[4] != '-') { printf("Enter month as YYYY-MM.\n"); continue; }
        int ok = 1;
        for (int i = 0; i < 7; i++) {
            if (i == 4) continue;
            if (buf[i] < '0' || buf[i] > '9') { ok = 0; break; }
        }
        if (!ok) { printf("Enter month as YYYY-MM.\n"); continue; }
        return;
    }
}

/*
 * Rule-based payroll calculation:
 *   per_day_rate = basic_salary / 30
 *   deductions   = per_day_rate * absent_days_in_month
 *   net_pay      = basic_salary - deductions
 */
void generate_payroll(void) {
    Payroll p;
    p.employee_id = read_int("Employee ID: ");

    if (!employee_exists(p.employee_id)) {
        printf("Employee ID %d does not exist.\n", p.employee_id);
        return;
    }

    read_valid_month(p.month, sizeof(p.month));

    if (remove_existing_payroll(p.employee_id, p.month)) {
        printf("A payroll record for Employee %d in %s already existed — regenerating it.\n",
               p.employee_id, p.month);
    }

    p.basic_salary = read_positive_float("Basic salary: ");

    int absent_days = count_absent_days(p.employee_id, p.month);
    float per_day_rate = p.basic_salary / 30.0f;
    p.deductions = per_day_rate * absent_days;
    p.net_pay = p.basic_salary - p.deductions;

    p.id = next_id(PAY_FILE);
    FILE *fp = fopen(PAY_FILE, "a");
    if (!fp) { printf("Could not open payroll.txt\n"); return; }
    fprintf(fp, "%d|%d|%s|%.2f|%.2f|%.2f\n",
            p.id, p.employee_id, p.month, p.basic_salary, p.deductions, p.net_pay);
    fclose(fp);

    printf("Payroll generated: %d absent day(s) -> deduction Rs.%.2f -> net pay Rs.%.2f\n",
           absent_days, p.deductions, p.net_pay);

    char msg[100];
    snprintf(msg, sizeof(msg), "Payslip for %s generated: net pay Rs.%.2f", p.month, p.net_pay);
    push_notification(p.employee_id, msg);
}

void view_payslips(int employee_id) {
    FILE *fp = fopen(PAY_FILE, "r");
    if (!fp) { printf("No payroll records found.\n"); return; }

    char line[MAX_LINE];
    Payroll p;
    printf("%-4s %-6s %-8s %-10s %-10s %-10s\n", "ID", "EmpID", "Month", "Basic", "Deduct", "NetPay");
    printf("----------------------------------------------------------\n");
    int any = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_payroll(line, &p) && (employee_id == 0 || p.employee_id == employee_id)) {
            printf("%-4d %-6d %-8s %-10.2f %-10.2f %-10.2f\n",
                   p.id, p.employee_id, p.month, p.basic_salary, p.deductions, p.net_pay);
            any = 1;
        }
    }
    fclose(fp);
    if (!any) printf("(no payslips)\n");
}

void delete_payroll_for_employee(int employee_id) {
    FILE *fp = fopen(PAY_FILE, "r");
    if (!fp) return;
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!tmp) { fclose(fp); return; }

    char line[MAX_LINE];
    Payroll p;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_payroll(line, &p) && p.employee_id == employee_id) continue; /* drop */
        fputs(line, tmp);
    }
    fclose(fp);
    fclose(tmp);
    remove(PAY_FILE);
    rename(TMP_FILE, PAY_FILE);
}
