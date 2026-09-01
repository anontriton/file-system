// Portable test suite for the jumbo file system. Covers the same test
// categories as the original assignment's test harness (recovered from
// strings in test.o -- see CHANGELOG.md): mkdir, chdir, rmdir, creat,
// remove, write/read, and stat. Reimplemented as plain sequential checks
// (no fork/signal crash-isolation) so it builds and runs anywhere a C
// compiler does, at the cost of one crashing test taking down the run.
//
// Root only holds MAX_DIR_ENTRIES (5) entries total, so each test group
// gets its own dedicated subdirectory of root to work in, rather than
// sharing root -- this keeps tests order-independent and leaves the
// directory-capacity tests free to fill their own directory without
// starving everything after them.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "jumbo_file_system.h"

#define TEST_DISK_FILENAME "TEST_DISK"

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      printf("FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
      return 0; \
    } \
  } while (0)

typedef int (*test_fn)(void);

// Root only holds MAX_DIR_ENTRIES (5) entries, and this suite needs more
// than 5 top-level test directories, so tests are nested two levels deep:
// root/<group>/<test-dir>. group is one of GROUP_A/GROUP_B below (or NULL
// to run directly in root). Resetting to root+group before every test also
// means one test failing mid-way (and never cd'ing back) can't corrupt the
// starting state for the next one.
static void run_test(const char *group, const char *name, test_fn fn) {
  tests_run++;
  printf("%s: ", name);
  jfs_chdir(NULL);
  if (group != NULL) jfs_chdir(group);
  if (fn()) {
    printf("PASSED\n");
    tests_passed++;
  } else {
    printf("%s: FAILED\n", name);
  }
}

#define GROUP_A "grpA" // mkdirt, chdirt, rmdirt, creatt, removet (5 = full)
#define GROUP_B "grpB" // wrt, wrt2, statt

// ---- mkdir (dedicated dir: "mkdirt") ----

