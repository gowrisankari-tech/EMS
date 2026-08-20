#ifndef DEPARTMENT_H
#define DEPARTMENT_H

#include "common.h"

void add_department(void);
void view_departments(void);
void assign_department_head(void);

/* Used by employee.c to validate a department_id before linking an
   employee to it. */
int department_exists(int id);

/* Cascade helper: when an employee is deleted, any department that had
   them set as head_employee_id is reset to unassigned (0) rather than
   left pointing at a nonexistent employee. */
void clear_department_head_if_employee(int employee_id);

#endif
