#ifndef DASHBOARD_H
#define DASHBOARD_H

/* Single-screen summary combining counts from every other module's file.
   This is distinct from reports.c (which does deeper aggregations/exports) -
   Dashboard is the at-a-glance view shown right after login. */
void show_dashboard(void);

#endif
