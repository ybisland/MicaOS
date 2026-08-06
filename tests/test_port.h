#ifndef TEST_PORT_H
#define TEST_PORT_H

/*
 * Test output port.
 *
 * The test runner is platform-neutral. Projects may override test_output() to
 * send text to UART, SWO/ITM, RTT, semihosting, or a host console.
 *
 * Example for host tests:
 *
 *   #include <stdio.h>
 *
 *   void test_output(const char *s)
 *   {
 *       fputs(s, stdout);
 *   }
 *
 * Example for UART tests:
 *
 *   void test_output(const char *s)
 *   {
 *       while (*s != '\0') {
 *           uart_putc(*s++);
 *       }
 *   }
 */
void test_output(const char *s);

/*
 * Called when a test assertion fails.
 *
 * The default implementation does nothing. Projects may override it to break
 * into a debugger, toggle a GPIO, or stop after the first failure.
 */
void test_on_fail(void);

#endif /* TEST_PORT_H */
