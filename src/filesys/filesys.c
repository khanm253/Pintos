#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"

/* Partition that contains the file system. */
struct block *fs_device;


static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  initialize_cache();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  write_to_disk (1);
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool isdir)
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
  block_sector_t inode_sector = 0;
  struct dir *dir = find_directory(name);

  bool success = false;

      success = (dir != NULL
		 && free_map_allocate (1, &inode_sector)
		 && inode_create (inode_sector, initial_size, isdir)
		 && dir_add (dir, fname, inode_sector));

  if (!success && inode_sector != 0)
  free_map_release (inode_sector, 1);
  dir_close (dir);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  if (strlen(name) == 0)
      return NULL;

  struct dir* dir = find_directory(name);
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
  //Attain the file name from the path
  char* file_name = initialtoken;
  struct inode *inode = NULL;

  if (dir != NULL)
    {
     struct inode* file_inode = dir_get_inode(dir);
     int file_inumber = inode_get_inumber(file_inode);
     bool dir_is_root= (file_inumber == 1);

      if ((strlen(file_name) == 0 && dir_is_root ))
	{
      //proven to be the root directory so cast to
      //file and return
      struct file* root = (struct file *) dir;
	  return root;
	}else if (strcmp(file_name, "..") == 0)
	{
	    //revert inode to parent directory
        inode = set_parent_inode(dir);
        if(!inode){
            return NULL;
        }
	}else if(strcmp(file_name, ".") == 0){

          //return casted current directory
          struct file* cwd = (struct file *) dir;
      	  return cwd;

	}else
    {
          dir_lookup (dir, file_name, &inode);

        }

    }

  dir_close (dir);
  struct dir* new_dir = dir_open(inode);
  struct file* opened_file = (struct file *) new_dir;
  if(!inode){
    return NULL;
  }

  bool file = !inode->isdir;
  if (file)
    {
      struct file* opened_file = file_open (inode);
      return opened_file;

    }

  return opened_file;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  struct dir* dir = find_directory(name);
  char arr[strlen(name) + 1];
  strlcpy (arr, name, strlen(name)+1);

  char*save_ptr;
  char *token = strtok_r(arr, "/", &save_ptr);
  char*initialtoken = "";
  //iterate to find the the file named NAME.
  while (token != NULL)
    {

      initialtoken = token;
      token = strtok_r (NULL, "/", &save_ptr);
    }
  char* fname = initialtoken;
  bool success = dir != NULL && dir_remove (dir, fname);
  //close directory when found
  dir_close (dir);


  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

//Finds the directory to be changed to during
//SYSCALL_CHDIR from the full path. Takes into
// account thr root directory, "." and ".." directories.
//Returns the directory obtained from the path. Null otherwise.
struct dir* find_directory(const char* path)
{
  char arr[strlen(path) + 1];
  strlcpy (arr, path, strlen(path)+1);

  char *store;
  char *path_next = NULL;
  char *token;

  struct dir* directory;
  directory = check_valid_cwd(arr[0]);

  if ((token = strtok_r(arr, "/", &store)))
    {
      path_next = strtok_r(NULL, "/", &store);
    }

  char* iterate;
  for (iterate = path_next; iterate != NULL;iterate = strtok_r(NULL, "/", &store))
    {
      //if token points to current directory
      //keep iterating
      if (strcmp(token, ".") == 0)
	{
      token = iterate;

	}else{
      struct inode *inode=NULL;
	  if (strcmp(token, "..") == 0)
	    {
	       //set inode to that of parent directory
           inode = set_parent_inode(directory);
           if(!inode){
            return NULL;
           }
	    }
	  else
	    {
	      //Attempt to find directory through lookup after not
	      //reverting to parent or current directory
	      bool dir_found = dir_lookup(directory, token, &inode);
          if(!dir_found){
             return NULL;
          }
	    }

      //check if inode set is a valid directory or not
      //if so set the return to the opened directory
      bool is_directory =  inode->isdir;
	  if (is_directory)
	    {
	      dir_close(directory);
	      directory = dir_open(inode);
	    }

      token = iterate;
	}

  }
  return directory;
}









