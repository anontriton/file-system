#ifndef BASIC_FILE_SYSTEM_H
#define BASIC_FILE_SYSTEM_H

#include "raw_disk.h"

/* bfs_mount
 *   mounts the underlying raw disk and, on first use, formats it: marks
 *   block 0 (the free-block bitmap itself) and block 1 (the root directory)
 *   as allocated.
 * filename - name of the DISK file on the real filesystem
 * returns 0 on success or -1 on error
 */
int bfs_mount(const char *filename);

/* bfs_unmount
 *   unmounts the underlying raw disk.
 * returns 0 on success or -1 on error
 */
int bfs_unmount(void);

/* allocate_block
 *   finds a free block via the block-0 bitmap, marks it allocated, and
 *   returns its block number.
 * returns the allocated block number, or 0 if the disk is full
 */
block_num_t allocate_block(void);

/* release_block
 *   marks the given block as free in the block-0 bitmap.
 * block_num - the block to free
 * returns 0 on success or -1 on error
 */
int release_block(block_num_t block_num);

#endif // BASIC_FILE_SYSTEM_H
