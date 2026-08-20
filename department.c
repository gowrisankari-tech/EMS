#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "department.h"
#include "employee.h"

#define DEPT_FILE DATA_DIR "departments.txt"
#define TMP_FILE DATA_DIR "departments.tmp"

static int parse_department(const char *line, Department *d) {
    return sscanf(line, "%d|%49[^|]|%d", &d->id, d->name, &d->head_employee_id) == 3;
}

int department_exists(int id) {
    FILE *fp = fopen(DEPT_FILE, "r");
    if (!fp) return 0;
    char line[MAX_LINE];
    Department d;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_department(line, &d) && d.id == id) { fclose(fp); return 1; }
    }
    fclose(fp);
    return 0;
}

static int department_name_exists(const char *name) {
    FILE *fp = fopen(DEPT_FILE, "r");
    if (!fp) return 0;
    char line[MAX_LINE];
    Department d;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_department(line, &d) && str_ieq(d.name, name)) { fclose(fp); return 1; }
    }
    fclose(fp);
    return 0;
}

void add_department(void) {
    Department d;
    for (;;) {
        read_nonempty_line("Department name: ", d.name, sizeof(d.name));
        if (department_name_exists(d.name)) {
            printf("A department named '%s' already exists.\n", d.name);
            continue;
        }
        break;
    }
    d.id = next_id(DEPT_FILE);
    d.head_employee_id = 0; /* assigned later via assign_department_head */

    FILE *fp = fopen(DEPT_FILE, "a");
    if (!fp) { printf("Could not open departments.txt\n"); return; }
    fprintf(fp, "%d|%s|%d\n", d.id, d.name, d.head_employee_id);
    fclose(fp);
    printf("Department added with ID %d.\n", d.id);
}

void view_departments(void) {
    FILE *fp = fopen(DEPT_FILE, "r");
    if (!fp) { printf("No departments found.\n"); return; }

    char line[MAX_LINE];
    Department d;
    printf("%-4s %-20s %-10s\n", "ID", "Name", "HeadEmpID");
    printf("------------------------------------------\n");
    int any = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_department(line, &d)) { printf("%-4d %-20s %-10d\n", d.id, d.name, d.head_employee_id); any = 1; }
    }
    fclose(fp);
    if (!any) printf("(no departments yet)\n");
}

void assign_department_head(void) {
    int dept_id = read_int("Department ID: ");
    if (!department_exists(dept_id)) {
        printf("Department ID %d not found.\n", dept_id);
        return;
    }
    int emp_id = read_int("Employee ID to set as head: ");

    if (!employee_exists(emp_id)) {
        printf("Employee ID %d does not exist. Aborting.\n", emp_id);
        return;
    }

    FILE *fp = fopen(DEPT_FILE, "r");
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!fp || !tmp) { printf("Could not open departments.txt\n"); return; }

    char line[MAX_LINE];
    Department d;
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_department(line, &d) && d.id == dept_id) {
            d.head_employee_id = emp_id;
            found = 1;
        }
        fprintf(tmp, "%d|%s|%d\n", d.id, d.name, d.head_employee_id);
    }
    fclose(fp);
    fclose(tmp);
    remove(DEPT_FILE);
    rename(TMP_FILE, DEPT_FILE);

    if (found) printf("Department %d head set to Employee %d.\n", dept_id, emp_id);
    else printf("Department ID %d not found.\n", dept_id);
}

void clear_department_head_if_employee(int employee_id) {
    FILE *fp = fopen(DEPT_FILE, "r");
    if (!fp) return;
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!tmp) { fclose(fp); return; }

    char line[MAX_LINE];
    Department d;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_department(line, &d)) {
            if (d.head_employee_id == employee_id) d.head_employee_id = 0;
            fprintf(tmp, "%d|%s|%d\n", d.id, d.name, d.head_employee_id);
        } else {
            fputs(line, tmp);
        }
    }
    fclose(fp);
    fclose(tmp);
    remove(DEPT_FILE);
    rename(TMP_FILE, DEPT_FILE);
}
