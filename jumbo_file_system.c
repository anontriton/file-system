
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "jumbo_file_system.h"

typedef char bool_t;
#define TRUE 1
#define FALSE 0

// Each data block ends with a block_num_t pointing at the next block in the
// file's chain, so only this many bytes per block actually hold file content.
#define DATA_BYTES_PER_BLOCK (BLOCK_SIZE - (int)sizeof(block_num_t))


static block_num_t current_dir;

// bool helper function to determine directory
static bool_t is_dir(block_num_t block_num) {
  char block_data[BLOCK_SIZE];
  if (read_block(block_num, block_data) != 0) {
    return FALSE;
  }
  return block_data[0] == 1;
}

/* jfs_mount
 *   prepares the DISK file on the _real_ file system to have file system
 *   blocks read and written to it.  The application _must_ call this function
 *   exactly once before calling any other jfs_* functions.  If your code
 *   requires any additional one-time initialization before any other jfs_*
 *   functions are called, you can add it here.
 * filename - the name of the DISK file on the _real_ file system
 * returns 0 on success or -1 on error; errors should only occur due to
 *   errors in the underlying disk syscalls.
 */
int jfs_mount(const char* filename) {
  int ret = bfs_mount(filename);
  current_dir = 1;
  return ret;
}

/* jfs_mkdir
 *   creates a new subdirectory in the current directory
 * directory_name - name of the new subdirectory
 * returns 0 on success or one of the following error codes on failure:
 *   E_EXISTS, E_MAX_NAME_LENGTH, E_MAX_DIR_ENTRIES, E_DISK_FULL
 */
int jfs_mkdir(const char* directory_name) {
  if (strlen(directory_name) > MAX_NAME_LENGTH) { return E_MAX_NAME_LENGTH; }

  char block_data[BLOCK_SIZE];
  if (read_block(current_dir, block_data) != 0) { return -1; }

  int entry_size = 1 + 1 + MAX_NAME_LENGTH + 2;
  int max_entries = BLOCK_SIZE / entry_size;
  int found_empty = -1;

  for (int i = 0; i < max_entries; i++) {
    int offset = i * entry_size;
    if (block_data[offset] == 1 && strncmp(&block_data[offset + 2],
      directory_name, MAX_NAME_LENGTH) == 0) {
      return E_EXISTS;
    }
    if (block_data[offset] == 0 && found_empty == -1) {
      found_empty = i;
    }
  }

  if (found_empty == -1) { return E_MAX_DIR_ENTRIES; }

  block_num_t new_dir_block = allocate_block();
  if (new_dir_block == 0) { return E_DISK_FULL; }

  char new_block_data[BLOCK_SIZE];
  memset(new_block_data, 0, BLOCK_SIZE); // clear all data first

  // check if all entries in the new directory are marked as unused
  for (int i = 0; i < max_entries; i++) {
    int offset = i * entry_size;
    memset(new_block_data + offset, 0, entry_size); // clear entry
  }

  if (write_block(new_dir_block, new_block_data) != 0) { return -1; }

  // update the found_empty entry in the current directory block
  int offset = found_empty * entry_size;
  block_data[offset] = 1;  // set 'used' flag
  block_data[offset + 1] = 1;  // set 'is_dir' flag
  strncpy(&block_data[offset + 2], directory_name, MAX_NAME_LENGTH);
  memcpy(&block_data[offset + 2 + MAX_NAME_LENGTH], &new_dir_block,
         sizeof(block_num_t));
  
  if (write_block(current_dir, block_data) != 0) { return -1; }

  return E_SUCCESS;
}

/* jfs_chdir
 *   changes the current directory to the specified subdirectory, or changes
 *   the current directory to the root directory if the directory_name is NULL
 * directory_name - name of the subdirectory to make the current
 *   directory; if directory_name is NULL then the current directory
 *   should be made the root directory instead
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_NOT_DIR
 */
