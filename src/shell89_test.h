#ifndef SHELL89_TEST_H
#define SHELL89_TEST_H

/* Test-only hooks.  They exist only in a library compiled with
 * -DSHELL89_TESTING and are never part of the installed interface. */

#define SHELL89_TEST_MALLOC 1
#define SHELL89_TEST_REALLOC 2
#define SHELL89_TEST_PIPE 3
#define SHELL89_TEST_FCNTL 4
#define SHELL89_TEST_DUP 5
#define SHELL89_TEST_POSIX_SPAWN 6
#define SHELL89_TEST_PTHREAD_CREATE 7
#define SHELL89_TEST_PTHREAD_JOIN 8
#define SHELL89_TEST_WAITPID 9
#define SHELL89_TEST_READ 10
#define SHELL89_TEST_WRITE 11
#define SHELL89_TEST_OPEN 12

void shell89_test_fault_reset(void);
void shell89_test_fault_arm(int site, unsigned long nth);
void shell89_test_fault_disarm(void);
int shell89_test_fault_should_fail(int site);

#endif
