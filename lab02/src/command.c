#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "header.h"
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>

/* Internal function */
static void command_process(const char *str)
{
    char buffer[MAX_COMMAND];
    strncpy(buffer, str, sizeof(buffer));
    buffer[sizeof(buffer)-1] = '\0';

    char *cmd = strtok(buffer, " \t\n");
    if (!cmd) return;

    const char *p = str + strlen(cmd);
    while (*p == ' ' || *p == '\t') p++;
    const char *args = *p ? p : NULL;

    if (strcmp(cmd, "show") == 0) {
        command_show();
    }
    else if (strcmp(cmd, "add") == 0) {
        command_add(args);
    }
    else if (strcmp(cmd, "modify") == 0) {
        command_modify(args);
    }
    else if (strcmp(cmd, "remove") == 0) {
        command_remove(args);
    }
    else if (strcmp(cmd, "help") == 0) {
        command_help();
    }
    else if (strcmp(cmd, "exit") == 0) {
        exit(0);
    }
    else {
        command_help();
    }
}

/* Command Loop */
void command_loop(void)
{
    char input[MAX_COMMAND];

    printf("20211407> ");
    while(fgets(input, MAX_COMMAND, stdin)){
        input[strcspn(input, "\n")] = '\0';
        if(strlen(input) > 0) command_process(input);
        
        printf("\n20211407> ");
    }

    return;
}

/* Command */

// 1. help
// print usage
void command_help() {
    printf("Usage:\n");
    printf("  > show\n");
    printf("    <none> : show monitoring daemon process info\n\n");
    printf("  > add <DIR_PATH> [OPTION]...\n");
    printf("    <none> : add daemon process monitoring the <DIR_PATH> directory\n");
    printf("    -d <OUTPUT_PATH> : Specify the output directory where <DIR_PATH> will be arranged\n");
    printf("    -i <TIME_INTERVAL> : Set the time interval (in seconds) for the daemon process monitor\n");
    printf("    -l <MAX_LOG_LINES> : Set the maximum number of log lines\n");
    printf("    -x <EXCLUDE_PATH1,EXCLUDE_PATH2,...> : Exclude directories\n");
    printf("    -e <EXTENSION1,EXTENSION2,...> : Specify file extensions for organization\n");
    printf("    -m <M> : Specify the duplicate file handling mode (1~3)\n\n");
    printf("  > modify <DIR_PATH> [OPTION]...\n");
    printf("    <none> : modify daemon process config\n\n");
    printf("  > remove <DIR_PATH>\n");
    printf("    <none> : remove daemon process monitoring the <DIR_PATH> directory\n\n");
    printf("  > help\n");
    printf("    Display this help information\n\n");
    printf("  > exit\n");
    printf("    Exit the program\n");
}

// 2. add
// --- Helper functions for validation ---
static int path_exists_and_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int has_read_access(const char *path) {
    return access(path, R_OK) == 0;
}

static const char* get_home_dir(void) {
    struct passwd *pw = getpwuid(getuid());
    return pw ? pw->pw_dir : NULL;
}

static int is_within_home(const char *path) {
    char resolved[PATH_MAX];
    if (!realpath(path, resolved)) return 0;
    const char *home = get_home_dir();
    size_t len = strlen(home);
    return strncmp(resolved, home, len) == 0;
}

static int is_subpath(const char *parent, const char *child) {
    char rp[PATH_MAX], rc[PATH_MAX];
    if (!realpath(parent, rp) || !realpath(child, rc)) return 0;
    size_t lp = strlen(rp);
    return strncmp(rp, rc, lp) == 0 && (rc[lp] == '/' || rc[lp] == '\0');
}

