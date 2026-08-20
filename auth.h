#ifndef AUTH_H
#define AUTH_H

#include "common.h"

/* Attempts login; on success fills *out_user and returns 1, else returns 0. */
int login(User *out_user);

/* Registers a new user (Admin-only action from the menu). */
void register_user(void);

/* Seeds a default admin account if users.txt doesn't exist yet. */
void ensure_default_admin(void);

#endif
