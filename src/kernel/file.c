#include "process.h"
#include "file.h"
#include "memory.h"
#include "print.h"
#include "lib.h"
#include "debug.h"

static struct FCB *fcb_table;
static struct FileDesc *file_desc_table;

static struct BPB* get_fs_bpb(void)
{
    uint32_t lba = *(uint32_t*)(P2V(FS_BASE) + 0x1be + 8);

    return (struct BPB*)P2V(FS_BASE + lba * 512);
}

static uint16_t *get_fat_table(void)
{
    struct BPB* p = get_fs_bpb();
    uint32_t offset = (uint32_t)p->reserved_sector_count * p->bytes_per_sector;

    return (uint16_t *)((uint8_t*)p + offset);
}

static uint16_t get_cluster_value(uint32_t cluster_index)
{
    uint16_t * fat_table = get_fat_table();

    return fat_table[cluster_index];
}

static uint32_t get_cluster_size(void)
{
    struct BPB* bpb = get_fs_bpb();

    return (uint32_t)bpb->bytes_per_sector * bpb->sectors_per_cluster;
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
    dir_size = p->root_entry_count * sizeof(struct DirEntry);

    return res_size + fat_size + dir_size +
        (index - 2) * ((uint32_t)p->sectors_per_cluster * p->bytes_per_sector);
}

static uint32_t get_root_directory_count(void)
{
    struct BPB* bpb = get_fs_bpb();

    return bpb->root_entry_count;
}

static struct DirEntry *get_root_directory(void)
{
    struct BPB* p; 
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

static bool search_in_directory(uint32_t dir_cluster, char *name, char *ext, struct DirEntry *res, uint32_t *idx)
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
                if (idx)
                    *idx = i;
                return true;
            }
        }
    } else {
        struct BPB* bpb = get_fs_bpb();
        uint32_t cluster_size = get_cluster_size();
        uint32_t cluster = dir_cluster;
        uint32_t index = 0;
        while (cluster >= 2 && cluster < 0xfff7) {
            struct DirEntry *dir = (struct DirEntry*)((uint64_t)bpb + get_cluster_offset(cluster));
            for (uint32_t i = 0; i < cluster_size / sizeof(struct DirEntry); i++, index++) {
                if (dir[i].name[0] == ENTRY_EMPTY)
                    return false;
                if (dir[i].name[0] == ENTRY_DELETED || dir[i].attributes == 0xf)
                    continue;
                if (is_file_name_equal(&dir[i], name, ext)) {
                    if (res)
                        *res = dir[i];
                    if (idx)
                        *idx = index;
                    return true;
                }
            }
            cluster = get_cluster_value(cluster);
        }
    }

    return false;
}

static bool find_entry(char *path, struct DirEntry *res, uint32_t *dir_cluster, uint32_t *dir_index)
{
    char component[16];
    uint32_t cur = 0;
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
        uint32_t idx;
        if (!search_in_directory(cur, name, ext, &entry, &idx))
            return false;

        if (*p == '\0') {
            if (res)
                *res = entry;
            if (dir_cluster)
                *dir_cluster = cur;
            if (dir_index)
                *dir_index = idx;
            return true;
        }

        if ((entry.attributes & 0x10) == 0)
            return false;
        cur = entry.cluster_index;
    }
}

static uint32_t search_file(char *path)
{
    struct DirEntry e;
    uint32_t dirc, idx;
    if (!find_entry(path, &e, &dirc, &idx))
        return 0xffffffff;
    if (dirc != 0)
        return 0xffffffff;
    return idx;
}

static uint32_t get_fcb(uint32_t index)
{
    struct DirEntry *dir_table;

    if (fcb_table[index].count == 0) {        
        dir_table = get_root_directory();
        fcb_table[index].dir_index = index;
        fcb_table[index].file_size = dir_table[index].file_size;
        fcb_table[index].cluster_index = dir_table[index].cluster_index;
        memcpy(&fcb_table[index].name, &dir_table[index].name, 8);
        memcpy(&fcb_table[index].ext, &dir_table[index].ext, 3);
    }
    
    fcb_table[index].count++;

    return index;
}

static void put_fcb(struct FCB *fcb)
{
    ASSERT(fcb->count > 0);
    fcb->count--;
    if (fcb->count == 0)
        memset(fcb, 0, sizeof(struct FCB));
}

