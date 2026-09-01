// Interactive shell over the jumbo file system. Reimplemented from scratch
// against the exact command set, prompts, and error messages recovered from
// the original (precompiled-only) command_line.o via strings/DWARF -- see
// CHANGELOG.md.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jumbo_file_system.h"

#define DISK_FILENAME "DISK"
#define INPUT_BUFFER_SIZE 2048

static void print_error(int err, const char *name) {
  switch (err) {
    case E_NOT_EXISTS:        printf("directory not found: %s\n", name); break;
    case E_EXISTS:             printf("file already exists: %s\n", name); break;
    case E_NOT_DIR:            printf("%s is not a directory\n", name); break;
    case E_IS_DIR:             printf("%s is a directory\n", name); break;
    case E_NOT_EMPTY:          printf("directory %s is not empty\n", name); break;
    case E_MAX_NAME_LENGTH:    printf("%s exceeds the maximum file name length\n", name); break;
    case E_MAX_DIR_ENTRIES:    printf("directory is full (max entries reached)\n"); break;
    case E_MAX_FILE_SIZE:      printf("file is full (max file size reached)\n"); break;
    case E_DISK_FULL:          printf("disk is full\n"); break;
    case -1:                   printf("an unknown error occurred\n"); break;
    default:                   printf("an unrecognized error (%d) occurred\n", err); break;
  }
}

static void run_ls(void) {
  char *directories[MAX_DIR_ENTRIES + 1];
  char *files[MAX_DIR_ENTRIES + 1];

  int ret = jfs_ls(directories, files);
  if (ret != E_SUCCESS) {
    printf("ls failed - but ls should never fail!\n");
    return;
  }

  for (int i = 0; directories[i] != NULL; i++) {
    printf("%s/\n", directories[i]);
    free(directories[i]);
  }
  for (int i = 0; files[i] != NULL; i++) {
    printf("%s\n", files[i]);
    free(files[i]);
  }
}

static void run_stat(const char *name) {
  struct stats file_stats;
  int ret = jfs_stat(name, &file_stats);
  if (ret != E_SUCCESS) {
    print_error(ret, name);
    return;
  }

  if (file_stats.is_dir) {
    printf("Directory name: %s\n", file_stats.name);
    printf("Directory block number: %u\n", file_stats.block_num);
  } else {
    printf("File name: %s\n", file_stats.name);
    printf("Inode block number: %u\n", file_stats.block_num);
    printf("Number of data blocks: %u\n", file_stats.num_data_blocks);
    printf("File size: %u\n", file_stats.file_size);
  }
}

static void run_cat(const char *file_name) {
  char *buf = malloc(MAX_FILE_SIZE);
  if (buf == NULL) {
    perror(NULL);
    return;
  }
  memset(buf, 0, MAX_FILE_SIZE);

  unsigned short count = MAX_FILE_SIZE;
  int ret = jfs_read(file_name, buf, &count);
  if (ret != E_SUCCESS) {
    print_error(ret, file_name);
  } else {
    size_t written = fwrite(buf, 1, count, stdout);
    putchar('\n');
    if (written != count) {
      perror("Failed to write file data to stdout");
    }
  }

  free(buf);
}

static void run_head(const char *file_name, const char *num_bytes_str) {
  char *endptr;
  unsigned long requested = strtoul(num_bytes_str, &endptr, 10);
  if (*num_bytes_str == '\0' || *endptr != '\0') {
    printf("<num_bytes> must be an integer.\n");
    return;
  }
  if (requested > MAX_FILE_SIZE) {
    requested = MAX_FILE_SIZE;
  }

  char *buf = malloc((size_t)requested);
  if (buf == NULL && requested > 0) {
    perror(NULL);
    return;
  }

  unsigned short count = (unsigned short)requested;
  int ret = jfs_read(file_name, buf, &count);
  if (ret != E_SUCCESS) {
    print_error(ret, file_name);
  } else {
    size_t written = fwrite(buf, 1, count, stdout);
    putchar('\n');
    if (written != count) {
      perror("Failed to write file data to stdout");
    }
  }

  free(buf);
}

