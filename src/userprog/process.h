#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H
//New
#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

//New functions
int valid_name(const char * name);
void insert_child_list(struct thread *m);
void waiting_child(struct child* c);
struct child* get_child(int tid,int status);


#endif /* userprog/process.h */
