# MicaOS 测试模块

第一阶段测试模块用于验证不依赖内核调度的基础模块，例如 `dlist`、`slist`、`bitmap`、`bytebuf`、`packetbuf` 和 `slab`。

## 编译文件

当前测试骨架包含：

```text
tests/test.c
tests/test_all.c
tests/suites/test_dlist.c
```

被测模块需要按需加入编译。当前 `test_dlist.c` 只依赖 header-only 的 `dlist.h`，不需要额外编译 dlist 源文件。

## 输出接口

测试 runner 不绑定 `printf` 或 UART。工程需要提供：

```c
void test_output(const char *s)
{
    while (*s != '\0') {
        uart_putc(*s++);
    }
}
```

也可以覆盖：

```c
void test_on_fail(void)
{
    __BKPT(0);
}
```

如果没有提供这两个函数，默认 weak 实现不会输出，也不会停机。

## 最小入口

测试工程可以这样调用：

```c
#include "tests/test.h"
#include "tests/test_all.h"

int main(void)
{
    board_init();

    test_runner_init();
    test_all_run();
    (void)test_runner_summary();

    for (;;) {
    }
}
```

后续每增加一个测试套件，就在 `tests/suites/test_suites.h` 声明入口，并在 `tests/test_all.c` 中调用。
