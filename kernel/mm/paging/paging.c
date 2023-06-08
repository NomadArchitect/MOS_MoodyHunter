// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"

#include <mos/lib/structures/bitmap.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/paging/pagemap.h>
#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <stdlib.h>

#define PGD_FOR_VADDR(_vaddr, _um) (_vaddr >= MOS_KERNEL_START_VADDR ? &platform_info->kernel_mm : _um)

typedef struct
{
    bool do_unmap;
    bool do_mark_free;
    bool do_unref;
} vmm_iterate_unmap_flags_t;

static slab_t *mm_context_cache = NULL;
MOS_SLAB_AUTOINIT("mm_context", mm_context_cache, mm_context_t);

// ! BEGIN: CALLBACKS
static void vmm_iterate_unmap(const pgt_iteration_info_t *iter_info, const vmblock_t *block, pfn_t block_pfn, void *arg)
{
    MOS_UNUSED(iter_info);

    const bool do_unmap = arg ? ((vmm_iterate_unmap_flags_t *) arg)->do_unmap : true;
    const bool do_mark_free = arg ? ((vmm_iterate_unmap_flags_t *) arg)->do_mark_free : true;
    const bool do_unref = arg ? ((vmm_iterate_unmap_flags_t *) arg)->do_unref : true;

    mos_debug(vmm_impl, "unmapping " PTR_FMT " -> " PFN_FMT " (npages: %zu)", block->vaddr, block_pfn, block->npages);

    if (do_unmap)
        platform_mm_unmap_pages(PGD_FOR_VADDR(block->vaddr, block->address_space), block->vaddr, block->npages);

    if (do_mark_free)
        pagemap_mark_free(block->address_space->um_page_map, block->vaddr, block->npages);

    if (do_unref)
        pmm_unref_frames(block_pfn, block->npages);
}

static void vmm_iterate_copymap(const pgt_iteration_info_t *iter_info, const vmblock_t *block, pfn_t block_pfn, void *arg)
{
    vmblock_t *vblock = arg;
    if (!vblock->flags)
        vblock->flags = block->flags;
    else
        MOS_ASSERT_X(vblock->flags == block->flags, "flags mismatch");
    const ptr_t target_vaddr = vblock->vaddr + (block->vaddr - iter_info->vaddr_start); // we know that vaddr is contiguous, so their difference is the offset
    mos_debug(vmm_impl, "copymapping " PTR_FMT " -> " PFN_FMT " (npages: %zu)", target_vaddr, block_pfn, block->npages);
    pmm_ref_frames(block_pfn, block->npages);
    platform_mm_map_pages(PGD_FOR_VADDR(target_vaddr, vblock->address_space), target_vaddr, block_pfn, block->npages, block->flags);
}
// ! END: CALLBACKS

vmblock_t mm_alloc_pages(mm_context_t *mmctx, size_t n_pages, ptr_t hint_vaddr, valloc_flags valloc_flags, vm_flags flags)
{
    MOS_ASSERT(n_pages > 0);

    const ptr_t vaddr = mm_get_free_pages(mmctx, n_pages, hint_vaddr, valloc_flags);
    if (unlikely(vaddr == 0))
    {
        mos_warn("could not find free %zd pages", n_pages);
        return (vmblock_t){ 0 };
    }

    const vmblock_t block = { .address_space = mmctx, .vaddr = vaddr, .npages = n_pages, .flags = flags };

    mmctx = PGD_FOR_VADDR(vaddr, mmctx);

    spinlock_acquire(&mmctx->pgd_lock);
    const pfn_t pfn = pmm_allocate_frames(n_pages, PMM_ALLOC_NORMAL);

    if (unlikely(pfn_invalid(pfn)))
    {
        spinlock_release(&mmctx->pgd_lock);
        mos_warn("could not allocate %zd physical pages", n_pages);
        return (vmblock_t){ 0 };
    }

    pmm_ref_frames(pfn, n_pages);
    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to pfn " PFN_FMT, n_pages, vaddr, pfn);
    platform_mm_map_pages(mmctx, vaddr, pfn, n_pages, flags);

    spinlock_release(&mmctx->pgd_lock);

    return block;
}

vmblock_t mm_map_pages(mm_context_t *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    MOS_ASSERT(npages > 0);

    const vmblock_t block = { .address_space = mmctx, .vaddr = vaddr, .npages = npages, .flags = flags };

    mmctx = PGD_FOR_VADDR(vaddr, mmctx); // after block is initialized

    if (unlikely(mmctx->pgd == 0))
    {
        mos_warn("cannot map pages at " PTR_FMT ", pagetable is null", vaddr);
        return (vmblock_t){ 0 };
    }

    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to pfn " PFN_FMT, npages, vaddr, pfn);

    spinlock_acquire(&mmctx->pgd_lock);
    pagemap_mark_used(mmctx->um_page_map, vaddr, npages);
    pmm_ref_frames(pfn, npages);
    platform_mm_map_pages(mmctx, vaddr, pfn, npages, flags);
    spinlock_release(&mmctx->pgd_lock);

    return block;
}

vmblock_t mm_early_map_kernel_pages(ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    MOS_ASSERT(npages > 0);
    MOS_ASSERT(vaddr >= MOS_KERNEL_START_VADDR);

    mm_context_t *mmctx = &platform_info->kernel_mm;
    const vmblock_t block = { .address_space = mmctx, .vaddr = vaddr, .npages = npages, .flags = flags };

    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to " PFN_FMT, npages, vaddr, pfn);

    spinlock_acquire(&mmctx->pgd_lock);
    pagemap_mark_used(mmctx->um_page_map, vaddr, npages);
    platform_mm_map_pages(mmctx, vaddr, pfn, npages, flags);
    spinlock_release(&mmctx->pgd_lock);

    return block;
}