// Dispatches one already-tokenized command. argv[0] is the command name;
// argv[1..argc-1] are its arguments (for "append", argv[2] is the raw
// remainder of the line, spaces included).
static int run_command(int argc, char *argv[]) {
  const char *cmd = argv[0];

  if (strcmp(cmd, "cd") == 0) {
    if (argc > 2) { printf("ERROR: too many arguments on the command line\n"); return 0; }
    int ret = jfs_chdir(argc == 2 ? argv[1] : NULL);
    if (ret != E_SUCCESS) print_error(ret, argc == 2 ? argv[1] : "/");

  } else if (strcmp(cmd, "mkdir") == 0) {
    if (argc != 2) { printf("usage: mkdir <dir_name>\n"); return 0; }
    int ret = jfs_mkdir(argv[1]);
    if (ret != E_SUCCESS) print_error(ret, argv[1]);

  } else if (strcmp(cmd, "rmdir") == 0) {
    if (argc != 2) { printf("usage: rmdir <dir_name>\n"); return 0; }
    int ret = jfs_rmdir(argv[1]);
    if (ret != E_SUCCESS) print_error(ret, argv[1]);

  } else if (strcmp(cmd, "ls") == 0) {
    if (argc != 1) { printf("usage: ls\n"); return 0; }
    run_ls();

  } else if (strcmp(cmd, "touch") == 0) {
    if (argc != 2) { printf("usage: touch <file_name>\n"); return 0; }
    int ret = jfs_creat(argv[1]);
    if (ret != E_SUCCESS) print_error(ret, argv[1]);

  } else if (strcmp(cmd, "rm") == 0) {
    if (argc != 2) { printf("usage: rm <file_name>\n"); return 0; }
    int ret = jfs_remove(argv[1]);
    if (ret != E_SUCCESS) print_error(ret, argv[1]);

  } else if (strcmp(cmd, "stat") == 0) {
    if (argc != 2) { printf("usage: stat <file_name>\n"); return 0; }
    run_stat(argv[1]);

  } else if (strcmp(cmd, "cat") == 0) {
    if (argc != 2) { printf("usage: cat <file_name>\n"); return 0; }
    run_cat(argv[1]);

  } else if (strcmp(cmd, "head") == 0) {
    if (argc != 3) { printf("usage: head <file_name> <num_bytes>\n"); return 0; }
    run_head(argv[1], argv[2]);

  } else if (strcmp(cmd, "append") == 0) {
    if (argc != 3) { printf("usage: append <file_name> <data>\n"); return 0; }
    int ret = jfs_write(argv[1], argv[2], (unsigned short)strlen(argv[2]));
    if (ret != E_SUCCESS) print_error(ret, argv[1]);

  } else if (strcmp(cmd, "exit") == 0) {
    return 1;

  } else {
    printf("ERROR: unrecognized command\n");
  }

  return 0;
}

// Splits a line into: command, positional args, and (for "append" only) the
// rest of the line verbatim as the final argument, so appended data may
// contain spaces.
static int tokenize(char *line, char *argv[], int max_args) {
  int argc = 0;
  char *saveptr;
  char *token = strtok_r(line, " \t", &saveptr);

  while (token != NULL && argc < max_args) {
    argv[argc++] = token;

    if (argc == 2 && strcmp(argv[0], "append") == 0) {
      char *rest = saveptr;
      while (*rest == ' ' || *rest == '\t') rest++;
      if (*rest != '\0') {
        argv[argc++] = rest;
      }
      break;
    }

    token = strtok_r(NULL, " \t", &saveptr);
  }

  return argc;
}

int main(void) {
  if (jfs_mount(DISK_FILENAME) != 0) {
    fprintf(stderr, "ERROR: Mount failed.\n");
    return 1;
  }

  char input_buffer[INPUT_BUFFER_SIZE];

  while (1) {
    printf("jfs$ ");
    fflush(stdout);

    if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
      if (feof(stdin)) {
        break; // EOF (e.g. Ctrl-D): treat like "exit"
      }
      fprintf(stderr, "FATAL ERROR: fgets failed\n");
      jfs_unmount();
      return 1;
    }

    size_t len = strlen(input_buffer);
    if (len > 0 && input_buffer[len - 1] == '\n') {
      input_buffer[len - 1] = '\0';
    } else if (len == sizeof(input_buffer) - 1) {
      printf("ERROR: line exceeds maximum command line length\n");
      int c;
      while ((c = getchar()) != '\n' && c != EOF) {} // discard rest of line
      continue;
    }

    char line_copy[INPUT_BUFFER_SIZE];
    strncpy(line_copy, input_buffer, sizeof(line_copy));
    line_copy[sizeof(line_copy) - 1] = '\0';

    char *argv[3];
    int argc = tokenize(line_copy, argv, 3);
    if (argc == 0) {
      continue; // blank line
    }

    if (run_command(argc, argv)) {
      break; // "exit"
    }
  }

  jfs_unmount();
  return 0;
}
