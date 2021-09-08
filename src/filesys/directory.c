#include <string.h>
#include "filesys/directory.h"



/* A directory. */
struct dir
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode)
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL;
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir)
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir)
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use && !strcmp (name, e.name))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode)
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  lock_acquire(&dir_get_inode((struct dir *) dir)->lock);
  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  lock_release(&dir_get_inode((struct dir *) dir)->lock);

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);


  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    goto done;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:

  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name)
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);


  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  bool is_dir =  inode->isdir;

  if(is_dir){
  bool no_files = true;
  struct dir_entry dir;
  int offset = 0;

  while (inode_read_at (inode, &dir, sizeof dir, offset) == sizeof dir)
    {
      offset = offset +sizeof dir;
      if (dir.in_use)
        {
          no_files = false;
        }
    }

   if(!no_files){
        goto done;
   }
   int num_openers = inode->open_cnt;

   if(num_openers>1){
        goto done;
   }

  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;
  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e)
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);

          return true;
        }
    }
  return false;
}

//Used to reopen directories that are not the root
//directory or simply open the root directory for use
//Returns opened directory. Null if failed.
struct dir *refresh_dir(struct dir *directory,int i){

  struct dir* newdir=NULL;
  if(i==1){
      if(!directory){
         newdir = directory;
      }else{

        //reopens non-root directory
        newdir =  dir_reopen(directory);
      }
       return newdir;
  }else{

      //opens root directory
      return dir_open_root();
  }

}

//Checks if current working directory
//is the root directory or not. If it is
//opens root directory for operation and
// of not reopens the directory accordingly.
//Returns opened valid directory.
struct dir* check_valid_cwd(int root){

 struct dir* cwd;
 struct dir* directory = NULL;
 cwd = thread_current()->directory;

 //if current working directory is root or
 //not.If so opened. Other wise reopened
 //as a non-root file.
 if(root==47){
    directory =  refresh_dir(cwd,0);
 }else if (cwd!=NULL){

    directory =  refresh_dir(cwd,1);

 }else{

    directory = refresh_dir(cwd,0);
 }
 return directory;
}

//Closes the old directory provided in the parameter
//Opens a new directory and changes the current working directory
//of the thread to the new directory opened.
//Returns true if successful. False otherwise.
bool set_new_dir(struct inode* i,struct dir* old_dir){

 bool changed = false;
 dir_close(old_dir);

 struct dir* newdir = dir_open(i);
 if(!newdir){
    return changed;
 }

  changed = true;
  struct dir* cwd = thread_current()->directory;
  dir_close(cwd);
  thread_current()->directory = newdir;
  return changed;
}

//Used to initialize the threads current working directory to
//that given in the parameter after taking ".", length of filename
//and if it is the root directory or not into account.
//Returns true if successfully set.
bool set_current_dir(const char* fname,struct dir *directory){
  bool set = false;
  int root_inumber =1;
  struct  inode *in = dir_get_inode(directory);
  int dir_inumber = inode_get_inumber(in);
  int length_filename = strlen(fname);

  if((dir_inumber==root_inumber) && !length_filename){
    //changes current working directory to new dir
    thread_current()->directory = directory;
    set =  true;
  }

  bool track_current =  (strcmp(fname, ".") == 0);
  if(track_current){

    //changes current working directory to new dir
    thread_current()->directory = directory;
    set =  true;
  }

  return set;

}






