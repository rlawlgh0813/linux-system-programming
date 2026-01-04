#include <stdio.h>    // Standard I/O
#include <stdlib.h>   // malloc, free, exit, etc.
#include <string.h>   // strcpy, strcat, strcmp, etc.
#include <unistd.h>   // realpath, fork, execvp, etc.
#include <dirent.h>   // opendir, readdir, closedir, scandir
#include <sys/stat.h> // stat, lstat, mkdir
#include <sys/types.h>
#include <time.h>
#include <pwd.h>      // user info
#include <grp.h>      // group info
#include <fcntl.h>    // open flags
#include <errno.h>    // Error number definitions
#include <sys/wait.h> // waitpid

#define MAX_USER_INPUT 1024
#define MAX_FILENAME   256
#define MAX_PATH       4096
#define MAX_CONFLICTS  1000
#define MAX_GROUPS     100

// Represents a file or directory node in the tree
typedef struct FileNode {
    char name[MAX_FILENAME];   // File or directory name
    char path[MAX_PATH];       // Absolute path
    int is_dir;                // 1 if directory, 0 if file
    int is_root;               // 1 if this node is the root of the tree
    int is_last;               // 1 if this node is the last sibling
    struct stat st;            // Stat structure for file info
    struct FileNode *child;    // Pointer to first child (if directory)
    struct FileNode *sibling;  // Pointer to next sibling
} FileNode;

// Structure for directory linked list (if needed)
typedef struct DirNode {
    char path[MAX_PATH];
    struct DirNode *next;
} DirNode;

// Structure to represent a file in a conflict group (files with the same name)
typedef struct ConflictFile {
    char *filepath; // Full path to the file
    char *filename; // File name only
    time_t mtime;   // Last modification time
} ConflictFile;

// Structure for a group of conflicting files (same filename)
typedef struct ConflictGroup {
    char *filename;            // Conflicting filename
    ConflictFile **files;      // Array of pointers to ConflictFile
    int count;                 // Number of files in the group
    int capacity;              // Allocated capacity of the array
    struct ConflictGroup *next; // Pointer to next conflict group
} ConflictGroup;

// Structure for grouping files by extension
typedef struct ExtGroup {
    char *extension;           // File extension
    ConflictGroup *conflicts;  // Linked list of conflict groups within this extension
    struct ExtGroup *next;     // Pointer to next extension group
} ExtGroup;

// Structure for arrange command options
typedef struct Options {
    char *dir_path;       // Directory to be arranged
    char *result_dir;     // Output directory name (-d option)
    char **exclude_dirs;  // Array of directories to exclude (-x option)
    int exclude_count;    // Number of exclude directories
    char **extensions;    // Array of allowed extensions (-e option)
    int ext_count;        // Number of allowed extensions
    int time_limit;       // Time limit in seconds (-t option)
} Options;

// Function prototypes
void cmd_process(char *input);
void cmd_help(void);
void cmd_exit(void);
void cmd_arrange(char *path);   // Subroutine for "arrange" command

void cmd_tree(char *path, int opt_size, int opt_permission);
FileNode *create_node(const char *path, const char *name, int is_root, int is_last);
FileNode *get_root(const char *abs_path, const char *origin_path);
void build_tree(FileNode *parent);
void print_tree(FileNode *node, const char *prefix, int opt_size, int opt_permission);
void print_permissions(mode_t mode);
int is_inside_home(const char *path);
void count_nodes(FileNode *node, int *dir_count, int *file_count);
int cmp(const struct dirent **a, const struct dirent **b);
int expand_tilde(const char *input_path, char *expanded_path);

void print_usage(const char *progname);
int validate_path(const char *path);
int is_directory(const char *path);
void parse_options(int argc, char *argv[], Options *opts);
void free_options(Options *opts);

ExtGroup *create_ext_group(const char *extension);
ConflictGroup *create_conflict_group(const char *filename);
void add_file_to_groups(ExtGroup **ext_list, const char *filepath, const char *filename, const char *extension, time_t mtime);
ConflictGroup *find_conflict_group(ConflictGroup *head, const char *filename);
ExtGroup *find_ext_group(ExtGroup *head, const char *extension);

