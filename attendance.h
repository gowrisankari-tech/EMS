#ifndef ATTENDANCE_H
#define ATTENDANCE_H

#include "common.h"

/* Both are scoped to the calling employee's own ID — passed in by main.c
   from the logged-in session, never re-entered by hand, so an Employee
   can only check themself in/out (fixes the original design where any
   logged-in user could type an arbitrary Employee ID). */
void mark_checkin(int employee_id);
void mark_checkout(int employee_id);

void view_attendance_history(int employee_id); /* 0 = view all */

/* Admin/HR utility: for a given YYYY-MM-DD, writes an "Absent" row for
   every employee who has no attendance record for that date yet. Without
   this, "Absent" was a status the schema defined but nothing ever wrote,
   which silently made payroll deductions always compute to 0. */
void mark_absentees_for_date(const char *date);

/* Cascade helper used by employee.c when deleting an employee. */
void delete_attendance_for_employee(int employee_id);

#endif
