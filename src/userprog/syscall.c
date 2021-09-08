#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "threads/thread.h"
//NEW

static void syscall_handler (struct intr_frame *);



//This struct keeps track of the files
//used for reading and writing once opened
struct track_files
{
    struct list_elem file_elem;
    int not_file;
    struct dir *directory;
    struct file *address;
    int fd;
};


//Initializes syscall handling.
void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
    validate_memory_address((const void *) f->esp);
    validate_memory_address((const void *) (f->esp+1));
    validate_memory_address((const void *) (f->esp+2));
    validate_memory_address((const void *) (f->esp+3));



    int call = *((int *)f->esp);

    if(call==SYS_HALT){
				halt();
    }else if(call==SYS_EXIT){

        int *p = f->esp;
        validate_memory_address(p+1);
        exit(*(p+1));

    }else if(call==SYS_EXEC){

        int *p = f->esp;
        validate_memory_address(p+1);
        validate_memory_address(p+2);
        validate_memory_address(p+3);
        validate_memory_address((const void *)*(p+1));


        if (!pagedir_get_page(thread_current()->pagedir, (const void*)*(p+1)))
        {
          exit(-1);
        }
        f->eax = exec((const char*)*(p+1));

    }else if(call==SYS_WAIT){

                int *p = f->esp;
                validate_memory_address((const void*)(p+1));
				f->eax = wait((pid_t) *(p+1));

    }else if(call==SYS_CREATE){
        int *p = f->esp;
        validate_memory_address((const void*)(p+2));
        validate_memory_address((const void*)*(p+1));

        if (!pagedir_get_page(thread_current()->pagedir, (const void *) (const void*)*(p+4)))
        {
          exit(-1);
        }
        f->eax = create((const char *) *(p+1), (unsigned) *(p+2));

    }else if(call==SYS_REMOVE){
        int *p = f->esp;
        validate_memory_address(p+1);
        validate_memory_address((const void*)*(p+1));
        if (!pagedir_get_page(thread_current()->pagedir, (const void*)*(p+1)))
        {
          exit(-1);
        }
        f->eax = remove((const char *) *(p+1));

    }else if(call==SYS_OPEN){
        int *p = f->esp;
        validate_memory_address(p+1);
        if (!pagedir_get_page(thread_current()->pagedir, (const void*)*(p+1)))
        {
          exit(-1);
        }
        f->eax = open((const char *) *(p+1));

    }else if(call==SYS_FILESIZE){
        int *p = f->esp;
        validate_memory_address(p+1);
        f->eax = filesize((int)*(p+1));

    }else if(call==SYS_READ){
        int *p = f->esp;
        validate_memory_address((const void*)(p+1));
        validate_memory_address((const void*)(p+2));
        validate_memory_address((const void*)*(p+2));
        validate_memory_address((const void*)(p+3));


          int i;
          char *ptr  = (char * ) *(p+2);
          for (i = 0; i < *(p+3); i++)
            {
              validate_memory_address((const void *) ptr);
              ptr++;
            }



        if (!pagedir_get_page(thread_current()->pagedir, (const void *) *(p+2)))
        {
          exit(-1);
        }
        lock_acquire(&file_lock);
        f->eax = read(*(p+1), (void *) *(p+2), (unsigned) *(p+3));
        lock_release(&file_lock);
    }else if(call==SYS_WRITE){
        int *p = f->esp;
        validate_memory_address((const void*)(p+1));
        validate_memory_address((const void*)(p+2));
        validate_memory_address((const void*)(p+3));

        if (!pagedir_get_page(thread_current()->pagedir, (const void *) *(p+2)))
        {
          exit(-1);
        }

        lock_acquire(&file_lock);
        f->eax = write(*(p+1), (const void *) *(p+2), (unsigned) *(p+3));
        lock_release(&file_lock);
    }else if(call==SYS_SEEK){

        int *p = f->esp;
        //validate_memory_address((const void*)(p+1));
        validate_memory_address((const void*)(p+2));
        seek((int)*(p+1), (unsigned) *(p+2));

    }else if(call==SYS_TELL){

        int *p = f->esp;
        validate_memory_address((const void*)(p+1));
        f->eax = tell((int)*(p+1));

    }else if(call==SYS_CLOSE){

        int *p = f->esp;
        validate_memory_address((const void*)(p+1));
        close((int)*(p+1));

    }else if(call==SYS_CHDIR){
        int *p = f->esp;
        validate_memory_address((const void*)(p+1));
        f->eax = change_directory((const char *)*(p+1));


    }else if(call== SYS_MKDIR){
        int *p = f->esp;
        validate_memory_address((const void*)(p+1));
        f->eax =  filesys_create((const char *)*(p+1), 0, true);


    }else if(call==SYS_READDIR){

        int *p = f->esp;
        validate_memory_address((const void*)(p+1));
        validate_memory_address((const void*)(p+2));
        if (pagedir_get_page(thread_current()->pagedir, (const void *) *(p+2)) == NULL)
        {
          exit(-1);
        }
        struct track_files *track_file = find_track_file(*(p+1));
        if(track_file!=NULL){
            f->eax  = dir_readdir(track_file->directory, (char *) *(p+2));
        }else{
            f->eax = false;
        }



    }else if(call== SYS_ISDIR){
        int *p = f->esp;
        validate_memory_address((const void*)(p+1));
        struct track_files *track_file = find_track_file(*(p+1));
        f->eax = (track_file->not_file==1);


    }else if(call== SYS_INUMBER){
        int *p = f->esp;
        validate_memory_address((const void*)(p+1));
        struct track_files *track_file = find_track_file(*(p+1));
        int directory = track_file->not_file;
        if(!directory){

          f->eax   = inode_get_inumber(file_get_inode(track_file->address));
        }else{
          f->eax   = inode_get_inumber(dir_get_inode(track_file->directory));
        }


    }else{
        //exit(-1);
    }
}

