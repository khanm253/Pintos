#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
//NEW

#include <stdbool.h>
#include <debug.h>
#include <string.h>
#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "userprog/process.h"
#include "devices/input.h"
#include "userprog/pagedir.h"
#include "threads/init.h"
#include "devices/shutdown.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"

void syscall_init (void);

typedef int pid_t;


void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;
pid_t exec (const char *file);
int wait (pid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
struct track_files* find_track_file (int fd);
struct file *find_files(int file_descriptor,int mode);
void validate_memory_address(const void *ptr_to_check);
void empty_out_file_list(struct list* files);
bool change_directory(const char *name);


#endif /* userprog/syscall.h */