void mm_unmap_pages(mm_context_t *mmctx, ptr_t vaddr, size_t npages)
{
    MOS_ASSERT(npages > 0);

    mmctx = PGD_FOR_VADDR(vaddr, mmctx);
    spinlock_acquire(&mmctx->pgd_lock);
    platform_mm_iterate_table(mmctx, vaddr, npages, vmm_iterate_unmap, NULL);
    spinlock_release(&mmctx->pgd_lock);
}

vmblock_t mm_replace_mapping(mm_context_t *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    MOS_ASSERT(npages > 0);

    const vmblock_t block = { .address_space = mmctx, .vaddr = vaddr, .npages = npages, .flags = flags };

    mmctx = PGD_FOR_VADDR(vaddr, mmctx); // after block is initialized

    if (unlikely(mmctx->pgd == 0))
    {
        mos_warn("cannot fill pages at " PTR_FMT ", pagetable is null", vaddr);
        return (vmblock_t){ 0 };
    }

    mos_debug(vmm, "filling %zd pages at " PTR_FMT " with " PFN_FMT, npages, vaddr, pfn);

    // only unreference the physical frames
    static const vmm_iterate_unmap_flags_t umflags = { .do_unmap = false, .do_mark_free = false, .do_unref = true };

    spinlock_acquire(&mmctx->pgd_lock);
    // ref the frames first, so they don't get freed if we're [filling a page with itself]?
    // weird people do weird things
    pmm_ref_frames(pfn, npages);
    platform_mm_iterate_table(mmctx, vaddr, npages, vmm_iterate_unmap, (void *) &umflags);
    platform_mm_map_pages(mmctx, vaddr, pfn, npages, flags);
    spinlock_release(&mmctx->pgd_lock);

    return block;
}

vmblock_t mm_copy_maps(mm_context_t *from, ptr_t fvaddr, mm_context_t *to, ptr_t tvaddr, size_t npages, mm_copy_behavior_t behavior)
{
    MOS_ASSERT(npages > 0);
    mos_debug(vmm, "copying mapping from " PTR_FMT " to " PTR_FMT ", %zu pages", fvaddr, tvaddr, npages);

    from = PGD_FOR_VADDR(fvaddr, from);
    vmblock_t result = { .address_space = to, .vaddr = tvaddr, .npages = npages };
    to = PGD_FOR_VADDR(tvaddr, to); // after result is initialized

    if (unlikely(from->pgd == 0 || to->pgd == 0))
    {
        mos_warn("cannot remap pages from " PTR_FMT " to " PTR_FMT ", pagetable is null", fvaddr, tvaddr);
        return (vmblock_t){ 0 };
    }

    spinlock_acquire(&from->pgd_lock);
    if (to != from)
        spinlock_acquire(&to->pgd_lock);

    if (behavior != MM_COPY_ALLOCATED)
        pagemap_mark_used(to->um_page_map, tvaddr, npages);
    platform_mm_iterate_table(from, fvaddr, npages, vmm_iterate_copymap, &result);

    if (to != from)
        spinlock_release(&to->pgd_lock);
    spinlock_release(&from->pgd_lock);

    return result;
}

bool mm_get_is_mapped(mm_context_t *mmctx, ptr_t vaddr)
{
    return pagemap_get_mapped(mmctx->um_page_map, vaddr);
}

void mm_flag_pages(mm_context_t *mmctx, ptr_t vaddr, size_t npages, vm_flags flags)
{
    MOS_ASSERT(npages > 0);

    mmctx = PGD_FOR_VADDR(vaddr, mmctx);
    if (unlikely(mmctx->pgd == 0))
    {
        mos_warn("cannot flag pages at " PTR_FMT ", pagetable is null", vaddr);
        return;
    }

    mos_debug(vmm, "flagging %zd pages at " PTR_FMT " with flags %x", npages, vaddr, flags);

    spinlock_acquire(&mmctx->pgd_lock);
    platform_mm_flag_pages(mmctx, vaddr, npages, flags);
    spinlock_release(&mmctx->pgd_lock);
}

mm_context_t *mm_new_context(void)
{
    mm_context_t *mmctx = kmemcache_alloc(mm_context_cache);
    linked_list_init(&mmctx->mmaps);

    mmctx->pgd = platform_mm_create_user_pgd();

    if (unlikely(mmctx->pgd == 0))
        goto bail;

    mmctx->um_page_map = kzalloc(sizeof(page_map_t));
    if (unlikely(mmctx->um_page_map == 0))
        goto bail;

    return mmctx;

bail:
    mos_warn("cannot create user pgd");
    if (mmctx->um_page_map != 0)
        kfree(mmctx->um_page_map);
    if (mmctx->pgd != 0)
        mm_destroy_user_pgd(mmctx);
    return NULL;
}

void mm_destroy_user_pgd(mm_context_t *mmctx)
{
    if (unlikely(mmctx->pgd == 0))
    {
        mos_warn("cannot destroy user pgd, pagetable is null");
        return;
    }

    spinlock_acquire(&mmctx->pgd_lock);
    spinlock_acquire(&mmctx->um_page_map->lock);

    platform_mm_destroy_user_pgd(mmctx);
    kfree(mmctx->um_page_map);
}