void traverse_directory(const char *path, ExtGroup **ext_list, Options *opts);
int should_skip_dir(const char *dirname, Options *opts);
int is_valid_extension(const char *ext, Options *opts);

void handle_conflicts(ExtGroup *ext_list);
void copy_selected_files(ExtGroup *ext_list, const char *dest_dir);

char *get_extension(const char *filename);
char *concat_path(const char *dir, const char *file);
int copy_file(const char *src, const char *dst);

// Main function: prints prompt and processes user input
int main(void) {
    char input[MAX_USER_INPUT];
    printf("20211407> ");
    while (fgets(input, MAX_USER_INPUT, stdin)) {
        input[strcspn(input, "\n")] = '\0';
        if (strlen(input) > 0)
            cmd_process(input);
        printf("\n20211407> ");
    }
    return 0;
}

// Processes the user input command
void cmd_process(char *input) {
    // Tokenize the input using space as delimiter
    char *cmd = strtok(input, " ");
    if (!cmd) return;

    if (strcmp(cmd, "tree") == 0) {
        // Process the "tree" command
        char *path = strtok(NULL, " ");
        int on_filesize = 0, on_permission = 0;
        char *option;
        while ((option = strtok(NULL, " ")) != NULL) {
            if (strcmp(option, "-s") == 0)
                on_filesize = 1;
            else if (strcmp(option, "-p") == 0)
                on_permission = 1;
            else if (strcmp(option, "-sp") == 0)
                on_filesize = on_permission = 1;
            else {
                printf("Usage: tree <DIR_PATH> [-s] [-p]\n");
                return;
            }
        }
        if (!path) {
            printf("Usage: tree <DIR_PATH> [-s] [-p]\n");
            return;
        }
        cmd_tree(path, on_filesize, on_permission);
    }
    else if (strcmp(cmd, "arrange") == 0) {
        // Process the "arrange" command
        char *path = strtok(NULL, " ");
        if (!path) {
            printf("Usage : arrange <DIR_PATH> [OPTION]\n");
            return;
        }
        // Call the arrange subroutine with the directory path
        cmd_arrange(path);
    }
    else if (strcmp(cmd, "help") == 0) {
        cmd_help();
    }
    else if (strcmp(cmd, "exit") == 0) {
        cmd_exit();
    }
    else {
        cmd_help();
    }
}

// Prints help message for available commands
void cmd_help(void) {
   // Check for a specific command argument
    char *arg = strtok(NULL, " ");
    if(arg != NULL){
        if(strcmp(arg, "tree") == 0){
            printf("Usage: tree <DIR_PATH> [-s] [-p]\n"); // Help for tree
            return;
        }
        else if(strcmp(arg,"arrange") == 0){
            printf("Usage: arrange <DIR_PATH> [OPTION]\n"); // Help for arrange
            return;
        }
    }
    printf("Usage:\n");
    printf(" > tree <DIR_PATH> [OPTION]...\n");
    printf("  <none> : Display the directory structure recursively if <DIR_PATH> is a directoryn");
    printf("  -s : Display the directory structure recursively if <DIR_PATH> is a directory, including the size of each file\n");
    printf("  -p : Display the directory structure recursively if <DIR_PATH> is a directory, including the permissions of each directory and file\n");
    printf(" > arrange <DIR_PATH> [OPTION]...\n");
    printf("  <none> : Arrange the directory if <DIR_PATH> is a directory\n");
    printf("  -d <output_path> : Specify the output directory <output_path> where <DIR_PATH> will be arranged if <DIR_PATH> is a directory\n");
    printf("  -t <seconds> : Only arrange files that were modified more than <seconds> seconds ago\n");
    printf("  -x <exclude_path1, exclude_path2, ...> : Arrange the directory if <DIR_PATH> is a directory except for the files inside <exclude_path> directory\n");
    printf("  -e <extension1, extension2, ...> : Arrange the directory with the specified extension <extension1, extension2, ...>\n");
    printf(" > help [COMMAND]\n");
    printf(" > exit\n");
}

// Exits the program
void cmd_exit(void) {
    exit(0);
}

