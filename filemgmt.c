#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "filemgmt.h"
#include "employee.h"

#define DOC_FILE DATA_DIR "documents.txt"
#define DOC_DIR DATA_DIR "docs/"
#define TMP_FILE DATA_DIR "documents.tmp"

static int parse_document(const char *line, int *id, int *emp_id, char *fname, char *fpath) {
    return sscanf(line, "%d|%d|%99[^|]|%199[^\n]", id, emp_id, fname, fpath) == 4;
}

/* Byte-for-byte copy of the source file into data/docs/. */
static int copy_file(const char *src_path, const char *dest_path) {
    FILE *src = fopen(src_path, "rb");
    if (!src) return 0;
    FILE *dest = fopen(dest_path, "wb");
    if (!dest) { fclose(src); return 0; }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
        fwrite(buf, 1, n, dest);

    fclose(src);
    fclose(dest);
    return 1;
}

void upload_document(int employee_id) {
    int emp_id = employee_id;
    if (emp_id == 0) {
        emp_id = read_int("Employee ID: ");
    }

    if (!employee_exists(emp_id)) {
        printf("Employee ID %d does not exist.\n", emp_id);
        return;
    }

    char src_path[200], fname[100];
    read_nonempty_line("Path to file on this machine (e.g. /home/user/resume.pdf): ", src_path, sizeof(src_path));
    if (!file_exists(src_path)) {
        printf("Could not find a file at '%s'. Check the path and try again.\n", src_path);
        return;
    }
    read_nonempty_line("Save as (display name, e.g. resume.pdf): ", fname, sizeof(fname));
    /* Strip any '/', '\', or leading '.' so a value like "../../etc/passwd"
       can't be used to write outside data/docs/ (path traversal fix). */
    sanitize_filename(fname);

    system("mkdir -p " DOC_DIR);

    int id = 0;
    /* reuse next_id logic manually since doc IDs are the first field too */
    FILE *check = fopen(DOC_FILE, "r");
    if (check) {
        char line[MAX_LINE]; int last = 0;
        while (fgets(line, sizeof(line), check)) { int v = atoi(line); if (v > last) last = v; }
        fclose(check);
        id = last + 1;
    } else {
        id = 1;
    }

    char dest_path[220];
    snprintf(dest_path, sizeof(dest_path), "%semp%d_%d_%s", DOC_DIR, emp_id, id, fname);

    if (!copy_file(src_path, dest_path)) {
        printf("Could not read source file '%s'. Check the path and try again.\n", src_path);
        return;
    }

    FILE *fp = fopen(DOC_FILE, "a");
    if (!fp) { printf("Could not open documents.txt\n"); return; }
    fprintf(fp, "%d|%d|%s|%s\n", id, emp_id, fname, dest_path);
    fclose(fp);

    printf("Document uploaded and stored at: %s\n", dest_path);
}

void view_documents(int employee_id) {
    FILE *fp = fopen(DOC_FILE, "r");
    if (!fp) { printf("No documents found.\n"); return; }

    char line[MAX_LINE];
    int id, emp_id;
    char fname[100], fpath[200];
    printf("%-4s %-6s %-25s %s\n", "ID", "EmpID", "File Name", "Stored Path");
    printf("--------------------------------------------------------------\n");
    int any = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_document(line, &id, &emp_id, fname, fpath) &&
            (employee_id == 0 || emp_id == employee_id)) {
            printf("%-4d %-6d %-25s %s\n", id, emp_id, fname, fpath);
            any = 1;
        }
    }
    fclose(fp);
    if (!any) printf("(no documents)\n");
}

void delete_document(void) {
    int id = read_int("Document ID to delete: ");

    FILE *fp = fopen(DOC_FILE, "r");
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!fp || !tmp) { printf("Could not open documents.txt\n"); return; }

    char line[MAX_LINE];
    int rid, emp_id;
    char fname[100], fpath[200];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (parse_document(line, &rid, &emp_id, fname, fpath) && rid == id) {
            remove(fpath); /* delete the actual copied file too */
            found = 1;
            continue;
        }
        fputs(line, tmp);
    }
    fclose(fp);
    fclose(tmp);
    remove(DOC_FILE);
    rename(TMP_FILE, DOC_FILE);

    if (found) printf("Document %d deleted.\n", id);
    else printf("Document ID %d not found.\n", id);
}

void delete_documents_for_employee(int employee_id) {
    FILE *fp = fopen(DOC_FILE, "r");
    if (!fp) return;
    FILE *tmp = fopen(TMP_FILE, "w");
    if (!tmp) { fclose(fp); return; }

    char line[MAX_LINE];
    int rid, emp_id;
    char fname[100], fpath[200];
    while (fgets(line, sizeof(line), fp)) {
        if (parse_document(line, &rid, &emp_id, fname, fpath) && emp_id == employee_id) {
            remove(fpath); /* delete the copied file on disk too */
            continue;
        }
        fputs(line, tmp);
    }
    fclose(fp);
    fclose(tmp);
    remove(DOC_FILE);
    rename(TMP_FILE, DOC_FILE);
}