//Terminates Pintos by calling shutdown_power_off()
//(declared in "devices/shutdown.h"). This should be seldom used, because
//you lose some information about possible deadlock situations, etc.
void halt (void)
{
    //Fixed this
    printf("Inside halt\n");
	shutdown_power_off();
}

//Terminates the current user program, returning status
//to the kernel. If the process's parent waits for it (see below), this is the status
//that will be returned. Conventionally, a status of 0 indicates success and
//nonzero values indicate errors.
void exit (int status)
{
	struct list_elem *e;
    e = list_begin (&thread_current()->parent->children);

    while(e!=list_end (&thread_current()->parent->children)){
          struct child *child= list_entry (e, struct child, child_elem);
          if(child->tid == thread_current()->tid)
          {
          	child->exited = true;
          	child->exit_status = status;
          }

        e = list_next(e);
    }

    thread_current()->exit_status = status;

    thread_exit();
}


//Runs the executable whose name is given in cmd_line, passing any given arguments, and returns
//the new process's program id (pid). Must return pid -1, which otherwise should not be a valid pid,
//if the program cannot load or run for any reason. Thus, the parent process cannot return from the
//exec until it knows whether the child process successfully loaded its executable. You must use
//appropriate synchronization to ensure this.
pid_t exec (const char *cmd_line)
{
    pid_t pid =-1;
    lock_acquire(&file_lock);
    char * filename = malloc (strlen(cmd_line)+1);
    strlcpy(filename, cmd_line, strlen(cmd_line)+1);

    char * store;
    filename = strtok_r(filename," ",&store);

    struct file* file = filesys_open (filename);

	if(file!=NULL)
	{
	    file_close(file);
	    lock_release(&file_lock);
        pid= process_execute(cmd_line);

	}else{
            lock_release(&file_lock);
	}
	free(filename);


	return pid;
}

