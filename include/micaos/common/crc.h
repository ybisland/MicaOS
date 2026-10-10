/*
 * Copyright (c) 2026 Bin YIN
 * Copyright (c) 2018 Google LLC.
 * Copyright (c) 2018 Workaround GmbH.
 * Copyright (c) 2017 Intel Corporation.
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * CRC computation functions.
 *
 * This module provides the following CRC algorithms.  The polynomial values
 * in this table use the normal, non-reflected representation; reflected
 * implementations derive the corresponding right-shift polynomial internally.
 *
 * | API               | Algorithm          | Poly        | Init Value | XorOut     | RefIn    | RefOut   | Implementation     |
 * |-------------------|--------------------|-------------|------------|------------|----------|----------|--------------------|
 * | crc32_ieee        | CRC-32/IEEE        | 0x04C11DB7  | 0xFFFFFFFF | 0xFFFFFFFF | true     | true     | Table / bit-by-bit |
 * | crc32_mpeg2       | CRC-32/MPEG-2      | 0x04C11DB7  | 0xFFFFFFFF | 0x00000000 | false    | false    | Table / bit-by-bit |
 * | crc16_ibm         | CRC-16/IBM         | 0x8005      | 0x0000     | 0x0000     | true     | true     | Optimized          |
 * | crc16_maxim       | CRC-16/MAXIM       | 0x8005      | 0x0000     | 0xFFFF     | true     | true     | Optimized          |
 * | crc16_usb         | CRC-16/USB         | 0x8005      | 0xFFFF     | 0xFFFF     | true     | true     | Optimized          |
 * | crc16_modbus      | CRC-16/MODBUS      | 0x8005      | 0xFFFF     | 0x0000     | true     | true     | Optimized          |
 * | crc16_ccitt       | CRC-16/CCITT       | 0x1021      | 0x0000     | 0x0000     | true     | true     | Optimized          |
 * | crc16_ccitt_false | CRC-16/CCITT-FALSE | 0x1021      | 0xFFFF     | 0x0000     | false    | false    | Optimized          |
 * | crc16_x25         | CRC-16/X25         | 0x1021      | 0xFFFF     | 0xFFFF     | true     | true     | Optimized          |
 * | crc16_xmodem      | CRC-16/XMODEM      | 0x1021      | 0x0000     | 0x0000     | false    | false    | Optimized          |
 * | crc16             | Generic CRC-16     | *           | *          | *          | *        | *        | Bit-by-bit         |
 * | crc8_ccitt        | CRC-8/CCITT        | 0x07        | *          | 0x00       | false    | false    | Nibble table       |
 * | crc8              | Generic CRC-8      | *           | *          | *          | reversed | reversed | Bit-by-bit         |
 * | crc7_be           | CRC-7/MMC          | 0x09        | *          | 0x00       | false    | false    | Optimized          |
 *
 * '*': user supplied.
 *
 * The Init Value column lists the algorithm's internal initial register
 * value. For the APIs, use the corresponding *_INITIAL_VALUE macro as the
 * first CRC value. To calculate the CRC across non-contiguous blocks use the
 * return value from block N-1 as the CRC value for block N.
 * 
 * The generic CRC-16 and CRC-8 implementations support arbitrary parameters,
 * but use bit-by-bit processing and are slower than the fixed-algorithm
 * implementations.
 * 
 * CRC-32 table usage is controlled independently by `CRC32_IEEE_USE_TABLE` and
 * `CRC32_MPEG2_USE_TABLE`; each enabled 256-entry uint32_t table occupies
 * approximately 1 KB of ROM.
 */
#ifndef MICAOS_COMMON_CRC_H
#define MICAOS_COMMON_CRC_H

#include <micaos_config.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Set to 1 to use the CRC-32/IEEE lookup table (occupies 1 KB ROM), 0 by default. */
#ifndef CRC32_IEEE_USE_TABLE
#define CRC32_IEEE_USE_TABLE 0
#endif

/* Set to 1 to use the CRC-32/MPEG-2 lookup table (occupies 1 KB ROM), 0 by default. */
#ifndef CRC32_MPEG2_USE_TABLE
#define CRC32_MPEG2_USE_TABLE 0
#endif

