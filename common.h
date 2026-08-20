#ifndef COMMON_H
#define COMMON_H

#define MAX_LINE 512
#define MAX_NAME 50
#define MAX_DATE 11   /* YYYY-MM-DD + null */
#define MAX_TIME 6    /* HH:MM + null */
#define MAX_STATUS 15
#define DATA_DIR "data/"

/* ---------- Roles ---------- */
typedef enum { ROLE_ADMIN = 1, ROLE_HR = 2, ROLE_EMPLOYEE = 3 } Role;

/* ---------- Structs (mirror the DB schema from the design docs) ---------- */

typedef struct {
    int id;
    char username[MAX_NAME];
    unsigned long password_hash;   /* simple hash, see utils.c */
    Role role;
    int employee_id;               /* FK -> Employee.id, 0 if not linked */
} User;

typedef struct {
    int id;
    char first_name[MAX_NAME];
    char last_name[MAX_NAME];
    char email[MAX_NAME];
    char phone[15];
    int department_id;             /* FK -> Department.id */
    char designation[MAX_NAME];
    char date_of_joining[MAX_DATE];
} Employee;

typedef struct {
    int id;
    char name[MAX_NAME];
    int head_employee_id;          /* FK -> Employee.id */
} Department;

typedef struct {
    int id;
    int employee_id;               /* FK -> Employee.id */
    char date[MAX_DATE];
    char check_in[MAX_TIME];
    char check_out[MAX_TIME];
    char status[MAX_STATUS];       /* Present / Late / Absent */
} Attendance;

typedef struct {
    int id;
    int employee_id;               /* FK -> Employee.id */
    char leave_type[15];           /* Sick / Casual / Earned */
    char from_date[MAX_DATE];
    char to_date[MAX_DATE];
    char status[15];               /* Pending / Approved / Rejected */
} LeaveRequest;

typedef struct {
    int id;
    int employee_id;               /* FK -> Employee.id */
    char month[8];                 /* YYYY-MM */
    float basic_salary;
    float deductions;
    float net_pay;
} Payroll;

/* ---------- Utility function prototypes (utils.c) ---------- */
unsigned long hash_password(const char *password);
int next_id(const char *filepath);
void pause_screen(void);
void clear_input_buffer(void);
void read_line(char *buffer, int size);
int file_exists(const char *filepath);

/* Robust, validated console input. All loop until the user provides
   something parseable/acceptable instead of leaving indeterminate
   values on bad input (fixes UB from unchecked scanf("%d", ...)). */
int read_int(const char *prompt);
int read_int_range(const char *prompt, int min, int max);
float read_float(const char *prompt);
float read_positive_float(const char *prompt);
void read_nonempty_line(const char *prompt, char *buffer, int size);
int input_at_eof(void); /* true once stdin has hit real end-of-file */

/* Format/content validators */
int valid_date(const char *date);          /* strict YYYY-MM-DD */
int valid_email(const char *email);        /* has '@' and a '.' after it */
int str_ieq(const char *a, const char *b); /* case-insensitive strcmp==0 */

/* Strips path separators/leading dots so a user-supplied display name
   can never be used for directory traversal when building a file path. */
void sanitize_filename(char *name);

#endif
