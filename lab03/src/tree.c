#include <stdio.h>
#include "header.h"

/* -- Prototypes -- */
static void add_node(const char *name, int depth, int is_last, ext2_inode *inode);
static void clear_tree_list(void);
static void build_tree(const char *path, int depth, int recursive);
static void print_nodes(int show_size, int show_perm);
static int is_dir(ext2_inode *inode);
static int read_block(int blk, void *buf);
static int read_inode(int ino, ext2_inode *buf);
static int get_inode_by_path(const char *path, ext2_inode *inode);
static void format_permissions(uint16_t mode, char *buf);
static void format_size(uint32_t size, char *buf);
static void help(void);

/* -- Tree node -- */
typedef struct TreeNode {
    char *name;
    int depth;
    int is_last;
    ext2_inode inode;
    struct TreeNode *next;
} TreeNode;

static TreeNode *list_head = NULL;
static TreeNode *list_tail = NULL;

// add_node: append a node to the linked list
static void add_node(const char *name, int depth, int is_last, ext2_inode *inode) {
    TreeNode *node = malloc(sizeof(TreeNode));
    if (!node) { perror("malloc"); exit(EXIT_FAILURE); }
    node->name = strdup(name);
    node->depth = depth;
    node->is_last = is_last;
    node->inode = *inode;
    node->next = NULL;
    if (!list_head) list_head = node;
    else list_tail->next = node;
    list_tail = node;
}

// clear_tree_list: free all nodes
static void clear_tree_list(void) {
    TreeNode *cur = list_head;
    while (cur) {
        TreeNode *next = cur->next;
        free(cur->name);
        free(cur);
        cur = next;
    }
    list_head = list_tail = NULL;
}

// build_tree: scan directory entries and recurse
static void build_tree(const char *path, int depth, int recursive) {
    ext2_inode dir_inode;
    // Retrieve inode and verify directory
    if (get_inode_by_path(path, &dir_inode) < 0 || !is_dir(&dir_inode))
        return;

    int blocks = (dir_inode.i_size + block_size - 1) / block_size;
    char *buf = malloc(block_size);
    if (!buf) { perror("malloc"); return; }

    // Collect directory entries except '.' '..' and 'lost+found'
    typedef struct { uint32_t ino; char name[EXT2_NAME_LEN+1]; } DirEntry;
    DirEntry *entries = NULL;
    int count = 0, cap = 0;

    for (int i = 0; i < blocks; i++) {
        uint32_t blk = dir_inode.i_block[i];
        if (!blk) continue;
        if (read_block(blk, buf) < 0) continue;
        int off = 0;
        while (off < block_size) {
            ext2_dir_entry_2 *e = (ext2_dir_entry_2*)(buf + off);
            if (e->inode) {
                char name[EXT2_NAME_LEN+1] = {0};
                memcpy(name, e->name, e->name_len);
                // Skip special entries
                if (strcmp(name, ".") && strcmp(name, "..") && strcmp(name, "lost+found")) {
                    if (count >= cap) {
                        cap = cap ? cap * 2 : 8;
                        entries = realloc(entries, cap * sizeof *entries);
                        if (!entries) { perror("realloc"); exit(EXIT_FAILURE); }
                    }
                    entries[count].ino = e->inode;
                    strncpy(entries[count].name, name, EXT2_NAME_LEN);
                    count++;
                }
            }
            off += e->rec_len;
        }
    }
    free(buf);
    // Add each entry to node list and recurse if needed
    for (int i = 0; i < count; i++) {
        ext2_inode child;
        read_inode(entries[i].ino, &child);
        int last = (i == count - 1);
        add_node(entries[i].name, depth, last, &child);
        if (recursive && is_dir(&child)) {
            char subpath[1024];
            snprintf(subpath, sizeof(subpath), "%s/%s", path, entries[i].name);
            build_tree(subpath, depth + 1, recursive);
        }
    }

    free(entries);
}

// print_nodes: Iterate the node list and print a tree-like layout.
static void print_nodes(int show_size, int show_perm) {
    int last_flags[MAX_ARG] = {0};

    for (TreeNode *cur = list_head; cur; cur = cur->next) {
        // Print tree branches based on depth
        for (int level = 0; level < cur->depth; level++) {
            if (last_flags[level])
                printf("    ");
            else
                printf("│   ");
        }
        // Print the branch symbol
        printf(cur->is_last ? "└── " : "├── ");

        // Print permissions and size if requested
        if (show_perm || show_size) {
            printf("[");
            if (show_perm) {
                char p[16];
                format_permissions(cur->inode.i_mode, p);
                printf("%s", p);
            }
            if (show_perm && show_size) printf(" ");
            if (show_size) {
                char s[16];
                format_size(cur->inode.i_size, s);
                printf("%s", s);
            }
            printf("] ");
        }

        // Print the entry name
        printf("%s\n", cur->name);

        // Mark this level as last for indentation logic
        last_flags[cur->depth] = cur->is_last;
    }
}

