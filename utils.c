#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"

/*
 * Simple djb2-style hash used to avoid storing plaintext passwords in the
 * text files. This is NOT cryptographically secure (no salt, reversible
 * with effort) — it stands in for bcrypt, which needs an external library
 * not available in plain C. Good enough for a POC; call this out if asked
 * about production security.
 */
unsigned long hash_password(const char *password) {
    unsigned long hash = 5381;
    int c;
    while ((c = *password++))
        hash = ((hash << 5) + hash) + (unsigned long)c; /* hash * 33 + c */
    return hash;
}

/* Reads the last line's ID from a pipe-delimited file and returns id+1. */
int next_id(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return 1;

    char line[MAX_LINE];
    int last_id = 0;
    while (fgets(line, sizeof(line), fp)) {
        int id = atoi(line); /* id is always the first field */
        if (id > last_id) last_id = id;
    }
    fclose(fp);
    return last_id + 1;
}

int file_exists(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (fp) { fclose(fp); return 1; }
    return 0;
}

void pause_screen(void) {
    printf("\nPress Enter to continue...");
    fflush(stdout);
    int c;
    /* Consume one line so a leftover newline from a prior read_line()
       doesn't cause this to return instantly. */
    while ((c = getchar()) != '\n' && c != EOF) { }
}

/* Clears leftover characters (including the newline) from stdin. */
void clear_input_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

/* Set by read_line() whenever the underlying fgets() hits end-of-file
   (stdin closed/redirected input exhausted), as opposed to the user just
   pressing Enter on an empty line. Checked by read_nonempty_line() so it
   can exit cleanly instead of spinning forever re-prompting for input
   that can never arrive. */
static int g_stdin_eof = 0;

int input_at_eof(void) { return g_stdin_eof; }

/* Reads a line of input safely and strips the trailing newline. */
void read_line(char *buffer, int size) {
    if (fgets(buffer, size, stdin) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';
        else
            clear_input_buffer(); /* input was longer than buffer */
    } else {
        buffer[0] = '\0'; /* EOF: treat as empty rather than leaving garbage */
        g_stdin_eof = 1;
    }
}

/*
 * Reads a full line and validates it parses cleanly as an integer.
 * Loops (re-prompting) until valid input or EOF; on EOF returns 0 so
 * callers never see an indeterminate value. This replaces the previous
 * unchecked scanf("%d", ...) pattern used throughout the menus, which
 * left `choice`/ids undefined (UB) whenever a user typed non-numeric
 * input.
 */
int read_int(const char *prompt) {
    char buf[64];
    for (;;) {
        if (prompt) printf("%s", prompt);
        if (!fgets(buf, sizeof(buf), stdin)) {
            printf("\nInput ended unexpectedly. Exiting.\n");
            exit(0);
        }

        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n')
            buf[len - 1] = '\0';
        else
            clear_input_buffer();

        /* trim leading/trailing whitespace */
        char *start = buf;
        while (isspace((unsigned char)*start)) start++;
        if (*start == '\0') { printf("Please enter a number.\n"); continue; }

        char *end;
        long val = strtol(start, &end, 10);
        while (isspace((unsigned char)*end)) end++;
        if (*end != '\0') { printf("Invalid number, try again.\n"); continue; }
        if (val > 2147483647L || val < -2147483648L) {
            printf("Number out of range, try again.\n");
            continue;
        }
        return (int)val;
    }
}

int read_int_range(const char *prompt, int min, int max) {
    for (;;) {
        int v = read_int(prompt);
        if (v < min || v > max) {
            printf("Please enter a value between %d and %d.\n", min, max);
            continue;
        }
        return v;
    }
}

float read_float(const char *prompt) {
    char buf[64];
    for (;;) {
        if (prompt) printf("%s", prompt);
        if (!fgets(buf, sizeof(buf), stdin)) {
            printf("\nInput ended unexpectedly. Exiting.\n");
            exit(0);
        }

        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n')
            buf[len - 1] = '\0';
        else
            clear_input_buffer();

        char *start = buf;
        while (isspace((unsigned char)*start)) start++;
        if (*start == '\0') { printf("Please enter a number.\n"); continue; }

        char *end;
        float val = strtof(start, &end);
        while (isspace((unsigned char)*end)) end++;
        if (*end != '\0') { printf("Invalid number, try again.\n"); continue; }
        return val;
    }
}

float read_positive_float(const char *prompt) {
    for (;;) {
        float v = read_float(prompt);
        if (v <= 0.0f) { printf("Please enter a value greater than 0.\n"); continue; }
        return v;
    }
}

/* Re-prompts until the user provides at least one non-whitespace char.
   If stdin hits real EOF (as opposed to the user just pressing Enter),
   this stops re-prompting and exits the program cleanly rather than
   spinning forever — input that will never arrive can't be waited for. */
void read_nonempty_line(const char *prompt, char *buffer, int size) {
    for (;;) {
        if (prompt) printf("%s", prompt);
        read_line(buffer, size);
        if (input_at_eof()) {
            printf("\nInput ended unexpectedly. Exiting.\n");
            exit(0);
        }
        char *p = buffer;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') { printf("This field cannot be empty.\n"); continue; }
        return;
    }
}

/* Strict YYYY-MM-DD check: exact length/positions, plausible ranges.
   Does not special-case days-per-month (e.g. Feb 30 passes) — good enough
   for a POC, but rejects obviously malformed or out-of-range input, which
   is what actually caused bad data before. */
int valid_date(const char *date) {
    if (!date || strlen(date) != 10) return 0;
    if (date[4] != '-' || date[7] != '-') return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (!isdigit((unsigned char)date[i])) return 0;
    }
    int year = (date[0]-'0')*1000 + (date[1]-'0')*100 + (date[2]-'0')*10 + (date[3]-'0');
    int month = (date[5]-'0')*10 + (date[6]-'0');
    int day = (date[8]-'0')*10 + (date[9]-'0');
    if (year < 1900 || year > 2100) return 0;
    if (month < 1 || month > 12) return 0;
    if (day < 1 || day > 31) return 0;
    return 1;
}

int valid_email(const char *email) {
    const char *at = strchr(email, '@');
    if (!at || at == email) return 0;
    const char *dot = strchr(at, '.');
    if (!dot || dot == at + 1 || *(dot + 1) == '\0') return 0;
    return 1;
}

int str_ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* Removes any '/', '\\' characters and leading '.' characters from a
   user-supplied display name before it's used inside a constructed file
   path, so a value like "../../etc/passwd" can't escape data/docs/. */
void sanitize_filename(char *name) {
    char cleaned[256];
    size_t j = 0;
    for (size_t i = 0; name[i] != '\0' && j < sizeof(cleaned) - 1; i++) {
        char c = name[i];
        if (c == '/' || c == '\\') continue; /* drop path separators */
        cleaned[j++] = c;
    }
    cleaned[j] = '\0';

    /* strip any leading dots so ".." components collapse away */
    size_t k = 0;
    while (cleaned[k] == '.') k++;

    if (cleaned[k] == '\0') strcpy(cleaned, "file"); /* nothing left */
    else memmove(cleaned, cleaned + k, strlen(cleaned + k) + 1);

    strcpy(name, cleaned);
}
