#ifndef ASSERT_H
#define ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "compiler.h"

/*
 * Assertion helpers for embedded C modules.
 *
 * Usage:
 *   1. Use static_assert() or BUILD_BUG_ON_MSG() for compile-time checks such
 *      as structure size, alignment, and configuration constraints.
 *   2. Use BUILD_BUG_ON() or BUILD_BUG_ON_ZERO() when a compile-time check must
 *      appear inside an expression or legacy macro.
 *   3. Use ASSERT() for run-time programming errors such as invalid arguments,
 *      corrupted internal links, or impossible states.
 *
 * Design notes:
 *   - Requires C99 or later with GNU extensions.
 *     Recommended: C11 for better static_assert messages.
 *   - ASSERT() is controlled by ASSERT_DEBUG. By default it is enabled unless
 *     NDEBUG is defined.
 *   - In release builds, run-time assertions are disabled.
 *   - The default on_assert_failure() handler is weak, so projects may override
 *     it to log, reset, break into a debugger, or enter a fault state.
 *
 * Examples:
 *   static_assert(sizeof(void *) == 4, "this target must be 32-bit");
 *   BUILD_BUG_ON_MSG(sizeof(uint32_t) != 4, "uint32_t must be 4 bytes");
 *
 *   ASSERT(ptr != NULL);
 */

/* --------------------------------------------------------------------------
 * Compile-time assertions
 * -------------------------------------------------------------------------- */
#ifdef __cplusplus
/* C++11 and later provide static_assert as a keyword. */
#else
# ifndef static_assert
#  if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#   define static_assert(cond, msg) _Static_assert((cond), msg)
#  else
/*
 * C99 fallback: fail compilation by declaring a typedef with a negative array
 * size. The message is kept for source readability, but C99 compilers usually
 * report the generated typedef name instead of msg.
 */
#   ifndef ASSERT_CONCAT_
#    define ASSERT_CONCAT_(a, b) a##b
#   endif
#   ifndef ASSERT_CONCAT
#    define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)
#   endif
#   ifndef ASSERT_UNIQUE_NAME
#    ifdef __COUNTER__
#     define ASSERT_UNIQUE_NAME(prefix) ASSERT_CONCAT(prefix, __COUNTER__)
#    else
#     define ASSERT_UNIQUE_NAME(prefix) ASSERT_CONCAT(prefix, __LINE__)
#    endif
#   endif
#   define static_assert(cond, msg)                                             \
    typedef char ASSERT_UNIQUE_NAME(static_assertion_failed_)                   \
        [(cond) ? 1 : -1] __UNUSED
#  endif
# endif
#endif

/*
 * BUILD_BUG_ON - 编译期断言（条件为真时编译失败）
 *
 * 原理：当 cond 为真时，数组大小为 char[-1]，引发编译错误。
 *       当 cond 为假时，数组大小为 char[1]，正常编译。
 *
 * 用途：在编译期检查某个条件不应该成立。若条件违反，立即中止编译。
 *
 * 使用用例：
 *   // 检查整数大小必须是 4 字节
 *   BUILD_BUG_ON(sizeof(int) != 4);
 *
 *   // 检查指针必须是 8 字节（64 位系统）
 *   BUILD_BUG_ON(sizeof(void *) != 8);
 *
 *   // 检查结构体大小符合预期
 *   struct dma_desc { int a; int b; };
 *   BUILD_BUG_ON(sizeof(struct dma_desc) != 8);
 *
 *   // 检查配置宏的合法值
 *   #define MAX_RETRIES 0
 *   BUILD_BUG_ON(MAX_RETRIES < 1);  // 编译失败
 */
#ifndef BUILD_BUG_ON
# define BUILD_BUG_ON(cond) ((void)sizeof(char[1 - 2 * !!(cond)]))
#endif

