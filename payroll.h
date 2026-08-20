#ifndef PAYROLL_H
#define PAYROLL_H

#include "common.h"

void generate_payroll(void);
void view_payslips(int employee_id); /* 0 = view all */

/* Cascade helper used by employee.c when deleting an employee. */
void delete_payroll_for_employee(int employee_id);

#endif
