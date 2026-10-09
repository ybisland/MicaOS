#include <micaos/common/assert.h>

#if ASSERT_DEBUG
__NO_RETURN __WEAK void on_assert_failure(const char *expr,
                                          const char *file,
                                          int line)
{
    /* Weak default assert handler: stop here and wait for debugger/watchdog. */
    (void)expr;
    (void)file;
    (void)line;

    for (;;)
    {
        compiler_barrier();
    }
}
#endif
