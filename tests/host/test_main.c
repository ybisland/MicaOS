#include <stdio.h>

#include "tests/test.h"
#include "tests/test_all.h"

void test_output(const char *s)
{
    fputs(s, stdout);
}

int main(void)
{
    test_runner_init();
    test_all_run();
    return (int)test_runner_summary();
}
