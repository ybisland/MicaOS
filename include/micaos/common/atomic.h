/**
 * 原子操作，参考STM32 HAL
 * 适配单片机：Cortex-M
 */

#define SET_BIT(REG, BIT)     ((REG) |= (BIT))

#define CLEAR_BIT(REG, BIT)   ((REG) &= ~(BIT))

#define READ_BIT(REG, BIT)    ((REG) & (BIT))

#define CLEAR_REG(REG)        ((REG) = (0x0))

#define WRITE_REG(REG, VAL)   ((REG) = (VAL))

#define READ_REG(REG)         ((REG))

#define MODIFY_REG(REG, CLEARMASK, SETMASK)  WRITE_REG((REG), (((READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))

/* Use of interrupt control for register exclusive access */
/* Atomic 32-bit register access macro to set one or several bits */
#define ATOMIC_SET_BIT(REG, BIT)   \
    do {                           \
        uint32_t primask;          \
        primask = __get_PRIMASK(); \
        __set_PRIMASK(1);          \
        SET_BIT((REG), (BIT));     \
        __set_PRIMASK(primask);    \
    } while (0)

/* Atomic 32-bit register access macro to clear one or several bits */
#define ATOMIC_CLEAR_BIT(REG, BIT) \
    do {                           \
        uint32_t primask;          \
        primask = __get_PRIMASK(); \
        __set_PRIMASK(1);          \
        CLEAR_BIT((REG), (BIT));   \
        __set_PRIMASK(primask);    \
    } while (0)

/* Atomic 32-bit register access macro to clear and set one or several bits */
#define ATOMIC_MODIFY_REG(REG, CLEARMSK, SETMASK) \
    do {                                          \
        uint32_t primask;                         \
        primask = __get_PRIMASK();                \
        __set_PRIMASK(1);                         \
        MODIFY_REG((REG), (CLEARMSK), (SETMASK)); \
        __set_PRIMASK(primask);                   \
    } while (0)

/* Atomic 16-bit register access macro to set one or several bits */
#define ATOMIC_SETH_BIT(REG, BIT) ATOMIC_SET_BIT(REG, BIT)

/* Atomic 16-bit register access macro to clear one or several bits */
#define ATOMIC_CLEARH_BIT(REG, BIT) ATOMIC_CLEAR_BIT(REG, BIT)

/* Atomic 16-bit register access macro to clear and set one or several bits */
#define ATOMIC_MODIFYH_REG(REG, CLEARMSK, SETMASK) ATOMIC_MODIFY_REG(REG, CLEARMSK, SETMASK) \
