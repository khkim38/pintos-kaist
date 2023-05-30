#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

/* project2 system call */
#include "threads/init.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

/* project3 Anonymous Page: check buffer */
#include "vm/vm.h"
/* ------------------------------------- */

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

struct lock file_lock;

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&file_lock);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	// TODO: Your implementation goes here.
	/* project3 Stack Growth */
	thread_current()->rsp_stack = f->rsp;
	/* --------------------- */
	
	/* project2 */
	switch (f->R.rax)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi);
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi, f);
		break;
	case SYS_EXEC:
		/* project3 Anonymous Page: check buffer */
		check_address(f->R.rdi);
		/* ------------------------------------- */
		if (exec(f->R.rdi) == -1)
			exit(-1);
		// exec(f->R.rdi);
		break;
	case SYS_WAIT:
		f->R.rax = process_wait(f->R.rdi);
		break;
	case SYS_CREATE:
		/* project3 Anonymous Page: check buffer */
		check_address(f->R.rdi);
		/* ------------------------------------- */
		f->R.rax = (uint64_t)create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		/* project3 Anonymous Page: check buffer */
		check_address(f->R.rdi);
		/* ------------------------------------- */
		f->R.rax = (uint64_t)remove(f->R.rdi);
		break;
	case SYS_OPEN:
		/* project3 Anonymous Page: check buffer */
		check_address(f->R.rdi);
		/* ------------------------------------- */
		f->R.rax = (uint64_t)open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = (uint64_t)filesize(f->R.rdi);
		break;
	case SYS_READ:
		/* project3 Anonymous Page: check buffer */
		check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 1);
		/* ------------------------------------- */
		f->R.rax = (uint64_t)read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		/* project3 Anonymous Page: check buffer */
		check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 0);
		/* ------------------------------------- */
		f->R.rax = (uint64_t)write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = (uint64_t)tell(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	/* project3 Memory Mapped Files */
	case SYS_MMAP:
		f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
		break;
	case SYS_MUNMAP:
		check_address(f->R.rdi);
		munmap(f->R.rdi);
		break;
	/* ------------- */
	default:
		exit(-1);
		break;
	}
}

/* project3 Anonymous Page: check buffer */

struct page * check_address(void *addr){
	if(is_kernel_vaddr(addr)) exit(-1);
	
	return spt_find_page(&thread_current()->spt, addr);
}

void check_valid_buffer(void* buffer, unsigned size, void* rsp, bool to_write){
	struct page* page;

	if(buffer <= USER_STACK && buffer >= rsp) return;

	for(int i=0; i<size; i++){
		page = check_address(buffer+i);
		if(page == NULL) exit(-1);
		// if(to_write && !page->writable) exit(-1);
		if(to_write == true && page->writable == false) exit(-1);
	}
}
/* ------------------------------------- */

void halt(void)
{
	power_off();
}

void exit(int status)
{
	struct thread *curr = thread_current();
	curr->exit = status;
	printf("%s: exit(%d)\n", thread_current()->name, status);
	thread_exit();
}

int fork(const char *thread_name, struct intr_frame *f)
{
	return process_fork(thread_name, f);
}

int exec(char *file_name)
{
	// check_address(file_name);

	char *fn_copy = palloc_get_page(PAL_ZERO);
	tid_t tid;
	if (fn_copy == NULL)
		exit(-1);
	strlcpy(fn_copy, file_name, strlen(file_name) + 1);
	if ((tid = process_exec(fn_copy)) == -1)
		return 0;

	struct thread *child;
	struct thread *curr = thread_current();
	for (struct list_elem *e = list_begin(&curr->child_list); e != list_end(&curr->child_list); e = list_next(e))
	{
		struct thread *temp = list_entry(e, struct thread, child_elem);
		if (temp->tid == tid)
		{
			child = temp;
			break;
		}
	}
	if (child != NULL)
	{
		sema_down(&child->load_semaphore);
	}

	NOT_REACHED();
	return 0;
}

int create(const char *file, unsigned initial_size)
{
	// check_address(file);
	return filesys_create(file, initial_size);
}

int remove(const char *file)
{
	// check_address(file);
	return filesys_remove(file);
}