// Subroutine for "arrange" command: parses options, traverses directory, handles conflicts, and copies files
void cmd_arrange(char *path)
{
    // Build a temporary argv array: argv[0] is the target directory, followed by optional arguments.
    char *argv_arr[50];
    int argc = 0;
    argv_arr[argc++] = path;
    char resolved[MAX_PATH];
    char *token;
    while ((token = strtok(NULL, " ")) != NULL) {
        argv_arr[argc++] = token;
    }
   
    Options opts;
    memset(&opts, 0, sizeof(Options));
    parse_options(argc, argv_arr, &opts);

    // Validate the target directory path and check if it is a directory.
    if(!realpath(path, resolved)){
        printf("%s does not exist\n", path);
        free_options(&opts);
        return;
    }
    if(!is_directory(opts.dir_path)) {
        printf("%s is not a directory\n", opts.dir_path);
        free_options(&opts);
        return;
    }
    // Set the default result directory if not provided: <DIR_PATH>_arranged
    char default_result[MAX_FILENAME];
    if (!opts.result_dir) {
        snprintf(default_result, sizeof(default_result), "%s_arranged", opts.dir_path);
        opts.result_dir = default_result;
    }
   
    // Traverse the directory and group files by extension with conflict grouping.
    ExtGroup *ext_list = NULL;
    traverse_directory(opts.dir_path, &ext_list, &opts);
   
    // Handle conflicts: interactively resolve files with the same name using diff/vi.
    handle_conflicts(ext_list);
   
    // Copy the selected files into the result directory.
    copy_selected_files(ext_list, opts.result_dir);
   
    free_options(&opts);
    // Note: Freeing ext_list and related dynamic memory should be implemented as needed.

    // complete
    printf("%s arranged\n", path);
}

// Processes the "tree" command to display the directory structure recursively.
void cmd_tree(char *path, int opt_size, int opt_permission) {
    char real_path[MAX_PATH];
    struct stat statbuf;

    // Expand '~' to HOME if needed.
    char expanded_path[MAX_PATH];
    if (expand_tilde(path, expanded_path)) {
        path = expanded_path;
    }

    // Resolve the real path.
    if (!realpath(path, real_path)) {
        //fprintf(stderr, "%s does not exist\n", path);
        fprintf(stderr, "Usage : tree <DIR_PATH> [OPTION]...\n");
        return;
    }
    // Check if the path is inside HOME.
    if (!is_inside_home(real_path)) {
        fprintf(stderr, "%s is outside the home directory\n",path);
        return;
    }
    // Check if the path is a directory.
    if (lstat(real_path, &statbuf) == -1) {
        perror("lstat");
        return;
    }
    if (!S_ISDIR(statbuf.st_mode)) {
        fprintf(stderr, "%s is not a directory.\n", path);
        return;
    }
    // Build the file tree.
    FileNode *root = get_root(real_path, path);
    if (!root) {
        fprintf(stderr, "Error: cannot build tree.\n");
        return;
    }
    // Print the file tree.
    print_tree(root, "", opt_size, opt_permission);

    // Count directories and files (excluding the root) and print counts.
    int dir_count = 0, file_count = 0;
    count_nodes(root, &dir_count, &file_count);
    printf("\n%d directories, %d files\n", dir_count + 1, file_count);
}

// Expands '~' at the beginning of the input path to the HOME directory.
int expand_tilde(const char *input_path, char *expanded_path) {
    if (input_path[0] == '~') {
        char *home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "Error: HOME not set.\n");
            return 0;
        }
        // If path is "~" or "~/something", prepend home.
        if (input_path[1] == '/' || input_path[1] == '\0') {
            snprintf(expanded_path, MAX_PATH, "%s%s", home, input_path + 1);
            return 1;
        }
    }
    return 0;
}

// Allocates and initializes a new FileNode with the given path and name.
FileNode *create_node(const char *path, const char *name, int is_root, int is_last) {
    FileNode *node = malloc(sizeof(FileNode));
    if (!node) {
        fprintf(stderr, "Error: malloc failed.\n");
        exit(1);
    }
    strncpy(node->name, name, MAX_FILENAME);
    strncpy(node->path, path, MAX_PATH);
    lstat(path, &node->st);
    node->is_dir = S_ISDIR(node->st.st_mode);
    node->child = NULL;
    node->sibling = NULL;
    node->is_root = is_root;
    node->is_last = is_last;
    return node;
}

