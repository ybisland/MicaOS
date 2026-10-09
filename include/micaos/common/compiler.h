#ifndef COMPILER_H
#define COMPILER_H

#if defined(__GNUC__) || defined(__clang__) || (defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6100100))

#ifndef   __ASM
  #define __ASM                                  __asm
#endif
#ifndef   __INLINE
  #define __INLINE                               __inline
#endif
#ifndef   __STATIC_INLINE
  #define __STATIC_INLINE                        static __inline
#endif
#ifndef   __STATIC_FORCEINLINE
  #define __STATIC_FORCEINLINE                   __attribute__((always_inline)) static __inline
#endif
#ifndef   __NO_RETURN
  #define __NO_RETURN                            __attribute__((__noreturn__))
#endif
#ifndef   __NO_INLINE
  #define __NO_INLINE                            __attribute__((noinline))
#endif
#ifndef   __NAKED
  #define __NAKED                                __attribute__((naked))
#endif
#ifndef   __USED
  #define __USED                                 __attribute__((used))
#endif
#ifndef   __UNUSED
  #define __UNUSED                               __attribute__((unused))
#endif
#ifndef   __WEAK
  #define __WEAK                                 __attribute__((weak))
#endif
#ifndef   __ALIGNED
  #define __ALIGNED(x)                           __attribute__((aligned(x)))
#endif
#ifndef   __SECTION
  #define __SECTION(name)                        __attribute__((section(name)))
#endif
#ifndef   __PACKED
  #define __PACKED                               __attribute__((packed, aligned(1)))
#endif
#ifndef   __PACKED_STRUCT
  #define __PACKED_STRUCT                        struct __attribute__((packed, aligned(1)))
#endif
#ifndef   __PACKED_UNION
  #define __PACKED_UNION                         union __attribute__((packed, aligned(1)))
#endif

/// Branch prediction optimization
#ifndef   _LIKELY
  #define _LIKELY(x)                             __builtin_expect(!!(x), 1)
#endif
#ifndef   _UNLIKELY
  #define _UNLIKELY(x)                           __builtin_expect(!!(x), 0)
#endif

/// Compiler barrier
#ifndef   compiler_barrier
  #define compiler_barrier()                     __ASM volatile ("" ::: "memory")
#endif

#else
# error "compiler.h requires GCC, clang, or ARM Compiler style GNU C extensions"
#endif

#define LIB_UNUSED_PARAM(x) ((void)(x))

// Fallback for non-GCC/Clang compilers (no optimization)
#ifndef _LIKELY
# define _LIKELY(x)    (!!(x))
#endif
#ifndef _UNLIKELY
# define _UNLIKELY(x)  (!!(x))
#endif

#endif /* COMPILER_H */