int jfs_chdir(const char* directory_name) {
  if (directory_name == NULL) {
    current_dir = 1;  // set to root directory
    return E_SUCCESS;
  }

  char block_data[BLOCK_SIZE];

  if (read_block(current_dir, block_data) != 0) { return -1; }

    int entry_size = 1 + 1 + MAX_NAME_LENGTH + 2;
    int max_entries = BLOCK_SIZE / entry_size;

    for (int i = 0; i < max_entries; i++) {
      int offset = i * entry_size;

      if (block_data[offset] && strncmp(&block_data[offset + 2],
          directory_name, MAX_NAME_LENGTH) == 0) {
        block_num_t dir_block;

        memcpy(&dir_block, &block_data[offset + 2 + MAX_NAME_LENGTH],
               sizeof(block_num_t));

        if (block_data[offset + 1] == 1) { // check is_dir flag
          current_dir = dir_block;
          return E_SUCCESS;
        } else { return E_NOT_DIR; }
      }
    }

    return E_NOT_EXISTS;
}



/* jfs_ls
 *   finds the names of all the files and directories in the current directory
 *   and writes the directory names to the directories argument and the file
 *   names to the files argument
 * directories - array of strings; the function will set the strings in the
 *   array, followed by a NULL pointer after the last valid string; the strings
 *   should be malloced and the caller will free them
 * file - array of strings; the function will set the strings in the
 *   array, followed by a NULL pointer after the last valid string; the strings
 *   should be malloced and the caller will free them
 * returns 0 on success or one of the following error codes on failure:
 *   (this function should always succeed)
 */
int jfs_ls(char* directories[MAX_DIR_ENTRIES+1], char* files[MAX_DIR_ENTRIES+1])
{
  char block_data[BLOCK_SIZE];

  if (read_block(current_dir, block_data) != 0) { return -1; }

  int entry_size = 1 + 1 + MAX_NAME_LENGTH + 2;
  int max_entries = BLOCK_SIZE / entry_size;
  int dir_count = 0, file_count = 0;

  for (int i = 0; i < max_entries; i++) {
    int offset = i * entry_size;

    if (block_data[offset]) {
      char* name = malloc(MAX_NAME_LENGTH + 1);

      if (name == NULL) {
        // free previously allocated memory before returning
        while (dir_count > 0) free(directories[--dir_count]);
        while (file_count > 0) free(files[--file_count]);
        return -1; // memory allocation failed
      }

      strncpy(name, &block_data[offset + 2], MAX_NAME_LENGTH);
      name[MAX_NAME_LENGTH] = '\0'; // null termination

      // is_dir flag
      if (block_data[offset + 1] == 1) { directories[dir_count++] = name; } 
      else { files[file_count++] = name; }
    }
  }

  // mark end of directory and files arrays
  directories[dir_count] = NULL;
  files[file_count] = NULL;

  return E_SUCCESS;
}


/* jfs_rmdir
 *   removes the specified subdirectory of the current directory
 * directory_name - name of the subdirectory to remove
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_NOT_DIR, E_NOT_EMPTY
 */
int jfs_rmdir(const char* directory_name) {
  char block_data[BLOCK_SIZE];
  if (read_block(current_dir, block_data) != 0) { return -1; }

  int entry_size = 1 + 1 + MAX_NAME_LENGTH + 2;
  int max_entries = BLOCK_SIZE / entry_size;

  for (int i = 0; i < max_entries; i++) {
    int offset = i * entry_size;
    if (block_data[offset] && strncmp(&block_data[offset + 2],
        directory_name, MAX_NAME_LENGTH) == 0) {
      block_num_t dir_block;
            
      memcpy(&dir_block, &block_data[offset + 2 + MAX_NAME_LENGTH],
             sizeof(block_num_t));

      if (block_data[offset + 1] != 1) { return E_NOT_DIR; }

      char dir_data[BLOCK_SIZE];
      if (read_block(dir_block, dir_data) != 0) { return -1; }

      bool_t isEmpty = TRUE;
      for (int j = 0; j < max_entries; j++) {
        if (dir_data[j * entry_size]) {
          isEmpty = FALSE;
          break;
        }
      }

      if (isEmpty) {
        block_data[offset] = 0; // clear 'used' flag
        release_block(dir_block);  // release the directory block

        if (write_block(current_dir, block_data) != 0) { return -1; }
        
        return E_SUCCESS;
      } else { return E_NOT_EMPTY; }
    }
  }

  return E_NOT_EXISTS;
}



/* jfs_creat
 *   creates a new, empty file with the specified name
 * file_name - name to give the new file
 * returns 0 on success or one of the following error codes on failure:
 *   E_EXISTS, E_MAX_NAME_LENGTH, E_MAX_DIR_ENTRIES, E_DISK_FULL
 */
