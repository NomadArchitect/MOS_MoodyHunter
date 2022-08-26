// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/paging_impl.h"

#include "mos/mos_global.h"
#include "mos/printk.h"
#include "mos/types.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/pmem_freelist.h"
#include "mos/x86/x86_platform.h"

#define PAGEMAP_MAP(map, index)   map[index / PAGEMAP_WIDTH] |= 1 << (index % PAGEMAP_WIDTH)
#define PAGEMAP_UNMAP(map, index) map[index / PAGEMAP_WIDTH] &= ~(1 << (index % PAGEMAP_WIDTH))

void *pg_page_alloc(x86_pg_infra_t *pg, size_t n_page)
{
#define BIT_IS_SET(byte, bit) ((byte) & (1 << (bit)))
    // simply rename the variable, we are dealing with bitmaps
    size_t n_bits = n_page;
    size_t n_zero_bits = 0;

    u8 target_bit = 0;

    // always allocate after the end of the kernel
    size_t kernel_page_end = x86_kernel_end / X86_PAGE_SIZE;
    size_t target_pagemap_start = (kernel_page_end / PAGEMAP_WIDTH) + 1;

    for (size_t i = target_pagemap_start; n_zero_bits < n_bits; i++)
    {
        if (i >= MM_PAGE_MAP_SIZE)
        {
            mos_warn("failed to allocate %zu pages", n_page);
            return NULL;
        }
        pagemap_line_t current_byte = pg->page_map[i];

        if (current_byte == 0)
        {
            n_zero_bits += PAGEMAP_WIDTH;
            continue;
        }
        if (current_byte == (pagemap_line_t) ~0)
        {
            target_pagemap_start = i + 1;
            continue;
        }

        for (size_t bit = 0; bit < PAGEMAP_WIDTH; bit++)
        {
            if (!BIT_IS_SET(current_byte, bit))
                n_zero_bits++;
            else
                n_zero_bits = 0, target_bit = bit + 1, target_pagemap_start = i;
        }
    }

    size_t page_i = target_pagemap_start * PAGEMAP_WIDTH + target_bit;
    void *vaddr = (void *) (page_i * X86_PAGE_SIZE);
    mos_debug("paging: allocating page %zu to %zu (aka starting at %p)", page_i, page_i + n_page, vaddr);

    uintptr_t paddr = pmem_freelist_get_free_pages(n_page);

    if (paddr == 0)
    {
        mos_panic("OOM");
        return NULL;
    }

    pg_map_pages(pg, (uintptr_t) vaddr, paddr, n_page, VM_WRITABLE);
    return vaddr;
}

bool pg_page_free(x86_pg_infra_t *pg, uintptr_t vptr, size_t n_page)
{
    size_t page_index = vptr / X86_PAGE_SIZE;
    mos_debug("paging: freeing %zu to %zu", page_index, page_index + n_page);
    pg_unmap_pages(pg, vptr, n_page);
    return true;
}

void pg_page_flag(x86_pg_infra_t *pg, uintptr_t vaddr, size_t n, page_flags flags)
{
    mos_debug("paging: setting flags [%x] to [" PTR_FMT "] +%zu pages", flags, vaddr, n);
    for (size_t i = 0; i < n; i++)
    {
        uintptr_t vaddr_n = vaddr + i * X86_PAGE_SIZE;

        size_t page_index = vaddr_n / X86_PAGE_SIZE;
        size_t page_table_index = page_index / 1024;

        if (!pg->pgtable[page_table_index].present)
            mos_panic("page table entry is not present");

        pg->pgtable[page_table_index].writable = flags & VM_WRITABLE;
        pg->pgtable[page_table_index].usermode = flags & VM_USERMODE;
        pg->pgtable[page_table_index].accessed = flags & VM_ACCESSED;
        pg->pgtable[page_table_index].cache_disabled = flags & VM_CACHE_DISABLED;
    }
}

void pg_map_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, uintptr_t paddr_start, size_t n_page, u32 flags)
{
    pmem_freelist_remove_region(paddr_start, n_page * X86_PAGE_SIZE);
    pg_do_map_pages(pg, vaddr_start, paddr_start, n_page, flags);
}