/*
 * BUILD_BUG_ON_MSG - 编译期断言（带自定义错误消息）
 *
 * 原理：使用 C11 的 static_assert 进行编译期检查。
 *       若条件为真，编译失败并显示自定义错误消息。
 *
 * 用途：在编译期检查条件，失败时显示友好的错误信息。
 *       相比 BUILD_BUG_ON，能够提供更清晰的诊断消息。
 *
 * 使用用例：
 *   // 检查结构体对齐要求
 *   struct uart_regs { / * 32 个字节 * / };
 *   BUILD_BUG_ON_MSG(sizeof(struct uart_regs) != 0x20,
 *                    "uart_regs must be exactly 32 bytes");
 *
 *   // 检查缓冲区大小配置
 *   #define RING_BUFFER_SIZE 128
 *   BUILD_BUG_ON_MSG(RING_BUFFER_SIZE < 256,
 *                    "RING_BUFFER_SIZE must be at least 256 bytes");
 *
 *   // 检查硬件定时器频率设置
 *   #define TIMER_FREQ_HZ 1000
 *   BUILD_BUG_ON_MSG(TIMER_FREQ_HZ > 100000,
 *                    "TIMER_FREQ_HZ exceeds maximum frequency");
 */
#ifndef BUILD_BUG_ON_MSG
# define BUILD_BUG_ON_MSG(cond, msg) static_assert(!(cond), msg)
#endif

/*
 * BUILD_BUG_ON_ZERO - 编译期断言（可作为表达式使用）
 *
 * 原理：与 BUILD_BUG_ON 原理相同，但返回表达式值 0。
 *       当条件为假时，整个表达式的值为 (1 - 1) = 0。
 *
 * 用途：在需要表达式结果的地方进行编译期检查。
 *       可用于数组初始化、宏定义、条件表达式等场景。
 *
 * 使用用例：
 *   // 在数组声明中检查条件
 *   int arr[256 + BUILD_BUG_ON_ZERO(sizeof(int) != 4)];
 *   // 若 sizeof(int) == 4，数组大小为 256 + 0 = 256
 *   // 若 sizeof(int) != 4，编译失败
 *
 *   // 在结构体初始化中检查
 *   struct config cfg = {
 *       .version = 1,
 *       .flags = BUILD_BUG_ON_ZERO(MAX_NODES > 1024),
 *   };
 *
 *   // 在宏定义中检查参数范围
 *   #define SET_TIMEOUT(ms) \\
 *       do { set_timeout_impl(ms + BUILD_BUG_ON_ZERO((ms) < 10)); } while(0)
 *   // 若 ms >= 10，则正常展开为 set_timeout_impl(ms + 0)
 *   // 若 ms < 10，编译失败
 *
 *   // 检查编译时常量的有效性
 *   #define CLOCK_DIVIDER 8
 *   int freq = 1000000 / (CLOCK_DIVIDER + BUILD_BUG_ON_ZERO(CLOCK_DIVIDER == 0));
 */
#ifndef BUILD_BUG_ON_ZERO
# define BUILD_BUG_ON_ZERO(cond) ((int)sizeof(char[1 - 2 * !!(cond)]) - 1)
#endif

/* --------------------------------------------------------------------------
 * Run-time assertions
 * -------------------------------------------------------------------------- */

#ifndef ASSERT_DEBUG
# ifdef NDEBUG
#  define ASSERT_DEBUG 0
# else
#  define ASSERT_DEBUG 1
# endif
#endif

#if ASSERT_DEBUG
# ifndef ASSERT
__NO_RETURN __WEAK void on_assert_failure(const char *expr,
                                          const char *file,
                                          int line);
#  define ASSERT(cond)                                          \
    do {                                                        \
        if (_UNLIKELY(!(cond))) {                               \
            on_assert_failure(#cond, __FILE__, (int)__LINE__);  \
        }                                                       \
    } while (0)
# endif
#else
# ifndef ASSERT
#  define ASSERT(cond)      ((void)sizeof(cond))
# endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* ASSERT_H */