int command_add(const char *args) {
    // settind
    char *buf = strdup(args);
    if (!buf) return -1;
    char *tok = strtok(buf, " \t");
    if (!tok) {
        fprintf(stderr, "Usage: add <DIR_PATH> [OPTIONS]\n");
        free(buf);
        return -1;
    }

    // raw inputs
    char src_raw[PATH_MAX];            
    char output_raw[PATH_MAX] = "";  
    long interval = 10;               // time_interval
    size_t max_logs = 10;              // max_log_lines
    int mode = 1;                     // mode
    // exclude paths: collect multiple
    char **excl_list = NULL;
    int excl_count = 0;
    // extensions comma-separated
    char *extensions = NULL;

    strncpy(src_raw, tok, PATH_MAX);

    // options
    while ((tok = strtok(NULL, " \t")) != NULL) {
        if (tok[0] != '-' || strlen(tok) != 2) {
            fprintf(stderr, "Unknown option or token: %s\n", tok);
            free(buf);
            return -1;
        }
        char opt = tok[1];
        if (opt == 'd') {
            char *arg = strtok(NULL, " \t");
            if (!arg){
                fprintf(stderr, "Option -d requires a path\n");
                free(buf);
                return -1;
            }
            // cannot be subpath of src (will set after src realpath)
            strncpy(output_raw, arg, PATH_MAX);
        }
        else if (opt == 'i') {
            char *arg = strtok(NULL, " \t");
            if (!arg || !isdigit(arg[0])) {
                fprintf(stderr, "Option -i requires a natural number\n");
                free(buf);
                return -1;
            }
            interval = atol(arg);
            if (interval <= 0) {
                fprintf(stderr, "Invalid interval: %s\n", arg);
                free(buf);
                return -1; 
            }
        }
        else if (opt == 'l') {
            char *arg = strtok(NULL, " \t");
            if (!arg || !isdigit(arg[0])) {
                fprintf(stderr, "Option -l requires a natural number\n");
                free(buf); 
                return -1;
            }
            max_logs = (size_t)atoi(arg);
        }
        else if (opt == 'm') {
            char *arg = strtok(NULL, " \t");
            if (!arg || !isdigit(arg[0])) {
                fprintf(stderr, "Option -m requires a number 1-3\n");
                free(buf);
                return -1;
            }
            mode = atoi(arg);
            if (mode < 1 || mode > 3) {
                fprintf(stderr, "Mode must be 1,2 or 3\n");
                free(buf);
                return -1;
            }
        }
         else if (opt == 'x') {
            char *arg;
            while ((arg = strtok(NULL, " \t")) != NULL && !(arg[0] == '-' && strlen(arg) >= 2)) {
                if (!path_exists_and_dir(arg) || !has_read_access(arg)) {
                    fprintf(stderr, "Error: %s is not accessible directory\n", arg);
                    free(buf);
                    return -1;
                }
                excl_list = realloc(excl_list, sizeof(char*) * (excl_count + 1));
                excl_list[excl_count++] = strdup(arg);
            }
            if (arg && arg[0] == '-' && strlen(arg) >= 2) {
                size_t len = strlen(arg);
                memmove(buf, arg, len + 1);
            }
        }
        else if (opt == 'e') {
            char *arg = strtok(NULL, " \t");
            if (!arg) { fprintf(stderr, "Option -e requires extensions\n"); free(buf); return -1; }
            extensions = strdup(arg);
        }
        else {
            fprintf(stderr, "Unknown option -%c\n", opt);
            free(buf); return -1;
        }
    }
    free(buf);

    // source path process
    char real_src[PATH_MAX];
    if (!realpath(src_raw, real_src)) {
        fprintf(stderr, "Error: %s is not valid path\n", src_raw);
        return -1;
    }
    if (!path_exists_and_dir(real_src) || !has_read_access(real_src)) {
        fprintf(stderr, "Error: %s is not accessible directory\n", real_src);
        return -1;
    }
    if (!is_within_home(real_src)) {
        printf("%s is outside the home directory\n", real_src);
        return -1;
    }
    // already monitored or parent/subdir of monitored
    if (daemon_is_monitored(real_src)) {
        fprintf(stderr, "Error: %s is already monitored\n", real_src);
        return -1;
    }
    char **mon_paths; int mon_count;
    mon_paths = daemon_list_all_paths(&mon_count);
    for (int i = 0; i < mon_count; i++) {
        if (is_subpath(mon_paths[i], real_src) || is_subpath(real_src, mon_paths[i])) {
            fprintf(stderr, "Error: %s conflicts with monitored path %s\n", real_src, mon_paths[i]);
            return -1;
        }
    }

    // output path
char parent[PATH_MAX], resolved_parent[PATH_MAX], real_out[PATH_MAX];
strncpy(parent, output_raw[0] ? output_raw : real_src, sizeof(parent));
dirname(parent);
if (!realpath(parent, resolved_parent)) {
    perror("realpath(parent)");
    return -1;
}
if (!is_within_home(resolved_parent) || is_subpath(real_src, resolved_parent)) {
    fprintf(stderr, "Error: output path %s is invalid\n", resolved_parent);
    return -1;
}
if (output_raw[0]) {
    const char *base = strrchr(output_raw, '/');
    base = base ? base + 1 : output_raw;
    snprintf(real_out, sizeof(real_out), "%s/%s", resolved_parent, base);
} else {
    snprintf(real_out, sizeof(real_out), "%s_arranged", real_src);
}
    if (mkdir(real_out, 0755) < 0 && errno != EEXIST) {
        perror("mkdir(real_out)");
        return -1;
    }

    // exclusive path
    for (int i = 0; i < excl_count; i++) {
        char exrp[PATH_MAX]; realpath(excl_list[i], exrp);
        if (!is_subpath(real_src, exrp)) {
            fprintf(stderr, "Error: exclude path %s is not under %s\n", excl_list[i], real_src);
            return -1;
        }
        for (int j = i+1; j < excl_count; j++) {
            char er2[PATH_MAX]; realpath(excl_list[j], er2);
            if (is_subpath(exrp, er2) || is_subpath(er2, exrp)) {
                fprintf(stderr, "Error: exclude paths %s and %s overlap\n", excl_list[i], excl_list[j]);
                return -1;
            }
        }
    }

    // make config
    char conf_path[PATH_MAX];
    snprintf(conf_path, sizeof(conf_path), "%s/ssu_cleanupd.config", real_src);
    if (access(conf_path, F_OK) != 0) {
        if (write_config_file(conf_path,
                              real_src,
                              real_out,
                              interval,
                              max_logs,
                              excl_list,
                              excl_count,
                              extensions,
                              mode) < 0) {
            fprintf(stderr, "Failed to write config\n");
            return -1;
        }
    }

    // create log
    char log_path[PATH_MAX];
    snprintf (log_path,sizeof(log_path),"%s/ssu_cleanupd.log", real_src);
    FILE *fp = fopen(log_path, "a");
    if(fp) fclose(fp);

    // create daemon
    if (daemon_add(real_src,
                   real_out,
                   conf_path,
                   interval,
                   max_logs,
                   excl_list,
                   excl_count,
                   extensions,
                   mode) < 0) {
        fprintf(stderr, "Failed to start daemon\n");
        return -1;
    }

    return 0;
}

