#ifndef THIRD_PARTY_SNAPPY_OPENSOURCE_CMAKE_CONFIG_H_
#define THIRD_PARTY_SNAPPY_OPENSOURCE_CMAKE_CONFIG_H_

#include <functional>

/* Define to 1 if the compiler supports __attribute__((always_inline)). */
#define HAVE_ATTRIBUTE_ALWAYS_INLINE 1

/* Define to 1 if the compiler supports __builtin_ctz and friends. */
#define HAVE_BUILTIN_CTZ 1

/* Define to 1 if the compiler supports __builtin_expect. */
#define HAVE_BUILTIN_EXPECT 1

/* Define to 1 if you have a definition for mmap() in <sys/mman.h>. */
/* #undef HAVE_FUNC_MMAP */

/* Define to 1 if you have a definition for sysconf() in <unistd.h>. */
/* #undef HAVE_FUNC_SYSCONF */

/* Define to 1 if you have the `lzo2' library (-llzo2). */
/* #undef HAVE_LIBLZO2 */

/* Define to 1 if you have the `z' library (-lz). */
#define HAVE_LIBZ 1

/* Define to 1 if you have the <sys/mman.h> header file. */
/* #undef HAVE_SYS_MMAN_H */

/* Define to 1 if you have the `lz4' library (-llz4). */
/* #undef HAVE_LIBLZ4 */

/* Define to 1 if you have the <sys/resource.h> header file. */
/* #undef HAVE_SYS_RESOURCE_H */

/* Define to 1 if you have the <sys/time.h> header file. */
/* #undef HAVE_SYS_TIME_H */

/* Define to 1 if you have the <sys/uio.h> header file. */
/* #undef HAVE_SYS_UIO_H */

/* Define to 1 if you have the <unistd.h> header file. */
/* #undef HAVE_UNISTD_H */

/* Define to 1 if you have the <windows.h> header file. */
#define HAVE_WINDOWS_H 1

/* Define to 1 if you target processors with SSSE3+ and have <tmmintrin.h>. */
#define SNAPPY_HAVE_SSSE3 0

/* Define to 1 if you target processors with BMI2+ and have <bmi2intrin.h>. */
#define SNAPPY_HAVE_BMI2 0

/* Define to 1 if you target processors with NEON and have <arm_neon.h>. */
#define SNAPPY_HAVE_NEON 0

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef SNAPPY_IS_BIG_ENDIAN */

/* Define to 1 if the compiler supports __builtin_prefetch. */
#define HAVE_BUILTIN_PREFETCH 1

/* Define to 1 if you target processors with SSE4.2 and have <crc32intrin.h>. */
#define SNAPPY_HAVE_X86_CRC32 0

/* Define to 1 if you target processors with RVV1.0 and have <riscv_vector.h>. */
#define SNAPPY_RVV_1 0

/* Define to 1 if you target processors with RVV0.7 and have <riscv_vector.h>. */
#define SNAPPY_RVV_0_7 0

/* Define to 1 if you have <arm_neon.h> and <arm_acle.h> and want to optimize
   compression speed by using __crc32cw from <arm_acle.h>. */
#define SNAPPY_HAVE_NEON_CRC32 0
#endif  // THIRD_PARTY_SNAPPY_OPENSOURCE_CMAKE_CONFIG_H_
