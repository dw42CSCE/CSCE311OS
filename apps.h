#ifndef APPS_H
#define APPS_H

/* Return 0 on success, -1 if not found (run directly) */
int app_run(const char *name);

/* spawn app as background thread; return tid or -1 */
int app_spawn(const char *name);

void app_list(void);

#endif
