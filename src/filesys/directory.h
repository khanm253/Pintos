#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H
#include "threads/thread.h"
#include <stdbool.h>
#include <stddef.h>
#include "devices/block.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include <list.h>
#include <stdio.h>
#include "filesys/inode.h"



/* Maximum length of a file name component.
   This is the traditional UNIX maximum length.
   After directories are implemented, this maximum length may be
   retained, but much longer full path names must be allowed. */
#define NAME_MAX 14

struct inode;

/* Opening and closing directories. */
bool dir_create (block_sector_t sector, size_t entry_cnt);
struct dir *dir_open (struct inode *);
struct dir *dir_open_root (void);
struct dir *dir_reopen (struct dir *);
void dir_close (struct dir *);
struct inode *dir_get_inode (struct dir *);

/* Reading and writing. */
bool dir_lookup (const struct dir *, const char *name, struct inode **);
bool dir_add (struct dir *, const char *name, block_sector_t);
bool set_new_dir(struct inode* i,struct dir* old_dir);
bool dir_readdir (struct dir *, char name[NAME_MAX + 1]);
struct dir *refresh_dir(struct dir *directory,int i);
bool dir_remove (struct dir *, const char *name);
struct dir* check_valid_cwd(int root);
bool set_current_dir(const char* fname,struct dir* directory);


#endif /* filesys/directory.h */
