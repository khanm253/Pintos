#include "filesys/cache.h"

//List used to keep track of cache elements
struct list list_cache;

//lenghth of the list of cache entries in the list above
int length;


//Initializes the list of cache entries,
//set length of empty list and writes the
//thread involved to the disk
void initialize_cache (void)
{
  list_init(&list_cache);

  //length is set to 0 beacause list is initially empty
  length = 0;

  //spawn thread for write-behind
  thread_create("name", 0, write_to_disk , NULL);
}


//Looks for and returns a cache element with sector matching
//that given in the parameter.If such a an entry is not found
//reverts to eviction.If still not found returns NULL.
struct cache_list_elem* get_cache (block_sector_t sector
					     )
{
  struct cache_list_elem *cache = NULL;
  struct list_elem *e = NULL;
    e = list_begin(&list_cache);

    while(e!=list_end(&list_cache)){
       struct cache_list_elem *match = list_entry(e, struct cache_list_elem, elem);
      if (match->sector == sector)
	{
	   //cache entry with matching
	   //sector found
	   cache=match;
	}
     e = list_next(e);
    }
    if (!cache)
    {
      //eviction is opted to find cache element
      cache = cache_evict(sector);

      return cache;
    }else{
      return cache;
    }
}


//Looks for a cache entry that is not in use.If found write
//reads from block accordingly. If not found keeps looping through
//the list of cache and frees caches previously used.Eventually
//finds an available cache.
struct cache_list_elem* cache_evict (block_sector_t sector)
{
  struct cache_list_elem *cache;
  bool is_full = length >=64;
  if (is_full)
    {
      struct list_elem *e = list_begin(&list_cache);
      while (1)
	{
	      cache = list_entry(e, struct cache_list_elem, elem);
	      cache->open_cnt=0;
	      if (!cache->used)
		{
		      //unused cache found
		      block_write(fs_device, cache->sector, &cache->block);

              break;
		}
	      else
		{
             //set to unused to be found in the
             //next iteration
             if(cache->open_cnt>0){
               continue;
             }
             cache->used = false;


		}
        e = list_next(e);
        if(list_empty(&list_cache)){
            break;
        }
        struct list_elem *end = list_end(&list_cache);
          if (e==end)
        {
          e = list_begin(&list_cache);
       }

	}

      //initiazlize sector to that
      //provided in the parameter
	  cache->sector = sector;
	  cache->open_cnt++;
      block_read(fs_device, cache->sector, &cache->block);

  }else
    {
       length++;
       //if cache list not full, insert the
       //new found entry to the list
       cache = insert_cache(sector);

    }

  return cache;
}


//Initializes he cache entry with the given sector
//and inserts it to the list of cache
struct cache_list_elem* insert_cache(int sector){

  struct cache_list_elem *cache;
      cache = malloc(sizeof(struct cache_list_elem));
      cache->sector = sector;
      block_read(fs_device, cache->sector, &cache->block);
      list_push_back(&list_cache, &cache->elem);
  return cache;

}

//Implementation of write behind principle
//Keep blocks in the cache, instead of immediately writing modified data to disk.
//Write blocks to disk whenever they are evicted.But eventually writes all blocks
//to disk.
void write_to_disk (int i)
{
 while (1)
    {
      if(i!=1){
       //delay for write-behind implementation
       timer_sleep(1000);
      }
        struct list_elem *e;
         for(e = list_begin(&list_cache);e!=list_end(&list_cache);e = list_next(e)){

              //traverse through each cache element in the cache list
              //and write to block
              struct cache_list_elem *cache = list_entry(e, struct cache_list_elem, elem);
              block_write (fs_device, cache->sector, &cache->block);
         }

         if(i==1){
            break;
         }
    }
}




