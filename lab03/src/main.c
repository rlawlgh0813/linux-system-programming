#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "header.h"

// Global variables defined in header.h
int fs_fd;
struct ext2_super_block sb;
struct ext2_group_desc gd;
int block_size;
int inode_size;

// init_ext2_structures: open image, read superblock and group descriptor
int init_ext2_structures(const char *disk_image) {
    // Open disk image read-only
    fs_fd = open(disk_image, O_RDONLY);
    if (fs_fd < 0) {
        perror("open");
        return -1;
    }
   
    // Read superblock at offset 1024
    if (lseek(fs_fd, 1024, SEEK_SET) < 0) {
        perror("lseek superblock");
        close(fs_fd);
        return -1;
    }
    if (read(fs_fd, &sb, sizeof(sb)) != sizeof(sb)) {
        perror("read superblock");
        close(fs_fd);
        return -1;
    }

    // Verify EXT2 magic number
    if (sb.s_magic != 0xEF53) {
        fprintf(stderr, "magic number failed to initialize ext2\n");
        close(fs_fd);
        return -1;
    }

    // Compute block size and inode size
    block_size = 1024 << sb.s_log_block_size;
    inode_size = sb.s_inode_size;

    // Read first group descriptor
	off_t gd_offset = (sb.s_first_data_block + 1) * block_size;
    if (lseek(fs_fd, gd_offset, SEEK_SET) < 0) {
        perror("lseek group_desc");
        close(fs_fd);
        return -1;
    }
    if (read(fs_fd, &gd, sizeof(gd)) != sizeof(gd)) {
        perror("read group_desc");
        close(fs_fd);
        return -1;
    }
	
    return 0;
}

// main: Program entry point
int main(int argc, char *argv[]) {
    // Validate command-line usage
    if (argc < 2) {
        fprintf(stderr, "Usage Error : %s <EXT2_IMAGE>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Initialize EXT2 structures
    if (init_ext2_structures(argv[1]) < 0) {
        return EXIT_FAILURE;
    }

    // Enter command loop
    cmd_loop();

    // Clean up: close the filesystem image file descriptor
    close(fs_fd);
    return EXIT_SUCCESS;
}
