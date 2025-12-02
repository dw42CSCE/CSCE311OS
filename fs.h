#ifndef FS_H
#define FS_H

#define FS_MAX_FILES 16
#define FS_NAME_LEN 16
#define FS_DATA_LEN 256

void fs_init(void);
void fs_format(void);
int fs_write(const char *name, const char *data);
int fs_read(const char *name, char *out, int out_sz);
int fs_delete(const char *name);
void fs_list(void);

#endif
