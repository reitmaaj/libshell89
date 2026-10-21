#ifndef SHELL89_FAULT_H
#define SHELL89_FAULT_H

#include "shell89_test.h"

void shell89_test_fault_reset(void);
void shell89_test_fault_arm(int site, unsigned long nth);
void shell89_test_fault_disarm(void);
int shell89_test_fault_should_fail(int site);
unsigned long shell89_test_fault_calls(int site);
int shell89_test_fault_fired(void);

#endif
