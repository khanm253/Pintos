#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "filesys/directory.h"

struct bitmap;


/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    off_t length;                       /* File size in bytes. */
    off_t block_read_length;            //used for synchronization of block read and write
    size_t double_indirectindex;        //index to the double indirect pointer
    bool isdir;                         //true if inode represents directory.false otherwise
    block_sector_t block_parent;        //points to the block_parent directory
    struct lock lock;                   //lock for synchronization.
    block_sector_t blocks[14];          //Pointers to blocks
    size_t directindex;                 //index to direct block pointer
    size_t indirectindex;               //index to indirect block pointer
  };

void inode_init (void);
bool inode_create (block_sector_t, off_t, bool);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
int expand_till_double_indirectindex(int diff,struct inode* inode);
void min_indirect_expansion(struct inode* inode,block_sector_t *blocks);
void max_indirect_expansion(struct inode* inode);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
void inode_allow_write (struct inode *);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
struct inode* set_parent_inode(struct dir* directory);
off_t inode_length (struct inode *);
int expand_till_indirectindex(struct inode* inode,int diff,int index);

#endif /* filesys/inode.h */
