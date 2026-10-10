#include "tests/test.h"

#include <micaos/common/crc.h>

static const uint8_t test_crc_input[] = "123456789";

static void test_crc32_standard_check_values(void)
{
    TEST_EQ_U32(0xCBF43926U,
                crc32_ieee(CRC32_IEEE_INITIAL_VALUE,
                           test_crc_input,
                           sizeof(test_crc_input) - 1U));
    TEST_EQ_U32(0x0376E6E7U,
                crc32_mpeg2(CRC32_MPEG2_INITIAL_VALUE,
                            test_crc_input,
                            sizeof(test_crc_input) - 1U));
}

static void test_crc16_standard_check_values(void)
{
    TEST_EQ_U32(0xBB3DU,
                crc16_ibm(CRC16_IBM_INITIAL_VALUE,
                          test_crc_input,
                          sizeof(test_crc_input) - 1U));
    TEST_EQ_U32(0x4B37U,
                crc16_modbus(CRC16_MODBUS_INITIAL_VALUE,
                             test_crc_input,
                             sizeof(test_crc_input) - 1U));
    TEST_EQ_U32(0x29B1U,
                crc16_ccitt_false(CRC16_CCITT_FALSE_INITIAL_VALUE,
                                  test_crc_input,
                                  sizeof(test_crc_input) - 1U));
    TEST_EQ_U32(0x906EU,
                crc16_x25(CRC16_X25_INITIAL_VALUE,
                          test_crc_input,
                          sizeof(test_crc_input) - 1U));
}

static void test_crc_update_supports_segmented_input(void)
{
    const size_t split = 4U;
    uint32_t crc = crc32_ieee(CRC32_IEEE_INITIAL_VALUE,
                              test_crc_input,
                              split);

    crc = crc32_ieee(crc,
                     test_crc_input + split,
                     (sizeof(test_crc_input) - 1U) - split);

    TEST_EQ_U32(0xCBF43926U, crc);
}

void test_crc_run(void)
{
    TEST_RUN(test_crc32_standard_check_values);
    TEST_RUN(test_crc16_standard_check_values);
    TEST_RUN(test_crc_update_supports_segmented_input);
}