int open_file(struct Process *proc, char *path_name)
{
    int fd = -1;
    int file_desc_index = -1;
    struct DirEntry entry;
    uint32_t dir_cluster;

    for (int i = 0; i < 100; i++) {
        if (proc->file[i] == NULL) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        return -1;
    }

    for (int i = 0; i < PAGE_SIZE / sizeof(struct FileDesc); i++) {
        if (file_desc_table[i].fcb == NULL) {
            file_desc_index = i;
            break;
        }
    }

    if (file_desc_index == -1)
        return -1;

    if (!find_entry(path_name, &entry, &dir_cluster, NULL))
        return -1;
    if ((entry.attributes & 0x10) != 0)
        return -1;

    struct FCB *fcb = NULL;
    for (uint32_t i = 0; i < PAGE_SIZE / sizeof(struct FCB); i++) {
        if (fcb_table[i].count == 0) {
            fcb = &fcb_table[i];
            break;
        }
    }
    if (fcb == NULL)
        return -1;

    memset(fcb, 0, sizeof(struct FCB));
    memcpy(fcb->name, entry.name, 8);
    memcpy(fcb->ext, entry.ext, 3);
    fcb->cluster_index = entry.cluster_index;
    fcb->file_size = entry.file_size;
    fcb->count = 1;
    fcb->attributes = entry.attributes;
    fcb->dir_index = 0;

    memset(&file_desc_table[file_desc_index], 0, sizeof(struct FileDesc));
    file_desc_table[file_desc_index].fcb = fcb;
    file_desc_table[file_desc_index].count = 1;
    proc->file[fd] = &file_desc_table[file_desc_index];

    return fd;
}

static uint32_t read_raw_data(uint32_t cluster_index, char *buffer, uint32_t position, uint32_t size)
{
    uint32_t read_size = 0;
    uint32_t index = cluster_index;
    uint32_t cluster_size = get_cluster_size(); 
    uint32_t count = position / cluster_size;
    uint32_t offset = position % cluster_size;

    for (uint32_t i = 0; i < count; i++) {
        index = get_cluster_value(index);
        ASSERT(index < 0xfff7);
    }

    struct BPB* bpb =  get_fs_bpb();
    char *data;

    if (offset != 0) {
        read_size = (offset + size) <= cluster_size ? size : (cluster_size - offset);
        data = (char*)((uint64_t)bpb + get_cluster_offset(index));
        memcpy(buffer, data + offset, read_size);
        buffer += read_size;
        index = get_cluster_value(index);
    }

    while (read_size < size && index < 0xfff7) {
        data = (char*)((uint64_t)bpb + get_cluster_offset(index));

        if (read_size + cluster_size >= size) {
            memcpy(buffer, data, size - read_size);
            read_size = size;
            break;     
        }   
              
        memcpy(buffer, data, cluster_size);
        buffer += cluster_size;
        read_size += cluster_size;
        index = get_cluster_value(index);
    }
    
    return read_size;
}

static uint32_t get_total_sectors(void)
{
    struct BPB* bpb = get_fs_bpb();

    return bpb->sector_count != 0 ? bpb->sector_count : bpb->large_sector_count;
}

static uint32_t get_total_clusters(void)
{
    struct BPB* bpb = get_fs_bpb();
    uint32_t root_sectors = ((uint32_t)bpb->root_entry_count * sizeof(struct DirEntry) + bpb->bytes_per_sector - 1) / bpb->bytes_per_sector;
    uint32_t data_sectors = get_total_sectors() - bpb->reserved_sector_count - bpb->fat_count * bpb->sectors_per_fat - root_sectors;

    return data_sectors / bpb->sectors_per_cluster + 2;
}

static uint32_t allocate_cluster(uint16_t prev)
{
    uint16_t *fat = get_fat_table();
    uint32_t count = get_total_clusters();

    for (uint32_t i = 2; i < count; i++) {
        if (fat[i] == 0) {
            fat[i] = 0xffff;
            if (prev >= 2)
                fat[prev] = i;
            struct BPB* bpb = get_fs_bpb();
            memset((char*)((uint64_t)bpb + get_cluster_offset(i)), 0, get_cluster_size());
            return i;
        }
    }

    return 0;
}

static void release_chain(uint32_t start)
{
    uint16_t *fat = get_fat_table();
    uint32_t index = start;

    while (index >= 2 && index < 0xfff7) {
        uint32_t next = fat[index];
        fat[index] = 0;
        index = next;
    }
}