int jfs_creat(const char* file_name) {
  if (strlen(file_name) > MAX_NAME_LENGTH) {
      return E_MAX_NAME_LENGTH;
  }

  char block_data[BLOCK_SIZE];
  if (read_block(current_dir, block_data) != 0) { return -1; }

  int entry_size = 1 + 1 + MAX_NAME_LENGTH + 2;
  int max_entries = BLOCK_SIZE / entry_size;

  int found_empty = -1;
  for (int i = 0; i < max_entries; i++) {
    int offset = i * entry_size;
    if (block_data[offset] && strncmp(&block_data[offset + 2], file_name,
        MAX_NAME_LENGTH) == 0) {
      return E_EXISTS;
      }

      if (!block_data[offset] && found_empty == -1) { found_empty = i; }
    }

    if (found_empty == -1) { return E_MAX_DIR_ENTRIES; }

    block_num_t file_block = allocate_block();
    if (file_block == 0) { return E_DISK_FULL; }

    // initialize new file block to zero
    char new_file_block_data[BLOCK_SIZE];
    memset(new_file_block_data, 0, BLOCK_SIZE);
    if (write_block(file_block, new_file_block_data) != 0) { return -1; }

    // set the directory entry for the new file
    int offset = found_empty * entry_size;
    memset(&block_data[offset], 0, entry_size);  // clear the dir entry
    block_data[offset] = 1;  // set 'used' flag
    block_data[offset + 1] = 0;  // set 'is_file' flag,
    strncpy(&block_data[offset + 2], file_name, MAX_NAME_LENGTH);
    memcpy(&block_data[offset + 2 + MAX_NAME_LENGTH], &file_block,
           sizeof(block_num_t));

    if (write_block(current_dir, block_data) != 0) { return -1; }

    return E_SUCCESS;
}

/* jfs_remove
 *   deletes the specified file and all its data (note that this cannot delete
 *   directories; use rmdir instead to remove directories)
 * file_name - name of the file to remove
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR
 */
int jfs_remove(const char* file_name) {
  char block_data[BLOCK_SIZE];
  if (read_block(current_dir, block_data) != 0) { return -1; }

  int entry_size = 1 + 1 + MAX_NAME_LENGTH + 2;
  int max_entries = BLOCK_SIZE / entry_size;

  for (int i = 0; i < max_entries; i++) {
    int offset = i * entry_size;

    if (block_data[offset]) { // entry is used
      char* name = &block_data[offset + 2];

      if (strncmp(name, file_name, MAX_NAME_LENGTH) == 0) {
        block_num_t file_block;
        memcpy(&file_block, &block_data[offset + 2 + MAX_NAME_LENGTH],
               sizeof(block_num_t));

        if (is_dir(file_block)) { return E_IS_DIR; }

        block_data[offset] = 0;  // mark as unused
        release_block(file_block);  // release the file block

        if (write_block(current_dir, block_data) != 0) { return -1; }
        return E_SUCCESS;
      }
    }
  }

  return E_NOT_EXISTS;
}

/* jfs_stat
 *   returns the file or directory stats (see struct stat for details)
 * name - name of the file or directory to inspect
 * buf  - pointer to a struct stat (already allocated by the caller) where the
 *   stats will be written
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS
 */
int jfs_stat(const char* name, struct stats* buf) {
  char block_data[BLOCK_SIZE];
  if (read_block(current_dir, block_data) != 0) { return -1; }

  int entry_size = 1 + 1 + MAX_NAME_LENGTH + 2;
  int max_entries = BLOCK_SIZE / entry_size;

  for (int i = 0; i < max_entries; i++) {
    int offset = i * entry_size;
    if (block_data[offset] && strncmp(&block_data[offset + 2], name,
        MAX_NAME_LENGTH) == 0) {
      block_num_t block_num;

      memcpy(&block_num, &block_data[offset + 2 + MAX_NAME_LENGTH],
             sizeof(block_num_t));

      buf->is_dir = block_data[offset + 1]; // get is_dir flag
      buf->block_num = block_num; // set block number

      strncpy(buf->name, name, MAX_NAME_LENGTH); // set name
      buf->name[MAX_NAME_LENGTH] = '\0'; // ensure null termination

      if (!buf->is_dir) {
        // only set these for files
        char file_block_data[BLOCK_SIZE];
        if (read_block(block_num, file_block_data) != 0) { return -1; }

        // assuming file size is stored at start of the file's block
        buf->file_size = *(uint32_t*)file_block_data;

        // calculate the number of data blocks used by file
        buf->num_data_blocks = (buf->file_size + DATA_BYTES_PER_BLOCK - 1) / DATA_BYTES_PER_BLOCK;
      } else {
        // if dir, ignore these fields
        buf->file_size = 0;
        buf->num_data_blocks = 0;
      }

      return E_SUCCESS;
    }
  }

  return E_NOT_EXISTS;
}

