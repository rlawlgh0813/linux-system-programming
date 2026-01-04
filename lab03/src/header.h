#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdint.h>

/* --- macro --- */
#define EXT2_NAME_LEN  255
#define EXT2_N_BLOCKS  15
#define EXT2_FT_DIR    2
#define EXT2_S_IFDIR 0x4000
#define MAX_PATH 4096
#define MAX_FILE 255
#define MAX_LINE 256
#define MAX_ARG 20

/* --- user defined structure --- */
// On-disk ext2 superblock (fields through inode size)
typedef struct ext2_super_block {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;            // 0xEF53
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
} ext2_super_block;

// On-disk ext2 group descriptor (minimal)
typedef struct ext2_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    // ... other fields omitted
} ext2_group_desc;

// On-disk ext2 inode (simplified)
typedef struct ext2_inode {
    uint16_t i_mode;               // file type & permissions
    uint16_t i_uid;                // owner UID
    uint32_t i_size;               // size in bytes
    uint32_t i_atime;              // access time
    uint32_t i_ctime;              // creation time
    uint32_t i_mtime;              // modification time
    uint32_t i_dtime;              // deletion time
    uint16_t i_gid;                // group ID
    uint16_t i_links_count;        // hard links count
    uint32_t i_blocks;             // number of 512B blocks allocated
    uint32_t i_flags;              // flags
    uint32_t i_osd1;               // OS dependent 1
    uint32_t i_block[EXT2_N_BLOCKS]; // block pointers
    uint32_t i_generation;         // file version (for NFS)
    uint32_t i_file_acl;           // file ACL
    uint32_t i_dir_acl;            // directory ACL or high 32 bits of file size
    uint32_t i_faddr;              // fragment address
    uint8_t  i_osd2[12];           // OS dependent 2
} ext2_inode;

// On-disk ext2 directory entry (v2)
typedef struct ext2_dir_entry_2 {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[EXT2_NAME_LEN];
} ext2_dir_entry_2;

// Global variables for ext2 image state
extern int fs_fd;                     // file descriptor of image
extern ext2_super_block sb;           // superblock
extern ext2_group_desc gd;            // group descriptor
extern int block_size;
extern int inode_size;

// Initialization and main loop
int init_ext2_structures(const char *disk_image);
void cmd_loop(void);

// Command functions
void cmd_tree(int argc, char *argv[]);
void cmd_print(int argc, char *argv[]);
void cmd_help(char *arg);
