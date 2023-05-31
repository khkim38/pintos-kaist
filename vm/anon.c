/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

/* project3 Anonymous Page */
#include <bitmap.h>
#include "threads/vaddr.h"
#include "threads/mmu.h"
/* ----------------------- */
#include "vm/vm.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
/* project3 Anonymous Page */
struct bitmap *swap_table;
/* ----------------------- */
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	/* project3 Anonymous Page */
	swap_disk = disk_get(1, 1);
	int swap_size = disk_size(swap_disk) * DISK_SECTOR_SIZE / PGSIZE;
	swap_table = bitmap_create(swap_size);
	/* ----------------------- */
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	/* project3 Swap In/Out */
	struct uninit_page *uninit = &page->uninit;
	memset(uninit,0,sizeof(struct uninit_page));
	return true;
	/* -------------------- */
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	int n_sector=PGSIZE/DISK_SECTOR_SIZE;
	/* project3 Swap In/Out */
	int page_swap = anon_page->swap_slot;
	if(!bitmap_test(swap_table,page_swap)){
		return false;
	}
	for (int i=0;i<n_sector;i++){
		disk_read(swap_disk,(page_swap*n_sector)+i,kva+(DISK_SECTOR_SIZE*i));
	}
	// bitmap_flip(swap_table,page_n);
	bitmap_set(swap_table,page_swap,false);

	return true;
	/* -------------------- */
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	int n_sector=PGSIZE/DISK_SECTOR_SIZE;
	/* project3 Swap In/Out */
	int page_swap=bitmap_scan(swap_table,0,1,false);
	if(page_swap==BITMAP_ERROR){
		return false;
	}
	for (int i=0;i<n_sector;i++){
		disk_write(swap_disk,(page_swap*n_sector)+i,page->va+(DISK_SECTOR_SIZE*i));
	}
	// bitmap_flip(swap_table,page_n);
	bitmap_set(swap_table,page_swap,true);
	pml4_clear_page(thread_current()->pml4,page->va);
	anon_page->swap_slot=page_swap;
	return true;
	/* -------------------- */
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
