#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "employee.h"
#include "department.h"
#include "attendance.h"
#include "leave.h"
#include "payroll.h"
#include "filemgmt.h"
#include "notifications.h"

#define EMP_FILE DATA_DIR "employees.txt"
#define TMP_FILE DATA_DIR "employees.tmp"

static int parse_employee(const char *line, Employee *e) {
    return sscanf(line, "%d|%49[^|]|%49[^|]|%49[^|]|%14[^|]|%d|%49[^|]|%10[^\n]",
                  &e->id, e->first_name, e->last_name, e->email, e->phone,
                  &e->department_id, e->designation, e->date_of_joining) == 8;
}

static void print_employee_row(const Employee *e) {
    printf("%-4d %-12s %-12s %-22s %-13s %-6d %-15s %-10s\n",
           e->id, e->first_name, e->last_name, e->email, e->phone,
           e->department_id, e->designation, e->date_of_joining);
}

static void print_header(void) {
    printf("%-4s %-12s %-12s %-22s %-13s %-6s %-15s %-10s\n",
           "ID", "First", "Last", "Email", "Phone", "DeptID", "Designation", "DOJ");
    printf("--------------------------------------------------------------------------------------\n");
}

int employee_exists(int employee_id) {
    FILE *fp = fopen(EMP_FILE, "r");
    if (!fp) return 0;
    char line[MAX_LINE];
    Employee e;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_employee(line, &e) && e.id == employee_id) { fclose(fp); return 1; }
    }
    fclose(fp);
    return 0;
}

int get_all_employee_ids(int *out_ids, int max) {
    FILE *fp = fopen(EMP_FILE, "r");
    if (!fp) return 0;
    char line[MAX_LINE];
    Employee e;
    int count = 0;
    while (count < max && fgets(line, sizeof(line), fp)) {
        if (parse_employee(line, &e)) out_ids[count++] = e.id;
    }
    fclose(fp);
    return count;
}

/* Reads and validates a phone number: 7-14 digits, optional leading '+'. */
static void read_valid_phone(const char *prompt, char *buf, int size) {
    for (;;) {
        read_nonempty_line(prompt, buf, size);
        int i = 0, digits = 0;
        if (buf[0] == '+') i = 1;
        int ok = 1;
        for (; buf[i] != '\0'; i++) {
            if (buf[i] < '0' || buf[i] > '9') { ok = 0; break; }
            digits++;
        }
        if (!ok || digits < 7 || digits > 14) {
            printf("Enter a valid phone number (7-14 digits, optional leading '+').\n");
            continue;
        }
        return;
    }
}

static void read_valid_email(const char *prompt, char *buf, int size) {
    for (;;) {
        read_nonempty_line(prompt, buf, size);
        if (!valid_email(buf)) { printf("Enter a valid email address (e.g. name@example.com).\n"); continue; }
        return;
    }
}

static void read_valid_dept_id(const char *prompt, int *out) {
    for (;;) {
        int id = read_int(prompt);
        if (id == 0) { *out = 0; return; } /* unassigned is allowed */
        if (!department_exists(id)) {
            printf("Department ID %d does not exist. Enter 0 for unassigned, or add the department first.\n", id);
            continue;
        }
        *out = id;
        return;
    }
}

static void read_valid_date_field(const char *prompt, char *buf, int size) {
    for (;;) {
        read_nonempty_line(prompt, buf, size);
        if (!valid_date(buf)) { printf("Enter a valid date as YYYY-MM-DD.\n"); continue; }
        return;
    }
}

void add_employee(void) {
    Employee e;
    e.id = next_id(EMP_FILE);

    read_nonempty_line("First name: ", e.first_name, sizeof(e.first_name));
    read_nonempty_line("Last name: ", e.last_name, sizeof(e.last_name));
    read_valid_email("Email: ", e.email, sizeof(e.email));
    read_valid_phone("Phone: ", e.phone, sizeof(e.phone));
    read_valid_dept_id("Department ID (0 if unassigned): ", &e.department_id);
    read_nonempty_line("Designation: ", e.designation, sizeof(e.designation));
    read_valid_date_field("Date of joining (YYYY-MM-DD): ", e.date_of_joining, sizeof(e.date_of_joining));

    FILE *fp = fopen(EMP_FILE, "a");
    if (!fp) { printf("Could not open employees.txt\n"); return; }
    fprintf(fp, "%d|%s|%s|%s|%s|%d|%s|%s\n",
            e.id, e.first_name, e.last_name, e.email, e.phone,
            e.department_id, e.designation, e.date_of_joining);
    fclose(fp);
    printf("Employee added with ID %d.\n", e.id);
}

