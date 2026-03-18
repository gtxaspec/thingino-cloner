#ifndef CLONER_REMOTE_H
#define CLONER_REMOTE_H

#ifndef _WIN32

int remote_connect(const char *host, int port, const char *token);
void remote_disconnect(void);
int remote_list_devices(void);
const char *remote_detect_variant(int device_index);
int remote_bootstrap(int device_index, const char *cpu_variant, const char *firmware_dir);
int remote_write_firmware(int device_index, const char *cpu_variant, const char *firmware_file);
int remote_read_firmware(int device_index, const char *output_file);

#else /* _WIN32 */

#include <stdio.h>
static inline int remote_connect(const char *h, int p, const char *t) {
    (void)h;
    (void)p;
    (void)t;
    fprintf(stderr, "Remote mode not supported on Windows\n");
    return -1;
}
static inline void remote_disconnect(void) {}
static inline int remote_list_devices(void) {
    return -1;
}
static inline int remote_bootstrap(int i, const char *c, const char *f) {
    (void)i;
    (void)c;
    (void)f;
    return -1;
}
static inline int remote_write_firmware(int i, const char *c, const char *f) {
    (void)i;
    (void)c;
    (void)f;
    return -1;
}
static inline int remote_read_firmware(int i, const char *o) {
    (void)i;
    (void)o;
    return -1;
}

#endif /* _WIN32 */

#endif
