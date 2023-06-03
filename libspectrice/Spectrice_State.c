/**************************************/
//! Spectrice: Spectral Freezing Tool
//! Copyright (C) 2022, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
/**************************************/
#include "Fourier.h"
#include "Spectrice.h"
#include "Spectrice_Helper.h"
/**************************************/

#define MIN_CHANS     1
#define MAX_CHANS   255
#define MIN_BANDS     8
#define MAX_BANDS 65536

/**************************************/

/*!
  -Sine:     Valid for nHops >= 2.
  -Hann:     Valid for nHops >= 3.
  -Hamming:  Valid for nHops >= 3.
  -Blackman: Valid for nHops >= 5.
  -Nuttall:  Valid for nHops >= 7.
!*/
static int InitXformWindow(float *w, int N, int nHops, int Type) {
	int n;
	float Sum = 0.0f;
	switch(Type) {
		case SPECTRICE_WINDOW_TYPE_SINE: {
			if(nHops < 2) return 0;
			for(n=0;n<N/2;n++) {
				w[n] = (
					+1.0f*sinf((n+0.5f) * (float)(M_PI) / N)
				);
				Sum += SQR(w[n]);
			}
		} break;

		case SPECTRICE_WINDOW_TYPE_HANN: {
			if(nHops < 3) return 0;
			for(n=0;n<N/2;n++) {
				w[n] = (
					+0.5f
					-0.5f*cosf((n+0.5f) * (float)(2*M_PI) / N)
				);
				Sum += SQR(w[n]);
			}
		} break;

		case SPECTRICE_WINDOW_TYPE_HAMMING: {
			if(nHops < 3) return 0;
			for(n=0;n<N/2;n++) {
				w[n] = (
					+(25/46.0f)
					-(21/46.0f)*cosf((n+0.5f) * (float)(2*M_PI) / N)
				);
				Sum += SQR(w[n]);
			}
		} break;

		case SPECTRICE_WINDOW_TYPE_BLACKMAN: {
			if(nHops < 5) return 0;
			for(n=0;n<N/2;n++) {
				w[n] = (
					+0.42f
					-0.50f*cosf((n+0.5f) * (float)(2*M_PI) / N)
					+0.08f*cosf((n+0.5f) * (float)(4*M_PI) / N)
				);
				Sum += SQR(w[n]);
			}
		} break;

		//! "Some Windows with Very Good Sidelobe Behavior", A. Nuttall
		//! DOI: 10.1109/TASSP.1981.1163506
		//! Eq. 37 (minimum 4-term window)
		case SPECTRICE_WINDOW_TYPE_NUTTALL: {
			if(nHops < 7) return 0;
			for(n=0;n<N/2;n++) {
				w[n] = (
					+0.3635819f
					-0.4891775f*cosf((n+0.5f) * (float)(2*M_PI) / N)
					+0.1365995f*cosf((n+0.5f) * (float)(4*M_PI) / N)
					-0.0106411f*cosf((n+0.5f) * (float)(6*M_PI) / N)
				);
				Sum += SQR(w[n]);
			}
		} break;

		default: return 0;
	}
	float Norm = sqrtf(1.0f / (Sum * nHops));
	for(n=0;n<N/2;n++) w[n] *= Norm;
	return 1;
}

/**************************************/

