#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <utime.h>
#include <limits.h>
#include "header.h"  // prototypes for arrange_directory, log_event

extern daemon_config_t *current_cfg;
extern bool current_first_run;

// Helper: get file extension (without dot), or empty string if none
static const char* get_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

// Helper: copy file from src to dst, return 0 on success
static int copy_file(const char *src, const char *dst, mode_t mode) {
    int infd = open(src, O_RDONLY);
    if (infd < 0) return -1;
    int outfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (outfd < 0) { close(infd); return -1; }
    char buf[8192];
    ssize_t n;
    while ((n = read(infd, buf, sizeof(buf))) > 0) {
        char *p = buf;
        while (n > 0) {
            ssize_t written = write(outfd, p, n);
            if (written < 0) { close(infd); close(outfd); return -1; }
            n -= written;
            p += written;
        }
    }
    close(infd);
    close(outfd);
    return 0;
}

// Recursive scan and arrange
static void scan_dir(const char *base_src,
                     const char *curr_src,
                     const char *dst,
                     char **exclude_paths,
                     int exclude_count,
                     char **ext_list,
                     int ext_count,
                     int all_ext,
                     int mode) {
    DIR *dir = opendir(curr_src);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, "ssu_cleanupd.config") == 0 || strcmp(entry->d_name, "ssu_cleanud.log") == 0)
            continue;
        // Build full source path
        char src_path[PATH_MAX];
        snprintf(src_path, sizeof(src_path), "%s/%s", curr_src, entry->d_name);
        struct stat st;
        if (lstat(src_path, &st) < 0) continue;
        if (S_ISDIR(st.st_mode)) {
            // Check exclude paths
            int excluded = 0;
            for (int i = 0; i < exclude_count; i++) {
                if (strncmp(src_path, exclude_paths[i], strlen(exclude_paths[i])) == 0) {
                    excluded = 1;
                    break;
                }
            }
            if (!excluded) {
                scan_dir(base_src, src_path, dst,
                         exclude_paths, exclude_count,
                         ext_list, ext_count, all_ext, mode);
            }
        }
        else if (S_ISREG(st.st_mode)) {
            // Extension filter
            const char *ext = get_extension(entry->d_name);
            int match = all_ext;
            
            const char *ext0 = get_extension(entry->d_name);
            if(strcmp(ext0,"log") == 0) continue;

            if (!all_ext) {
                for (int i = 0; i < ext_count; i++) {
                    if (strcmp(ext_list[i], ext) == 0) {
                        match = 1; break;
                    }
                }
            }
            if (!match) continue;
            // Prepare dest directory for this extension
            char ext_dirname[NAME_MAX + 1];
            if (ext[0] != '\0') snprintf(ext_dirname, sizeof(ext_dirname), "%s", ext);
            else strncpy(ext_dirname, "others", sizeof(ext_dirname));
            char dest_dir[PATH_MAX];
            snprintf(dest_dir, sizeof(dest_dir), "%s/%s", dst, ext_dirname);
            if (mkdir(dest_dir, 0755) < 0 && errno != EEXIST) continue;
            // Prepare dest file path
            char dest_path[PATH_MAX];
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name);
            // Duplicate handling
            int do_copy = 1;
            struct stat dst_st;
            if (stat(dest_path, &dst_st) == 0) {
                if (mode == 1) {
                    // Keep newest
                    if (st.st_mtime <= dst_st.st_mtime) do_copy = 0;
                } else if (mode == 2) {
                    // Keep oldest
                    if (st.st_mtime >= dst_st.st_mtime) do_copy = 0;
                } else if (mode == 3) {
                    // Skip duplicates
                    do_copy = 0;
                }
            }
            if (!do_copy) continue;
            // Copy file
            if (copy_file(src_path, dest_path, st.st_mode & 0777) < 0) continue;
            // Preserve modification time
            struct utimbuf times = { st.st_atime, st.st_mtime };
            utime(dest_path, &times);
            if(!current_first_run) log_event(current_cfg, src_path, dest_path);
        }
    }
    closedir(dir);
}

/**
 * Arrange files in 'src' into subdirs under 'dst' by extension
 */
void arrange_directory(const char *src,
                       const char *dst,
                       char **exclude_paths,
                       int exclude_count,
                       const char *extensions_str,
                       int mode) {
    // Parse extensions list
    char **ext_list = NULL;
    int ext_count = 0;
    int all_ext = 0;
    if (!extensions_str || strlen(extensions_str) == 0) {
        all_ext = 1;
    } else {
        char *exts = strdup(extensions_str);
        char *token = strtok(exts, ",");
        while (token) {
            ext_list = realloc(ext_list, sizeof(char*) * (ext_count + 1));
            ext_list[ext_count++] = strdup(token);
            token = strtok(NULL, ",");
        }
        free(exts);
        if (ext_count == 0) all_ext = 1;
    }
    // Ensure base output exists
    mkdir(dst, 0755);
    // Recursive scan and arrange
    scan_dir(src, src, dst,
             exclude_paths, exclude_count,
             ext_list, ext_count,
             all_ext, mode);
    // Clean up ext_list
    for (int i = 0; i < ext_count; i++) free(ext_list[i]);
    free(ext_list);
}