int command_show(void) {
    daemon_t **list;
    int count = daemon_list_all(&list);
    while (1) {
        printf("Current working daemon process list\n\n");
        printf("0. exit\n");
        for (int i = 0; i < count; i++) {
            printf("%d. %s\n", i + 1, list[i]->monitoring_path);
        }
        
        printf("\nSelect one to see process info : ");
        char buf[32];
        if (!fgets(buf, sizeof(buf), stdin)) return 0;
        int sel = atoi(buf);
        if (sel == 0) {
            return 0;
        }
        if (sel < 0 || sel > count) {
            printf("Please check your input is valid\n\n");
            continue;
        }
        
        // valid selection
        daemon_t *d = list[sel - 1];
        parse_config(d->config_file, &d->cfg);
        
        // Config detail
        printf("\n1. config detail\n\n");
        printf("monitoring_path : %s\n", d->monitoring_path);
        printf("pid : %d\n", d->pid);
        printf("start_time : %s\n", d->cfg.start_time);
        printf("output_path : %s\n", d->output_path);
        printf("time_interval : %ld\n", d->cfg.time_interval);
        if (d->cfg.max_log_lines > 0)
            printf("max_log_lines : %zu\n", d->cfg.max_log_lines);
        else
            printf("max_log_lines : none\n");
        if (d->cfg.exclude_count > 0) {
            printf("exclude_path : ");
            for (int j = 0; j < d->cfg.exclude_count; j++) {
                printf("%s%s", d->cfg.exclude_paths[j], j < d->cfg.exclude_count - 1 ? "," : "");
            }
            printf("\n");
        } else {
            printf("exclude_path : none\n");
        }
        printf("extension : %s\n", *d->cfg.extensions ? d->cfg.extensions : "all");
        printf("mode : %d\n", d->cfg.mode);
        // Log detail
        printf("\n2. log detail\n\n");
        char log_path[MAX_PATH];
        snprintf(log_path, sizeof(log_path), "%s/ssu_cleanupd.log", d->monitoring_path);
        FILE *lf = fopen(log_path, "r");
        if (lf) {
            char line[MAX_PATH * 2];
            while (fgets(line, sizeof(line), lf)) {
                printf("%s", line);
            }
            fclose(lf);
        } else {
            printf("\n");
        }
        return 0;
    }
}


