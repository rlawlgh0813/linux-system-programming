#include <stdio.h>
#include <stdint.h>
#include "header.h"

// Prototypes
void cmd_print(int argc, char *argv[]);
static int read_block(int blk, void *buf);
static int read_inode(int ino, ext2_inode *buf);
static int get_inode_by_path(const char *path, ext2_inode *inode);
static int is_file(ext2_inode *inode);
static void help(void);
static void read_indirect(uint32_t blk, int level, uint32_t **out, int *count, int *cap);

// cmd_print: implement "print" command
void cmd_print(int argc, char *argv[]) {
    // Ensure a path argument is provided
    if (argc < 2) {
		help();
        return;
    }
    // parse path
    const char *path = argv[1];
    // parse options
    int max_lines = -1;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "print: option requires an argument -- 'n'\n");
                return;
            }
            max_lines = atoi(argv[i+1]);
            i++;
        } else {
            help();
            return;
        }
    }
    // get inode for path
    ext2_inode inode;
    if (get_inode_by_path(path, &inode) < 0) {
        help();
        return;
    }
    // ensure it's a file, not directory
    if (!is_file(&inode)) {
        fprintf(stderr, "Error: '%s' is not file\n", path);
        return;
    }
    // open buffer for reading blocks
    char *buf = malloc(block_size);
    if (!buf) { perror("malloc"); return; }
    uint32_t remaining = inode.i_size;
    int lines_printed = 0;
    
    // array for block num
    int cap = 16, count = 0;
    uint32_t *blocks = malloc(cap * sizeof *blocks);

    // direct (0..11)
    for (int i = 0; i < 12; i++) {
        if (inode.i_block[i]) {
            if (count >= cap) {
                cap *= 2;
                blocks = realloc(blocks, cap * sizeof *blocks);
            }
            blocks[count++] = inode.i_block[i];
        }
    }

    // single indirect
    if (inode.i_block[12])
        read_indirect(inode.i_block[12], 1, &blocks, &count, &cap);

    // double indirect
    if (inode.i_block[13])
        read_indirect(inode.i_block[13], 2, &blocks, &count, &cap);

    // triple indirect
    if (inode.i_block[14])
        read_indirect(inode.i_block[14], 3, &blocks, &count, &cap);

    for (int idx = 0; idx < count && remaining > 0; idx++) {
        uint32_t blk = blocks[idx];
        if (read_block(blk, buf) < 0) break;

        uint32_t to_write = remaining < (uint32_t)block_size
                        ? (uint32_t)remaining
                        : (uint32_t)block_size;

        if (max_lines < 0) {
            if (write(STDOUT_FILENO, buf, to_write) < 0) {
                perror("write");
                break;
            }
        } else {
            for (uint32_t j = 0; j < to_write; j++) {
                putchar(buf[j]);
                if (buf[j] == '\n' && ++lines_printed >= max_lines) {
                    free(buf);
                    free(blocks);
                    return;
                }
            }
        }
        remaining -= to_write;
    }

    free(blocks);
}

// print usage
static void help(void) {
	printf("Usage : print <PATH> [OPTION]...\n");
}

// is_file: true if inode is regular file
static int is_file(ext2_inode *inode) {
    // use POSIX check on mode bits
    return !S_ISDIR(inode->i_mode);
}

// read_block: read block from disk image
static int read_block(int blk, void *buf) {
    off_t off = (off_t)blk * block_size;
    if (lseek(fs_fd, off, SEEK_SET) < 0) return -1;
    return read(fs_fd, buf, block_size) == block_size ? 0 : -1;
}

// read_inode: read inode from inode table
static int read_inode(int ino, ext2_inode *buf) {
    off_t base = (off_t)gd.bg_inode_table * block_size;
    off_t off = base + (ino - 1) * inode_size;
    if (lseek(fs_fd, off, SEEK_SET) < 0) return -1;
    return read(fs_fd, buf, inode_size) == inode_size ? 0 : -1;
}

// get_inode_by_path: same logic as in tree.c
static int get_inode_by_path(const char *path, ext2_inode *inode) {
    // special-case root
    if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0) {
        if (read_inode(2, inode) < 0) return -1;
        return 2;
    }
    char *copy = strdup(path);
    if (!copy) return -1;
    int cur_ino = 2;
    ext2_inode cur;
    if (read_inode(cur_ino, &cur) < 0) { free(copy); return -1; }
    char *tok = strtok(copy, "/");
    while (tok) {
        if (!S_ISDIR(cur.i_mode)) break;
        int blocks = (cur.i_size + block_size - 1) / block_size;
        char *buf = malloc(block_size);
        int found = 0;
        for (int bi = 0; bi < blocks && !found; bi++) {
            uint32_t blk = cur.i_block[bi];
            if (!blk || read_block(blk, buf) < 0) continue;
            int off = 0;
            while (off < block_size) {
                ext2_dir_entry_2 *e = (ext2_dir_entry_2*)(buf + off);
                if (e->inode) {
                    char name[EXT2_NAME_LEN+1] = {0};
                    memcpy(name, e->name, e->name_len);
                    if (strcmp(name, tok) == 0) {
                        cur_ino = e->inode;
                        if (read_inode(cur_ino, &cur) < 0) { free(buf); break; }
                        found = 1;
                        break;
                    }
                }
                off += e->rec_len;
            }
        }
        free(buf);
        if (!found) break;
        tok = strtok(NULL, "/");
    }
    free(copy);
    if (tok) return -1;
    *inode = cur;
    return cur_ino;
}

static void read_indirect(uint32_t blk, int level, uint32_t **out, int *count, int *cap) {
    // block num buffer
    uint32_t *buf = malloc(block_size);
    if (!buf) return;
    // read block
    if (read_block(blk, buf) < 0) { free(buf); return; }
    int entries = block_size / sizeof(uint32_t);

    for (int i = 0; i < entries; i++) {
        uint32_t nblk = buf[i];
        if (!nblk) break;
        if (level == 1) {
            // data block
            if (*count >= *cap) {
                *cap *= 2;
                *out = realloc(*out, (*cap) * sizeof(uint32_t));
            }
            (*out)[(*count)++] = nblk;
        } else {
            read_indirect(nblk, level - 1, out, count, cap);
        }
    }
    free(buf);
}

