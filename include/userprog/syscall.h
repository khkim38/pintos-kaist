#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/*project2*/
#include "threads/synch.h"
struct lock filesys_lock;
void syscall_init (void);

void check_address(void *addr);
// void directory_to_file(int file);

void halt(void);
void exit(int status);
// int fork(const char *thread_name, struct intr_frame *f);
int exec(char *file_name);
//int wait (tid_t pid);
int create (const char *file, unsigned initial_size);
int remove (const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
/* -------- */

#endif /* userprog/syscall.h */
