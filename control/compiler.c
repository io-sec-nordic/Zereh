#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int zereh_compile_generated_bpf(const struct zereh_config *cfg)
{
    pid_t pid;
    int status = 0;
    char *argv[20];
    int argc = 0;

    if (!cfg) {
        return -1;
    }

    argv[argc++] = "clang";
    argv[argc++] = "-O3";
    argv[argc++] = "-g";
    argv[argc++] = "-target";
    argv[argc++] = "bpf";
    argv[argc++] = "-D__TARGET_ARCH_x86";
    argv[argc++] = "-Wall";
    argv[argc++] = "-Werror";

    if (!cfg->optimize_jump_tables) {
        argv[argc++] = "-fno-jump-tables";
    }

    argv[argc++] = "-I./xdp";
    argv[argc++] = "-I./include";
    argv[argc++] = "-I/usr/include";
#if defined(__x86_64__)
    argv[argc++] = "-I/usr/include/x86_64-linux-gnu";
#elif defined(__aarch64__)
    argv[argc++] = "-I/usr/include/aarch64-linux-gnu";
#endif
    argv[argc++] = "-c";
    argv[argc++] = (char *)cfg->generated_source;
    argv[argc++] = "-o";
    argv[argc++] = (char *)cfg->generated_object;
    argv[argc] = NULL;

    pid = fork();
    if (pid < 0) {
        perror("fork clang");
        return -1;
    }

    if (pid == 0) {
        execvp("clang", argv);
        perror("execvp clang");
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid clang");
        return -1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "clang failed with status=%d\n", status);
        return -1;
    }

    return 0;
}
