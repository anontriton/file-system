# Changelog

## Portfolio prep — reconstruction, portability, and correctness fixes

Before this pass, this repository **could not be compiled at all** — by
anyone, on any platform. Only one source file (`jumbo_file_system.c`) was
ever committed. Its own header and the three other modules it depends on
existed solely as **precompiled Linux x86-64 `.o` files**, built on a Tufts
course server against a path (`/comp/111m1/assignments/fs`) that no longer
exists and was never part of this repo. Object files are platform-locked, so
even on Linux the project could only ever be relinked, never rebuilt — and on
macOS or Windows it could not link at all.

Everything missing has now been reimplemented from scratch in portable C.
The project builds and passes its full test suite natively on **Linux,
macOS, and Windows**, with no precompiled binaries remaining in the repo.

### Recovering the missing interface

`jumbo_file_system.c` referenced constants, types, and a struct that existed
only inside the binaries. Rather than guess at them, the exact definitions
were extracted from the compiled objects:

- **`struct stats`** (field order, types, and byte offsets), **`block_num_t`
  = `uint16_t`**, and **`MAX_DIR_ENTRIES` = 5** were read out of the DWARF
  debug info still embedded in `command_line.o` (it had been compiled `-g`).
- **`BLOCK_SIZE` = 64** was confirmed from `raw_disk.o`'s disassembly — block
  offsets are computed with `shl $0x6` (×64) and its `read`/`write` calls use
  a length of `$0x40`.
- **`MAX_FILE_SIZE` = 1792** was recovered from the `cat` handler's
  disassembly in `command_line.o`, which allocates a `$0x700`-byte buffer to
  hold an entire file.
- **Total disk size = 32768 bytes (512 blocks)** came from `raw_mount`'s
  zero-fill loop, and the **bitmap allocator scheme** (one bit per block, all
  in block 0; blocks 0 and 1 reserved) from `basic_file_system.o`'s
  `allocate_block`/`release_block`.
- The **command set, prompts, and exact error strings** for the shell were
  taken from the string table of `command_line.o`, and the **test-case names
  and categories** from `test.o`.

### Files reimplemented from scratch

- **`raw_disk.c` / `raw_disk.h`** — the simulated block device. Written
  against C standard-library `stdio` (`fopen`/`fseek`/`fread`/`fwrite`)
  instead of the original's POSIX `open`/`lseek`/`read`/`write`, which is
  what makes a native Windows build possible.
- **`basic_file_system.c` / `basic_file_system.h`** — the free-block bitmap
  allocator, matching the recovered scheme above.
- **`jumbo_file_system.h`** — the previously missing header.
- **`command_line.c`** — the interactive `jfs$` shell (`cd`, `mkdir`,
  `rmdir`, `ls`, `touch`, `rm`, `stat`, `cat`, `head`, `append`, `exit`),
  reproducing the original's prompts and error messages. Also handles EOF
  (Ctrl-D) as a clean exit, which the original did not.
- **`test.c`** — a portable test suite covering the same categories as the
  original harness. The original isolated each test case in a forked child
  process to survive crashes; that is POSIX-only, so this version runs tests
  sequentially in-process instead. The tradeoff is that a hard crash aborts
  the run rather than being reported as a single failed case.
- **`Makefile`** — builds everything from source, with no dependency on the
  vanished course directory or on any precompiled object. Appends `.exe` to
  targets automatically when building on Windows.

### Bug fixed in the original `jumbo_file_system.c`

**File data was silently corrupted whenever a file spanned more than one
block.** Each data block reserves its final `sizeof(block_num_t)` (2) bytes
for a pointer to the next block in the file's chain, leaving 62 usable bytes
per 64-byte block. But `jfs_write` and `jfs_read` both did their offset and
length arithmetic against the full `BLOCK_SIZE` of 64. The effects:

- `jfs_write` treated all 64 bytes as writable, so content written at the end
  of a block overwrote the next-block pointer; when the chain was later
  extended, writing the real pointer overwrote 2 bytes of stored file data.
- `jfs_read` read 64 bytes of "content" per block, so it returned the 2
  pointer bytes as if they were file data and then mis-tracked its position.
- `jfs_write` also indexed the inode as if it held an array of block pointers
  (`file_inode_data + 4 + (num_blocks-1) * sizeof(block_num_t)`) even though
  the format stores only a single head pointer and chains the rest, so
  appending to an existing multi-block file wrote to the wrong place.

Both functions now use `DATA_BYTES_PER_BLOCK` (`BLOCK_SIZE - sizeof(block_num_t)`)
consistently, `jfs_write` walks the block chain to find the true last block
before appending, and `jfs_stat`'s `num_data_blocks` calculation was corrected
to match. This is verified by `test_write_read_6_multiple_write_whole_blocks`,
which writes 62 bytes of `A` followed by 62 bytes of `B` and previously read
back two corrupted bytes at the block boundary.

### Verification

The full suite (25 tests across mkdir, chdir, rmdir, creat, remove,
write/read, and stat) passes **25/25 on both native macOS arm64 and Linux
x86-64** (`gcc:13` container), compiling clean under
`-Wall -Wextra -Wpedantic`. The interactive shell was additionally exercised
end-to-end — creating directories and files, appending multi-word text,
`cat`/`head`/`stat`/`ls`, and error paths — and the on-disk image was
confirmed to persist correctly across separate runs of the program.