/* jfs_write
 *   appends the data in the buffer to the end of the specified file
 * file_name - name of the file to append data to
 * buf - buffer containing the data to be written (note that the data could be
 *   binary, not text, and even if it is text should not be assumed to be null
 *   terminated)
 * count - number of bytes in buf (write exactly this many)
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR, E_MAX_FILE_SIZE, E_DISK_FULL
 */
int jfs_write(const char* file_name, const void* buf, unsigned short count) {
  if (count == 0) return E_SUCCESS;  // Nnthing to write

  char block_data[BLOCK_SIZE];
  if (read_block(current_dir, block_data) != 0) return -1;

  int entry_size = 1 + 1 + MAX_NAME_LENGTH + 2;
  int max_entries = BLOCK_SIZE / entry_size;
  for (int i = 0; i < max_entries; i++) {
    int offset = i * entry_size;
    if (block_data[offset] && strncmp(&block_data[offset + 2], file_name, MAX_NAME_LENGTH) == 0) {
      if (block_data[offset + 1] != 0) return E_IS_DIR;  // not a file

        block_num_t file_block;
        memcpy(&file_block, &block_data[offset + 2 + MAX_NAME_LENGTH], sizeof(block_num_t));

        char* file_inode_data = malloc(BLOCK_SIZE);
        if (file_inode_data == NULL) return -1;  // memory allocation failed

        if (read_block(file_block, file_inode_data) != 0) {
          free(file_inode_data);
          return -1;
        }

        unsigned int current_size = *(unsigned int*)file_inode_data;
        if (current_size + count > MAX_FILE_SIZE) {
          free(file_inode_data);
          return E_MAX_FILE_SIZE;  // check file size limit
        }

        unsigned int bytes_written = 0;
        block_num_t current_data_block = *(block_num_t*)(file_inode_data + 4);
        unsigned int current_block_offset = current_size % DATA_BYTES_PER_BLOCK;

        // Walk the block chain to the last data block, which is where an
        // append continues from.
        if (current_data_block != 0) {
          char link_buf[BLOCK_SIZE];
          while (1) {
            if (read_block(current_data_block, link_buf) != 0) {
              free(file_inode_data);
              return -1;
            }
            block_num_t next = *(block_num_t*)(link_buf + DATA_BYTES_PER_BLOCK);
            if (next == 0) break;
            current_data_block = next;
          }
        }

        while (bytes_written < count) {
          char data_block_buf[BLOCK_SIZE];

          if (current_block_offset == 0) {
            block_num_t prev_data_block = current_data_block;
            current_data_block = allocate_block();
            if (current_data_block == 0) {
              free(file_inode_data);
              return E_DISK_FULL;
            }

            memset(data_block_buf, 0, BLOCK_SIZE);

            if (prev_data_block != 0) {
              // Link the previous block to this new one, without disturbing
              // the content bytes already stored in it.
              char prev_buf[BLOCK_SIZE];
              if (read_block(prev_data_block, prev_buf) != 0) {
                free(file_inode_data);
                return -1;
              }
              *(block_num_t*)(prev_buf + DATA_BYTES_PER_BLOCK) = current_data_block;
              if (write_block(prev_data_block, prev_buf) != 0) {
                free(file_inode_data);
                return -1;
              }
            } else {
              *(block_num_t*)(file_inode_data + 4) = current_data_block;
            }
          } else {
            if (read_block(current_data_block, data_block_buf) != 0) {
              free(file_inode_data);
              return -1;
            }
          }

          // Only the first DATA_BYTES_PER_BLOCK bytes of a block hold file
          // content; the final sizeof(block_num_t) bytes are the next-block
          // pointer and must never be overwritten with data.
          unsigned int space_in_block = DATA_BYTES_PER_BLOCK - current_block_offset;
          unsigned int write_size = (count - bytes_written < space_in_block) ? count - bytes_written : space_in_block;

          memcpy(data_block_buf + current_block_offset, (char*)buf + bytes_written, write_size);
          if (write_block(current_data_block, data_block_buf) != 0) {
            free(file_inode_data);
            return -1;
          }

          current_size += write_size;
          bytes_written += write_size;
          current_block_offset = (current_block_offset + write_size) % DATA_BYTES_PER_BLOCK;
        }

        // update inode with new file size
        *(unsigned int*)file_inode_data = current_size;
        if (write_block(file_block, file_inode_data) != 0) {
          free(file_inode_data);
          return -1;
        }

        free(file_inode_data);
        return E_SUCCESS;
    }
  }
  return E_NOT_EXISTS;
}

