#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <utime.h>
#include <pwd.h>
#include <ctype.h>
#include "header.h"  // daemon_t, daemon_config_t, prototypes for arrange_directory, daemon_list_all, etc.

#define MAX_DAEMONS 128
static daemon_t *daemon_list[MAX_DAEMONS];
static int daemon_count = 0;
daemon_config_t *current_cfg = NULL;
bool current_first_run = true;

// Forward declarations
static void daemonize_process(void);
static void monitor_loop(daemon_t *d);
static int is_subpath(const char *parent, const char *child);
static char *trim(char *s);
static int count_ext(const char *extensions_str);

// Add a new daemon: fork, daemonize child, and register in parent
int daemon_add(const char *monitor_path,
               const char *output_path,
               const char *config_file,
               long interval,
               size_t max_logs,
               char **exclude_paths,
               int exclude_count,
               const char *extensions,
               int mode)
{
    if (daemon_count >= MAX_DAEMONS) return -1;
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid > 0) {
        // Parent: register daemon metadata
        daemon_t *d = malloc(sizeof(*d));
        if (!d) return -1;
        memset(d, 0, sizeof(*d));
        d->pid = pid;
        d->monitoring_path = strdup(monitor_path);
        d->output_path     = strdup(output_path);
        d->config_file     = strdup(config_file);
        // Initialize config
        d->cfg.time_interval = interval;
        d->cfg.max_log_lines = max_logs;
        d->cfg.exclude_paths = exclude_paths;
        d->cfg.exclude_count = exclude_count;
        d->cfg.extensions    = strdup(extensions ? extensions : "");
        d->cfg.ext_count     = count_ext(extensions);
        d->cfg.mode          = mode;
        daemon_list[daemon_count++] = d;
        return 0;
    }
    // Child: become daemon
    daemonize_process();
    // Initialize own struct
    daemon_t self;
    memset(&self, 0, sizeof(self));
    self.pid = getpid();
    self.monitoring_path = strdup(monitor_path);
    self.output_path     = strdup(output_path);
    self.config_file     = strdup(config_file);
    // Initial config load
    parse_config(self.config_file, &self.cfg);
    // Enter monitoring loop
    monitor_loop(&self);
    exit(0);
}

// Remove daemon by PID: send SIGTERM and unregister
int daemon_remove(pid_t pid) {
    for (int i = 0; i < daemon_count; i++) {
        if (daemon_list[i]->pid == pid) {
            kill(pid, SIGTERM);
            waitpid(pid, NULL, 0);
            // Free and shift list
            free(daemon_list[i]->monitoring_path);
            free(daemon_list[i]->output_path);
            free(daemon_list[i]->config_file);
            free(daemon_list[i]->cfg.extensions);
            free(daemon_list[i]);
            for (int j = i; j < daemon_count - 1; j++)
                daemon_list[j] = daemon_list[j+1];
            daemon_count--;
            return 0;
        }
    }
    return -1;
}

// Reload daemon config by PID: send SIGHUP
int daemon_reload(pid_t pid) {
    for (int i = 0; i < daemon_count; i++) {
        if (daemon_list[i]->pid == pid) {
            kill(pid, SIGHUP);
            return 0;
        }
    }
    return -1;
}

// List all registered daemon metadata
int daemon_list_all(daemon_t ***out_list) {
    *out_list = daemon_list;
    return daemon_count;
}

// List all monitored paths for conflict detection
char **daemon_list_all_paths(int *out_count) {
    if (out_count) *out_count = daemon_count;
    char **paths = malloc(sizeof(char*) * daemon_count);
    for (int i = 0; i < daemon_count; i++) {
        paths[i] = daemon_list[i]->monitoring_path;
    }
    return paths;
}

// Check if a path is already monitored (exact match)
int daemon_is_monitored(const char *path) {
    for (int i = 0; i < daemon_count; i++) {
        if (strcmp(daemon_list[i]->monitoring_path, path) == 0)
            return 1;
    }
    return 0;
}

// Core monitoring loop: parse config, arrange, log, and sleep
static void monitor_loop(daemon_t *d) {
    struct timespec interval;
    while (1) {
        parse_config(d->config_file, &d->cfg);
        d->cfg.pid = d->pid;
        d->cfg.monitoring_path = d->monitoring_path;
        d->cfg.output_path = d->output_path;
        interval.tv_sec  = d->cfg.time_interval;
        interval.tv_nsec = 0;
        current_cfg = &d->cfg;
        arrange_directory(d->monitoring_path,
                          d->output_path,
                          d->cfg.exclude_paths,
                          d->cfg.exclude_count,
                          d->cfg.extensions,
                          d->cfg.mode);
        current_first_run = false;
        nanosleep(&interval, NULL);
    }
}

// Daemonize process using double-fork method
static void daemonize_process(void) {
    pid_t sid;
    if ((sid = setsid()) < 0) exit(1);
    // restore default SIGHUP handler
    signal(SIGHUP, SIG_DFL);
    pid_t pid = fork();
    if (pid < 0) exit(1);
    if (pid > 0) exit(0);
    umask(0);
    chdir("/");
    for (int fd = 0; fd < 3; fd++) close(fd);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);
}

