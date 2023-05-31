/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* project3 Memory Management */
struct list frame_table;
/* -------------------------- */

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	/* project3 Memory Management */
	list_init (&frame_table);
	/* -------------------------- */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* project3 Anonymous Page */
		struct page *page = (struct page*) malloc (sizeof (struct page));

		switch(VM_TYPE(type)){
			case VM_ANON:
				uninit_new(page, upage, init, type, aux, anon_initializer);
				break;
			case VM_FILE:
				uninit_new(page, upage, init, type, aux, file_backed_initializer);
				break;
		}
		page->writable = writable;

		return spt_insert_page(spt, page);
		/* ----------------------- */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	// struct page *page = NULL;
	/* TODO: Fill this function. */
	/* project3 Memory Management */
	struct page *page = (struct page*) malloc (sizeof (struct page));
	struct hash_elem *e;

	page->va = pg_round_down(va);
	e = hash_find (&spt->hash_table, &page->hash_elem);

	free(page);

	if (e != NULL){
		page = hash_entry (e, struct page, hash_elem);
	}
	else {
		page = NULL;
	}
	/* -------------------------- */
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	/* project3 Memory Management */
	struct hash_elem *e = hash_insert (&spt->hash_table, &page->hash_elem);
	if (e == NULL){
		succ = true;
	}
	else {
		succ = false;
	}
	/* -------------------------- */

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	// struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	/* project3 Memory Management */
	struct frame *victim = list_entry (list_pop_front (&frame_table), struct frame, frame_elem);
	/* -------------------------- */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	/* TODO: swap out the victim and return the evicted frame. */
	/* project3 Memory Management */
	struct frame *victim = vm_get_victim ();
	swap_out (victim->page);

	victim->page->frame = NULL;
	victim->page = NULL;

	return victim;
	/* -------------------------- */
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	/* project3 Memory Management */
	struct frame *frame = (struct frame *) malloc (sizeof (struct frame));
	/* TODO: Fill this function. */
	frame->kva = palloc_get_page (PAL_USER);
	if (frame->kva == NULL){
		frame = vm_evict_frame ();
		return frame;
	}

	list_push_back (&frame_table, &frame->frame_elem);
	frame->page = NULL;
	/* -------------------------- */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	/* project3 Stack Growth */
	if(vm_alloc_page(VM_ANON | VM_MARKER_0, addr, 1))
    {
        vm_claim_page(addr);
        thread_current()->stack_bottom -= PGSIZE;
    }
	/* --------------------- */
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	/* project3 Anonymous Page */
	if (is_kernel_vaddr(addr)) {
        return false;
	}

    void *rsp_stack = is_kernel_vaddr(f->rsp) ? thread_current()->rsp_stack : f->rsp;
    if (not_present){
        if (!vm_claim_page(addr)) {
            if (rsp_stack - 8 <= addr && USER_STACK - 0x100000 <= addr && addr <= USER_STACK) {
                vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
                return true;
            }
            return false;
        }
        else
            return true;
    }
    return false;
	/* ----------------------- */
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	/* TODO: Fill this function */
	/* project3 Memory Management */
	struct page *page = spt_find_page (&thread_current ()->spt, va);
	
	if (page == NULL)
		return false;
	/* -------------------------- */

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* project3 Memory Management */
	if (!pml4_set_page(thread_current ()->pml4, page->va, frame->kva, page->writable))
	{
		return false;
	}
	/* -------------------------- */

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	/* project3 Memory Management */
	hash_init (&spt->hash_table, hash_func, hash_less, NULL);
	/* -------------------------- */
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	/* project3 Swap In/Out */
	// struct thread *curr=thread_current();
	// struct hash_iterator h_i;
	// struct hash *parent = &src->hash_table;
	// hash_first(&h_i,parent);
	// while(hash_next(&h_i)){
	// 	struct page *parent_page=hash_entry(hash_cur(&h_i),struct page,hash_elem);
	// 	if(parent_page->operations->type==VM_UNINIT){
	// 		vm_initializer *init=parent_page->uninit.init;
	// 		void *aux=parent_page->uninit.aux;
	// 		vm_alloc_page_with_initializer(parent_page->uninit.type,parent_page->va,parent_page->writable,init,aux);
	// 	} else {
	// 		struct page *child_page=(struct page*)malloc(sizeof(struct page));
	// 		memcpy(child_page,parent_page,sizeof(struct page));
	// 		if(!spt_insert_page(dst,child_page)){
	// 			return false;
	// 		}
	// 		if(!pml4_set_page(curr->pml4,child_page->va,child_page->frame->kva,false)){
	// 			return false;
	// 		}
	// 	}
	// }

	// struct thread *curr = thread_current();
	// struct hash *parent_table = &src->hash_table;
	// struct hash_iterator i;

	// hash_first (&i, parent_table);
	// while (hash_next(&i)){
	// 	struct page *parent_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		
	// 	if (parent_page->operations->type == VM_UNINIT){
	// 		vm_initializer *init = parent_page->uninit.init;
	// 		void *aux = parent_page->uninit.aux;
	// 		vm_alloc_page_with_initializer(parent_page->uninit.type, parent_page->va, parent_page->writable, init, aux);
	// 		// printf("c\n");
	// 	}
	// }

	// hash_first (&i, parent_table);
	// while (hash_next(&i)){
	// 	struct page *parent_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		
	// 	if (parent_page->operations->type == VM_ANON){
	// 		struct page *child_page = (struct page *) malloc (sizeof (struct page));
	// 		child_page->operations = parent_page->operations;
	// 		child_page->va = parent_page->va;
	// 		child_page->writable = parent_page->writable;
	// 		child_page->frame = NULL;
	// 		child_page->anon.swap_slot = parent_page->anon.swap_slot;

	// 		vm_claim_page(child_page->va);
			
	// 		if (!spt_insert_page(dst, child_page)){
	// 			return false;
	// 		}
	// 		// printf("a\n");
	// 	}
	// 	else if (parent_page->operations->type == VM_FILE){
	// 		struct page *child_page = (struct page *) malloc (sizeof (struct page));
	// 		child_page->operations = parent_page->operations;
	// 		child_page->va = parent_page->va;
	// 		child_page->writable = parent_page->writable;
	// 		child_page->frame = NULL;
	// 		memcpy(&child_page->file, &parent_page->file, sizeof(struct file_page));
			


	return true;
	/* --------------------- */
}
/* project3 Memory Mapped Files: Munmap */
void spt_destructor(struct hash_elem *e){
	struct page *p=hash_entry(e,struct page,hash_elem);
	free(p);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */


	struct thread *curr = thread_current();
	struct list_elem *temp_elem = list_begin(&curr->mmap_list);

	for (; temp_elem != list_tail(&curr->mmap_list); ){
		struct mmap_file *temp_mmap = list_entry(temp_elem, struct mmap_file, elem);
		temp_elem = temp_elem->next;
		munmap(temp_mmap->addr);
	}

	hash_destroy(&spt->hash_table,spt_destructor);
}
/* ------------------------------------ */

/* project3 Memory Management: additional function */
unsigned hash_func (const struct hash_elem *p_, void *aux UNUSED) {
    const struct page *p = hash_entry(p_, struct page, hash_elem);
    return hash_bytes(&p->va, sizeof p->va);
}

bool hash_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
    const struct page *a = hash_entry(a_, struct page, hash_elem);
    const struct page *b = hash_entry(b_, struct page, hash_elem);

    return a->va < b->va;
}
/* ----------------------------------------------- */