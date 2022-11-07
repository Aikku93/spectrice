/**************************************/
//! Spectrice: Spectral Freezing Tool
//! Copyright (C) 2022, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#pragma once
/**************************************/
#define ABS(x) ((x) < 0 ? (-(x)) : (x))
#define SQR(x) ((x)*(x))
/**************************************/

#define SPECTRICE_FORCED_INLINE static inline __attribute__((always_inline))
#define SPECTRICE_ASSUME(Cond) (Cond) ? ((void)0) : __builtin_unreachable()
#define SPECTRICE_ASSUME_ALIGNED(x,Align) x = __builtin_assume_aligned(x,Align)

/**************************************/

#define SPECTRICE_BUFFER_ALIGNMENT 64u //! Always align memory to 64-byte boundaries (preparation for AVX-512)
#define SPECTRICE_IS_POWEROF_2(x) (((x) & (-(x))) == (x))

/**************************************/
//! EOF
/**************************************/
