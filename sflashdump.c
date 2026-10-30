#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <ps5/kernel.h>

#define SYSCORE_AUTHID 0x4800000000000007ULL

typedef struct {
    char useless1[45];
    char message[3075];
} notify_request_t;
int sceKernelSendNotificationRequest(int, notify_request_t*, size_t, int);
int sceKernelMkdir(const char *path, int mode);

static void notify(const char *fmt, ...) {
    notify_request_t req;
    memset(&req, 0, sizeof(req));
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    vsnprintf(req.message, sizeof(req.message), fmt, ap);
    __builtin_va_end(ap);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

static void elevate(void) {
    pid_t pid = getpid();
    kernel_set_ucred_uid(pid, 0);
    kernel_set_ucred_ruid(pid, 0);
    kernel_set_ucred_svuid(pid, 0);
    kernel_set_ucred_rgid(pid, 0);
    kernel_set_ucred_svgid(pid, 0);
    kernel_set_ucred_authid(pid, SYSCORE_AUTHID);
    kernel_set_proc_rootdir(pid, kernel_get_root_vnode());
    kernel_set_proc_jaildir(pid, 0);
}

#define SFLASH_DEV "/dev/sflash0"
#define OUT_PATH   "/data/sflash0_dump.bin"
#define BUFSZ      0x40000

int main(void) {
    printf("\n=== sflash dump ===\n\n");
    elevate();

    int fd = open(SFLASH_DEV, O_RDONLY);
    if (fd < 0) {
        printf("[!] %s: %s\n", SFLASH_DEV, strerror(errno));
        notify("sflash: failed to open %s", SFLASH_DEV);
        return 1;
    }

    off_t sz = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    if (sz > 0)
        printf("[+] size = 0x%lx (%lld MB)\n", (unsigned long)sz, (long long)sz / 1024 / 1024);

    sceKernelMkdir("/data", 0777);
    int out = open(OUT_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0) {
        printf("[!] open %s: %s\n", OUT_PATH, strerror(errno));
        notify("sflash: can't create output file");
        close(fd);
        return 1;
    }

    notify("Dumping sflash...");

    static uint8_t buf[BUFSZ];
    uint64_t total = 0;
    int last_pct = -1;
    while (1) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            printf("[!] read error at 0x%llx: %s\n", (unsigned long long)total, strerror(errno));
            break;
        }
        if (n == 0) break;
        if (write(out, buf, n) != n) {
            printf("[!] short write\n");
            break;
        }
        total += (uint64_t)n;

        if (sz > 0) {
            int pct = (int)(total * 100 / sz);
            if (pct / 10 != last_pct / 10) {
                notify("Dumping sflash... %d%%", pct);
                last_pct = pct;
            }
        }
    }

    close(out);
    close(fd);
    printf("\n[+] dumped 0x%llx bytes -> %s\n", (unsigned long long)total, OUT_PATH);
    notify("sflash dumped (%llu MB) -> %s", (unsigned long long)(total / 1024 / 1024), OUT_PATH);
    return 0;
}
