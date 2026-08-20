#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

/* Console-based stand-in for email/SMS (no external mail/SMS API in plain C).
   Every meaningful event (leave decision, payroll run) logs a notification
   here; the recipient sees it via "View My Notifications". */
void push_notification(int employee_id, const char *message);
void view_notifications(int employee_id); /* 0 = view all, Admin/HR only */

/* employee_id = 0 (Admin/HR) can mark any notification read; a nonzero
   employee_id may only mark their own — fixes the original version, which
   let any logged-in user mark any notification ID as read regardless of
   who it belonged to. */
void mark_notification_read(int employee_id);

/* Cascade helper used by employee.c when deleting an employee. */
void delete_notifications_for_employee(int employee_id);

#endif
