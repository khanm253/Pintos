#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/block.h"
#include "devices/timer.h"
#include "filesys/filesys.h"

#include <list.h>

struct cache_list_elem {
  block_sector_t block[BLOCK_SECTOR_SIZE]; // used for data reading from disk

  bool used; // tracks if the data/cache entry has been used from the list of cache

  int open_cnt;   // Processes that have opened cache-block  for reading/writing

  block_sector_t sector;  // the current sector for the cache data

  struct list_elem elem;  //holds the elements that are in the cache

};


struct cache_list_elem* insert_cache(int sector);
void initialize_cache (void);
struct cache_list_elem* get_cache (block_sector_t sector);
struct cache_list_elem* cache_evict (block_sector_t sector);
void write_to_disk  (int i);

#endif /* filesys/cache.h */