// Recursively builds the tree by scanning the directory using scandir and sorting entries.
void build_tree(FileNode *parent) {
    if (!parent->is_dir) return;
    struct dirent **namelist = NULL;
    int n = scandir(parent->path, &namelist, NULL, cmp);
    if (n < 0) {
        fprintf(stderr, "opendir failed: %s\n", parent->path);
        return;
    }
    FileNode *first_child = NULL;
    FileNode *prev = NULL;
    for (int i = 0; i < n; i++) {
        struct dirent *entry = namelist[i];
        if (!entry) continue;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            free(entry);
            continue;
        }
        if (strlen(entry->d_name) > MAX_FILENAME) {
            fprintf(stderr, "Error: Too long file name.\n");
            free(entry);
            continue;
        }
        char child_path[MAX_PATH];
        int len = snprintf(child_path, sizeof(child_path), "%s/%s", parent->path, entry->d_name);
        if (len < 0 || len >= (int)sizeof(child_path)) {
            fprintf(stderr, "Error: Too long path.\n");
            free(entry);
            continue;
        }
        FileNode *child = create_node(child_path, entry->d_name, 0, 0);
        if (!first_child)
            first_child = child;
        if (prev)
            prev->sibling = child;
        prev = child;
        free(entry);
    }
    free(namelist);
    if (prev)
        prev->is_last = 1;
    parent->child = first_child;
    // Recursively build the tree for each child.
    for (FileNode *c = parent->child; c != NULL; c = c->sibling)
        build_tree(c);
}

// Creates the root FileNode and builds the tree.
FileNode *get_root(const char *abs_path, const char *origin_path) {
    FileNode *root = create_node(abs_path, origin_path, 1, 0);
    build_tree(root);
    return root;
}

// Prints file permissions in the format [drwxr-xr-x].
void print_permissions(mode_t mode) {
    printf("[");
    printf((S_ISDIR(mode)) ? "d" : "-");
    printf((mode & S_IRUSR) ? "r" : "-");
    printf((mode & S_IWUSR) ? "w" : "-");
    printf((mode & S_IXUSR) ? "x" : "-");
    printf((mode & S_IRGRP) ? "r" : "-");
    printf((mode & S_IWGRP) ? "w" : "-");
    printf((mode & S_IXGRP) ? "x" : "-");
    printf((mode & S_IROTH) ? "r" : "-");
    printf((mode & S_IWOTH) ? "w" : "-");
    printf((mode & S_IXOTH) ? "x" : "-");
    printf("]");
}

// Checks whether the given path is within the user's HOME directory.
int is_inside_home(const char *path) {
    char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Error: HOME not set.\n");
        return 0;
    }
    return strncmp(path, home, strlen(home)) == 0;
}

// Recursively prints the tree structure with optional file size and permissions.
void print_tree(FileNode *node, const char *prefix, int opt_size, int opt_permission) {
    if (!node) return;
    if (node->is_root) {
        // Print the root exactly as given by the user.
        printf("%s\n", node->name);
    } else {
        printf("%s", prefix);
        printf("%s", node->is_last ? "└─ " : "├─ ");
        if (opt_permission) {
            print_permissions(node->st.st_mode);
            printf(" ");
        }
        if (opt_size) {
            printf("[%ld] ", node->st.st_size);
        }
        printf("%s", node->name);
        if (node->is_dir)
            printf("/");
        printf("\n");
    }
    char new_prefix[MAX_PATH];
    if (node->is_root)
        strcpy(new_prefix, "");
    else {
        strcpy(new_prefix, prefix);
        if (node->is_last)
            strcat(new_prefix, "   ");
        else
            strcat(new_prefix, "│   ");
    }
    for (FileNode *child = node->child; child != NULL; child = child->sibling)
        print_tree(child, new_prefix, opt_size, opt_permission);
}

// Recursively counts directories and files (excluding the root node).
void count_nodes(FileNode *node, int *dir_count, int *file_count) {
    if (!node) return;
    if (!node->is_root) {
        if (node->is_dir)
            (*dir_count)++;
        else
            (*file_count)++;
    }
    for (FileNode *c = node->child; c; c = c->sibling)
        count_nodes(c, dir_count, file_count);
}

