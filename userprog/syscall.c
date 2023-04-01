#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	/* project2 */
	switch (f->R.rax){
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
			break;
		// case SYS_FORK:
		// 	f->R.rax = fork(f->R.rdi, f);
		// 	break;
		case SYS_EXEC:
			if (exec(f->R.rdi) == -1) exit(-1);
			break;
		case SYS_WAIT:
			f->R.rax = process_wait(f->R.rdi);
			break;
		case SYS_CREATE:
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_OPEN:
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK:
			seek(f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL:
			f->R.rax = tell(f->R.rdi);
			break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
		default:
			exit(-1);
			break;	
	}
	/* -------- */
}

/* project2 */
void check_address(void *addr) {
	struct thread *cur = thread_current();
	if (addr == NULL || !(is_user_vaddr(addr)) || pml4_get_page(cur->pml4, addr) == NULL)
		exit(-1);
}

void halt(void){
	power_off();
}

void exit(int status){
	thread_current()->exit = status;
	printf("%s: exit(%d)\n", thread_current()->name, status);
	thread_exit();
}

// int fork(const char *thread_name, struct intr_frame *f){
// 	return 0;
// }

int exec(char *file_name) {
	check_address(file_name);

	char *fn_copy = palloc_get_page(0);
	if (fn_copy == NULL) exit(-1);
	strlcpy(fn_copy, file_name, strlen(file_name)+1);
	if (process_exec(fn_copy) == -1) return -1;

	NOT_REACHED();
	return 0;
}

int create(const char *file, unsigned initial_size) {
	check_address(file);
	return filesys_create(file, initial_size);
}

int remove(const char *file) {
	check_address(file);
	return filesys_remove(file);
}

int open(const char *file){
	check_address(file);

	struct thread *cur = thread_current();
	struct file *file_obj = filesys_open(file);
	if (file_obj == NULL) return -1;

	int fd_idx = cur->fd_idx;
	while (cur->file_list[fd_idx] != NULL){
		if (fd_idx > 100) {
			file_close(file_obj);
			return -1;
		}
		fd_idx += 1;
	}
	cur->fd_idx = fd_idx;
	cur->file_list[fd_idx] = file;

	return fd_idx;
}

int filesize(int fd){
	return 0;
}

int read(int fd, void *buffer, unsigned size){
	return 0;
}

int write(int fd, const void *buffer, unsigned size){
	printf("%s", buffer);
	return size;
}

void seek(int fd, unsigned position){
	return 0;
}

unsigned tell(int fd){
	return 0;
}

void close(int fd){
	return 0;
}

/* -------- */
