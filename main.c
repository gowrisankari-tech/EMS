#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "auth.h"
#include "employee.h"
#include "department.h"
#include "attendance.h"
#include "leave.h"
#include "payroll.h"
#include "reports.h"
#include "dashboard.h"
#include "filemgmt.h"
#include "notifications.h"

static void admin_hr_menu(User *user);
static void employee_menu(User *user);
static void read_valid_report_month(char *buf, int size);

int main(void) {
    system("mkdir -p data");
    ensure_default_admin();

    printf("=========================================\n");
    printf("   EMPLOYEE MANAGEMENT SYSTEM (C / Linux)\n");
    printf("=========================================\n\n");

    User current;
    int attempts = 0;
    while (attempts < 3) {
        if (login(&current)) {
            printf("\nWelcome, %s! Role: %s\n",
                   current.username,
                   current.role == ROLE_ADMIN ? "Admin" :
                   current.role == ROLE_HR ? "HR" : "Employee");
            break;
        }
        printf("Invalid username or password.\n\n");
        attempts++;
    }
    if (attempts >= 3) {
        printf("Too many failed attempts. Exiting.\n");
        return 1;
    }

    if (current.role == ROLE_ADMIN || current.role == ROLE_HR)
        admin_hr_menu(&current);
    else
        employee_menu(&current);

    printf("\nGoodbye.\n");
    return 0;
}

static void admin_hr_menu(User *user) {
    int choice;
    do {
        printf("\n----- ADMIN/HR MENU -----\n");
        printf(" 1. Add Employee\n");
        printf(" 2. View Employees\n");
        printf(" 3. Update Employee\n");
        printf(" 4. Delete Employee\n");
        printf(" 5. Search Employee\n");
        printf(" 6. Add Department\n");
        printf(" 7. View Departments\n");
        printf(" 8. Assign Department Head\n");
        printf(" 9. View All Attendance\n");
        printf("10. View All Leave Requests\n");
        printf("11. Approve/Reject Leave\n");
        printf("12. Generate Payroll\n");
        printf("13. View All Payslips\n");
        printf("14. Report: Headcount by Department\n");
        printf("15. Report: Attendance Summary\n");
        printf("16. Report: Payroll Summary (by month)\n");
        printf("17. Dashboard\n");
        printf("18. Upload Document\n");
        printf("19. View Documents\n");
        printf("20. Delete Document\n");
        printf("21. View All Notifications\n");
        printf("22. Mark Absentees for a Date\n");
        if (user->role == ROLE_ADMIN)
            printf("23. Register New User\n");
        printf(" 0. Logout\n");
        choice = read_int("Choice: ");

        switch (choice) {
            case 1: add_employee(); break;
            case 2: view_employees(); break;
            case 3: update_employee(); break;
            case 4: delete_employee(); break;
            case 5: search_employee(); break;
            case 6: add_department(); break;
            case 7: view_departments(); break;
            case 8: assign_department_head(); break;
            case 9: view_attendance_history(0); break;
            case 10: view_leave_requests(0); break;
            case 11: approve_or_reject_leave(); break;
            case 12: generate_payroll(); break;
            case 13: view_payslips(0); break;
            case 14: report_headcount_by_department(); break;
            case 15: report_attendance_summary(); break;
            case 16: {
                char month[8];
                read_valid_report_month(month, sizeof(month));
                report_payroll_summary(month);
                break;
            }
            case 17: show_dashboard(); break;
            case 18: upload_document(0); break; /* 0 = admin will be prompted for whom */
            case 19: view_documents(0); break;
            case 20: delete_document(); break;
            case 21: view_notifications(0); break;
            case 22: {
                char date[MAX_DATE];
                read_nonempty_line("Date to mark absentees for (YYYY-MM-DD): ", date, sizeof(date));
                mark_absentees_for_date(date);
                break;
            }
            case 23:
                if (user->role == ROLE_ADMIN) register_user();
                else printf("Only Admin can register users.\n");
                break;
            case 0: break;
            default: printf("Invalid choice.\n");
        }
        if (choice != 0) pause_screen();
    } while (choice != 0);
}

/* Every action here is scoped to user->employee_id from the authenticated
   session — none of these prompt the person to type an Employee ID, so a
   logged-in Employee can only ever act on (or view) their own records. */
static void employee_menu(User *user) {
    int choice;
    do {
        printf("\n----- EMPLOYEE MENU -----\n");
        printf("1. Check In\n");
        printf("2. Check Out\n");
        printf("3. View My Attendance\n");
        printf("4. Apply for Leave\n");
        printf("5. View My Leave Requests\n");
        printf("6. View My Leave Balance\n");
        printf("7. View My Payslips\n");
        printf("8. View My Documents\n");
        printf("9. Upload My Document\n");
        printf("10. View My Notifications\n");
        printf("11. Mark Notification as Read\n");
        printf("0. Logout\n");
        choice = read_int("Choice: ");

        if (user->employee_id == 0 && choice != 0) {
            printf("Your login isn't linked to an Employee record — ask Admin to link it (option 23 in Admin/HR menu).\n");
            pause_screen();
            continue;
        }

        switch (choice) {
            case 1: mark_checkin(user->employee_id); break;
            case 2: mark_checkout(user->employee_id); break;
            case 3: view_attendance_history(user->employee_id); break;
            case 4: apply_leave(user->employee_id); break;
            case 5: view_leave_requests(user->employee_id); break;
            case 6: printf("Remaining leave balance: %d day(s)\n", leave_balance(user->employee_id)); break;
            case 7: view_payslips(user->employee_id); break;
            case 8: view_documents(user->employee_id); break;
            case 9: upload_document(user->employee_id); break;
            case 10: view_notifications(user->employee_id); break;
            case 11: mark_notification_read(user->employee_id); break;
            case 0: break;
            default: printf("Invalid choice.\n");
        }
        if (choice != 0) pause_screen();
    } while (choice != 0);
}

static void read_valid_report_month(char *buf, int size) {
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
