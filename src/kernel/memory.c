#include "memory.h"
#include "print.h"
#include "debug.h"
#include "lib.h"
#include "stddef.h"
#include "trap.h"
#include "stdbool.h"

static void free_region(uint64_t v, uint64_t e);
static uint16_t *page_refs;

static struct FreeMemRegion free_mem_region[50];
static struct Page free_memory;
static uint64_t memory_end;
static uint64_t total_mem;
extern char end;

#define PAGE_INDEX(pa) ((pa) / PAGE_SIZE)

static void set_page_ref(uint64_t pa, uint16_t val)
{
    page_refs[PAGE_INDEX(pa)] = val;
}

static void inc_page_ref(uint64_t pa)
{
    page_refs[PAGE_INDEX(pa)]++;
}

static uint16_t get_page_ref(uint64_t pa)
{
    return page_refs[PAGE_INDEX(pa)];
}

void page_incref(uint64_t pa)
{
    inc_page_ref(pa);
}

void page_decref(uint64_t pa)
{
    uint16_t *ref = &page_refs[PAGE_INDEX(pa)];
    if (*ref > 0)
        (*ref)--;
    if (*ref == 0)
        kfree(P2V(pa));
}

uint16_t page_getref(uint64_t pa)
{
    return get_page_ref(pa);
}

void init_memory(void)
{
    int32_t count = *(int32_t*)0x20000;
    struct E820 *mem_map = (struct E820*)0x20008;	
    int free_region_count = 0;

    ASSERT(count <= 50);

	for(int32_t i = 0; i < count; i++) {        
        if(mem_map[i].type == 1) {			
            free_mem_region[free_region_count].address = mem_map[i].address;
            free_mem_region[free_region_count].length = mem_map[i].length;
            total_mem += mem_map[i].length;
            free_region_count++;
        }
        printk("%x  %uKB  %u\n",mem_map[i].address,mem_map[i].length/1024,(uint64_t)mem_map[i].type);
	}

    for (int i = 0; i < free_region_count; i++) {                  
        uint64_t vstart = P2V(free_mem_region[i].address);
        uint64_t vend = vstart + free_mem_region[i].length;

        if (vstart > (uint64_t)&end) {
            free_region(vstart, vend);
        } 
        else if (vend > (uint64_t)&end) {
            free_region((uint64_t)&end, vend);
        }       
    }
    
    memory_end = (uint64_t)free_memory.next + PAGE_SIZE;

    page_refs = kalloc();
    if (page_refs)
        memset(page_refs, 0, PAGE_SIZE);
}

uint64_t get_total_memory(void)
{
    return total_mem/1024/1024;
}

static void free_region(uint64_t v, uint64_t e)
{
    for (uint64_t start = PA_UP(v); start+PAGE_SIZE <= e; start += PAGE_SIZE) {        
        if (start+PAGE_SIZE <= 0xffff800030000000) {            
           kfree(start);
        }
    }
}

void kfree(uint64_t v)
{
    ASSERT(v % PAGE_SIZE == 0);
    ASSERT(v >= (uint64_t) & end);
    ASSERT(v+PAGE_SIZE <= 0xffff800030000000);

    uint64_t pa = V2P(v);
    set_page_ref(pa, 0);

    struct Page *page_address = (struct Page*)v;
    page_address->next = free_memory.next;
    free_memory.next = page_address;
}

void* kalloc(void)
{
    struct Page *page_address = free_memory.next;

    if (page_address != NULL) {
        ASSERT((uint64_t)page_address % PAGE_SIZE == 0);
        ASSERT((uint64_t)page_address >= (uint64_t)&end);
        ASSERT((uint64_t)page_address+PAGE_SIZE <= 0xffff800030000000);

        free_memory.next = page_address->next;
        set_page_ref(V2P(page_address), 1);
    }
    
    return page_address;
}

