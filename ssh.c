/***********************/
/* EXECUTE SSH SESSION */
/***********************/

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define USERNAME "dgraph"
#define PASSWORD "dgraph123456"

int exec_ssh(char *hostname, char *cmd, ...) {
    /* execute a command on a remote server using SSH */
    pid_t pid;
    int status;
    int i = 0;
    char *argv[100], *cur;
    va_list ap;
    /* initialize arguments list */
    va_start(ap, cmd);
    /* build argv */
    argv[i++] = "sshpass";
    argv[i++] = "-p";
    argv[i++] = PASSWORD;
    argv[i++] = "ssh";
    argv[i++] = "-o";
    argv[i++] = "StrictHostKeyChecking no";
    argv[i++] = "-l";
    argv[i++] = USERNAME;
    argv[i++] = hostname;
    argv[i++] = cmd;
    while (cur = va_arg(ap, char *)) {
        argv[i++] = cur;
    }
    argv[i++] = NULL;
    /* execute ssh */
    if (pid = fork()) {
        /* parent */
        /*waitpid(pid, &status, 0);*/
    } else {
        /* child */
        close(0);
        execvp(argv[0], argv);
        fprintf(stderr, "Error: can't execute %s\n", argv[0]);
        exit(-1);
    }
}
