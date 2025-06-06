#ifndef _LIB_H_
#define _LIB_H_

#include <stdint.h>

struct DirEntry {
    uint8_t name[8];
    uint8_t ext[3];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t create_ms;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t attr_index;
    uint16_t m_time;
    uint16_t m_date;
    uint16_t cluster_index;
    uint32_t file_size;
} __attribute__((packed));

struct SchedInfo {
    int pid;
    int priority;
    unsigned long runtime;
    int time_slice;
};

#define ENTRY_AVAILABLE 0
#define ENTRY_DELETED 0xe5

void sleepu(uint64_t ticks);
void exitu(void);
void waitu(int pid);
unsigned char keyboard_readu(void);
int get_total_memoryu(void);
int open_file(char *name);
int read_file(int fd, void *buffer, uint32_t size);
void close_file(int fd);
int get_file_size(int fd);
int read_root_directory(void *buffer);
int create_file(char *name);
int write_file(int fd, void *buffer, uint32_t size);
int delete_file(char *name);
int mkdir(const char *path);
int opendir(const char *path);
int readdir(int fd, struct DirEntry *entry);
int rmdir(const char *path);
int fork(void);
void exec(char *name);
void *sbrk(int64_t inc);
int socket(int type);
int sendto(int sock, void *buf, uint32_t size);
int recvfrom(int sock, void *buf, uint32_t size);
void set_priority(int prio);
int get_priority(void);
unsigned long get_runtime(void);
int get_sched_info(struct SchedInfo *info);

#endif