/* jfs_read
 *   reads the specified file and copies its contents into the buffer, up to a
 *   maximum of *ptr_count bytes copied (but obviously no more than the file
 *   size, either)
 * file_name - name of the file to read
 * buf - buffer where the file data should be written
 * ptr_count - pointer to a count variable (allocated by the caller) that
 *   contains the size of buf when it's passed in, and will be modified to
 *   contain the number of bytes actually written to buf (e.g., if the file is
 *   smaller than the buffer) if this function is successful
 * returns 0 on success or one of the following error codes on failure:
 *   E_NOT_EXISTS, E_IS_DIR
 */
int jfs_read(const char* file_name, void* buf, unsigned short* ptr_count) {
    char block_data[BLOCK_SIZE];
    if (read_block(current_dir, block_data) != 0) return -1;

    int entry_size = 1 + 1 + MAX_NAME_LENGTH + 2;
    int max_entries = BLOCK_SIZE / entry_size;
    for (int i = 0; i < max_entries; i++) {
      int offset = i * entry_size;
      if (block_data[offset] && strncmp(&block_data[offset + 2], file_name, MAX_NAME_LENGTH) == 0) {
        block_num_t file_block;
        memcpy(&file_block, &block_data[offset + 2 + MAX_NAME_LENGTH], sizeof(block_num_t));

        if (block_data[offset + 1] != 0) return E_IS_DIR;

        char file_inode_data[BLOCK_SIZE];
        if (read_block(file_block, file_inode_data) != 0) return -1;

        unsigned int file_size = *(unsigned int*)file_inode_data;
        unsigned int to_read = (*ptr_count > file_size) ? file_size : *ptr_count;
        unsigned int bytes_read = 0;
        block_num_t current_data_block = *(block_num_t*)(file_inode_data + 4);

        while (bytes_read < to_read) {
          char data_block_buf[BLOCK_SIZE];
          if (read_block(current_data_block, data_block_buf) != 0) return -1;
          // Mirrors jfs_write: only DATA_BYTES_PER_BLOCK bytes per block are
          // content; the rest is the next-block pointer.
          unsigned int read_size = (to_read - bytes_read > DATA_BYTES_PER_BLOCK) ? DATA_BYTES_PER_BLOCK : to_read - bytes_read;

          memcpy((char*)buf + bytes_read, data_block_buf, read_size);
          bytes_read += read_size;
          if (bytes_read < to_read) {
            current_data_block = *(block_num_t*)(data_block_buf + DATA_BYTES_PER_BLOCK);
          }
        }

        *ptr_count = bytes_read;
        return E_SUCCESS;
      }
    }
  return E_NOT_EXISTS;
}

/* jfs_unmount
 *   makes the file system no longer accessible (unless it is mounted again).
 *   This should be called exactly once after all other jfs_* operations are
 *   complete; it is invalid to call any other jfs_* function (except
 *   jfs_mount) after this function complete.  Basically, this closes the DISK
 *   file on the _real_ file system.  If your code requires any clean up after
 *   all other jfs_* functions are done, you may add it here.
 * returns 0 on success or -1 on error; errors should only occur due to
 *   errors in the underlying disk syscalls.
 */
int jfs_unmount(void) {
  int ret = bfs_unmount();
  return ret;
}
