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

// breakpoint.c: install a breakpoint in the beginning of a function

#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
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

// This instruction should raise a SIGTRAP. Since the child is being traced,
// any signal should result in a the child being stopped.
// We could add instructions for more architectures too
const long X86_BREAK_INSTR = 0xcc; // int 3

#define DEREF_AS_LONG(f) (*((long*)(f)))

static void breakpoint(void) {
    printf("child: returning from breakpoint(). Its first bytes are %lx\n",
        DEREF_AS_LONG(breakpoint));
}

static void child(void) {
    ptrace(PTRACE_TRACEME, 0, 0, 0);

    // Stop this process. This serves to notify the parent, which shoud be
    // wait()ing for us, that we're ready to receive ptrace operations;
    // additionally, these operations require the process to be stopped anyway.
    if (kill(getpid(), SIGSTOP) < 0) {
        DIE("kill");
    }

    // here, we should have been stopped and continued already.
    printf("child: calling breakpoint(). Its first bytes are %lx\n",
        DEREF_AS_LONG(breakpoint));
    // calling this function should raise SIGTRAP, which will cause this
    // process to stop again.
    breakpoint();
}

static long get_original_instr(pid_t child_pid, void *child_fn_addr) {
    long instr = ptrace(PTRACE_PEEKTEXT, child_pid, child_fn_addr, NULL);
    // the instruction could be a value that turns out to be negative
    // when cast to a long, hence we check errno as well
    if (instr == -1 && errno != 0) {
        ERR("ptrace PEEKTEXT");
    }
    return instr;
}

static void set_instr(pid_t child_pid, void *child_bp_addr, long instr) {
    if (ptrace(PTRACE_POKETEXT, child_pid, child_bp_addr, instr) < 0) {
        ERR("ptrace POKEDATA");
    }
}

static void parent(pid_t child_pid) {
    // Don't die on errors on the parent:
    // even on error, we want to wait for the child to finish

    int wstatus;
    // wait for child to send SIGSTOP to itself.
    if (waitpid(child_pid, &wstatus, 0) < 0) {
        DIE("waitpid");
    }
    long orig_instr = get_original_instr(child_pid, breakpoint);
    printf("parent: orig_instr = %lx\n", orig_instr);
    if (errno == 0) {
        // install breakpoint
        set_instr(child_pid, breakpoint, X86_BREAK_INSTR);
        if (ptrace(PTRACE_CONT, child_pid, 0, 0) < 0) {
            ERR("ptrace CONT");
        } else {
            // wait for the child to be stopped after reaching the breakpoint
            if (waitpid(child_pid, &wstatus, 0) < 0) {
                DIE("waitpid");
            }
            if (!WIFSTOPPED(wstatus)) {
                fprintf(stderr,
                    "child not stopped after reaching breakpoint!\n");
            } else {
                printf("parent: child stopped with SIG%s. Resuming soon\n",
                    sigabbrev_np(WSTOPSIG(wstatus)));
            }

            sleep(3);

            // restore original instruction and continue
            set_instr(child_pid, breakpoint, orig_instr);
            if (ptrace(PTRACE_CONT, child_pid, 0, 0) < 0) {
                ERR("ptrace CONT");
            }

        }
    }

    if (waitpid(child_pid, &wstatus, 0) < 0) {
        DIE("waitpid");
    }
    if (!WIFEXITED(wstatus)) {
        fprintf(stderr, "child didn't exit normally\n");
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
