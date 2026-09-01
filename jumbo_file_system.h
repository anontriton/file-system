#ifndef JUMBO_FILE_SYSTEM_H
#define JUMBO_FILE_SYSTEM_H

#include <stdint.h>
#include "basic_file_system.h"

#define MAX_NAME_LENGTH 7
#define MAX_DIR_ENTRIES 5
#define MAX_FILE_SIZE 1792

enum {
  E_SUCCESS = 0,
  E_EXISTS,
  E_NOT_EXISTS,
  E_NOT_DIR,
  E_IS_DIR,
  E_NOT_EMPTY,
  E_MAX_NAME_LENGTH,
  E_MAX_DIR_ENTRIES,
  E_DISK_FULL,
  E_MAX_FILE_SIZE,
};

struct stats {
  uint32_t is_dir;
  char name[MAX_NAME_LENGTH + 1];
  block_num_t block_num;
  uint16_t num_data_blocks;
  uint32_t file_size;
};

int jfs_mount(const char* filename);
int jfs_mkdir(const char* directory_name);
int jfs_chdir(const char* directory_name);
int jfs_ls(char* directories[MAX_DIR_ENTRIES + 1], char* files[MAX_DIR_ENTRIES + 1]);
int jfs_rmdir(const char* directory_name);
int jfs_creat(const char* file_name);
int jfs_remove(const char* file_name);
int jfs_stat(const char* name, struct stats* buf);
int jfs_write(const char* file_name, const void* buf, unsigned short count);
int jfs_read(const char* file_name, void* buf, unsigned short* ptr_count);
int jfs_unmount(void);

#endif // JUMBO_FILE_SYSTEM_H
