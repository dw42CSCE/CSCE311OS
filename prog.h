#ifndef PROG_H
#define PROG_H

#define PROG_MAX 8
#define PROG_NAME 16
#define PROG_SCRIPT 256

/* capability bits */
#define CAP_UART   0x1
#define CAP_FS_R   0x2
#define CAP_FS_W   0x4
#define CAP_SPAWN  0x8

void prog_init(void);
int prog_load(const char *name, const char *script, int caps);
int prog_load_file(const char *name, const char *file, int caps);
int prog_run(const char *name);
int prog_run_all(void);
int prog_drop(const char *name);
int prog_save(const char *name, const char *file);
void prog_list(void);

#endif