// Custom comparator for scandir to sort directory entries alphabetically.
int cmp(const struct dirent **a, const struct dirent **b) {
    return strcmp((*a)->d_name, (*b)->d_name);
}

// Parses the command-line-like arguments for the arrange command.
// argv[0] is the target directory; subsequent tokens are options (-d, -x, -e, -t).
void parse_options(int argc, char *argv[], Options *opts) {
    opts->dir_path = argv[0];
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            opts->result_dir = argv[++i];
        } else if (strcmp(argv[i], "-x") == 0 && i + 1 < argc) {
            char *token = strtok(argv[++i], ",");
            while (token) {
                opts->exclude_dirs = realloc(opts->exclude_dirs, sizeof(char*) * (opts->exclude_count + 1));
                opts->exclude_dirs[opts->exclude_count] = strdup(token);
                opts->exclude_count++;
                token = strtok(NULL, ",");
            }
        } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            char *token = strtok(argv[++i], ",");
            while (token) {
                opts->extensions = realloc(opts->extensions, sizeof(char*) * (opts->ext_count + 1));
                opts->extensions[opts->ext_count] = strdup(token);
                opts->ext_count++;
                token = strtok(NULL, ",");
            }
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            opts->time_limit = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unknown option or missing argument: %s\n", argv[i]);
        }
    }
}

// Frees any dynamically allocated memory in the Options structure.
void free_options(Options *opts) {
    if (opts->exclude_dirs) {
        for (int i = 0; i < opts->exclude_count; i++) {
            free(opts->exclude_dirs[i]);
        }
        free(opts->exclude_dirs);
        opts->exclude_dirs = NULL;
    }
    if (opts->extensions) {
        for (int i = 0; i < opts->ext_count; i++) {
            free(opts->extensions[i]);
        }
        free(opts->extensions);
        opts->extensions = NULL;
    }
}

// Creates a new ExtGroup node for a given file extension.
ExtGroup *create_ext_group(const char *extension) {
    ExtGroup *group = (ExtGroup *)calloc(1, sizeof(ExtGroup));
    if (!group) return NULL;
    group->extension = strdup(extension);
    group->conflicts = NULL;
    group->next = NULL;
    return group;
}

// Creates a new ConflictGroup node for a given filename.
ConflictGroup *create_conflict_group(const char *filename) {
    ConflictGroup *cg = (ConflictGroup *)calloc(1, sizeof(ConflictGroup));
    if (!cg) return NULL;
    cg->filename = strdup(filename);
    cg->count = 0;
    cg->capacity = 0;
    cg->files = NULL;
    cg->next = NULL;
    return cg;
}