/* CRC values(Init ^ XorOut) for starting or restarting the CRC apis. */
#define CRC32_IEEE_INITIAL_VALUE        0x00000000U /* (0xFFFFFFFFU ^ 0xFFFFFFFFU) */
#define CRC32_MPEG2_INITIAL_VALUE       0xFFFFFFFFU
#define CRC16_IBM_INITIAL_VALUE         0x0000U
#define CRC16_MAXIM_INITIAL_VALUE       0xFFFFu     /* (0x0000U ^ 0xFFFFU) */
#define CRC16_USB_INITIAL_VALUE         0x0000U     /* (0xFFFFU ^ 0xFFFFU) */
#define CRC16_MODBUS_INITIAL_VALUE      0xFFFFU
#define CRC16_CCITT_INITIAL_VALUE       0x0000U
#define CRC16_CCITT_FALSE_INITIAL_VALUE 0xFFFFU
#define CRC16_X25_INITIAL_VALUE         0x0000U     /* (0xFFFFU ^ 0xFFFFU) */
#define CRC16_XMODEM_INITIAL_VALUE      0x0000U
#define CRC8_CCITT_INITIAL_VALUE        0xFF


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Update a CRC-32/MPEG-2 checksum.
 *
 * Pass CRC32_MPEG2_INITIAL_VALUE as crc to start  a calculation. For a
 * segmented calculation, pass the return value from the previous call.
 *
 * @param crc  Current CRC state; use CRC32_MPEG2_INITIAL_VALUE to start
 *             a calculation, then pass the return value for the next block.
 * @param data Pointer to input buffer.
 * @param len  Length of buffer in bytes.
 *
 * @return Updated CRC-32 value.
 */
uint32_t crc32_mpeg2(uint32_t crc, const uint8_t *data, size_t len);

/**
 * @brief Update a CRC-32/IEEE checksum.
 *
 * Pass CRC32_IEEE_INITIAL_VALUE as crc to start a calculation. For a
 * segmented calculation, pass the return value from the previous call.
 *
 * @param crc  Current externally visible CRC state; use CRC32_IEEE_INITIAL_VALUE to
 *             start a calculation, then pass the return value for
 *             the next block.
 * @param data Pointer to input buffer.
 * @param len  Length of buffer in bytes.
 *
 * @return Updated CRC-32 value.
 */
uint32_t crc32_ieee(uint32_t crc, const uint8_t *data, size_t len);

/**
 * @brief Update a generic CRC-16 checksum.
 *
 * This is a generic bit-by-bit implementation which supports arbitrary CRC16
 * parameters. Its performance is lower than the fixed-polynomial optimized
 * implementations below. Pass the INITIAL_VALUE CRC value for the first block, then
 * pass the return value for each subsequent block.
 *
 * @param crc Current externally visible CRC state. For the first block, use
 *        Init ^ XorOut when RefIn equals RefOut; otherwise use
 *        reflect16(Init ^ XorOut).
 * @param data Input buffer
 * @param len Length of the input in bytes
 * @param polynomial The normal-form polynomial, omitting the leading x^16
 *        coefficient
 * @param xorout Value XORed with the CRC register before returning
 * @param refin If true, process input bytes and the CRC register LSB first
 * @param refout If different from refin, reflect the CRC register before
 *        applying xorout
 *
 * @return CRC16 value
 */
uint16_t crc16(uint16_t crc, const uint8_t *data, size_t len,
        uint16_t polynomial, uint16_t xorout, bool refin, bool refout);

/**
 * @brief Update a CRC-16/IBM checksum.
 *
 * Poly=0x8005, Init=0x0000, XorOut=0x0000, RefIn=true, RefOut=true.
 * Pass CRC16_IBM_INITIAL_VALUE for the first block, then pass the return value for
 * each subsequent block.
 *
 * @param crc Current CRC state; use CRC16_IBM_INITIAL_VALUE for the first block
 * @param data Input buffer
 * @param len  Length of input buffer in bytes
 *
 * @return CRC16 value
 */
uint16_t crc16_ibm(uint16_t crc, const uint8_t *data, size_t len);

/**
 * @brief Update a CRC-16/MAXIM checksum.
 *
 * Poly=0x8005, Init=0x0000, XorOut=0xFFFF, RefIn=true, RefOut=true.
 * Pass CRC16_MAXIM_INITIAL_VALUE for the first block, then pass the return value
 * for each subsequent block.
 *
 * @param crc Current CRC state; use CRC16_MAXIM_INITIAL_VALUE for the first block
 * @param data Input buffer
 * @param len  Length of input buffer in bytes
 *
 * @return CRC16 value
 */
uint16_t crc16_maxim(uint16_t crc, const uint8_t *data, size_t len);

/**
 * @brief Update a CRC-16/USB checksum.
 *
 * Poly=0x8005, Init=0xFFFF, XorOut=0xFFFF, RefIn=true, RefOut=true.
 * Pass CRC16_USB_INITIAL_VALUE for the first block, then pass the return value for
 * each subsequent block.
 *
 * @param crc Current CRC state; use CRC16_USB_INITIAL_VALUE for the first block
 * @param data Input buffer
 * @param len  Length of input buffer in bytes
 *
 * @return CRC16 value
 */
uint16_t crc16_usb(uint16_t crc, const uint8_t *data, size_t len);

/**
 * @brief Update a CRC-16/MODBUS checksum.
 *
 * Poly=0x8005, Init=0xFFFF, XorOut=0x0000, RefIn=true, RefOut=true.
 * Pass CRC16_MODBUS_INITIAL_VALUE for the first block, then pass the return value
 * for each subsequent block.
 *
 * @param crc Current CRC state; use CRC16_MODBUS_INITIAL_VALUE for the first block
 * @param data Input buffer
 * @param len  Length of input buffer in bytes
 *
 * @return CRC16 value
 */
