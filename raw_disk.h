#ifndef RAW_DISK_H
#define RAW_DISK_H

#include <stdint.h>

// Size, in bytes, of a single block on the simulated disk.
#define BLOCK_SIZE 64

// A block number on the simulated disk (block 0 is the free-block bitmap).
typedef uint16_t block_num_t;

/* raw_mount
 *   opens (creating if necessary) the given file on the real filesystem to
 *   back the simulated disk, and ensures it is exactly DISK_SIZE bytes.
 * filename - path to the backing file on the real filesystem
 * returns 0 on success or -1 on error
 */
int raw_mount(const char *filename);

/* raw_unmount
 *   closes the backing disk file.
 * returns 0 on success or -1 on error
 */
int raw_unmount(void);

/* read_block
 *   reads exactly one BLOCK_SIZE-byte block from the simulated disk.
 * block_num - which block to read
 * block_data - buffer of at least BLOCK_SIZE bytes to read into
 * returns 0 on success or -1 on error
 */
int read_block(block_num_t block_num, void *block_data);

/* write_block
 *   writes exactly one BLOCK_SIZE-byte block to the simulated disk.
 * block_num - which block to write
 * block_data - buffer of at least BLOCK_SIZE bytes to write from
 * returns 0 on success or -1 on error
 */
int write_block(block_num_t block_num, const void *block_data);

#endif // RAW_DISK_H
