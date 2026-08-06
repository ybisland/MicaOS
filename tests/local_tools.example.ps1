# Local MCU test tool configuration template.
#
# Copy this file to:
#   tests/local_tools.ps1
#
# local_tools.ps1 is ignored by git and should contain machine-specific paths.

# DAP-Link / CMSIS-DAP path. Recommended for DAP-Link.
$NORA_FLASH_TOOL = "pyocd"
$NORA_PYOCD_TARGET = "stm32g0b1re"

# Optional STM32CubeProgrammer CLI path. Mostly useful for ST-LINK.
$STM32_PROGRAMMER_CLI = ""

# UART used by MCU test logs.
$NORA_TEST_UART = "COM26"
$NORA_TEST_BAUD = 115200