int Spectrice_Init(struct Spectrice_t *State, int WindowType, const float *PrimingInput, const float *FreezeSnapshot) {
	int n;

	//! Clear anything that is needed for EncoderState_Destroy()
	State->BufferData = NULL;

	//! Verify parameters
	//! NOTE: We can't combine FreezePhase with a snapshot. It's technically
	//! possible to do so, but this will be left for a future update.
	int nChan      = State->nChan;
	int BlockSize  = State->BlockSize;
	int nHops      = State->nHops;
	if(nChan     < MIN_CHANS || nChan     > MAX_CHANS) return 0;
	if(BlockSize < MIN_BANDS || BlockSize > MAX_BANDS) return 0;
	if(nHops     < 2         || nHops     > BlockSize) return 0;
	if(!SPECTRICE_IS_POWEROF_2(BlockSize) || !SPECTRICE_IS_POWEROF_2(nHops)) return 0;
	if(FreezeSnapshot && State->FreezePhase) return 0;

	//! Get buffer offsets and allocation size
	int AllocSize = 0;
#define CREATE_BUFFER(Name, Sz) uintptr_t Name##_Offs = AllocSize; AllocSize += (Sz)
	CREATE_BUFFER(Window,    (sizeof(float) * (BlockSize/2)) * 1);
	CREATE_BUFFER(BfTemp,    (sizeof(float) * (BlockSize  )) * 2);
	CREATE_BUFFER(BfInvLap,  (sizeof(float) * (BlockSize  )) * nChan);
	CREATE_BUFFER(BfFwdLap,  (sizeof(float) * (BlockSize  )) * nChan);
	CREATE_BUFFER(BfAbs,     (sizeof(float) * (BlockSize/2)) * nChan);
	CREATE_BUFFER(BfArg,     (sizeof(float) * (BlockSize/2)) * (State->FreezePhase ? nChan : 0));
	CREATE_BUFFER(BfArgOld,  (sizeof(float) * (BlockSize/2)) * (State->FreezePhase ? nChan : 0));
	CREATE_BUFFER(BfArgStep, (sizeof(float) * (BlockSize/2)) * (State->FreezePhase ? nChan : 0));
#undef CREATE_BUFFER

	//! Allocate buffer space
	char *Buf = State->BufferData = malloc(SPECTRICE_BUFFER_ALIGNMENT-1 + AllocSize);
	if(!Buf) return 0;

	//! Initialize pointers
	Buf += (-(uintptr_t)Buf) & (SPECTRICE_BUFFER_ALIGNMENT-1);
	State->Window    = (float*)(Buf + Window_Offs);
	State->BfTemp    = (float*)(Buf + BfTemp_Offs);
	State->BfInvLap  = (float*)(Buf + BfInvLap_Offs);
	State->BfFwdLap  = (float*)(Buf + BfFwdLap_Offs);
	State->BfAbs     = (float*)(Buf + BfAbs_Offs);
	State->BfArg     = (float*)(Buf + BfArg_Offs);
	State->BfArgOld  = (float*)(Buf + BfArgOld_Offs);
	State->BfArgStep = (float*)(Buf + BfArgStep_Offs);

	//! Set initial state
	State->BlockIdx = 0;
	if(!InitXformWindow(State->Window, BlockSize, nHops, WindowType)) {
		Spectrice_Destroy(State);
		return 0;
	}
	if(State->FreezePhase) {
		for(n=0;n<(BlockSize/2)*nChan;n++) State->BfArg    [n] = 0.0f;
		for(n=0;n<(BlockSize/2)*nChan;n++) State->BfArgOld [n] = 0.0f;
		for(n=0;n<(BlockSize/2)*nChan;n++) State->BfArgStep[n] = 0.0f;
	}

	//! Transform the "snapshot" window for freezing
	if(FreezeSnapshot) {
		int Chan;
		float *BfAbs = State->BfAbs;
		float *BfDFT = State->BfTemp;
		const float *Window = State->Window;
		for(Chan=0;Chan<nChan;Chan++) {
			for(n=0;n<BlockSize/2;n++) {
				BfDFT[            n] = Window[n] * FreezeSnapshot[(            n)*nChan + Chan];
				BfDFT[BlockSize-1-n] = Window[n] * FreezeSnapshot[(BlockSize-1-n)*nChan + Chan];
			}
			Fourier_FFTReCenter(BfDFT, BfDFT+BlockSize, BlockSize);
			for(n=0;n<BlockSize/2;n++) {
				float Re  = BfDFT[n*2+0];
				float Im  = BfDFT[n*2+1];
				float Abs = sqrtf(SQR(Re) + SQR(Im));
				BfAbs[n] = Abs;
			}
			BfAbs += BlockSize/2;
		}
		State->HaveSnapshot = 1;
	} else {
		float *BfAbs = State->BfAbs;
		for(n=0;n<(BlockSize/2)*nChan;n++) BfAbs[n] = 0.0f;
	}

	//! Prime input buffer
	for(n=0;n<BlockSize*nChan;n++) State->BfFwdLap[n] = 0.0f;
	for(n=0;n<BlockSize*nChan;n++) State->BfInvLap[n] = 0.0f;
	if(PrimingInput) Spectrice_Process(State, NULL, PrimingInput);

	//! Success
	return 1;
}

/**************************************/

void Spectrice_Destroy(struct Spectrice_t *State) {
	//! Free buffer space
	free(State->BufferData);
}

/**************************************/
//! EOF
/**************************************/
