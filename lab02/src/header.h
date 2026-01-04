#ifndef HEADER_H
#define HEADER_H

#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <libgen.h>

// Maximum path length
#define MAX_PATH 4096
#define MAX_COMMAND 1000

// Daemon configuration structure
typedef struct {
    char   *monitoring_path;   // Monitored directory path
    pid_t   pid;               // Daemon PID
    char   *start_time;        // Daemon start time string
    char   *output_path;       // Output directory path
    long    time_interval;     // Monitoring interval (seconds)
    size_t  max_log_lines;     // Maximum log lines (0 = unlimited)
    char  **exclude_paths;     // List of paths to exclude
    int     exclude_count;     // Number of exclude paths
    char   *extensions;        // Comma-separated extensions to include
    int     ext_count;         // Number of extensions
    int     mode;              // Duplicate handling mode
} daemon_config_t;

// Daemon metadata
typedef struct {
    pid_t            pid;               // Daemon PID
    char            *monitoring_path;   // Monitored directory
    char            *output_path;       // Output directory
    char            *config_file;       // Path to config file
    daemon_config_t  cfg;               // Current configuration
} daemon_t;

// command.c
int command_add(const char *args);
int command_show(void);
int command_modify(const char *args);
int command_remove(const char *args);
void command_help(void);
void command_loop(void);

// daemon.c
int daemon_add(const char *monitor_path,
               const char *output_path,
               const char *config_file,
               long interval,
               size_t max_logs,
               char **exclude_paths,
               int exclude_count,
               const char *extensions,
               int mode);
int daemon_remove(pid_t pid);
int daemon_reload(pid_t pid);
int daemon_list_all(daemon_t ***out_list);
char **daemon_list_all_paths(int *out_count);
int daemon_is_monitored(const char *path);
int parse_config(const char *config_file, daemon_config_t *cfg);
int log_event(const daemon_config_t *cfg,
              const char *src_path,
              const char *dst_path);
int write_config_file(const char *path,
                      const char *monitoring_path,
                      const char *output_path,
                      long time_interval,
                      size_t max_log_lines,
                      char **exclude_paths,
                      int exclude_count,
                      const char *extensions,
                      int mode);

// arrange.c
void arrange_directory(const char *src,
                       const char *dst,
                       char **exclude_paths,
                       int exclude_count,
                       const char *extensions,
                       int mode);

#endif // HEADER_H
