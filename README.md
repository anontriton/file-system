# Jumbo File System

A working file system built from scratch in C — directories, files, block allocation, and persistent storage — implemented on top of a simulated block device, with an interactive shell to explore it. No OS filesystem APIs are used for the filesystem itself; everything down to the free-block bitmap is hand-implemented against raw 64-byte block reads and writes.

**Status:** ✅ Builds and passes 25/25 tests natively on **Linux, macOS, and Windows**. See [CHANGELOG.md](CHANGELOG.md) for how the missing half of this project was reconstructed and what was fixed.

## Tech Stack

- **C** (C11, builds with `gcc`, `clang`, or MinGW) — standard library only, zero external dependencies
- `make` build system

## Architecture

Three layers, each only talking to the one below it:

```
command_line.c / test.c        interactive jfs$ shell, test suite
        │
        ▼
jumbo_file_system.c            directories, files, read/write/stat
        │                      (jfs_mkdir, jfs_creat, jfs_write, ...)
        ▼
basic_file_system.c            free-block allocator (bitmap in block 0)
        │                      (allocate_block, release_block)
        ▼
raw_disk.c                     simulated block device -> a flat DISK file
                               (read_block, write_block: 64 bytes each)
```

**On-disk layout.** The disk is a single 32,768-byte file: 512 blocks of 64 bytes. Block 0 is a bitmap where each bit marks one block as free or allocated. Block 1 is the root directory. Everything else is allocated on demand.

**Directory blocks** hold up to 5 fixed-size entries, each packed by hand as: a used flag, an is-directory flag, a 7-byte name, and a 2-byte pointer to the entry's own block.

**File blocks** are a linked chain: the file's inode block stores its size plus a pointer to the first data block, and each data block spends its final 2 bytes pointing at the next — leaving 62 bytes per block for actual content, which caps a file at 1,792 bytes.

## Key Technical Features

- **Real block-level storage** — every operation ultimately becomes a 64-byte `read_block`/`write_block` against a flat file; nothing is cached in memory between calls, so the filesystem state genuinely lives on "disk" and survives across separate runs of the program.
- **Bitmap free-space management** — `allocate_block` scans a one-bit-per-block bitmap for the first free block and claims it; `release_block` clears it. Freed blocks are correctly reused by later allocations.
- **Hierarchical directories** — `jfs_mkdir`, `jfs_chdir`, `jfs_rmdir`, and `jfs_ls` implement a real directory tree with arbitrary nesting, including refusing to remove a non-empty directory.
- **Block-chained files** — `jfs_write` appends across an arbitrary number of chained blocks, allocating and linking new ones as the file grows; `jfs_read` walks the chain back. Handles binary data with embedded zero bytes, not just text.
- **Manual byte-level serialization** — directory entries and inodes are packed and parsed directly at raw byte offsets, with no structs written to disk, keeping the on-disk format explicit and compact.
- **Faithful error semantics** — distinct error codes for every failure mode (`E_EXISTS`, `E_NOT_DIR`, `E_IS_DIR`, `E_NOT_EMPTY`, `E_MAX_NAME_LENGTH`, `E_MAX_DIR_ENTRIES`, `E_MAX_FILE_SIZE`, `E_DISK_FULL`), each surfaced as a readable message by the shell.

## Local Setup & Installation

**Prerequisites:** a C compiler and `make`. No libraries to install.

### Linux

```bash
sudo apt install build-essential   # Debian/Ubuntu; or: sudo dnf install gcc make
git clone https://github.com/anontriton/file-system.git
cd file-system
make
```

### macOS

```bash
xcode-select --install             # installs clang + make, if not already present
git clone https://github.com/anontriton/file-system.git
cd file-system
make
```

### Windows

Using [MSYS2](https://www.msys2.org/) (recommended) or MinGW-w64, from the MSYS2 UCRT64 shell:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc make
git clone https://github.com/anontriton/file-system.git
cd file-system
make
```

This produces `command_line.exe` and `test.exe`. WSL works too — follow the Linux instructions inside it.

### Running

```bash
./command_line
```

```
jfs$ mkdir docs
jfs$ cd docs
jfs$ touch notes.t
jfs$ append notes.t hello world
jfs$ cat notes.t
hello world
jfs$ stat notes.t
File name: notes.t
Inode block number: 3
Number of data blocks: 1
File size: 11
jfs$ ls
notes.t
jfs$ exit
```

Available commands: `cd [dir]`, `mkdir <dir>`, `rmdir <dir>`, `ls`, `touch <file>`, `rm <file>`, `stat <name>`, `cat <file>`, `head <file> <n>`, `append <file> <data>`, `exit`.

State is stored in a `DISK` file created in the working directory, and persists between runs — delete it to start from an empty filesystem.

Run the test suite:

```bash
make check
```

No environment variables are required.

> **Note on limits:** names are capped at 7 characters, directories hold 5 entries, and files max out at 1,792 bytes. These are properties of the original on-disk format, preserved intentionally.

## Future Roadmap / Enhancements

- [ ] Support paths (`cd a/b/c`, `cat docs/notes.t`) instead of single-level names only
- [ ] Add `..` / parent-directory traversal
- [ ] Grow the format's limits: longer filenames, more directory entries, larger files (indirect blocks instead of a single chain)
- [ ] Track and expose free-space usage (a `df`-style command)
- [ ] Add a fragmentation-aware or first-fit-vs-best-fit allocation comparison
- [ ] Restore per-test crash isolation in the test suite in a cross-platform way, so a segfault reports one failed case instead of aborting the run
- [ ] Journaling or crash-consistency: currently an interrupted write can leave a partially-linked chain

## Demo / Screenshots

<!-- Add a terminal recording or screenshot here showing a session in ./command_line -- creating dirs/files, appending, cat/stat/ls -- and a passing `make check` run, e.g. docs/demo.gif or docs/screenshot.png -->
