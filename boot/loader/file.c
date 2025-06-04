#include "file.h"
#include "lib.h"
#include "print.h"
#include "debug.h"
#include "stdbool.h"


static struct BPB* get_fs_bpb(void)
{
    uint32_t lba = *(uint32_t*)((uint64_t)FS_BASE + 0x1be + 8);

    return (struct BPB*)((uint64_t)FS_BASE + lba * 512);
}

static uint16_t *get_fat_table(void)
{
    struct BPB* p = get_fs_bpb();
    uint32_t offset = (uint32_t)p->reserved_sector_count * p->bytes_per_sector;

    return (uint16_t *)((uint8_t*)p + offset);
}

static uint16_t get_cluster_value(uint32_t cluster_index)
{
    uint16_t *fat_table = get_fat_table();

    return fat_table[cluster_index];
}

static uint32_t get_cluster_offset(uint32_t index)
{
    uint32_t res_size;
    uint32_t fat_size;
    uint32_t dir_size;

    ASSERT(index >= 2);

    struct BPB* p = get_fs_bpb();

    res_size = (uint32_t)p->reserved_sector_count * p->bytes_per_sector;
    fat_size = (uint32_t)p->fat_count * p->sectors_per_fat * p->bytes_per_sector;
    dir_size = (uint32_t)p->root_entry_count * sizeof(struct DirEntry);

    return res_size + fat_size + dir_size +
        (index - 2) * ((uint32_t)p->sectors_per_cluster * p->bytes_per_sector);
}

static uint32_t get_cluster_size(void)
{
    struct BPB* bpb = get_fs_bpb();

    return (uint32_t)bpb->bytes_per_sector * bpb->sectors_per_cluster;
}

static uint32_t get_root_directory_count(void)
{
    struct BPB* bpb = get_fs_bpb();

    return bpb->root_entry_count;
}

static struct DirEntry *get_root_directory(void)
{
    struct BPB *p; 
    uint32_t offset; 

    p = get_fs_bpb();
    offset = (p->reserved_sector_count + (uint32_t)p->fat_count * p->sectors_per_fat) * p->bytes_per_sector;

    return (struct DirEntry *)((uint8_t*)p + offset);
}

static bool is_file_name_equal(struct DirEntry *dir_entry, char *name, char *ext)
{
    bool status = false;

    if (memcmp(dir_entry->name, name, 8) == 0 &&
        memcmp(dir_entry->ext, ext, 3) == 0) {
        status = true;
    }

    return status;
}

static bool split_path(char *path, char *name, char *ext)
{
    int i;

    for (i = 0; i < 8 && path[i] != '.' && path[i] != '\0'; i++) {
        if (path[i] == '/') {
            return false;
        }

        name[i] = path[i];
    }

    if (path[i] == '.') {
        i++;
        
        for (int j = 0; j < 3 && path[i] != '\0'; i++, j++) {
            if (path[i] == '/') {
                return false;
            }

            ext[j] = path[i];
        }
    }

    if (path[i] != '\0') {        
        return false;
    }

    return true;
}

static bool search_in_directory(uint32_t dir_cluster, char *name, char *ext, struct DirEntry *res)
{
    if (dir_cluster == 0) {
        struct DirEntry *dir = get_root_directory();
        uint32_t count = get_root_directory_count();
        for (uint32_t i = 0; i < count; i++) {
            if (dir[i].name[0] == ENTRY_EMPTY || dir[i].name[0] == ENTRY_DELETED)
                continue;
            if (dir[i].attributes == 0xf)
                continue;
            if (is_file_name_equal(&dir[i], name, ext)) {
                if (res)
                    *res = dir[i];
                return true;
            }
        }
    } else {
        struct BPB* bpb = get_fs_bpb();
        uint32_t cluster_size = get_cluster_size();
        uint32_t cluster = dir_cluster;
        while (cluster >= 2 && cluster < 0xfff7) {
            struct DirEntry *dir = (struct DirEntry*)((uint64_t)bpb + get_cluster_offset(cluster));
            for (uint32_t i = 0; i < cluster_size / sizeof(struct DirEntry); i++) {
                if (dir[i].name[0] == ENTRY_EMPTY)
                    return false;
                if (dir[i].name[0] == ENTRY_DELETED || dir[i].attributes == 0xf)
                    continue;
                if (is_file_name_equal(&dir[i], name, ext)) {
                    if (res)
                        *res = dir[i];
                    return true;
                }
            }
            cluster = get_cluster_value(cluster);
        }
    }

    return false;
}

static bool find_entry(char *path, struct DirEntry *res)
{
    char component[16];
    uint32_t dir_cluster = 0;
    char *p = path;

    while (1) {
        int len = 0;
        while (p[len] != '\0' && p[len] != '/')
            len++;
        memcpy(component, p, len);
        component[len] = '\0';
        if (p[len] == '/')
            p += len + 1;
        else
            p += len;

        char name[8] = "        ";
        char ext[3] = "   ";
        if (!split_path(component, name, ext))
            return false;

        struct DirEntry entry;
        if (!search_in_directory(dir_cluster, name, ext, &entry))
            return false;

        if (*p == '\0') {
            if (res)
                *res = entry;
            return true;
        }

        if ((entry.attributes & 0x10) == 0)
            return false;
        dir_cluster = entry.cluster_index;
    }
}

static uint32_t read_raw_data(uint32_t cluster_index, char *buffer, uint32_t size)
{
    struct BPB* bpb;
    char *data;
    uint32_t read_size = 0;
    uint32_t cluster_size; 
    uint32_t index; 
    
    bpb = get_fs_bpb();
    cluster_size = get_cluster_size();
    index = cluster_index;

    if (index < 2) {
        return 0xffffffff;
    }
    
    while (read_size < size) {
        data  = (char *)((uint64_t)bpb + get_cluster_offset(index));
        index = get_cluster_value(index);
        
        if (index >= 0xfff7) {
            memcpy(buffer, data, size - read_size);
            read_size += size - read_size;
            break;
        }

        memcpy(buffer, data, cluster_size);

        buffer += cluster_size;
        read_size += cluster_size;
    }

    return read_size;
}

static uint32_t read_file(uint32_t cluster_index, void *buffer, uint32_t size)
{
    return read_raw_data(cluster_index, buffer, size);
}

int load_file(char *path, uint64_t addr)
{
    struct DirEntry entry;
    if (!find_entry(path, &entry))
        return -1;

    if (read_file(entry.cluster_index, (void*)addr, entry.file_size) == entry.file_size)
        return 0;

    return -1;
}

void init_fs(void)
{
    uint8_t *p = (uint8_t*)get_fs_bpb();
    
    if (p[0x1fe] != 0x55 || p[0x1ff] != 0xaa) {
        printk("invalid signature\n");
        ASSERT(0);
    }
}