static PDPTR find_pml4t_entry(uint64_t map, uint64_t v, int alloc, uint32_t attribute)
{
    PDPTR *map_entry = (PDPTR*)map;
    PDPTR pdptr = NULL;
    unsigned int index = (v >> 39) & 0x1FF;

    if ((uint64_t)map_entry[index] & PTE_P) {
        pdptr = (PDPTR)P2V(PDE_ADDR(map_entry[index]));       
    } 
    else if (alloc == 1) {
        pdptr = (PDPTR)kalloc();          
        if (pdptr != NULL) {     
            memset(pdptr, 0, PAGE_SIZE);     
            map_entry[index] = (PDPTR)(V2P(pdptr) | attribute);           
        }
    } 

    return pdptr;    
}

PD find_pdpt_entry(uint64_t map, uint64_t v, int alloc, uint32_t attribute)
{
    PDPTR pdptr = NULL;
    PD pd = NULL;
    unsigned int index = (v >> 30) & 0x1FF;

    pdptr = find_pml4t_entry(map, v, alloc, attribute);
    if (pdptr == NULL)
        return NULL;
       
    if ((uint64_t)pdptr[index] & PTE_P) {      
        pd = (PD)P2V(PDE_ADDR(pdptr[index]));      
    }
    else if (alloc == 1) {
        pd = (PD)kalloc();  
        if (pd != NULL) {    
            memset(pd, 0, PAGE_SIZE);       
            pdptr[index] = (PD)(V2P(pd) | attribute);
        }
    } 

    return pd;
}

bool map_pages(uint64_t map, uint64_t v, uint64_t e, uint64_t pa, uint32_t attribute)
{
    uint64_t vstart = PA_DOWN(v);
    uint64_t vend = PA_UP(e);
    PD pd = NULL;
    unsigned int index;

    ASSERT(v < e);
    ASSERT(pa % PAGE_SIZE == 0);
    ASSERT(pa+vend-vstart <= 1024*1024*1024);

    do {
        pd = find_pdpt_entry(map, vstart, 1, attribute);    
        if (pd == NULL) {
            return false;
        }

        index = (vstart >> 21) & 0x1FF;
        ASSERT(((uint64_t)pd[index] & PTE_P) == 0);

        pd[index] = (PDE)(pa | attribute | PTE_ENTRY);

        vstart += PAGE_SIZE;
        pa += PAGE_SIZE;
    } while (vstart + PAGE_SIZE <= vend);
  
    return true;
}

void switch_vm(uint64_t map)
{
    load_cr3(V2P(map));   
}

uint64_t setup_kvm(void)
{
    uint64_t page_map = (uint64_t)kalloc();

    if (page_map != 0) {
        memset((void*)page_map, 0, PAGE_SIZE);        
        if (!map_pages(page_map, KERNEL_BASE, P2V(0x40000000), V2P(KERNEL_BASE), PTE_P|PTE_W)) {
            free_vm(page_map, PAGE_SIZE);
            page_map = 0;
        }
    }
    return page_map;
}

void init_kvm(void)
{
    uint64_t page_map = setup_kvm();
    ASSERT(page_map != 0);
    switch_vm(page_map);
    //printk("memory manager is working now");
}

bool setup_uvm(uint64_t map, uint64_t start, int size)
{
    bool status = false;
    void *page = kalloc();

    if (page != NULL) {
        memset(page, 0, PAGE_SIZE);
        status = map_pages(map, 0x400000, 0x400000+PAGE_SIZE, V2P(page), PTE_P|PTE_W|PTE_U);
        if (status == true) {
            memcpy(page, (void*)start, size);
        }
        else {
            kfree((uint64_t)page);
            free_vm(map, PAGE_SIZE);
        }
    }
    
    return status;
}