// Check if 'child' is subpath of 'parent'
static int is_subpath(const char *parent, const char *child) {
    char rp[PATH_MAX], rc[PATH_MAX];
    if (!realpath(parent, rp) || !realpath(child, rc)) return 0;
    size_t lp = strlen(rp);
    return strncmp(rp, rc, lp) == 0 && (rc[lp] == '/' || rc[lp] == '\0');
}

// Trim leading/trailing whitespace
static char *trim(char *s) {
    char *end;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

// Count comma-separated extensions
static int count_ext(const char *extensions_str) {
    if (!extensions_str || *extensions_str == '\0') return 0;
    int count = 0;
    char *copy = strdup(extensions_str);
    char *tok = strtok(copy, ",");
    while (tok) {
        count++;
        tok = strtok(NULL, ",");
    }
    free(copy);
    return count;
}

// Parse config file into daemon_config_t
int parse_config(const char *config_file, daemon_config_t *cfg) {
    FILE *fp = fopen(config_file, "r");
    if (!fp) return -1;
    // set defaults
    cfg->time_interval = 10;
    cfg->max_log_lines = 0;
    cfg->exclude_paths = NULL;
    cfg->exclude_count = 0;
    free(cfg->extensions);
    cfg->extensions = strdup("");
    cfg->ext_count = 0;
    cfg->mode = 1;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *p = strchr(line, ':');
        if (!p) continue;
        *p = '\0';
        char *key = trim(line);
        char *val = trim(p + 1);
        if (strcmp(key, "monitoring_path") == 0) {
            // ignored in reload
        } else if (strcmp(key, "pid") == 0) {
            cfg->pid = (pid_t)atoi(val);
        } else if (strcmp(key, "start_time") == 0) {
            free(cfg->start_time);
            cfg->start_time = strdup(val);
        } else if (strcmp(key, "output_path") == 0) {
            free(cfg->output_path);
            cfg->output_path = strdup(val);
        } else if (strcmp(key, "time_interval") == 0) {
            cfg->time_interval = atol(val);
        } else if (strcmp(key, "max_log_lines") == 0) {
            if (strcmp(val, "none") != 0) cfg->max_log_lines = (size_t)atoi(val);
        } else if (strcmp(key, "exclude_path") == 0) {
            if (strcmp(val, "none") != 0) {
                char *tok = strtok(val, ",");
                while (tok) {
                    cfg->exclude_paths = realloc(cfg->exclude_paths,
                        sizeof(char*) * (cfg->exclude_count + 1));
                    cfg->exclude_paths[cfg->exclude_count++] = strdup(trim(tok));
                    tok = strtok(NULL, ",");
                }
            }
        } else if (strcmp(key, "extension") == 0) {
            if (strcmp(val, "all") != 0) {
                free(cfg->extensions);
                cfg->extensions = strdup(val);
                cfg->ext_count = count_ext(val);
            }
        } else if (strcmp(key, "mode") == 0) {
            cfg->mode = atoi(val);
        }
    }
    fclose(fp);
    return 0;
}

// Log an event and enforce max_log_lines
int log_event(const daemon_config_t *cfg,
              const char *src_path,
              const char *dst_path)
{
    // timestamp
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    char tbuf[9];
    snprintf(tbuf,sizeof(tbuf),"%02d:%02d:%02d",lt.tm_hour,lt.tm_min,lt.tm_sec);
    
    // new entry
    char entry[PATH_MAX*2];
    snprintf(entry, sizeof(entry), "[%s] [%d] [%s] [%s]\n",
             tbuf, (int)cfg->pid, src_path, dst_path);
    // read existing
    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "%s/ssu_cleanupd.log", cfg->monitoring_path);
    FILE *fp = fopen(log_path, "r");
    char **lines = NULL; size_t count = 0;
    if (fp) {
        char buf[PATH_MAX*2];
        while (fgets(buf, sizeof(buf), fp)) {
            lines = realloc(lines, sizeof(char*)*(count+1));
            lines[count++] = strdup(buf);
        }
        fclose(fp);
    }
    lines = realloc(lines, sizeof(char*)*(count+1));
    lines[count++] = strdup(entry);
    // enforce max
    if (cfg->max_log_lines > 0 && count > cfg->max_log_lines) {
        size_t to_remove = count - cfg->max_log_lines;
        for (size_t i = 0; i < to_remove; i++) free(lines[i]);
        memmove(lines, lines + to_remove, sizeof(char*) * (count - to_remove));
        count = cfg->max_log_lines;
    }
    fp = fopen(log_path, "w");
    if (!fp) { for (size_t i=0;i<count;i++) free(lines[i]); free(lines); return -1; }
    for (size_t i=0;i<count;i++) { fputs(lines[i], fp); free(lines[i]); }
    free(lines);
    fclose(fp);
    return 0;
}
