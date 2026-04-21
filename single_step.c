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

// single_step.c: demonstrate usage of SINGLESTEP ptrace operation

#define _GNU_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/personality.h>
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

static void child(void) {
    ptrace(PTRACE_TRACEME, 0, 0, 0);

    // Stop this process. This serves to notify the parent, which shoud be
    // wait()ing for us, that we're ready to receive ptrace operations;
    // additionally, these operations require the process to be stopped anyway.
    if (kill(getpid(), SIGSTOP) < 0) {
        DIE("kill");
    }

    // here, we should have been stopped and continued already.
    printf("child done.\n");
}

static void print_child_rip(pid_t child_pid) {
    struct user_regs_struct regs;
    if (ptrace(PTRACE_GETREGS, child_pid, NULL, &regs) < 0) {
        ERR("ptrace GETREGS");
    } else {
        printf("child RIP = 0x%llx\n", regs.rip);
        errno = 0;
        long next_instr = ptrace(PTRACE_PEEKDATA, child_pid, regs.rip, NULL);
        // next_instr could be a negative number, so check errno as well
        if (next_instr == -1 && errno != 0) {
            ERR("ptrace PEEKDATA");
        } else {
            printf("next instructions = 0x%lx\n", next_instr);
        }
    }
}

static void parent(pid_t child_pid) {
    // Don't die on errors on the parent, except on waitpid():
    // even on error, we want to wait for the child to finish

    int wstatus;
    // wait for child to send SIGSTOP to itself.
    if (waitpid(child_pid, &wstatus, 0) < 0) {
        DIE("waitpid");
    }

    print_child_rip(child_pid);

    if (ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0) < 0) {
        ERR("ptrace SINGLESTEP");
    } else {
        if (waitpid(child_pid, &wstatus, 0) < 0) {
            DIE("waitpid");
        }
        if (!WIFSTOPPED(wstatus)) {
            fprintf(stderr, "child not stopped when it should be\n");
        }
    }

    print_child_rip(child_pid);

    if (ptrace(PTRACE_CONT, child_pid, 0, 0) < 0) {
        ERR("ptrace CONT");
    }

    if (waitpid(child_pid, &wstatus, 0) < 0) {
        DIE("waitpid");
    }

    if (!WIFEXITED(wstatus)) {
        fprintf(stderr, "child didn't exit normally: ");
        if (WIFSIGNALED(wstatus)) {
            fprintf(stderr, "got signal SIG%s",
                sigabbrev_np(WTERMSIG(wstatus)));
        } else if (WIFSTOPPED(wstatus)) {
            fprintf(stderr, "child stopped by signal SIG%s",
                sigabbrev_np(WSTOPSIG(wstatus)));
        }
        fprintf(stderr, "\n");
    }
}

int main(int argc, char **argv) {
    pid_t pid = fork();
    if (pid < 0) {
        DIE("fork");
    } else if (pid == 0) {
        child();
    } else {
        parent(pid);
    }
    return 0;
}
