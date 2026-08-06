#ifndef TEST_H
#define TEST_H

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*test_case_func_t)(void);

void test_runner_init(void);
void test_run_case(const char *name, test_case_func_t func);
uint32_t test_runner_summary(void);
uint32_t test_runner_total(void);
uint32_t test_runner_failed(void);

void test_fail(const char *expr, const char *file, int line);
void test_fail_eq_u32(const char *expected_expr,
                      const char *actual_expr,
                      uint32_t expected,
                      uint32_t actual,
                      const char *file,
                      int line);
void test_fail_eq_size(const char *expected_expr,
                       const char *actual_expr,
                       size_t expected,
                       size_t actual,
                       const char *file,
                       int line);
void test_fail_eq_ptr(const char *expected_expr,
                      const char *actual_expr,
                      const void *expected,
                      const void *actual,
                      const char *file,
                      int line);

#define TEST_RUN(fn) test_run_case(#fn, (fn))

#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            test_fail(#cond, __FILE__, (int)__LINE__);                      \
        }                                                                   \
    } while (0)

#define TEST_REQUIRE(cond)                                                  \
    do {                                                                    \
        if (!(cond)) {                                                      \
            test_fail(#cond, __FILE__, (int)__LINE__);                      \
            return;                                                         \
        }                                                                   \
    } while (0)

#define TEST_EQ_U32(expected, actual)                                       \
    do {                                                                    \
        uint32_t test_expected_ = (uint32_t)(expected);                     \
        uint32_t test_actual_ = (uint32_t)(actual);                         \
        if (test_expected_ != test_actual_) {                               \
            test_fail_eq_u32(#expected,                                     \
                             #actual,                                       \
                             test_expected_,                                \
                             test_actual_,                                  \
                             __FILE__,                                      \
                             (int)__LINE__);                                \
        }                                                                   \
    } while (0)

#define TEST_EQ_SIZE(expected, actual)                                      \
    do {                                                                    \
        size_t test_expected_ = (size_t)(expected);                         \
        size_t test_actual_ = (size_t)(actual);                             \
        if (test_expected_ != test_actual_) {                               \
            test_fail_eq_size(#expected,                                    \
                              #actual,                                      \
                              test_expected_,                               \
                              test_actual_,                                 \
                              __FILE__,                                     \
                              (int)__LINE__);                               \
        }                                                                   \
    } while (0)

#define TEST_EQ_PTR(expected, actual)                                       \
    do {                                                                    \
        const void *test_expected_ = (const void *)(expected);              \
        const void *test_actual_ = (const void *)(actual);                  \
        if (test_expected_ != test_actual_) {                               \
            test_fail_eq_ptr(#expected,                                     \
                             #actual,                                       \
                             test_expected_,                                \
                             test_actual_,                                  \
                             __FILE__,                                      \
                             (int)__LINE__);                                \
        }                                                                   \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* TEST_H */