// Modify (reload) daemon
int command_modify(const char *args) {
    if (!args) {
        fprintf(stderr, "Usage: modify <DIR_PATH> [OPTION]...\n");
        return -1;
    }

    // 복사 버퍼 확보
    char *buf = strdup(args);
    if (!buf) return -1;

    // 첫 번째 토큰: 모니터링할 디렉토리
    char *tok = strtok(buf, " \t");
    if (!tok) {
        fprintf(stderr, "Usage: modify <DIR_PATH> [OPTION]...\n");
        free(buf);
        return -1;
    }
    char src_raw[PATH_MAX];
    strncpy(src_raw, tok, PATH_MAX);

    // 절대경로 변환 및 검증
    char real_src[PATH_MAX];
    if (!realpath(src_raw, real_src)) {
        fprintf(stderr, "Error: %s is not valid path\n", src_raw);
        free(buf);
        return -1;
    }

    // 대상 데몬 찾기
    daemon_t **list;
    int count = daemon_list_all(&list);
    int idx = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i]->monitoring_path, real_src) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        fprintf(stderr, "No daemon watching %s\n", real_src);
        free(buf);
        return -1;
    }

    // 기존 config 로드
    daemon_t *d = list[idx];
    parse_config(d->config_file, &d->cfg);

    // 옵션 파싱: add와 동일하게 구현
    char output_raw[PATH_MAX] = "";
    long interval       = d->cfg.time_interval;
    size_t max_logs     = d->cfg.max_log_lines;
    int mode            = d->cfg.mode;
    char **excl_list    = d->cfg.exclude_paths;
    int excl_count      = d->cfg.exclude_count;
    char *extensions    = strdup(d->cfg.extensions);

    while ((tok = strtok(NULL, " \t")) != NULL) {
        char opt = tok[1];
        char *arg = strtok(NULL, " \t");
        switch (opt) {
            case 'd':
                if (!arg) {
                    fprintf(stderr,"-d requires a path\n");
                    free(buf);
                    return -1;
                }
                strncpy(output_raw, arg, PATH_MAX);
                break;
            case 'i':
                if (!arg || !isdigit(arg[0])) {
                    fprintf(stderr,"-i requires a number\n"); 
                    free(buf);
                    return -1;
                }
                interval = atol(arg);
                break;
            case 'l':
                if (!arg || !isdigit(arg[0])) {
                    fprintf(stderr,"-l requires a number\n");
                    free(buf);
                    return -1;
                }
                max_logs = atoi(arg);
                break;
            case 'x':
                // 기존 리스트 해제 후 재생성
                for (int j=0; j<excl_count; j++) free(excl_list[j]);
                free(excl_list); excl_list = NULL; excl_count = 0;
                while (arg && arg[0] != '-') {
                    excl_list = realloc(excl_list, sizeof(char*)*(excl_count+1));
                    excl_list[excl_count++] = strdup(arg);
                    arg = strtok(NULL, " \t");
                }
                break;
            case 'e':
                free(extensions);
                extensions = strdup(arg ? arg : "");
                break;
            case 'm':
                if (!arg || !isdigit(arg[0])) {
                    fprintf(stderr,"-m requires 1-3\n"); 
                    free(buf);
                    return -1;
                }
                mode = atoi(arg);
                break;
            default:
                fprintf(stderr, "Unknown option -%c\n", opt);
                free(buf);
                return -1;
        }
    }

    // config 파일에 덮어쓰기
    if (write_config_file(d->config_file,
                          real_src,
                          output_raw[0] ? output_raw : d->output_path,
                          interval,
                          max_logs,
                          excl_list,
                          excl_count,
                          extensions,
                          mode) < 0) {
        fprintf(stderr, "Failed to write config\n");
        free(buf);
        return -1;
    }

    // SIGHUP 전송
    if (daemon_reload(d->pid) == 0) {
        printf("Reload signal sent to PID %d\n", d->pid);
    } else {
        fprintf(stderr, "Failed to reload PID %d\n", d->pid);
    }

    free(buf);
    return 0;
}

