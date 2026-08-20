#ifndef EMPLOYEE_H
#define EMPLOYEE_H

#include "common.h"

void add_employee(void);
void view_employees(void);
void update_employee(void);
void delete_employee(void); /* cascades: attendance, leave, payroll, documents, notifications */
void search_employee(void);

/* Used by other modules (attendance/leave/payroll/reports) to confirm an
   employee_id exists before writing a dependent record. */
int employee_exists(int employee_id);

/* Fills out_ids with up to max employee IDs on file, returns how many
   were written. Used by attendance.c to mark absentees across all staff. */
int get_all_employee_ids(int *out_ids, int max);

#endif