void free_pages(uint64_t map, uint64_t vstart, uint64_t vend)
{
    unsigned int index; 

    ASSERT(vstart % PAGE_SIZE == 0);
    ASSERT(vend % PAGE_SIZE == 0);

    do {
        PD pd = find_pdpt_entry(map, vstart, 0, 0);

        if (pd != NULL) {
            index = (vstart >> 21) & 0x1FF;
            if (pd[index] & PTE_P) {        
                page_decref(PTE_ADDR(pd[index]));
                pd[index] = 0;
            }
        }

        vstart += PAGE_SIZE;
    } while (vstart+PAGE_SIZE <= vend);
}

static void free_pdt(uint64_t map)
{
    PDPTR *map_entry = (PDPTR*)map;

    for (int i = 0; i < 512; i++) {
        if ((uint64_t)map_entry[i] & PTE_P) {            
            PD *pdptr = (PD*)P2V(PDE_ADDR(map_entry[i]));
            
            for (int j = 0; j < 512; j++) {
                if ((uint64_t)pdptr[j] & PTE_P) {
                    page_decref(PDE_ADDR(pdptr[j]));
                    pdptr[j] = 0;
                }
            }
        }
    }
}

static void free_pdpt(uint64_t map)
{
    PDPTR *map_entry = (PDPTR*)map;

    for (int i = 0; i < 512; i++) {
        if ((uint64_t)map_entry[i] & PTE_P) {
            page_decref(PDE_ADDR(map_entry[i]));
            map_entry[i] = 0;
        }
    }
}

static void free_pml4t(uint64_t map)
{
    page_decref(V2P(map));
}

void free_vm(uint64_t map, uint64_t size)
{
    free_pages(map, 0x400000, 0x400000 + PA_UP(size));
    free_pdt(map);
    free_pdpt(map);
    free_pml4t(map);
}

bool copy_uvm(uint64_t dst_map, uint64_t src_map, int size)
{
    int copied = 0;
    for (int off = 0; off < size; off += PAGE_SIZE) {
        void *page = kalloc();
        if (page == NULL) {
            free_pages(dst_map, 0x400000, 0x400000 + copied);
            return false;
        }
        memset(page, 0, PAGE_SIZE);
        if (!map_pages(dst_map, 0x400000 + off, 0x400000 + off + PAGE_SIZE,
                       V2P(page), PTE_P|PTE_W|PTE_U)) {
            kfree((uint64_t)page);
            free_pages(dst_map, 0x400000, 0x400000 + copied);
            return false;
        }

        PD pd = find_pdpt_entry(src_map, 0x400000 + off, 0, 0);
        if (pd == NULL) {
            free_pages(dst_map, 0x400000, 0x400000 + off + PAGE_SIZE);
            return false;
        }

        unsigned int index = ((0x400000 + off) >> 21) & 0x1FF;
        ASSERT((uint64_t)pd[index] & PTE_P);
        uint64_t start = P2V(PTE_ADDR(pd[index]));
        int to_copy = size - off;
        if (to_copy > PAGE_SIZE)
            to_copy = PAGE_SIZE;
        memcpy(page, (void*)start, to_copy);
        copied += PAGE_SIZE;
    }

    return true;
}

bool share_uvm(uint64_t dst_map, uint64_t src_map, int size)
{
    for (int off = 0; off < size; off += PAGE_SIZE) {
        PD pd = find_pdpt_entry(src_map, 0x400000 + off, 0, 0);
        if (pd == NULL)
            return false;

        unsigned int idx = ((0x400000 + off) >> 21) & 0x1FF;
        if (!(pd[idx] & PTE_P))
            continue;

        uint64_t pa = PTE_ADDR(pd[idx]);
        pd[idx] &= ~PTE_W;

        PD dstpd = find_pdpt_entry(dst_map, 0x400000 + off, 1, PTE_P|PTE_U);
        if (!dstpd)
            return false;
        dstpd[idx] = (PDE)(pa | PTE_P | PTE_U | PTE_ENTRY);

        page_incref(pa);
    }

    return true;
}

void invalidate_tlb(void)
{
    load_cr3(read_cr3());
}

