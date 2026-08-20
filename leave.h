#ifndef LEAVE_H
#define LEAVE_H

#include "common.h"

/* Scoped to the calling employee's own ID (passed in from the session by
   main.c), so an Employee can only apply for leave for themself. */
void apply_leave(int employee_id);
void view_leave_requests(int employee_id); /* 0 = view all (HR/Admin) */
void approve_or_reject_leave(void);
int leave_balance(int employee_id); /* returns remaining days out of ANNUAL_LEAVE_QUOTA */

/* Exposed for unit testing the inclusive day-count logic in isolation. */
int leave_days_between(const char *from, const char *to);

/* Cascade helper used by employee.c when deleting an employee. */
void delete_leave_for_employee(int employee_id);

#endif
