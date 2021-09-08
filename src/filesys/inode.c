#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

int directindex;


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[107];               /* Not used. */
    block_sector_t blocks[14];         /* Pointers to blocks */
    block_sector_t block_parent;       //points to the block_parent directory
    int indirectindex;                 //index to indirect block pointer
    int directindex;                   //index to direct block pointer
    bool isdir;                        //true if inode represents directory.false otherwise
    int double_indirectindex;          //index to the double indirect pointer
  };

off_t inode_expand (struct inode *inode, off_t allocated_sec);
size_t indirect_expansion (struct inode *inode,
				    size_t space_to_fill);
void initialize_inode(struct inode_disk* disk,struct inode* inode,
            off_t length,size_t directindex,size_t indirectindex,
            size_t double_indirectindex,bool isdir,block_sector_t block_parent,
            off_t block_read_length,int mode,int mode2);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_data_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);

      int indirectindexes[128];
      bool valid_direct_position = (pos<BLOCK_SECTOR_SIZE*4);
      bool valid_inode_position = (pos < BLOCK_SECTOR_SIZE*(4+9*128));
      if ( pos>=0 &&  valid_direct_position)
	{
	  block_sector_t valid_sector =  inode->blocks[pos / BLOCK_SECTOR_SIZE];
	  return valid_sector;
	}
      else if (pos>=0 && valid_inode_position)
	{
	  pos =pos - BLOCK_SECTOR_SIZE*4;
	  int indirectindex = pos / (BLOCK_SECTOR_SIZE*128) + 4;
	  block_read(fs_device, inode->blocks[indirectindex], &indirectindexes);
	  pos =pos % (BLOCK_SECTOR_SIZE*128);
	  return indirectindexes[pos / BLOCK_SECTOR_SIZE];
	}else {
	  return -1;
	}

}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool isdir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->block_parent = ROOT_DIR_SECTOR;
      disk_inode->length = length;
      disk_inode->isdir = isdir;
      disk_inode->magic = INODE_MAGIC;
      struct inode inode;
      initialize_inode(NULL,&inode,0,0,0,0,false,0,0,0,100);
      inode_expand(&inode, disk_inode->length);
      disk_inode->indirectindex = inode.indirectindex;
      disk_inode->directindex = inode.directindex;
      memcpy(&disk_inode->blocks, &inode.blocks,
	    14*sizeof(block_sector_t));
      block_write (fs_device, sector, disk_inode);
      success = true;
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  struct inode_disk data;
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init(&inode->lock);
  block_read(fs_device, inode->sector, &data);
  initialize_inode(NULL,inode,data.length,data.directindex,
                   data.indirectindex,data.double_indirectindex,
                   data.isdir,data.block_parent,data.length,1,100);
  memcpy(&inode->blocks, &data.blocks, 14*sizeof(block_sector_t));
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          free_map_release (inode->sector, 1);

        }
      else
	{
	  struct inode_disk disk_inode;
	    initialize_inode(&disk_inode,NULL,inode->length,
                      inode->directindex,inode->indirectindex
                      ,inode->double_indirectindex,
                      inode->isdir,inode->block_parent,0,100,0);
	    memcpy(&disk_inode.blocks, &inode->blocks,
		 14*sizeof(block_sector_t));
        block_write(fs_device, inode->sector, &disk_inode);
	}
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  off_t length = inode->block_read_length;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      struct cache_list_elem *cache = get_cache(sector_idx);
      memcpy (buffer + bytes_read, (uint8_t *) &cache->block + sector_ofs,
	      chunk_size);
      cache->used= true;

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

  int write_length = offset+size;
  if ( inode->length< write_length)
    {
      lock_acquire(&inode->lock);
      inode->length = inode_expand(inode, write_length);
      lock_release(&inode->lock);
    }

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode,
						  offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length(inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      struct cache_list_elem *cache = get_cache(sector_idx);
      memcpy ((uint8_t *) &cache->block + sector_ofs, buffer + bytes_written,
	      chunk_size);
      cache->used = true;
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  inode->block_read_length = inode->length;
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (struct inode *inode)
{
  return inode->length;
}

//The allocated sector is used to calculate the new amount of sectors
//that need to be expanded by. If no further space needs to be allocated,
//total allocated sectors are returned. If the required expansion is below the
//indirect index proceed to expand.Else expand till the double indirect index.
off_t inode_expand (struct inode *inode, off_t allocated_sec)
{

  int new_size = bytes_to_data_sectors(allocated_sec);
  int old_size = bytes_to_data_sectors(inode->length);


  int diff = new_size-old_size;

  if (diff == 0)
    {
      return allocated_sec;
    }

  if(expand_till_indirectindex(inode,diff,4)==0){
      inode->directindex = directindex;
    return allocated_sec;
  }
  inode->directindex = directindex;
  directindex = allocated_sec;

  return expand_till_double_indirectindex(diff,inode);
}

//Expands till the double indirect index is reached.
//Returns the unexpanded number of sectors in the end.
int expand_till_double_indirectindex(int diff,struct inode* inode){
  int difference = diff;
  while (inode->directindex < 13)
    {
      difference = indirect_expansion(inode, difference);
      if (difference == 0)
        return directindex;

  }

  int space_left = difference*BLOCK_SECTOR_SIZE;
  int allocated_space =directindex;
  int unexpanded_space = allocated_space - space_left;

return unexpanded_space;

}


//Initializes disk_inode and inode structs with attributes given in the parameters
  void initialize_inode(struct inode_disk* disk_inode,struct inode* inode,
                        off_t length,size_t directindex,size_t indirectindex,
                size_t double_indirectindex,bool isdir,block_sector_t block_parent,
                                 off_t block_read_length,int mode,int mode2){
     if(!mode2){
	    disk_inode->magic = INODE_MAGIC;
	    disk_inode->directindex = directindex;
	    disk_inode->isdir = isdir;
	    disk_inode->double_indirectindex = double_indirectindex;
	    disk_inode->block_parent = block_parent;
	    disk_inode->length = length;
	    disk_inode->indirectindex = indirectindex;
	    return;
     }
     inode->length = length;
     inode->directindex = directindex;
     inode->double_indirectindex = double_indirectindex;
     inode->indirectindex = indirectindex;
     if(!mode){
        return;
     }
     inode->isdir = isdir;
     inode->block_read_length =  block_read_length;
     inode->block_parent = block_parent;


  }


//Helper used to allocate sectors till the indirect index.
//Leave and return the difference between old and new allocated spaces
//when index is reached.
int expand_till_indirectindex(struct inode* inode,int diff,int index){
     int i=0;
     int difference = diff;
     if(index==4){
         for (i = inode->directindex;i < index;i++)
        {
          free_map_allocate (1, &inode->blocks[i]);
          i++;
          difference--;
          if(!difference){
            break;
          }
          i--;
        }
     }

  directindex = i;
  return difference;
}

//If indirectindex is at a minimum(0) allocation/expansion
//proceeds while and wrote to block instead of disk.
//When maximum allocation has been finished, indirect index is set back to 0.
size_t indirect_expansion (struct inode *inode,
				  size_t space_to_fill)
{

  block_sector_t sectors[128];
  min_indirect_expansion(inode,sectors);

    int i;
    for (i = inode->indirectindex;i < 128;i++)
        {
          free_map_allocate (1, &sectors[i]);
          i++;
          space_to_fill--;
          if(!space_to_fill){
            break;
          }
          i--;
        }
  inode->indirectindex = i;
  block_write(fs_device, inode->blocks[inode->directindex], sectors);
  max_indirect_expansion(inode);

  return space_to_fill;
}

//Checks if there is still space for sector allocation.If so allocates.
//Otherwise reads from block.
void min_indirect_expansion(struct inode* inode,block_sector_t *sectors){
  int current_indirectindex = inode->indirectindex;
  if (current_indirectindex != 0)
    {
      block_read(fs_device, inode->blocks[inode->directindex], sectors);

    }
  else
    {
      free_map_allocate(1, &inode->blocks[inode->directindex]);
    }

}

//Used when max sector allocation has been achieved.
//Indirect index is set back to 0 to allow further
//allocation.
void max_indirect_expansion(struct inode* inode){
  if (inode->indirectindex != 128)
    {
      return;
    }
      inode->directindex++;
      inode->indirectindex = 0;
}

//Helper used to attain and return block_parent inode when
//handling the ".." directory.
struct inode* set_parent_inode(struct dir* directory){
     struct inode *in = dir_get_inode(directory);
     int block_parent_inumber = inode_get_inumber(in);
     struct inode* block_parent_inode = inode_open(block_parent_inumber);


     return block_parent_inode;
}