void view_employees(void) {
    FILE *fp = fopen(EMP_FILE, "r");
    if (!fp) { printf("No employees found.\n"); return; }

    char line[MAX_LINE];
    Employee e;
    print_header();
    int any = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_employee(line, &e)) { print_employee_row(&e); any = 1; }
    }
    fclose(fp);
    if (!any) printf("(no employee records yet)\n");
}

void search_employee(void) {
    int id = read_int("Enter Employee ID to search: ");

    FILE *fp = fopen(EMP_FILE, "r");
    if (!fp) { printf("No employees found.\n"); return; }

    char line[MAX_LINE];
    Employee e;
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_employee(line, &e) && e.id == id) {
            print_header();
            print_employee_row(&e);
            found = 1;
            break;
        }
    }
    fclose(fp);
    if (!found) printf("Employee ID %d not found.\n", id);
}

void update_employee(void) {
    int id = read_int("Enter Employee ID to update: ");

    if (!employee_exists(id)) {
        printf("Employee ID %d not found.\n", id);
        return;
    }

    FILE *fp = fopen(EMP_FILE, "r");
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!fp || !tmp) { printf("Could not open employees.txt\n"); return; }

    char line[MAX_LINE];
    Employee e;
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_employee(line, &e) && e.id == id) {
            found = 1;
            printf("Updating Employee %d (leave blank to keep current value)\n", id);

            char buf[MAX_NAME];
            printf("First name [%s]: ", e.first_name); read_line(buf, sizeof(buf));
            if (strlen(buf) > 0) strcpy(e.first_name, buf);

            printf("Last name [%s]: ", e.last_name); read_line(buf, sizeof(buf));
            if (strlen(buf) > 0) strcpy(e.last_name, buf);

            printf("Email [%s]: ", e.email); read_line(buf, sizeof(buf));
            if (strlen(buf) > 0) {
                if (valid_email(buf)) strcpy(e.email, buf);
                else printf("  Invalid email format — kept previous value.\n");
            }

            printf("Phone [%s]: ", e.phone); read_line(buf, sizeof(buf));
            if (strlen(buf) > 0) {
                int i = (buf[0] == '+') ? 1 : 0, digits = 0, ok = 1;
                for (; buf[i] != '\0'; i++) {
                    if (buf[i] < '0' || buf[i] > '9') { ok = 0; break; }
                    digits++;
                }
                if (ok && digits >= 7 && digits <= 14) strcpy(e.phone, buf);
                else printf("  Invalid phone format — kept previous value.\n");
            }

            printf("Designation [%s]: ", e.designation); read_line(buf, sizeof(buf));
            if (strlen(buf) > 0) strcpy(e.designation, buf);

            fprintf(tmp, "%d|%s|%s|%s|%s|%d|%s|%s\n",
                    e.id, e.first_name, e.last_name, e.email, e.phone,
                    e.department_id, e.designation, e.date_of_joining);
        } else {
            fputs(line, tmp);
        }
    }
    fclose(fp);
    fclose(tmp);
    remove(EMP_FILE);
    rename(TMP_FILE, EMP_FILE);

    if (found) printf("Employee %d updated.\n", id);
    else printf("Employee ID %d not found.\n", id); /* defensive; already checked above */
}

/* Deletes the employee record AND cascades the delete across every other
   module's data file so no orphaned rows referencing this employee_id are
   left behind (attendance, leave, payroll, documents on disk + their
   index, notifications, and department-head assignment). */
void delete_employee(void) {
    int id = read_int("Enter Employee ID to delete: ");

    if (!employee_exists(id)) {
        printf("Employee ID %d not found.\n", id);
        return;
    }

    printf("This will permanently delete Employee %d and all their attendance,\n", id);
    printf("leave, payroll, document, and notification records. Continue? (y/n): ");
    char confirm[8];
    read_line(confirm, sizeof(confirm));
    if (!(confirm[0] == 'y' || confirm[0] == 'Y')) {
        printf("Delete cancelled.\n");
        return;
    }

    FILE *fp = fopen(EMP_FILE, "r");
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!fp || !tmp) { printf("Could not open employees.txt\n"); return; }

    char line[MAX_LINE];
    Employee e;
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_employee(line, &e) && e.id == id) {
            found = 1;
            continue; /* skip writing this line = delete */
        }
        fputs(line, tmp);
    }
    fclose(fp);
    fclose(tmp);
    remove(EMP_FILE);
    rename(TMP_FILE, EMP_FILE);

    if (found) {
        clear_department_head_if_employee(id);
        delete_attendance_for_employee(id);
        delete_leave_for_employee(id);
        delete_payroll_for_employee(id);
        delete_documents_for_employee(id);
        delete_notifications_for_employee(id);
        printf("Employee %d and all related records deleted.\n", id);
    } else {
        printf("Employee ID %d not found.\n", id);
    }
}
