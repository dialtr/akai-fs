# Tracing and Debugging Notes

## Goal

The goal of this document is to record information that I have learned
from tracing / debugging the program as I try to learn how it works.

## Variables

* **disk_num** appears to be some kind of global 

## Lifecycle of main

1. Print banner
2. init_blk_cache()
3. Set blk_ache_enable = 1 
  * Why is this decoupled from init_block_cache()? the fact the cache is
    enabled should be something that init_block_cache() does itself. It
    would also be better to store this in a static (at least) rather than
    a global.
4. Set up some variables to track wave file playing.
  * TODO(tdial): Wave file playback should be deleted from this library.
    It is not relevant to *reading* the files and disks. It belongs in
    the utility, not the library.
5. akaiutil_main.c: 192 - initialize globals to track "lock file"
  * TODO(tdial): Find out why we are locking the files. Is it because we
    write to them?
6. process arguments
7. For -f option, call open_fisk, defined in *akai_util.h* 
  1. akaiutil.c:60 - open_disk()
  2. check disk_num to see if we've exceeded disk_num max. DISK_NUM_MAX
     is limited to 64. Why? TODO(tdial): Ask author about this limit.
  3. What are psuedo-disks?
  4. At akaiutil.c:84 we init an fd that is likely the physical file we
     are opening to read. TODO(tdial): we should operate on virtualized
     files / interfaces. The APIs should not operate on physical FD's
     because then it will be more difficult to create tests.
  5. **KEY** akaiutil.c:158, we call akai_disksize(). This accepts an
     fd, a starting offset, and a "psuedo disk size" whatever that is.
  6. akaiutil.c:258 - function akai_disksize(). 
  7. AKAI_DISKSIZE_MAX defines the largest disk size (defined akaiutil.h)
  8. Looks for the first offset that is a power of 2 after the size of the disk.
 

## Modules

### Block Cache

There is a block cache initialized by the **init_block_cache** function. It
appears to me as though there are 512 entries (defined by BLK_CACHE_NUM).
The cache appears to be an array of entries where each entry is a boolean
*valid* indicating whether the entry is valid and a pointer to a block
represented by *buf*. 

This function is defined in *akaiutil_io.c*.