uint16_t crc16_modbus(uint16_t crc, const uint8_t *data, size_t len);

/**
 * @brief Update a CRC-16/CCITT checksum.
 *
 * Poly=0x1021, Init=0x0000, XorOut=0x0000, RefIn=true, RefOut=true.
 * Pass CRC16_CCITT_INITIAL_VALUE for the first block, then pass the return value
 * for each subsequent block.
 *
 * @param crc Current CRC state; use CRC16_CCITT_INITIAL_VALUE for the first block
 * @param data Input buffer
 * @param len  Length of input buffer in bytes
 *
 * @return CRC16 value
 */
uint16_t crc16_ccitt(uint16_t crc, const uint8_t *data, size_t len);

/**
 * @brief Update a CRC-16/CCITT-FALSE checksum.
 *
 * Poly=0x1021, Init=0xFFFF, XorOut=0x0000, RefIn=false, RefOut=false.
 * Pass CRC16_CCITT_FALSE_INITIAL_VALUE for the first block, then pass the return
 * value for each subsequent block.
 *
 * @param crc Current CRC state; use CRC16_CCITT_FALSE_INITIAL_VALUE for the first
 *        block
 * @param data Input buffer
 * @param len  Length of input buffer in bytes
 *
 * @return CRC16 value
 */
uint16_t crc16_ccitt_false(uint16_t crc, const uint8_t *data, size_t len);

/**
 * @brief Update a CRC-16/X25 checksum.
 *
 * Poly=0x1021, Init=0xFFFF, XorOut=0xFFFF, RefIn=true, RefOut=true.
 * Pass CRC16_X25_INITIAL_VALUE for the first block, then pass the return value for
 * each subsequent block.
 *
 * @param crc Current CRC state; use CRC16_X25_INITIAL_VALUE for the first block
 * @param data Input buffer
 * @param len  Length of input buffer in bytes
 *
 * @return CRC16 value
 */
uint16_t crc16_x25(uint16_t crc, const uint8_t *data, size_t len);

/**
 * @brief Update a CRC-16/XMODEM checksum.
 *
 * Poly=0x1021, Init=0x0000, XorOut=0x0000, RefIn=false, RefOut=false.
 * Pass CRC16_XMODEM_INITIAL_VALUE for the first block, then pass the return value
 * for each subsequent block.
 *
 * @param crc Current CRC state; use CRC16_XMODEM_INITIAL_VALUE for the first block
 * @param data Input buffer
 * @param len  Length of input buffer in bytes
 *
 * @return CRC16 value
 */
uint16_t crc16_xmodem(uint16_t crc, const uint8_t *data, size_t len);

/**
 * @brief Compute a generic CRC-8 checksum of a buffer.
 *
 * This is a bit-by-bit calculation with no padding. The final result is
 * XORed with xorout. When reversed is true, polynomial must be in the
 * right-shift form expected by the reflected calculation.
 *
 * @param data Input bytes for the computation
 * @param len Length of the input in bytes
 * @param polynomial The polynomial to use, omitting the leading x^8
 *        coefficient
 * @param initial_value Initial CRC register value
 * @param xorout Value XORed with the CRC register before returning
 * @param reversed Should we use reflected/reversed values or not
 *
 * @return CRC8 value
 */
uint8_t crc8(const uint8_t *data, size_t len, uint8_t polynomial,
      uint8_t initial_value, uint8_t xorout, bool reversed);

/**
 * @brief Compute a CRC-8/CCITT checksum of a buffer.
 *
 * Poly=0x07, RefIn=false, RefOut=false, XorOut=0x00. The initial value is
 * supplied by the caller; use CRC8_CCITT_INITIAL_VALUE for the usual seed.
 * This is a one-shot calculation and does not add padding.
 *
 * @param initial_value Initial CRC register value
 * @param buf Input buffer
 * @param len Length of input buffer in bytes
 *
 * @return CRC8 value
 */
uint8_t crc8_ccitt(uint8_t initial_value, const void *buf, size_t len);

/**
 * @brief Compute a CRC-7/MMC checksum of a buffer.
 *
 * See JESD84-A441. Used by the MMC protocol.
 *
 * Poly=0x09, RefIn=false, RefOut=false, XorOut=0x00. The CRC is left
 * justified, so bit 7 of the result is bit 6 of the CRC.
 *
 * @param seed Initial CRC register value
 * @param data Input buffer
 * @param len  Length of input buffer in bytes
 *
 * @return CRC7 value
 */
uint8_t crc7_be(uint8_t seed, const uint8_t *data, size_t len);


#ifdef __cplusplus
}
#endif

#endif /* MICAOS_COMMON_CRC_H */
