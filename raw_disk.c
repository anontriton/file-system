// Portable (Linux/macOS/Windows) simulated block device, backed by a plain
// file on the real filesystem. Uses only standard C stdio, no POSIX-only
// APIs, so it builds with gcc/clang/MSVC without platform-specific code.

#include "raw_disk.h"
#include <stdio.h>
#include <string.h>

// Total number of blocks on the simulated disk (block 0 is the free-block
// bitmap -- see basic_file_system.c -- so this covers 511 usable blocks).
#define NUM_BLOCKS 512
#define DISK_SIZE ((long)BLOCK_SIZE * NUM_BLOCKS)

static FILE *disk_file = NULL;

int raw_mount(const char *filename) {
  disk_file = fopen(filename, "r+b");
  if (disk_file == NULL) {
    disk_file = fopen(filename, "w+b");
    if (disk_file == NULL) {
      return -1;
    }
  }

  if (fseek(disk_file, 0, SEEK_END) != 0) {
    fclose(disk_file);
    disk_file = NULL;
    return -1;
  }

  long current_size = ftell(disk_file);
  if (current_size < 0) {
    fclose(disk_file);
    disk_file = NULL;
    return -1;
  }

  // Pad a new (or short) disk file out to exactly DISK_SIZE bytes so every
  // block number in range can be read/written from the start.
  if (current_size < DISK_SIZE) {
    char zeros[BLOCK_SIZE];
    memset(zeros, 0, BLOCK_SIZE);
    long remaining = DISK_SIZE - current_size;

    while (remaining > 0) {
      size_t chunk = (remaining < BLOCK_SIZE) ? (size_t)remaining : (size_t)BLOCK_SIZE;
      if (fwrite(zeros, 1, chunk, disk_file) != chunk) {
        fclose(disk_file);
        disk_file = NULL;
        return -1;
      }
      remaining -= (long)chunk;
    }

    if (fflush(disk_file) != 0) {
      fclose(disk_file);
      disk_file = NULL;
      return -1;
    }
  }

  return 0;
}

int raw_unmount(void) {
  if (disk_file == NULL) {
    return -1;
  }

  int result = (fclose(disk_file) == 0) ? 0 : -1;
  disk_file = NULL;
  return result;
}

int read_block(block_num_t block_num, void *block_data) {
  if (disk_file == NULL) return -1;
  if (fseek(disk_file, (long)block_num * BLOCK_SIZE, SEEK_SET) != 0) return -1;
  if (fread(block_data, 1, BLOCK_SIZE, disk_file) != BLOCK_SIZE) return -1;
  return 0;
}

int write_block(block_num_t block_num, const void *block_data) {
  if (disk_file == NULL) return -1;
  if (fseek(disk_file, (long)block_num * BLOCK_SIZE, SEEK_SET) != 0) return -1;
  if (fwrite(block_data, 1, BLOCK_SIZE, disk_file) != BLOCK_SIZE) return -1;
  if (fflush(disk_file) != 0) return -1;
  return 0;
}