// print usage
static void help() {
	printf("Usage : tree <PATH> [OPTION]...\n");
}

// is_dir: check inode mode for directory
static int is_dir(ext2_inode *inode) {
    return (inode->i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR;
}

// read_block: read a block from disk image
static int read_block(int blk, void *buf) {
    off_t off = (off_t)blk * block_size;
    if (lseek(fs_fd, off, SEEK_SET) < 0) return -1;
    return read(fs_fd, buf, block_size) == block_size ? 0 : -1;
}

// read_inode: read inode from table
static int read_inode(int ino, ext2_inode *buf) {
    off_t base = (off_t)gd.bg_inode_table * block_size;
    off_t off = base + (ino - 1) * inode_size;
    if (lseek(fs_fd, off, SEEK_SET) < 0) return -1;
    return read(fs_fd, buf, inode_size) == inode_size ? 0 : -1;
}

// get_inode_by_path: resolve path to inode
static int get_inode_by_path(const char *path, ext2_inode *inode) {
    // Handle root or current directory
	if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0) {
		ext2_inode root;
        if (read_inode(2, &root) < 0) return -1;
        *inode = root;
        return 2;
    }

	char *copy = strdup(path);
    if (!copy) return -1;
    int cur_ino = 2;
    ext2_inode cur;
    if (read_inode(cur_ino, &cur) < 0) { free(copy); return -1; }
    char *tok = strtok(copy, "/");
    // Traverse each component
    while (tok) {
        if (!S_ISDIR(cur.i_mode)) break;
        int blocks = (cur.i_size + block_size - 1) / block_size;
        char *buf = malloc(block_size);
        int found = 0;
        for (int i = 0; i < blocks && !found; i++) {
            uint32_t blk = cur.i_block[i];
			if (!blk || read_block(blk, buf) < 0) continue;
            int off = 0;
            while (off < block_size) {
                ext2_dir_entry_2 *e = (ext2_dir_entry_2*)(buf + off);
                if (e->inode) {
                    char name[EXT2_NAME_LEN+1] = {0};
                    memcpy(name, e->name, e->name_len);
                    if (strcmp(name, tok) == 0) {
                        cur_ino = e->inode;
                        read_inode(cur_ino, &cur);
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

// format_permissions: build permission string
static void format_permissions(uint16_t mode, char *buf) {
    buf[0] = S_ISDIR(mode) ? 'd' : '-';
    const char *perm = "rwxrwxrwx";
    for (int i = 0; i < 9; i++) buf[i+1] = (mode & (1 << (8 - i))) ? perm[i] : '-';
    buf[10] = '\0';
}

// format_size: file size in bytes
static void format_size(uint32_t size, char *buf) {
    sprintf(buf, "%u", size);
}

// cmd_tree: entry point for tree command
void cmd_tree(int argc, char *argv[]) {
    int recursive = 0, show_size = 0, show_perm = 0;

    // Usage if no path
    if (argc < 2) { help(); return; }
    const char *path = argv[1];
    // parse options
    for (int i = 2; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] == '\0') { help(); return; }
        for (char *p = &argv[i][1]; *p; p++) {
            if (*p == 'r') recursive = 1;
            else if (*p == 's') show_size = 1;
            else if (*p == 'p') show_perm = 1;
            else { help(); return; }
        }
    }
    // clear old list
    clear_tree_list();
    // validate path
    ext2_inode root;
    if (get_inode_by_path(path, &root) < 0) { help(); return; }
    if (!is_dir(&root)) { fprintf(stderr, "Error: '%s' is not directory\n", path); return; }

    // print and recurse
    printf("%s\n", path);
    build_tree(path, 0, recursive);
	print_nodes(show_size, show_perm);
	
	// number of directory and files
	int dir_count = 1;
	int file_count = 0;
	for (TreeNode *cur = list_head; cur; cur = cur->next) {
	    if (is_dir(&cur->inode)) dir_count++;
		else file_count++;
	}
	printf("\n%d directories, %d files\n\n", dir_count, file_count);

    // Clean up node list
	clear_tree_list();
}