// Remove daemon
int command_remove(const char *args) {
    if (!args) {
        fprintf(stderr, "Usage: remove <DIR_PATH>\n");
        return -1;
    }
    
    char real_src[PATH_MAX];
    if (!realpath(args, real_src)) {
        fprintf(stderr, "Error: %s is not a valid path\n", args);
        return -1;
    }
    
    daemon_t **list;
    int count = daemon_list_all(&list);
    pid_t target_pid = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i]->monitoring_path, real_src) == 0) {
            target_pid = list[i]->pid;
            break;
        }
    }
    if (target_pid == 0) {
        fprintf(stderr, "No daemon watching %s\n", real_src);
        return -1;
    }
    
    if (daemon_remove(target_pid) == 0) {
        printf("Daemon watching %s removed (PID %d)\n", real_src, target_pid);
        return 0;
    } else {
        fprintf(stderr, "Failed to remove daemon PID %d\n", target_pid);
        return -1;
    }
}



// Write configuration file
int write_config_file(const char *path,
                      const char *monitoring_path,
                      const char *output_path,
                      long time_interval,
                      size_t max_log_lines,
                      char **exclude_paths,
                      int exclude_count,
                      const char *extensions,
                      int mode)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char tbuf[20];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm);
    fprintf(fp, "monitoring_path : %s\n", monitoring_path);
    fprintf(fp, "pid             : %d\n", getpid());
    fprintf(fp, "start_time      : %s\n", tbuf);
    fprintf(fp, "output_path     : %s\n", output_path);
    fprintf(fp, "time_interval   : %ld\n", time_interval);
    if (max_log_lines > 0)
        fprintf(fp, "max_log_lines   : %zu\n", max_log_lines);
    else
        fprintf(fp, "max_log_lines   : none\n");
    if (exclude_paths && *exclude_paths) {
        fprintf(fp, "exclude_path    : ");
        for (int i = 0; exclude_paths[i]; i++)
            fprintf(fp, "%s%s", exclude_paths[i], exclude_paths[i+1] ? "," : "\n");
    } else {
        fprintf(fp, "exclude_path    : none\n");
    }
    fprintf(fp, "extension       : %s\n", extensions && *extensions ? extensions : "all");
    fprintf(fp, "mode            : %d\n", mode);
    fclose(fp);
    return 0;
}
