#include "tests/test.h"
#include "tests/test_port.h"
#include "common/compiler.h"

typedef struct test_runner {
    uint32_t total;
    uint32_t failed_cases;
    uint32_t failed_assertions;
    uint32_t current_failed;
} test_runner_t;

static test_runner_t test_runner_;

__WEAK void test_output(const char *s)
{
    (void)s;
}

__WEAK void test_on_fail(void)
{
}

static void test_write_u32(uint32_t value)
{
    char buf[10];
    uint32_t pos = 0U;

    if (value == 0U) {
        test_output("0");
        return;
    }

    while (value != 0U) {
        buf[pos++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (pos != 0U) {
        char out[2];

        out[0] = buf[--pos];
        out[1] = '\0';
        test_output(out);
    }
}

static void test_write_size(size_t value)
{
    char buf[20];
    size_t pos = 0U;

    if (value == 0U) {
        test_output("0");
        return;
    }

    while (value != 0U) {
        buf[pos++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (pos != 0U) {
        char out[2];

        out[0] = buf[--pos];
        out[1] = '\0';
        test_output(out);
    }
}

static void test_write_hex_nibble(uintptr_t value)
{
    char out[2];
    const char *digits = "0123456789ABCDEF";

    out[0] = digits[value & 0xFU];
    out[1] = '\0';
    test_output(out);
}

static void test_write_ptr(const void *ptr)
{
    uintptr_t value = (uintptr_t)ptr;
    uint32_t shift = (uint32_t)(sizeof(uintptr_t) * 8U);
    bool started = false;

    test_output("0x");

    while (shift != 0U) {
        uintptr_t nibble;

        shift -= 4U;
        nibble = (value >> shift) & 0xFU;
        if ((nibble != 0U) || started || (shift == 0U)) {
            test_write_hex_nibble(nibble);
            started = true;
        }
    }
}

void test_runner_init(void)
{
    test_runner_.total = 0U;
    test_runner_.failed_cases = 0U;
    test_runner_.failed_assertions = 0U;
    test_runner_.current_failed = 0U;
    test_output("[TEST] start\n");
}

void test_run_case(const char *name, test_case_func_t func)
{
    test_runner_.total++;
    test_runner_.current_failed = 0U;

    test_output("[ RUN  ] ");
    test_output(name);
    test_output("\n");

    func();

    if (test_runner_.current_failed == 0U) {
        test_output("[ PASS ] ");
    } else {
        test_runner_.failed_cases++;
        test_output("[ FAIL ] ");
    }
    test_output(name);
    test_output("\n");
}

uint32_t test_runner_summary(void)
{
    test_output("[TEST] total=");
    test_write_u32(test_runner_.total);
    test_output(" passed=");
    test_write_u32(test_runner_.total - test_runner_.failed_cases);
    test_output(" failed_cases=");
    test_write_u32(test_runner_.failed_cases);
    test_output(" failed_assertions=");
    test_write_u32(test_runner_.failed_assertions);
    test_output("\n");

    return test_runner_.failed_cases;
}

uint32_t test_runner_total(void)
{
    return test_runner_.total;
}

uint32_t test_runner_failed(void)
{
    return test_runner_.failed_cases;
}

void test_fail(const char *expr, const char *file, int line)
{
    test_runner_.failed_assertions++;
    test_runner_.current_failed++;

    test_output("  assert failed: ");
    test_output(expr);
    test_output(" at ");
    test_output(file);
    test_output(":");
    test_write_u32((uint32_t)line);
    test_output("\n");

    test_on_fail();
}

void test_fail_eq_u32(const char *expected_expr,
                      const char *actual_expr,
                      uint32_t expected,
                      uint32_t actual,
                      const char *file,
                      int line)
{
    test_runner_.failed_assertions++;
    test_runner_.current_failed++;

    test_output("  equal failed: ");
    test_output(expected_expr);
    test_output(" != ");
    test_output(actual_expr);
    test_output(" expected=");
    test_write_u32(expected);
    test_output(" actual=");
    test_write_u32(actual);
    test_output(" at ");
    test_output(file);
    test_output(":");
    test_write_u32((uint32_t)line);
    test_output("\n");

    test_on_fail();
}

void test_fail_eq_size(const char *expected_expr,
                       const char *actual_expr,
                       size_t expected,
                       size_t actual,
                       const char *file,
                       int line)
{
    test_runner_.failed_assertions++;
    test_runner_.current_failed++;

    test_output("  equal failed: ");
    test_output(expected_expr);
    test_output(" != ");
    test_output(actual_expr);
    test_output(" expected=");
    test_write_size(expected);
    test_output(" actual=");
    test_write_size(actual);
    test_output(" at ");
    test_output(file);
    test_output(":");
    test_write_u32((uint32_t)line);
    test_output("\n");

    test_on_fail();
}

void test_fail_eq_ptr(const char *expected_expr,
                      const char *actual_expr,
                      const void *expected,
                      const void *actual,
                      const char *file,
                      int line)
{
    test_runner_.failed_assertions++;
    test_runner_.current_failed++;

    test_output("  equal failed: ");
    test_output(expected_expr);
    test_output(" != ");
    test_output(actual_expr);
    test_output(" expected=");
    test_write_ptr(expected);
    test_output(" actual=");
    test_write_ptr(actual);
    test_output(" at ");
    test_output(file);
    test_output(":");
    test_write_u32((uint32_t)line);
    test_output("\n");

    test_on_fail();
}