void pg_unmap_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, size_t n_page)
{
    uintptr_t paddr = pg_page_get_mapped_paddr(pg, vaddr_start);
    pg_do_unmap_pages(pg, vaddr_start, n_page);
    pmem_freelist_add_region(paddr, n_page * X86_PAGE_SIZE);
}

void pg_do_map_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, uintptr_t paddr_start, size_t n_page, u32 flags)
{
    mos_debug("paging: mapping %zu pages " PTR_FMT "-" PTR_FMT " at table %lu", n_page, vaddr_start, paddr_start, vaddr_start / X86_PAGE_SIZE);
    for (size_t i = 0; i < n_page; i++)
        pg_do_map_page(pg, vaddr_start + i * X86_PAGE_SIZE, paddr_start + i * X86_PAGE_SIZE, flags);
}

void pg_do_unmap_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, size_t n_page)
{
    mos_debug("paging: unmapping %zu pages " PTR_FMT " at table %lu", n_page, vaddr_start, vaddr_start / X86_PAGE_SIZE);
    for (size_t i = 0; i < n_page; i++)
        pg_do_unmap_page(pg, vaddr_start + i * X86_PAGE_SIZE);
}

void pg_do_map_page(x86_pg_infra_t *pg, uintptr_t vaddr, uintptr_t paddr, page_flags flags)
{
    flags |= VM_USERMODE;
    flags |= VM_WRITABLE;
    // ensure the page is aligned to 4096
    MOS_ASSERT_X(paddr < X86_MAX_MEM_SIZE, "physical address out of bounds");
    MOS_ASSERT_X(flags < 0x100, "invalid flags");
    MOS_ASSERT_X(vaddr % X86_PAGE_SIZE == 0, "vaddr is not aligned to 4096");

    // ! todo: ensure the offsets are correct for both paddr and vaddr
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff; // mask out the lower 12 bits

    x86_pgdir_entry *page_dir = &pg->pgdir[page_dir_index];
    x86_pgtable_entry *page_table;

    if (likely(page_dir->present))
    {
        page_table = ((x86_pgtable_entry *) (page_dir->page_table_addr << 12)) + page_table_index;
    }
    else
    {
        // mm_kernel_pgt???
        page_table = &pg->pgtable[page_dir_index * 1024 + page_table_index];
        page_dir->present = true;
        page_dir->page_table_addr = (uintptr_t) page_table >> 12;
    }

    page_dir->writable |= !!(flags & VM_WRITABLE);
    page_dir->usermode |= !!(flags & VM_USERMODE);

    MOS_ASSERT_X(page_table->present == false, "page is already mapped");

    page_table->present = true;
    page_table->writable = !!(flags & VM_WRITABLE);
    page_table->usermode = !!(flags & VM_USERMODE);
    page_table->phys_addr = (uintptr_t) paddr >> 12;

    // update the mm_page_map
    u32 pte_index = page_dir_index * 1024 + page_table_index;
    PAGEMAP_MAP(pg->page_map, pte_index);
}

void pg_do_unmap_page(x86_pg_infra_t *pg, uintptr_t vaddr)
{
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff;

    x86_pgdir_entry *page_dir = &pg->pgdir[page_dir_index];
    if (unlikely(!page_dir->present))
    {
        mos_panic("vmem '%lx' not mapped", vaddr);
        return;
    }

    x86_pgtable_entry *page_table = ((x86_pgtable_entry *) (page_dir->page_table_addr << 12)) + page_table_index;
    page_table->present = false;

    // update the mm_page_map
    u32 pte_index = page_dir_index * 1024 + page_table_index;
    PAGEMAP_UNMAP(pg->page_map, pte_index);
}

uintptr_t pg_page_get_mapped_paddr(x86_pg_infra_t *pg, uintptr_t vaddr)
{
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff;
    x86_pgdir_entry *page_dir = pg->pgdir + page_dir_index;

    if (unlikely(!page_dir->present))
        mos_panic("page directory for address '%lx' not mapped", vaddr);

    x86_pgtable_entry *page_table = pg->pgtable + page_dir_index * 1024 + page_table_index;
    if (unlikely(!page_table->present))
        mos_panic("vmem '%lx' not mapped", vaddr);

    return (page_table->phys_addr << 12) + (vaddr & 0xfff);
}