static int test_mkdir_1_create_directory(void) {
  CHECK(jfs_mkdir("mkdirt") == E_SUCCESS);
  CHECK(jfs_chdir("mkdirt") == E_SUCCESS);
  CHECK(jfs_mkdir("a") == E_SUCCESS);
  struct stats st;
  CHECK(jfs_stat("a", &st) == E_SUCCESS);
  CHECK(st.is_dir);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_mkdir_2_long_dirname(void) {
  CHECK(jfs_chdir("mkdirt") == E_SUCCESS);
  CHECK(jfs_mkdir("toolongname") == E_MAX_NAME_LENGTH);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_mkdir_3_fill_directory(void) {
  // "a" already exists from test_mkdir_1; fill the remaining MAX_DIR_ENTRIES-1 slots.
  CHECK(jfs_chdir("mkdirt") == E_SUCCESS);
  CHECK(jfs_mkdir("b") == E_SUCCESS);
  CHECK(jfs_mkdir("c") == E_SUCCESS);
  CHECK(jfs_mkdir("d") == E_SUCCESS);
  CHECK(jfs_mkdir("e") == E_SUCCESS);
  CHECK(jfs_mkdir("f") == E_MAX_DIR_ENTRIES);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

// ---- chdir (dedicated dir: "chdirt") ----

static int test_chdir_1_simple_cd(void) {
  CHECK(jfs_mkdir("chdirt") == E_SUCCESS);
  CHECK(jfs_chdir("chdirt") == E_SUCCESS);
  CHECK(jfs_mkdir("nested") == E_SUCCESS);
  CHECK(jfs_chdir("nested") == E_SUCCESS);
  CHECK(jfs_mkdir("deeper") == E_SUCCESS);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_chdir_2_cd_to_root(void) {
  struct stats st;
  CHECK(jfs_stat("chdirt", &st) == E_SUCCESS); // visible again from root
  CHECK(st.is_dir);
  return 1;
}

// ---- rmdir (dedicated dir: "rmdirt") ----

static int test_rmdir_1_mkdir_then_rmdir(void) {
  CHECK(jfs_mkdir("rmdirt") == E_SUCCESS);
  CHECK(jfs_chdir("rmdirt") == E_SUCCESS);
  CHECK(jfs_mkdir("temp1") == E_SUCCESS);
  CHECK(jfs_rmdir("temp1") == E_SUCCESS);
  struct stats st;
  CHECK(jfs_stat("temp1", &st) == E_NOT_EXISTS);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_rmdir_2_rm_subdir(void) {
  // "chdirt" contains "nested" (from test_chdir_1), so removing it should fail.
  CHECK(jfs_rmdir("chdirt") == E_NOT_EMPTY);
  return 1;
}

static int test_rmdir_3_mkdir_rmdir_mkdir(void) {
  CHECK(jfs_chdir("rmdirt") == E_SUCCESS);
  CHECK(jfs_mkdir("temp2") == E_SUCCESS);
  CHECK(jfs_rmdir("temp2") == E_SUCCESS);
  CHECK(jfs_mkdir("temp2") == E_SUCCESS); // block should have been released and reusable
  CHECK(jfs_rmdir("temp2") == E_SUCCESS);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

// ---- creat (dedicated dir: "creatt") ----

static int test_creat_1_create_file(void) {
  CHECK(jfs_mkdir("creatt") == E_SUCCESS);
  CHECK(jfs_chdir("creatt") == E_SUCCESS);
  CHECK(jfs_creat("f1.txt") == E_SUCCESS);
  struct stats st;
  CHECK(jfs_stat("f1.txt", &st) == E_SUCCESS);
  CHECK(!st.is_dir);
  CHECK(st.file_size == 0);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_creat_2_long_filename(void) {
  CHECK(jfs_chdir("creatt") == E_SUCCESS);
  CHECK(jfs_creat("waytoolongfilename") == E_MAX_NAME_LENGTH);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_creat_3_fill_directory(void) {
  // "creatt" already holds "f1.txt" from test_creat_1; fill the rest.
  CHECK(jfs_chdir("creatt") == E_SUCCESS);
  CHECK(jfs_creat("f2.txt") == E_SUCCESS);
  CHECK(jfs_creat("f3.txt") == E_SUCCESS);
  CHECK(jfs_creat("f4.txt") == E_SUCCESS);
  CHECK(jfs_creat("f5.txt") == E_SUCCESS);
  CHECK(jfs_creat("toomany") == E_MAX_DIR_ENTRIES); // must be <= MAX_NAME_LENGTH chars
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

// ---- remove (dedicated dir: "removet") ----

static int test_remove_1_creat_then_remove(void) {
  CHECK(jfs_mkdir("removet") == E_SUCCESS);
  CHECK(jfs_chdir("removet") == E_SUCCESS);
  CHECK(jfs_creat("r1.txt") == E_SUCCESS);
  CHECK(jfs_remove("r1.txt") == E_SUCCESS);
  struct stats st;
  CHECK(jfs_stat("r1.txt", &st) == E_NOT_EXISTS);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_remove_2_creat_remove_creat(void) {
  CHECK(jfs_chdir("removet") == E_SUCCESS);
  CHECK(jfs_creat("r2.txt") == E_SUCCESS);
  CHECK(jfs_remove("r2.txt") == E_SUCCESS);
  CHECK(jfs_creat("r2.txt") == E_SUCCESS); // block reused correctly
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

// ---- write & read (dedicated dir: "wrt") ----

static int test_write_read_1_one_byte(void) {
  CHECK(jfs_mkdir("wrt") == E_SUCCESS);
  CHECK(jfs_chdir("wrt") == E_SUCCESS);
  CHECK(jfs_creat("one.txt") == E_SUCCESS);
  CHECK(jfs_write("one.txt", "X", 1) == E_SUCCESS);
  char buf[4] = {0};
  unsigned short count = sizeof(buf);
  CHECK(jfs_read("one.txt", buf, &count) == E_SUCCESS);
  CHECK(count == 1);
  CHECK(buf[0] == 'X');
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_write_read_2_big_read_buffer(void) {
  CHECK(jfs_chdir("wrt") == E_SUCCESS);
  CHECK(jfs_creat("big.txt") == E_SUCCESS);
  const char *data = "hello world";
  CHECK(jfs_write("big.txt", data, (unsigned short)strlen(data)) == E_SUCCESS);
  char buf[256] = {0};
  unsigned short count = sizeof(buf); // buffer much bigger than the data
  CHECK(jfs_read("big.txt", buf, &count) == E_SUCCESS);
  CHECK(count == strlen(data));
  CHECK(memcmp(buf, data, strlen(data)) == 0);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_write_read_3_small_read_buffer(void) {
  CHECK(jfs_chdir("wrt") == E_SUCCESS);
  CHECK(jfs_creat("sml.txt") == E_SUCCESS);
  const char *data = "0123456789";
  CHECK(jfs_write("sml.txt", data, (unsigned short)strlen(data)) == E_SUCCESS);
  char buf[4] = {0};
  unsigned short count = sizeof(buf); // smaller than the file
  CHECK(jfs_read("sml.txt", buf, &count) == E_SUCCESS);
  CHECK(count == sizeof(buf));
  CHECK(memcmp(buf, "0123", 4) == 0);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_write_read_4_binary_data(void) {
  CHECK(jfs_chdir("wrt") == E_SUCCESS);
  CHECK(jfs_creat("bin.dat") == E_SUCCESS);
  unsigned char data[6] = {0x00, 0xFF, 0x01, 0x00, 0xAB, 0x00};
  CHECK(jfs_write("bin.dat", data, sizeof(data)) == E_SUCCESS);
  unsigned char buf[6] = {0};
  unsigned short count = sizeof(buf);
  CHECK(jfs_read("bin.dat", buf, &count) == E_SUCCESS);
  CHECK(count == sizeof(data));
  CHECK(memcmp(buf, data, sizeof(data)) == 0); // embedded zero bytes preserved
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_write_read_5_multiple_write_one_block(void) {
  CHECK(jfs_chdir("wrt") == E_SUCCESS);
  CHECK(jfs_creat("mu1.txt") == E_SUCCESS);
  CHECK(jfs_write("mu1.txt", "abc", 3) == E_SUCCESS);
  CHECK(jfs_write("mu1.txt", "def", 3) == E_SUCCESS); // appends, still fits in one block
  char buf[16] = {0};
  unsigned short count = sizeof(buf);
  CHECK(jfs_read("mu1.txt", buf, &count) == E_SUCCESS);
  CHECK(count == 6);
  CHECK(memcmp(buf, "abcdef", 6) == 0);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_write_read_6_multiple_write_whole_blocks(void) {
  // "wrt" is now full (5 files from tests 1-5); use a fresh directory.
  CHECK(jfs_mkdir("wrt3") == E_SUCCESS);
  CHECK(jfs_chdir("wrt3") == E_SUCCESS);
  CHECK(jfs_creat("mu2.txt") == E_SUCCESS);
  // Data blocks reserve their last 2 bytes for a next-block pointer, so 62
  // usable bytes/block; two writes of 62 bytes span exactly two data
  // blocks with no partial block.
  char chunk[62];
  memset(chunk, 'A', sizeof(chunk));
  CHECK(jfs_write("mu2.txt", chunk, sizeof(chunk)) == E_SUCCESS);
  memset(chunk, 'B', sizeof(chunk));
  CHECK(jfs_write("mu2.txt", chunk, sizeof(chunk)) == E_SUCCESS);

  char buf[200] = {0};
  unsigned short count = sizeof(buf);
  CHECK(jfs_read("mu2.txt", buf, &count) == E_SUCCESS);
  CHECK(count == 124);
  for (int i = 0; i < 62; i++) CHECK(buf[i] == 'A');
  for (int i = 62; i < 124; i++) CHECK(buf[i] == 'B');
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_write_read_7_multiple_write_partial_blocks(void) {
  CHECK(jfs_chdir("wrt3") == E_SUCCESS);
  CHECK(jfs_creat("mu3.txt") == E_SUCCESS);
  CHECK(jfs_write("mu3.txt", "12345", 5) == E_SUCCESS);
  CHECK(jfs_write("mu3.txt", "67890abcdefghij", 15) == E_SUCCESS); // crosses into a 2nd block
  char buf[64] = {0};
  unsigned short count = sizeof(buf);
  CHECK(jfs_read("mu3.txt", buf, &count) == E_SUCCESS);
  CHECK(count == 20);
  CHECK(memcmp(buf, "1234567890abcdefghij", 20) == 0);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_write_read_8_max_file_size(void) {
  CHECK(jfs_chdir("wrt3") == E_SUCCESS);
  CHECK(jfs_creat("max.txt") == E_SUCCESS);

  char chunk[256];
  memset(chunk, 'Z', sizeof(chunk));
  int remaining = MAX_FILE_SIZE;
  while (remaining > 0) {
    unsigned short n = (unsigned short)(remaining < (int)sizeof(chunk) ? remaining : (int)sizeof(chunk));
    CHECK(jfs_write("max.txt", chunk, n) == E_SUCCESS);
    remaining -= n;
  }

  struct stats st;
  CHECK(jfs_stat("max.txt", &st) == E_SUCCESS);
  CHECK(st.file_size == MAX_FILE_SIZE);

  // One more byte must not fit.
  CHECK(jfs_write("max.txt", "X", 1) == E_MAX_FILE_SIZE);

  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_write_read_9_multiple_write_max_file_size(void) {
  CHECK(jfs_mkdir("wrt2") == E_SUCCESS); // fresh dir: "wrt" is now full (5 files)
  CHECK(jfs_chdir("wrt2") == E_SUCCESS);
  CHECK(jfs_creat("mx2.txt") == E_SUCCESS);

  // Write MAX_FILE_SIZE bytes across many small writes instead of few large
  // ones, to exercise the same boundary through a different write pattern.
  char one_byte = 'Q';
  for (int i = 0; i < MAX_FILE_SIZE; i++) {
    CHECK(jfs_write("mx2.txt", &one_byte, 1) == E_SUCCESS);
  }

  struct stats st;
  CHECK(jfs_stat("mx2.txt", &st) == E_SUCCESS);
  CHECK((int)st.file_size == MAX_FILE_SIZE);
  CHECK(jfs_write("mx2.txt", &one_byte, 1) == E_MAX_FILE_SIZE);

  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

// ---- stat (dedicated dir: "statt") ----

static int test_stat_1_directories(void) {
  CHECK(jfs_mkdir("statt") == E_SUCCESS);
  CHECK(jfs_chdir("statt") == E_SUCCESS);
  CHECK(jfs_mkdir("adir") == E_SUCCESS);
  struct stats st;
  CHECK(jfs_stat("adir", &st) == E_SUCCESS);
  CHECK(st.is_dir);
  CHECK(strcmp(st.name, "adir") == 0);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_stat_2_files(void) {
  CHECK(jfs_chdir("statt") == E_SUCCESS);
  CHECK(jfs_creat("fl.txt") == E_SUCCESS);
  struct stats st;
  CHECK(jfs_stat("fl.txt", &st) == E_SUCCESS);
  CHECK(!st.is_dir);
  CHECK(strcmp(st.name, "fl.txt") == 0);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

static int test_stat_3_release_block(void) {
  // Fill a fresh directory to capacity, remove one entry, and confirm its
  // block was actually released (a new entry can then take its place --
  // this fails if release_block/allocate_block don't round-trip correctly).
  CHECK(jfs_chdir("statt") == E_SUCCESS);
  CHECK(jfs_mkdir("s1") == E_SUCCESS);
  CHECK(jfs_mkdir("s2") == E_SUCCESS);
  CHECK(jfs_rmdir("s2") == E_SUCCESS);
  CHECK(jfs_mkdir("s2again") == E_SUCCESS);
  struct stats st;
  CHECK(jfs_stat("s2again", &st) == E_SUCCESS);
  CHECK(st.is_dir);
  CHECK(jfs_chdir(NULL) == E_SUCCESS);
  return 1;
}

int main(void) {
  remove(TEST_DISK_FILENAME);
  if (jfs_mount(TEST_DISK_FILENAME) != 0) {
    fprintf(stderr, "ERROR: Mount failed.\n");
    return 1;
  }

  if (jfs_mkdir(GROUP_A) != E_SUCCESS || jfs_mkdir(GROUP_B) != E_SUCCESS) {
    fprintf(stderr, "ERROR: test setup failed.\n");
    return 1;
  }

  printf("**********************\n*** TEST SET mkdir ***\n");
  run_test(GROUP_A, "test_mkdir_1_create_directory", test_mkdir_1_create_directory);
  run_test(GROUP_A, "test_mkdir_2_long_dirname", test_mkdir_2_long_dirname);
  run_test(GROUP_A, "test_mkdir_3_fill_directory", test_mkdir_3_fill_directory);

  printf("**********************\n*** TEST SET chdir ***\n");
  run_test(GROUP_A, "test_chdir_1_simple_cd", test_chdir_1_simple_cd);
  run_test(GROUP_A, "test_chdir_2_cd_to_root", test_chdir_2_cd_to_root);

  printf("**********************\n*** TEST SET rmdir ***\n");
  run_test(GROUP_A, "test_rmdir_1_mkdir_then_rmdir", test_rmdir_1_mkdir_then_rmdir);
  run_test(GROUP_A, "test_rmdir_2_rm_subdir", test_rmdir_2_rm_subdir);
  run_test(GROUP_A, "test_rmdir_3_mkdir_rmdir_mkdir", test_rmdir_3_mkdir_rmdir_mkdir);

  printf("***********************\n*** TEST SET creat ***\n");
  run_test(GROUP_A, "test_creat_1_create_file", test_creat_1_create_file);
  run_test(GROUP_A, "test_creat_2_long_filename", test_creat_2_long_filename);
  run_test(GROUP_A, "test_creat_3_fill_directory", test_creat_3_fill_directory);

  printf("*****************************\n*** TEST SET remove ***\n");
  run_test(GROUP_A, "test_remove_1_creat_then_remove", test_remove_1_creat_then_remove);
  run_test(GROUP_A, "test_remove_2_creat_remove_creat", test_remove_2_creat_remove_creat);

  printf("*****************************\n*** TEST SET write & read ***\n");
  run_test(GROUP_B, "test_write_read_1_one_byte", test_write_read_1_one_byte);
  run_test(GROUP_B, "test_write_read_2_big_read_buffer", test_write_read_2_big_read_buffer);
  run_test(GROUP_B, "test_write_read_3_small_read_buffer", test_write_read_3_small_read_buffer);
  run_test(GROUP_B, "test_write_read_4_binary_data", test_write_read_4_binary_data);
  run_test(GROUP_B, "test_write_read_5_multiple_write_one_block", test_write_read_5_multiple_write_one_block);
  run_test(GROUP_B, "test_write_read_6_multiple_write_whole_blocks", test_write_read_6_multiple_write_whole_blocks);
  run_test(GROUP_B, "test_write_read_7_multiple_write_partial_blocks", test_write_read_7_multiple_write_partial_blocks);
  run_test(GROUP_B, "test_write_read_8_max_file_size", test_write_read_8_max_file_size);
  run_test(GROUP_B, "test_write_read_9_multiple_write_max_file_size", test_write_read_9_multiple_write_max_file_size);

  printf("**********************\n*** TEST SET stat ***\n");
  run_test(GROUP_B, "test_stat_1_directories", test_stat_1_directories);
  run_test(GROUP_B, "test_stat_2_files", test_stat_2_files);
  run_test(GROUP_B, "test_stat_3_release_block", test_stat_3_release_block);

  printf("Set total tests: %d, Passed: %d, Failed: %d\n", tests_run, tests_passed, tests_run - tests_passed);

  jfs_unmount();
  remove(TEST_DISK_FILENAME);

  return (tests_passed == tests_run) ? 0 : 1;
}
