#include <string.h>

#include "shell89_fault.h"

static int armed_site = 0;
static unsigned long armed_nth = 0;
static unsigned long call_count[32];
static int fired = 0;

void shell89_test_fault_reset(void)
{
    armed_site = 0;
    armed_nth = 0;
    fired = 0;
    memset(call_count, 0, sizeof call_count);
}

void shell89_test_fault_arm(int site, unsigned long nth)
{
    armed_site = site;
    armed_nth = nth;
    fired = 0;
    if (site >= 0 && site < 32)
    {
        call_count[site] = 0;
    }
}

void shell89_test_fault_disarm(void)
{
    armed_site = 0;
    armed_nth = 0;
}

int shell89_test_fault_should_fail(int site)
{
    if (site < 0 || site >= 32)
    {
        return 0;
    }
    call_count[site]++;
    if (site != armed_site || armed_nth == 0)
    {
        return 0;
    }
    if (call_count[site] == armed_nth)
    {
        fired = 1;
        armed_site = 0;
        return 1;
    }
    return 0;
}

unsigned long shell89_test_fault_calls(int site)
{
    if (site < 0 || site >= 32)
    {
        return 0;
    }
    return call_count[site];
}

int shell89_test_fault_fired(void)
{
    return fired;
}
