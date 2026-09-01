// Free-block allocator, implemented as a bitmap stored entirely in block 0
// (one bit per block; BLOCK_SIZE * 8 = 512 bits covers every block on the
// disk, including the bitmap block itself).

#include "basic_file_system.h"
#include <string.h>

int bfs_mount(const char *filename) {
  if (raw_mount(filename) != 0) {
    return -1;
  }

  unsigned char bitmap[BLOCK_SIZE];
  if (read_block(0, bitmap) != 0) {
    return -1;
  }

  // Low two bits of the bitmap's first byte track blocks 0 and 1. If both
  // are already marked allocated, the disk has already been formatted.
  if ((bitmap[0] & 0x3) == 0) {
    bitmap[0] |= 0x3; // mark block 0 (bitmap) and block 1 (root dir) used
    if (write_block(0, bitmap) != 0) {
      return -1;
    }
  }

  return 0;
}

int bfs_unmount(void) {
  return raw_unmount();
}

block_num_t allocate_block(void) {
  unsigned char bitmap[BLOCK_SIZE];
  if (read_block(0, bitmap) != 0) {
    return 0;
  }

  for (int byte_index = 0; byte_index < BLOCK_SIZE; byte_index++) {
    if (bitmap[byte_index] == 0xFF) {
      continue; // every block in this byte is already allocated
    }

    for (int bit = 0; bit < 8; bit++) {
      if ((bitmap[byte_index] & (1 << bit)) == 0) {
        bitmap[byte_index] |= (1 << bit);
        if (write_block(0, bitmap) != 0) {
          return 0;
        }
        return (block_num_t)(byte_index * 8 + bit);
      }
    }
  }

  return 0; // disk full: no free block found
}

int release_block(block_num_t block_num) {
  unsigned char bitmap[BLOCK_SIZE];
  if (read_block(0, bitmap) != 0) {
    return -1;
  }

  int byte_index = block_num / 8;
  int bit = block_num % 8;
  bitmap[byte_index] &= ~(1 << bit);

  if (write_block(0, bitmap) != 0) {
    return -1;
  }

  return 0;
}
