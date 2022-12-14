/**************************************/
//! Spectrice: Spectral Freezing Tool
//! Copyright (C) 2022, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#pragma once
/**************************************/

//! Available window types
#define SPECTRICE_WINDOW_TYPE_SINE     0
#define SPECTRICE_WINDOW_TYPE_HANN     1
#define SPECTRICE_WINDOW_TYPE_HAMMING  2
#define SPECTRICE_WINDOW_TYPE_BLACKMAN 3
#define SPECTRICE_WINDOW_TYPE_NUTTALL  4

/**************************************/

//! Global state structure
struct Spectrice_t {
	//! Global state (do not change after initialization)
	int   nChan;        //! Channels in encoding scheme
	int   BlockSize;    //! Transform block size
	int   nHops;        //! Number of STFT hops per block
	int   FreezeStart;  //! Position to begin the freeze operation (in samples)
	int   FreezePoint;  //! Position where freezing peaks out (in samples)
	float FreezeFactor; //! Freezing amount (0.0 = No freezing, 1.0 = Full freeze)
	int   FreezeAmp;    //! Freeze amplitude  (0 = False, 1 = True)
	int   FreezePhase;  //! Freeze phase step (0 = False, 1 = True)

	//! Internal state
	//! Buffer memory layout (excluding alignment padding):
	//!   char  _Padding[];
	//!   float Window          [BlockSize];
	//!   float BfTemp          [BlockSize*2];
	//!   float BfInvLap [nChan][BlockSize];
	//!   float BfFwdLap [nChan][BlockSize];
	//!   float BfAbs    [nChan][BlockSize/2];
	//!   float BfArg    [nChan][BlockSize/2];
	//!   float BfArgOld [nChan][BlockSize/2];
	//!   float BfArgStep[nChan][BlockSize/2];
	//! BufferData contains the original pointer returned by malloc().
	int    BlockIdx;
	void  *BufferData;
	float *Window;
	float *BfTemp;
	float *BfInvLap;
	float *BfFwdLap;
	float *BfAbs;
	float *BfArg;
	float *BfArgOld;
	float *BfArgStep;
};

/**************************************/

int  Spectrice_Init   (struct Spectrice_t *State, int WindowType, const float *PrimingInput);
void Spectrice_Destroy(struct Spectrice_t *State);
void Spectrice_Process(struct Spectrice_t *State, float *Output, const float *Input);

/**************************************/
//! EOF
/**************************************/