// Searches for an ExtGroup with the specified extension.
ExtGroup *find_ext_group(ExtGroup *head, const char *extension) {
    ExtGroup *cur = head;
    while (cur) {
        if (strcmp(cur->extension, extension) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

// Searches for a ConflictGroup with the specified filename.
ConflictGroup *find_conflict_group(ConflictGroup *head, const char *filename) {
    ConflictGroup *cur = head;
    while (cur) {
        if (strcmp(cur->filename, filename) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

// Adds a file (with its path, name, and modification time) to a ConflictGroup.
static void add_file_to_conflict_group(ConflictGroup *cg, const char *filepath, const char *filename, time_t mtime) {
    if (cg->count == cg->capacity) {
        int new_cap = (cg->capacity == 0) ? 4 : (cg->capacity * 2);
        cg->files = realloc(cg->files, sizeof(ConflictFile*) * new_cap);
        cg->capacity = new_cap;
    }
    ConflictFile *cf = (ConflictFile *)malloc(sizeof(ConflictFile));
    cf->filepath = strdup(filepath);
    cf->filename = strdup(filename);
    cf->mtime = mtime;
    cg->files[cg->count++] = cf;
}

// Adds a file into an ExtGroup based on its extension and organizes conflicts by filename.
void add_file_to_groups(ExtGroup **ext_list, const char *filepath, const char *filename, const char *extension, time_t mtime) {
    ExtGroup *eg = find_ext_group(*ext_list, extension);
    if (!eg) {
        eg = create_ext_group(extension);
        eg->next = *ext_list;
        *ext_list = eg;
    }
    ConflictGroup *cg = find_conflict_group(eg->conflicts, filename);
    if (!cg) {
        cg = create_conflict_group(filename);
        cg->next = eg->conflicts;
        eg->conflicts = cg;
    }
    add_file_to_conflict_group(cg, filepath, filename, mtime);
}

// Recursively traverses the directory tree starting at 'path' and groups files by extension according to Options.
void traverse_directory(const char *path, ExtGroup **ext_list, Options *opts) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "opendir error: %s\n", path);
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char *fullpath = concat_path(path, entry->d_name);
        if (!fullpath) {
            fprintf(stderr, "concat_path error\n");
            continue;
        }
        struct stat st;
        if (stat(fullpath, &st) < 0) {
            free(fullpath);
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            if (!should_skip_dir(entry->d_name, opts))
                traverse_directory(fullpath, ext_list, opts);
        } else if (S_ISREG(st.st_mode)) {
            if (opts->time_limit > 0) {
                time_t now = time(NULL);
                if (difftime(now, st.st_mtime) > opts->time_limit) {
                    free(fullpath);
                    continue;
                }
            }
            char *ext = get_extension(entry->d_name);
            if (opts->ext_count > 0) {
                if (!is_valid_extension(ext, opts)) {
                    free(fullpath);
                    continue;
                }
            }
            add_file_to_groups(ext_list, fullpath, entry->d_name, ext, st.st_mtime);
        }
        free(fullpath);
    }
    closedir(dir);
}

// Checks if a directory should be skipped based on the -x option.
int should_skip_dir(const char *dirname, Options *opts) {
    for (int i = 0; i < opts->exclude_count; i++) {
        if (strcmp(dirname, opts->exclude_dirs[i]) == 0)
            return 1;
    }
    return 0;
}

// Checks if the file's extension is allowed based on the -e option.
int is_valid_extension(const char *ext, Options *opts) {
    for (int i = 0; i < opts->ext_count; i++) {
        if (strcmp(ext, opts->extensions[i]) == 0)
            return 1;
    }
    return 0;
}

// Returns the file extension (substring after the last '.'); returns empty string if none.
char *get_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";
    return (char *)(dot + 1);
}

// Concatenates a directory and file name into a full path; returns a newly allocated string.
char *concat_path(const char *dir, const char *file) {
    char buf[MAX_PATH];
    if (snprintf(buf, sizeof(buf), "%s/%s", dir, file) >= (int)sizeof(buf))
        return NULL;
    return strdup(buf);
}

// Copies a file from src to dst using low-level I/O; returns 0 on success, -1 on error.
int copy_file(const char *src, const char *dst) {
    int fd_src = open(src, O_RDONLY);
    if (fd_src < 0) {
        perror("open src");
        return -1;
    }
    int fd_dst = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_dst < 0) {
        perror("open dst");
        close(fd_src);
        return -1;
    }
    char buffer[4096];
    ssize_t nread;
    while ((nread = read(fd_src, buffer, sizeof(buffer))) > 0) {
        write(fd_dst, buffer, nread);
    }
    close(fd_src);
    close(fd_dst);
    return 0;
}

// Handles conflicts when multiple files with the same name exist.
// For each conflict group, prompts the user to choose an option:
// 1) select [num]: keep only the specified file
// 2) diff [num1] [num2]: compare two files using diff (fork/execvp)
// 3) vi [num]: open the file in vi (fork/execvp)
// 4) do not select: ignore all files in this group
void handle_conflicts(ExtGroup *ext_list) {
    ExtGroup *eg = ext_list;
    while (eg) {
        ConflictGroup *cg = eg->conflicts;
        while (cg) {
            if (cg->count > 1) {
                // Print each conflicting file with numbering starting at 1.
                for (int i = 0; i < cg->count; i++) {
                    printf("%d. %s\n", i + 1, cg->files[i]->filepath);
                }
                int resolved = 0;
                while (!resolved) {
                    printf("\nchoose an option:\n");
                    printf("0. select [num]\n");
                    printf("1. diff [num1] [num2]\n");
                    printf("2. vi [num]\n");
                    printf("3. do not select\n");
                    printf("20211407> ");
                    char input[128];
                    if (!fgets(input, sizeof(input), stdin)) {
                        break;
                    }
                    char cmd[16];
                    int num = 0, num2 = 0;
                    cmd[0] = '\0';
                    int tokens = sscanf(input, "%s %d %d", cmd, &num, &num2);
                    if (strcmp(cmd, "select") == 0 && tokens >= 2) {
                        if (num > 0 && num <= cg->count) {
                            for (int i = 0; i < cg->count; i++) {
                                if (i != (num - 1)) {
                                    free(cg->files[i]->filepath);
                                    free(cg->files[i]->filename);
                                    cg->files[i]->filepath = NULL;
                                    cg->files[i]->filename = NULL;
                                }
                            }
                            resolved = 1;
                        } else {
                            printf("Invalid file number.\n");
                        }
                    }
                    else if (strcmp(cmd, "diff") == 0 && tokens >= 3) {
                        if (num > 0 && num2 > 0 && num <= cg->count && num2 <= cg->count) {
                            pid_t pid = fork();
                            if (pid == 0) {
                                char *args[] = {"diff", cg->files[num - 1]->filepath, cg->files[num2 - 1]->filepath, NULL};
                                execvp("diff", args);
                                perror("execvp diff");
                                exit(EXIT_FAILURE);
                            } else if (pid > 0) {
                                int status;
                                waitpid(pid, &status, 0);
                            } else {
                                perror("fork");
                            }
                        } else {
                            printf("Invalid file number(s) for diff.\n");
                        }
                    }
                    else if (strcmp(cmd, "vi") == 0 && tokens >= 2) {
                        if (num > 0 && num <= cg->count) {
                            pid_t pid = fork();
                            if (pid == 0) {
                                char *args[] = {"vi", cg->files[num - 1]->filepath, NULL};
                                execvp("vi", args);
                                perror("execvp vi");
                                exit(EXIT_FAILURE);
                            } else if (pid > 0) {
                                int status;
                                waitpid(pid, &status, 0);
                            } else {
                                perror("fork");
                            }
                        } else {
                            printf("Invalid file number for vi.\n");
                        }
                    }
                    else if (strcmp(cmd, "do") == 0 || strcmp(cmd, "do not") == 0) {
                        resolved = 1;
                        for (int i = 0; i < cg->count; i++) {
                            free(cg->files[i]->filepath);
                            free(cg->files[i]->filename);
                            cg->files[i]->filepath = NULL;
                            cg->files[i]->filename = NULL;
                        }
                    }
                    else {
                        printf("Invalid command.\n");
                    }
                }
            }
            cg = cg->next;
        }
        eg = eg->next;
    }
}

// Creates the result directory (and subdirectories for each extension) and copies selected files.
void copy_selected_files(ExtGroup *ext_list, const char *dest_dir) {
    if (mkdir(dest_dir, 0755) < 0) {
        if (errno != EEXIST) {
            perror("mkdir dest_dir");
            return;
        }
    }
    ExtGroup *eg = ext_list;
    while (eg) {
        char ext_dir[MAX_PATH];
        snprintf(ext_dir, sizeof(ext_dir), "%s/%s", dest_dir, eg->extension);
        if (mkdir(ext_dir, 0755) < 0) {
            if (errno != EEXIST) {
                perror("mkdir ext_dir");
                eg = eg->next;
                continue;
            }
        }
        ConflictGroup *cg = eg->conflicts;
        while (cg) {
            for (int i = 0; i < cg->count; i++) {
                ConflictFile *cf = cg->files[i];
                if (!cf->filepath)
                    continue;
                char *dst_path = concat_path(ext_dir, cf->filename);
                if (!dst_path)
                    continue;
                copy_file(cf->filepath, dst_path);
                free(dst_path);
            }
            cg = cg->next;
        }
        eg = eg->next;
    }
}

// Checks whether the given path is valid (e.g., not too long).
int validate_path(const char *path) {
    if (!path) return -1;
    if (strlen(path) >= MAX_PATH) {
        fprintf(stderr, "Error: path is too long.\n");
        return -1;
    }
    return 0;
}

// Checks if the given path refers to a directory.
int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        perror("stat");
        return 0;
    }
    return S_ISDIR(st.st_mode);
}