int open(const char *file)
{
	// check_address(file);
	/* project 3 Test case: open-null 해결 */
	if(file==NULL){
		return -1;
	}
	/* ---------------------------------- */
	struct thread *cur = thread_current();
	lock_acquire(&file_lock);
	struct file *file_obj = filesys_open(file);
	lock_release(&file_lock);
	if (file_obj == NULL)
	{
		return -1;
	}

	/* project2 syscall rox */
	// if(strcmp(file, thread_name()) == 0){
	// 	file_deny_write(file_obj);
	// }
	/* --------------------- */

	int fd_idx;
	for (fd_idx = 2; fd_idx < 128; fd_idx++)
	{
		if (cur->file_list[fd_idx] == NULL)
			break;
		fd_idx += 1;
	}
	if (fd_idx >= 128)
	{
		file_close(file_obj);
		return -1;
	}
	cur->fd_idx = fd_idx;
	cur->file_list[fd_idx] = file_obj;
	return fd_idx;
}

int filesize(int fd)
{
	struct thread *curr = thread_current();
	if (fd < 0)
		return -1;
	if (curr->file_list[fd] == NULL)
		return -1;
	return file_length(curr->file_list[fd]);
}
/*jhy*/
int read(int fd, void *buffer, unsigned size)
{
	// check_address(buffer);
	// check_address(buffer+size);
	int read_out;
	if (fd == 0)
	{
		for (int i = 0; i < size; i++)
		{
			((char *)buffer)[i] = input_getc();
		}
		read_out = size;
	}
	else if (fd < 2)
	{
		return -1;
	}
	else
	{
		struct thread *curr = thread_current();
		if (fd < 0)
			return -1;
		if (curr->file_list[fd] == NULL)
			return -1;
		lock_acquire(&file_lock);
		read_out = file_read(curr->file_list[fd], buffer, size);
		lock_release(&file_lock);
	}
	return read_out;
}
/*jh*/
int write(int fd, const void *buffer, unsigned size)
{
	// check_address(buffer);
	// check_address(buffer+size);
	int write_out;
	if (fd == 1)
	{
		putbuf(buffer, size);
		write_out = size;
	}
	else if (fd < 2)
	{
		return -1;
	}
	else
	{
		struct thread *curr = thread_current();
		if (fd < 0)
			return -1;
		if (curr->file_list[fd] == NULL)
			return -1;
		lock_acquire(&file_lock);
		// file_allow_write(curr->file_list[fd]);
		write_out = file_write(curr->file_list[fd], buffer, size);
		// file_deny_write(curr->file_list[fd]);
		lock_release(&file_lock);
	}
	return write_out;
}

void seek(int fd, unsigned position)
{
	struct thread *curr = thread_current();
	if (fd < 0)
		return -1;
	if (curr->file_list[fd] == NULL)
		return -1;
	file_seek(curr->file_list[fd], position);
}

unsigned tell(int fd)
{
	struct thread *curr = thread_current();
	if (fd < 0)
		return -1;
	if (curr->file_list[fd] == NULL)
		return -1;
	return file_tell(curr->file_list[fd]);
}
/*jh*/
void close(int fd)
{
	struct thread *t = thread_current();
	if (fd < 2)
		return;
	if (t->file_list[fd] == NULL)
		return;

	// file_allow_write(t->file_list[fd]);

	lock_acquire(&file_lock);
	if (t->file_list[fd] != NULL)
		file_close(t->file_list[fd]);
	t->file_list[fd] = NULL;
	lock_release(&file_lock);
}

/* -------- */

/* project3 Memory Mapped Files: Mmap & Munmap */
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset) {

    if (offset % PGSIZE != 0) {
        return NULL;
    }

    if (pg_round_down(addr) != addr || is_kernel_vaddr(addr) || addr == NULL || (long long)length <= 0)
        return NULL;
    
    if (fd == 0 || fd == 1)
        exit(-1);
    
    // vm_overlap
    if (spt_find_page(&thread_current()->spt, addr))
        return NULL;

    struct file *target = thread_current()->file_list[fd];

    if (target == NULL)
        return NULL;

    void * ret = do_mmap(addr, length, writable, target, offset);

    return ret;
}

void munmap (void *addr) {
    do_munmap(addr);
}
/* ------------- */