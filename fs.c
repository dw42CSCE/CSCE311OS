#include "fs.h"
#include "string.h"
#include "uart.h"

typedef struct {
    int used;
    char name[FS_NAME_LEN];
    char data[FS_DATA_LEN];
} fs_file;

static fs_file files[FS_MAX_FILES];

static int find_slot(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; ++i) {
        if (files[i].used && strcmp(files[i].name, name) == 0) return i;
    }
    return -1;
}

void fs_init(void) {
    fs_format();
}

void fs_format(void) {
    for (int i = 0; i < FS_MAX_FILES; ++i) {
        files[i].used = 0;
        files[i].name[0] = '\0';
        files[i].data[0] = '\0';
    }
}

int fs_write(const char *name, const char *data) {
    if (!name || !data) return -1;
    int idx = find_slot(name);
    if (idx < 0) {
        for (int i = 0; i < FS_MAX_FILES; ++i) {
            if (!files[i].used) { idx = i; break; }
        }
    }
    if (idx < 0) return -1;
    files[idx].used = 1;
    strlcpy(files[idx].name, name, FS_NAME_LEN);
    strlcpy(files[idx].data, data, FS_DATA_LEN);
    return 0;
}

int fs_read(const char *name, char *out, int out_sz) {
    int idx = find_slot(name);
    if (idx < 0 || !out || out_sz <= 0) return -1;
    strlcpy(out, files[idx].data, (unsigned long)out_sz);
    return 0;
}

int fs_delete(const char *name) {
    int idx = find_slot(name);
    if (idx < 0) return -1;
    files[idx].used = 0;
    files[idx].name[0] = '\0';
    files[idx].data[0] = '\0';
    return 0;
}

void fs_list(void) {
    uart_puts("fs:\n");
    for (int i = 0; i < FS_MAX_FILES; ++i) {
        if (files[i].used) {
            uart_puts(" - ");
            uart_puts(files[i].name);
            uart_puts(" (");
            /* show length */
            char buf[8]; int n = 0;
            unsigned long len = strlen(files[i].data);
            char digits[8]; int d = 0;
            if (len == 0) digits[d++] = '0';
            while (len) { digits[d++] = '0' + (len % 10); len /= 10; }
            for (int k = d - 1; k >= 0; --k) buf[n++] = digits[k];
            buf[n] = '\0';
            uart_puts(buf);
            uart_puts("b)\n");
        }
    }
}
