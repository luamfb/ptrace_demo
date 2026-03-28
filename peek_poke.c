/*
 * Copyright (C) 2026 Luana C M de F Barbosa
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#define ERR(msg)                                                        \
    do {                                                                \
        fprintf(stderr, "(errno = %s) ", strerrorname_np(errno));       \
        perror(msg);                                                    \
    } while (0)

#define DIE(msg)                                                        \
    do {                                                                \
        ERR(msg);                                                       \
        exit(EXIT_FAILURE);                                             \
    } while (0)

static void child(int pipe_writefd) {
    long var = 42;
    long *var_addr = &var;
    ptrace(PTRACE_TRACEME, 0, 0, 0);
    // transmit address of variable to parent
    if (write(pipe_writefd, &var_addr, sizeof(long*)) < 0) {
        DIE("write");
    }
    // HACK: keep the child alive for a while to make sure it'll receive
    // the SIGSTOP before dying
    sleep(5);

    // here, we should have been stopped and continued already.
    printf("child: var = %ld\n", var);
}

static bool stop_child_and_wait(pid_t child_pid) {
    int wstatus;
    if (kill(child_pid, SIGSTOP) < 0) {
        ERR("kill");
        return false;
    }
    if (waitpid(child_pid, &wstatus, 0) < 0) {
        ERR("waitpid");
        return false;
    }
    if (!WIFSTOPPED(wstatus)) {
        fprintf(stderr, "child still not stopped after sending SIGSTOP\n");
        return false;
    }
    return true;
}

static void print_child_regs(pid_t child_pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, child_pid, NULL, &regs) < 0) {
        ERR("ptrace PEEKUSER");
    } else {
        printf("a few registers from child process:\n"
            "\tRIP = %llx\n"
            "\tRSP = %llx\n"
            "\tRBP = %llx\n",
            regs.rip, regs.rsp, regs.rbp);
    }
}

static void print_child_var(pid_t child_pid, long *child_var_addr) {
    printf("child_var_addr = %p\n", child_var_addr);
    long child_var = ptrace(PTRACE_PEEKDATA,
        child_pid, child_var_addr, NULL);
    if (child_var < 0) {
        ERR("ptrace PEEKDATA");
    } else {
        printf("var obtained from PTRACE_PEEK: %ld\n", child_var);
    }
}

static void set_child_var(pid_t child_pid, long *child_var_addr, long value) {
    if (ptrace(PTRACE_POKEDATA, child_pid, child_var_addr, value) < 0) {
        ERR("ptrace POKEDATA");
    }
}

static void parent(pid_t child_pid, int pipe_readfd) {
    // Don't die on errors on the parent:
    // even on error, we want to wait for the child to finish

    long *child_var_addr = NULL;
    // read variable address from child
    if (read(pipe_readfd, &child_var_addr, sizeof(long*)) < 0) {
        ERR("read");
    }
    if (child_var_addr) {
        if (stop_child_and_wait(child_pid)) {
            print_child_regs(child_pid);
            print_child_var(child_pid, child_var_addr);
            const long NEW_VALUE = 51;
            set_child_var(child_pid, child_var_addr, NEW_VALUE);
            if (ptrace(PTRACE_CONT, child_pid, 0, 0) < 0) {
                ERR("ptrace CONT");
            }
        }
    } else {
        fprintf(stderr, "error: var_addr is still NULL after read()!\n");
    }

    int wstatus;
    if (waitpid(child_pid, &wstatus, 0) < 0) {
        DIE("waitpid");
    }
    if (!WIFEXITED(wstatus)) {
        fprintf(stderr, "child didn't exit normally\n");
    }
}

int main(int argc, char **argv) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        DIE("pipe");
    }

    pid_t pid = fork();
    if (pid < 0) {
        DIE("fork");
    } else if (pid == 0) {
        child(pipefd[1]);
    } else {
        parent(pid, pipefd[0]);
    }
    return 0;
}
