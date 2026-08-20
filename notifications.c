#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "notifications.h"

#define NOTIF_FILE DATA_DIR "notifications.txt"
#define TMP_FILE DATA_DIR "notifications.tmp"

static void get_timestamp(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d %H:%M", tm_info);
}

static int next_notif_id(void) {
    FILE *fp = fopen(NOTIF_FILE, "r");
    if (!fp) return 1;
    char line[MAX_LINE];
    int last = 0;
    while (fgets(line, sizeof(line), fp)) {
        int v = atoi(line);
        if (v > last) last = v;
    }
    fclose(fp);
    return last + 1;
}

/* Called by other modules (leave.c on approve/reject, payroll.c on generate)
   instead of a real email/SMS API — logs the event so the employee sees it
   next time they check their notifications. */
void push_notification(int employee_id, const char *message) {
    system("mkdir -p " DATA_DIR);
    int id = next_notif_id();
    char ts[20];
    get_timestamp(ts, sizeof(ts));

    FILE *fp = fopen(NOTIF_FILE, "a");
    if (!fp) return; /* fail silently — notification is best-effort, not core */
    fprintf(fp, "%d|%d|%s|Unread|%s\n", id, employee_id, message, ts);
    fclose(fp);
}

void view_notifications(int employee_id) {
    FILE *fp = fopen(NOTIF_FILE, "r");
    if (!fp) { printf("No notifications.\n"); return; }

    char line[MAX_LINE];
    int id, emp_id;
    char message[200], status[10], ts[20];
    int shown = 0;
    printf("%-4s %-8s %-45s %s\n", "ID", "Status", "Message", "Time");
    printf("------------------------------------------------------------------------\n");
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d|%d|%199[^|]|%9[^|]|%19[^\n]", &id, &emp_id, message, status, ts) == 5) {
            if (employee_id == 0 || emp_id == employee_id) {
                printf("%-4d %-8s %-45s %s\n", id, status, message, ts);
                shown++;
            }
        }
    }
    fclose(fp);
    if (!shown) printf("No notifications for this employee.\n");
}

void mark_notification_read(int employee_id) {
    int id = read_int("Notification ID to mark read: ");

    FILE *fp = fopen(NOTIF_FILE, "r");
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!fp || !tmp) { printf("Could not open notifications.txt\n"); return; }

    char line[MAX_LINE];
    int rid, emp_id;
    char message[200], status[10], ts[20];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d|%d|%199[^|]|%9[^|]|%19[^\n]", &rid, &emp_id, message, status, ts) == 5 &&
            rid == id && (employee_id == 0 || emp_id == employee_id)) {
            found = 1;
            fprintf(tmp, "%d|%d|%s|Read|%s\n", rid, emp_id, message, ts);
        } else {
            fputs(line, tmp);
        }
    }
    fclose(fp);
    fclose(tmp);
    remove(NOTIF_FILE);
    rename(TMP_FILE, NOTIF_FILE);

    if (found) printf("Notification %d marked as read.\n", id);
    else printf("Notification ID %d not found.\n", id);
}

void delete_notifications_for_employee(int employee_id) {
    FILE *fp = fopen(NOTIF_FILE, "r");
    if (!fp) return;
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!tmp) { fclose(fp); return; }

    char line[MAX_LINE];
    int rid, emp_id;
    char message[200], status[10], ts[20];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d|%d|%199[^|]|%9[^|]|%19[^\n]", &rid, &emp_id, message, status, ts) == 5 &&
            emp_id == employee_id) {
            continue; /* drop */
        }
        fputs(line, tmp);
    }
    fclose(fp);
    fclose(tmp);
    remove(NOTIF_FILE);
    rename(TMP_FILE, NOTIF_FILE);
}