//Waits for a child process pid and retrieves the child's exit status.
//If pid is still alive, waits until it terminates. Then, returns the status that
//pid passed to exit. If pid did not call exit(), but was terminated by the kernel
//(e.g. killed due to an exception), wait(pid) must return -1. It is perfectly legal
//for a parent process to wait for child processes that have already terminated by the
//time the parent calls wait, but the kernel must still allow the parent to retrieve its
//child's exit status, or learn that the child was terminated by the kernel.
int wait (pid_t pid)
{
  return process_wait(pid);
}

//Creates a new file called file initially initial_size bytes in size.
//Returns true if successful, false otherwise. Creating a new file
//does not open it: opening the new file is a separate operation which
//would require a open system call.
bool create (const char *file, unsigned initial_size)
{
  lock_acquire(&file_lock);
  bool b =  filesys_create(file, initial_size,false);
  lock_release(&file_lock);
  return  b;
}

//Creates a new file called file initially initial_size bytes in size.
//Returns true if successful, false otherwise. Creating a new file does
//not open it: opening the new file is a separate operation which would
//require a open system call.
bool remove (const char *file)
{
  bool removed;

  lock_acquire(&file_lock);
  removed= filesys_remove(file);
  lock_release(&file_lock);
  return removed;
}

//Empties out the elements representing files
//in a thread's file list. Used whilst it's time
//for a process to finish.
void empty_out_file_list(struct list* files)
{
	struct list_elem *e;
	while(!list_empty(files))
	{
		e = list_pop_front(files);
		struct track_files *f = list_entry (e, struct track_files, file_elem);
	      	file_close(f->address);
	      	list_remove(e);
	}
}

//Opens the file or directory differentiated by
// the boolean not_file called file. Returns a nonnegative integer handle
//called a "file descriptor" (fd), or -1 if the file could not be opened.
int open (const char *file)
{
  int fd = -1;

  struct file* fl = filesys_open(file);


  if(fl != NULL)
  {
    struct inode* file_inode = file_get_inode(fl);
    if (file_inode->isdir)
    {
      fd = thread_current ()->current_fd;
      thread_current ()->current_fd++;
      struct track_files *track_file = malloc(sizeof(struct track_files));
      track_file->directory = (struct dir*)fl;
      track_file->not_file = 1;
      track_file->fd = fd;
      list_push_front(&thread_current ()->fd_list, &track_file->file_elem);

    }else{
      fd = thread_current ()->current_fd;
      thread_current ()->current_fd++;
      struct track_files *track_file = malloc(sizeof(struct track_files));
      track_file->address = fl;
      track_file->directory = NULL;
      track_file->not_file = 0;
      track_file->fd = fd;
      list_push_front(&thread_current ()->fd_list, &track_file->file_elem);
    }


  }
  return fd;
}


//Looks for and returns the struct track_files that contains
//a file matching the fd given in the parameter
//Returns null if not found/
struct track_files* find_track_file (int fd)
{
  struct thread *current = thread_current();
  struct list_elem *e;
  e = list_begin (&current->fd_list);

   while(e!=list_end (&current->fd_list)){
        struct track_files *track_file = list_entry (e, struct track_files, file_elem);
          if (fd == track_file->fd)
	    {
	      return track_file;
	    }
	    e = list_next(e);
   }
  return NULL;
}

//Writes size bytes from buffer to the open file fd. Returns the
//number of bytes actually written, which may be less than size if
//some bytes could not be written.
int write (int fd, const void *buffer, unsigned size)
{
  int bytes=0;

  if(fd==0){
        bytes =0;
  }else if(fd ==1){
		bytes = size;
		putbuf(buffer, size);
  }else if(list_empty(&thread_current()->fd_list)){
        bytes =0;
  }else{
        struct file* f = find_files(fd,0);
        if(f==NULL){
            bytes =-1;
        }else{
        bytes = file_write(find_files(fd,0), buffer, size);
        }

  }

  return bytes;
}

