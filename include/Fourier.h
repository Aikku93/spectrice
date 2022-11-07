/**************************************/
//! Spectrice: Spectral Freezing Tool
//! Copyright (C) 2022, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#pragma once
/**************************************/

//! DCT-II/DCT-IV (scaled)
//! Arguments:
//!  Buf[N]
//!  Tmp[N]
//! Implemented transforms (matrix form):
//!  mtxDCTII  = Table[Cos[(n-1/2)(k-1  )Pi/N], {k,N}, {n,N}]
//!  mtxDCTIV  = Table[Cos[(n-1/2)(k-1/2)Pi/N], {k,N}, {n,N}]
//! Implementations from:
//!  "Signal Processing based on Stable radix-2 DCT I-IV Algorithms having Orthogonal Factors"
//!  DOI: 10.13001/1081-3810.3207
//! NOTE:
//!  -N must be a power of two, and >= 8.
void Fourier_DCT2(float *Buf, float *Tmp, int N);
void Fourier_DCT4(float *Buf, float *Tmp, int N);

//! Centered FFT/iFFT (scaled)
//! Arguments:
//!  Buf[N]
//!  Tmp[N]
//! FFT outputs N/2 complex lines (packed as {Re,Im}).
//! Centered DFT derivation:
//!  "The Centered Discrete Fourier Transform and a Parallel Implementation of the FFT"
//!  DOI: 10.1109/ICASSP.2011.5946834
//! NOTE:
//!  -N must be a power of two, and >= 16.
void Fourier_FFTReCenter (float *Buf, float *Tmp, int N); //! Buf[N], Tmp[N]
void Fourier_iFFTReCenter(float *Buf, float *Tmp, int N); //! Buf[N], Tmp[N]

/**************************************/
//! EOF
/**************************************/