static uint32_t write_raw_data(uint32_t cluster_index, char *buffer, uint32_t position, uint32_t size)
{
    uint32_t written = 0;
    uint32_t index = cluster_index;
    uint32_t cluster_size = get_cluster_size();
    uint32_t count = position / cluster_size;
    uint32_t offset = position % cluster_size;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t next = get_cluster_value(index);
        if (next >= 0xfff7) {
            next = allocate_cluster(index);
            if (next == 0)
                return written;
        }
        index = next;
    }

    struct BPB* bpb = get_fs_bpb();
    char *data;

    if (offset != 0) {
        uint32_t to_write = (offset + size) <= cluster_size ? size : (cluster_size - offset);
        data = (char*)((uint64_t)bpb + get_cluster_offset(index));
        memcpy(data + offset, buffer, to_write);
        written += to_write;
        buffer += to_write;
        size -= to_write;
        if (size == 0)
            return written;
        uint32_t next = get_cluster_value(index);
        if (next >= 0xfff7) {
            next = allocate_cluster(index);
            if (next == 0)
                return written;
        }
        index = next;
    }

    while (size > 0) {
        data = (char*)((uint64_t)bpb + get_cluster_offset(index));
        uint32_t to_write = size > cluster_size ? cluster_size : size;
        memcpy(data, buffer, to_write);
        written += to_write;
        buffer += to_write;
        size -= to_write;

        if (size == 0)
            break;

        uint32_t next = get_cluster_value(index);
        if (next >= 0xfff7) {
            next = allocate_cluster(index);
            if (next == 0)
                break;
        }
        index = next;
    }

    return written;
}

int read_file(struct Process *proc, int fd, void *buffer, uint32_t size)
{
    uint32_t position = proc->file[fd]->position;
    uint32_t file_size = proc->file[fd]->fcb->file_size;
    uint32_t read_size;

    if (position + size > file_size) {
        return -1;
    }

    read_size = read_raw_data(proc->file[fd]->fcb->cluster_index, buffer, position, size);
    proc->file[fd]->position += read_size;
    
    return read_size;
}

void close_file(struct Process *proc, int fd)
{
    put_fcb(proc->file[fd]->fcb);
    proc->file[fd]->count--;

    if (proc->file[fd]->count == 0) {
        proc->file[fd]->fcb = NULL;
    }

    proc->file[fd] = NULL;
}

uint32_t get_file_size(struct Process *proc, int fd)
{
    return proc->file[fd]->fcb->file_size;
}

int read_root_directory(char *buffer)
{
    struct DirEntry *dir_entry = get_root_directory();
    uint32_t count = get_root_directory_count();
    
    memcpy(buffer, dir_entry, count * sizeof(struct DirEntry));
        
    return count;
}

int create_file(char *path)
{
    char name[8] = {"        "};
    char ext[3] = {"   "};
    struct DirEntry *dir = get_root_directory();
    uint32_t count = get_root_directory_count();

    if (!split_path(path, name, ext))
        return -1;

    for (uint32_t i = 0; i < count; i++) {
        if (dir[i].name[0] != ENTRY_EMPTY && dir[i].name[0] != ENTRY_DELETED)
            if (is_file_name_equal(&dir[i], name, ext))
                return -1;
    }

    uint32_t free_index = 0xffffffff;
    for (uint32_t i = 0; i < count; i++) {
        if (dir[i].name[0] == ENTRY_EMPTY || dir[i].name[0] == ENTRY_DELETED) {
            free_index = i;
            break;
        }
    }

    if (free_index == 0xffffffff)
        return -1;

    uint32_t cluster = allocate_cluster(0);
    if (cluster == 0)
        return -1;

    memset(&dir[free_index], 0, sizeof(struct DirEntry));
    memcpy(dir[free_index].name, name, 8);
    memcpy(dir[free_index].ext, ext, 3);
    dir[free_index].cluster_index = cluster;
    dir[free_index].file_size = 0;

    return 0;
}

int write_file(struct Process *proc, int fd, void *buffer, uint32_t size)
{
    struct FCB *fcb = proc->file[fd]->fcb;
    uint32_t written = write_raw_data(fcb->cluster_index, buffer, proc->file[fd]->position, size);

    proc->file[fd]->position += written;
    if (proc->file[fd]->position > fcb->file_size)
        fcb->file_size = proc->file[fd]->position;

    struct DirEntry *dir = get_root_directory();
    dir[fcb->dir_index].file_size = fcb->file_size;
    dir[fcb->dir_index].cluster_index = fcb->cluster_index;

    return written;
}

int delete_file(char *path)
{
    uint32_t index = search_file(path);
    if (index == 0xffffffff)
        return -1;

    struct DirEntry *dir = get_root_directory();
    release_chain(dir[index].cluster_index);
    dir[index].name[0] = ENTRY_DELETED;

    return 0;
}

