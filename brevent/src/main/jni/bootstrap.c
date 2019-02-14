#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <paths.h>
#include <libgen.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#define PROJECT "https://github.com/liudongmiao/Brevent/issues"

#if defined(__LP64__)
#define APP_PROCESS "/system/bin/app_process64"
#else
#define APP_PROCESS "/system/bin/app_process32"
#endif

static int bootstrap() {
    char *arg[] = {APP_PROCESS, "/system/bin", "--nice-name=brevent_server",
                   "me.piebridge.brevent.loader.Brevent", NULL};

    return execv(arg[0], arg);
}

static void feedback() {
    printf("if you find any issues, please report bug to " PROJECT " with log\n"
                   "for crash log: logcat -b crash -d\n"
                   "for brevent log: logcat -b main -d -s BreventLoader BreventServer\n");
    fflush(stdout);
}

static void report(time_t now) {
    char command[BUFSIZ];
    char time[BUFSIZ];
    struct tm *tm = localtime(&now);
    strftime(time, sizeof(time), "%m-%d %H:%M:%S.000", tm);
    printf("please report bug to " PROJECT " with log below\n"
                   "--- crash start ---\n");
    fflush(stdout);
    sprintf(command, "logcat -b crash -t '%s' -d", time);
    printf(">>> %s\n", command);
    fflush(stdout);
    system(command);
    fflush(stdout);
    printf("--- crash end ---\n");
    printf("--- brevent start ---\n");
    fflush(stdout);
    sprintf(command, "logcat -b main -t '%s' -d -s BreventLoader BreventServer", time);
    printf(">>> %s\n", command);
    fflush(stdout);
    system(command);
    fflush(stdout);
    printf("--- brevent end ---\n");
    printf("cannot listen port, please report bug to " PROJECT " with log above\n");
    fflush(stdout);
}

static int get_pid() {
    int pid = 0;
    DIR *proc;
    struct dirent *entry;

    if (!(proc = opendir("/proc"))) {
        return pid;
    };

    while ((entry = readdir(proc))) {
        int id;
        FILE *fp;
        char buf[PATH_MAX];

        if (!(id = atoi(entry->d_name))) {
            continue;
        }
        sprintf(buf, "/proc/%u/cmdline", id);
        fp = fopen(buf, "r");
        if (fp != NULL) {
            fgets(buf, PATH_MAX - 1, fp);
            fclose(fp);
            if (!strcasecmp(buf, "brevent_server")) {
                pid = id;
                break;
            }
        }
    }
    closedir(proc);
    return pid;
}

static int check(time_t now) {
    int pid = 0;
    printf("checking..");
    for (int i = 0; i < 10; ++i) {
        int id = get_pid();
        if (pid == 0 && id > 0) {
            printf("brevent_server started, pid: %d\n", id);
            printf("checking for stable.");
            i = 0;
            pid = id;
        } else if (pid > 0 && id == 0) {
            printf("quited\n\n");
            fflush(stdout);
            report(now);
            return EXIT_FAILURE;
        }
        printf(".");
        fflush(stdout);
        sleep(1);
    }
    if (pid > 0) {
        printf("ok\n\n");
        fflush(stdout);
        feedback();
        return EXIT_SUCCESS;
    } else {
        printf("cannot listen port\n");
        fflush(stdout);
        report(now);
        return EXIT_FAILURE;
    }
}

static void check_original() {
    int pid;
    if ((pid = get_pid()) > 0) {
        printf("found old brevent_server, pid: %d, killing\n", pid);
        kill(pid, SIGTERM);
        for (int i = 0; i < 3; ++i) {
            if (get_pid() > 0) {
                sleep(1);
                kill(pid, SIGKILL);
            } else {
                return;
            }
        }
        printf("cannot kill original brevent_server, pid: %d\n", pid);
        exit(EPERM);
    }
}

int main(int argc, char **argv) {
    int fd;
    char classpath[0x1000];
    struct timeval tv;
    time_t now;

    check_original();

    sprintf(classpath, "CLASSPATH=%s/%s", dirname(argv[0]), "libloader.so");
    putenv(classpath);

    gettimeofday(&tv, NULL);
    switch (fork()) {
        case -1:
            perror("cannot fork");
            return -EPERM;
        case 0:
            break;
        default:
            now = tv.tv_sec;
            _exit(check(now));
    }

    if (setsid() == -1) {
        perror("cannot setsid");
        return -EPERM;
    }

    chdir("/");

    if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) {
            close(fd);
        }
    }

    return bootstrap();
}
