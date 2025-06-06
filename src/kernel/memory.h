#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"

struct E820 {
    uint64_t address;
    uint64_t length;
    uint32_t type;
} __attribute__((packed));

struct FreeMemRegion {
    uint64_t address;
    uint64_t length;
};

struct Page {
    struct Page* next;
};

/* reference counts for physical pages */
void page_incref(uint64_t pa);
void page_decref(uint64_t pa);
uint16_t page_getref(uint64_t pa);
bool share_uvm(uint64_t dst_map, uint64_t src_map, int size);

typedef uint64_t PDE;
typedef PDE* PD;
typedef PD* PDPTR;

#define PTE_P 1
#define PTE_W 2
#define PTE_U 4
#define PTE_ENTRY 0x80
#define KERNEL_BASE 0xffff800000000000
#define PAGE_SIZE (2*1024*1024)

#define PA_UP(v) ((((uint64_t)v + PAGE_SIZE-1) >> 21) << 21)
#define PA_DOWN(v) (((uint64_t)v >> 21) << 21)
#define P2V(p) ((uint64_t)(p) + KERNEL_BASE)
#define V2P(v) ((uint64_t)(v) - KERNEL_BASE)
#define PDE_ADDR(p) (((uint64_t)p >> 12) << 12)
#define PTE_ADDR(p) (((uint64_t)p >> 21) << 21)

void* kalloc(void);
void kfree(uint64_t v);
void init_memory(void);
void init_kvm(void);
bool map_pages(uint64_t map, uint64_t v, uint64_t e, uint64_t pa, uint32_t attribute);
void switch_vm(uint64_t map);
void load_cr3(uint64_t map);
void free_vm(uint64_t map, uint64_t size);
void free_pages(uint64_t map, uint64_t vstart, uint64_t vend);
void free_page(uint64_t map, uint64_t v, uint64_t e);
bool setup_uvm(uint64_t map, uint64_t start, int size);
uint64_t setup_kvm(void);
uint64_t get_total_memory(void);
bool copy_uvm(uint64_t dst_map, uint64_t src_map, int size);
PD find_pdpt_entry(uint64_t map, uint64_t v, int alloc, uint32_t attribute);

void init_kheap(void);
void *kmalloc(size_t size);
void kmfree(void *ptr);

void invalidate_tlb(void);


#endif