int mkdir(char *path)
{
    char name[8] = "        ";
    char ext[3] = "   ";
    struct DirEntry *dir = get_root_directory();
    uint32_t count = get_root_directory_count();

    if (!split_path(path, name, ext))
        return -1;

    for (uint32_t i = 0; i < count; i++) {
        if (dir[i].name[0] != ENTRY_EMPTY && dir[i].name[0] != ENTRY_DELETED)
            if (is_file_name_equal(&dir[i], name, ext))
                return -1;
    }

    uint32_t free_index = 0xffffffff;
    for (uint32_t i = 0; i < count; i++) {
        if (dir[i].name[0] == ENTRY_EMPTY || dir[i].name[0] == ENTRY_DELETED) {
            free_index = i;
            break;
        }
    }

    if (free_index == 0xffffffff)
        return -1;

    uint32_t cluster = allocate_cluster(0);
    if (cluster == 0)
        return -1;

    memset(&dir[free_index], 0, sizeof(struct DirEntry));
    memcpy(dir[free_index].name, name, 8);
    memcpy(dir[free_index].ext, ext, 3);
    dir[free_index].cluster_index = cluster;
    dir[free_index].file_size = 0;
    dir[free_index].attributes = 0x10;

    return 0;
}

int opendir(struct Process *proc, char *path)
{
    int fd = -1;
    int file_desc_index = -1;
    struct DirEntry entry;
    uint32_t dir_cluster;

    for (int i = 0; i < 100; i++) {
        if (proc->file[i] == NULL) { fd = i; break; }
    }
    if (fd == -1)
        return -1;

    for (int i = 0; i < PAGE_SIZE / sizeof(struct FileDesc); i++) {
        if (file_desc_table[i].fcb == NULL) { file_desc_index = i; break; }
    }
    if (file_desc_index == -1)
        return -1;

    if (!find_entry(path, &entry, &dir_cluster, NULL))
        return -1;
    if ((entry.attributes & 0x10) == 0)
        return -1;

    struct FCB *fcb = NULL;
    for (uint32_t i = 0; i < PAGE_SIZE / sizeof(struct FCB); i++) {
        if (fcb_table[i].count == 0) { fcb = &fcb_table[i]; break; }
    }
    if (!fcb) return -1;
    memset(fcb,0,sizeof(struct FCB));
    memcpy(fcb->name, entry.name,8);
    memcpy(fcb->ext, entry.ext,3);
    fcb->cluster_index = entry.cluster_index;
    fcb->file_size = entry.file_size;
    fcb->attributes = entry.attributes;
    fcb->count = 1;

    memset(&file_desc_table[file_desc_index],0,sizeof(struct FileDesc));
    file_desc_table[file_desc_index].fcb = fcb;
    file_desc_table[file_desc_index].count = 1;
    proc->file[fd] = &file_desc_table[file_desc_index];

    return fd;
}

int readdir(struct Process *proc, int fd, struct DirEntry *entry)
{
    struct FileDesc *desc = proc->file[fd];
    if (!desc || (desc->fcb->attributes & 0x10) == 0)
        return -1;

    if (desc->fcb->cluster_index < 2) {
        uint32_t count = get_root_directory_count();
        uint32_t idx = desc->position / sizeof(struct DirEntry);
        if (idx >= count)
            return 0;
        struct DirEntry *dir = get_root_directory();
        *entry = dir[idx];
        desc->position += sizeof(struct DirEntry);
        return 1;
    } else {
        if (read_raw_data(desc->fcb->cluster_index, (char*)entry, desc->position, sizeof(struct DirEntry)) != sizeof(struct DirEntry))
            return 0;
        desc->position += sizeof(struct DirEntry);
        if (entry->name[0] == ENTRY_EMPTY)
            return 0;
        return 1;
    }
}

int rmdir(char *path)
{
    uint32_t index = search_file(path);
    if (index == 0xffffffff)
        return -1;

    struct DirEntry *dir = get_root_directory();
    if ((dir[index].attributes & 0x10) == 0)
        return -1;
    release_chain(dir[index].cluster_index);
    dir[index].name[0] = ENTRY_DELETED;
    return 0;
}

static bool init_fcb(void)
{
    fcb_table = (struct FCB*)kalloc();

    if (fcb_table == NULL) {
	    return false;
    }

    memset(fcb_table, 0, PAGE_SIZE);

    return true;
}

static bool init_file_desc(void)
{
    file_desc_table = (struct FileDesc*)kalloc();

    if (file_desc_table == NULL) {
	    return false;
    }

    memset(file_desc_table, 0, PAGE_SIZE);

    return true;
}

void init_fs(void)
{
    uint8_t *p = (uint8_t*)get_fs_bpb();
    
    if (p[0x1fe] != 0x55 || p[0x1ff] != 0xaa) {
        printk("invalid signature\n");
        ASSERT(0);
    }
    
    ASSERT(init_fcb());
    ASSERT(init_file_desc());
}