//Returns the size, in bytes, of the file open as fd.
int filesize (int fd)
{
  int size = -1;
  size = file_length(find_files(fd,0));
  return size;
}

//Find and return the file that contains the same file descriptor
//as provided.Return NULL if not found.
struct file *find_files(int file_descriptor,int mode){
    struct file* f=NULL;
    struct list_elem *e=NULL;
    e= list_begin(&thread_current()->fd_list);
    while(e!=list_end(&thread_current()->fd_list)){
     struct track_files *t = list_entry (e, struct track_files, file_elem);
     if(file_descriptor==t->fd ){
        f = t->address;
        if(mode==1){
            list_remove(&t->file_elem);
        }
     }
     e = list_next(e);
    }
    return f;
}

//Reads size bytes from the file open as fd into buffer. Returns the number of bytes
//actually read (0 at end of file), or -1 if the file could not be
//read (due to a condition other than end of file).
//Fd 0 reads from the keyboard using input_getc().
int read (int fd, void *buffer, unsigned length)
{
  int number_bytes=-1;

  if(fd==1){
    number_bytes = 0;
  }else if(fd==0 ){


    number_bytes= input_getc();

  }else if(list_empty(&thread_current()->fd_list)){
    number_bytes = 0;
  }else{


        number_bytes = file_read(find_files(fd,0), buffer, length);

  }

  return number_bytes;
}

//Changes the next byte to be read or written in open file fd to position,
//expressed in bytes from the beginning of the file. (Thus,
//a position of 0 is the file's start.)
void seek (int fd, unsigned position)
{
  file_seek(find_files(fd,0),position);
}


//Returns the position of the next byte to be read or written in open file
//fd, expressed in bytes from the beginning of the file.
unsigned tell (int fd)
{
  int position =-1;

  lock_acquire(&file_lock);
  position = file_tell(find_files(fd,0));
  lock_release(&file_lock);
  return position;
}

//Closes file descriptor fd. Exiting or terminating
//a process implicitly closes all its open
//file descriptors, as if by calling this function for each one.
void close (int fd)
{
    lock_acquire(&file_lock);
    file_close(find_files(fd,1));
    lock_release(&file_lock);

}

//Ensures that uaddr is mapped
//as well as the validity for usage
//the system calls
void validate_memory_address (const void *ptr)
{
  if(!is_user_vaddr(ptr) || ptr== NULL)
	{
        exit(-1);
	}

  if(!pagedir_get_page(thread_current()->pagedir, ptr)){

        exit(-1);
  }

  if(ptr < (void *) 0x08048000){

    exit(-1);
  }
}

//Changes the current working directory of the process to dir,
//which may be relative or absolute. Returns true if successful, false on failure.
bool change_directory (const char* name)
{
  char arr[strlen(name) + 1];
  strlcpy (arr, name, strlen(name)+1);

  char*save_ptr;
  char *token = strtok_r(arr, "/", &save_ptr);
  char*initialtoken = "";
  while (token != NULL)
    {

      initialtoken = token;
      token = strtok_r (NULL, "/", &save_ptr);
    }
  char* fname = initialtoken;

  //Get the directory being switched to from
  //the whole path
  struct dir* directory = find_directory(name);
  struct inode *inode = NULL;


  if (directory != NULL)
    {
   //checks if the directory requires a change
   //if so changes to the new directory accordingly.
      if (set_current_dir(fname,directory))
	{
        return true;
	}

     bool parent_dir = strcmp(fname, "..") == 0;
      if (parent_dir)
	{
        inode = set_parent_inode(directory);
	}
      else
	{
	   //this is used to look up a directory
	   //other than the current directory or the
	   //parent directory
	   dir_lookup (directory, fname, &inode);
	}

    }

  return set_new_dir(inode,directory);
}































