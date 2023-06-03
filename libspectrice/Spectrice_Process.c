/**************************************/
//! Spectrice: Spectral Freezing Tool
//! Copyright (C) 2022, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#include <math.h>
#include <stdlib.h>
/**************************************/
#include "Fourier.h"
#include "Spectrice.h"
#include "Spectrice_Helper.h"
/**************************************/

void Spectrice_Process(struct Spectrice_t *State, float *Output, const float *Input) {
	int n, Chan, Hop;
	int nChan     = State->nChan;
	int BlockSize = State->BlockSize;
	int nHops     = State->nHops;

	//! Begin processing
	float *Window = State->Window;
	float *BfTemp = State->BfTemp;
	float *BfInvLap  = State->BfInvLap;
	float *BfFwdLap  = State->BfFwdLap;
	float *BfAbs     = State->BfAbs;
	float *BfArg     = State->BfArg;
	float *BfArgOld  = State->BfArgOld;
	float *BfArgStep = State->BfArgStep;
	for(Chan=0;Chan<nChan;Chan++) {
		for(Hop=0;Hop<nHops;Hop++) {
			int HopSize = BlockSize / nHops;

			//! Give some compiler hints
			SPECTRICE_ASSUME_ALIGNED(Window,    SPECTRICE_BUFFER_ALIGNMENT);
			SPECTRICE_ASSUME_ALIGNED(BfTemp,    SPECTRICE_BUFFER_ALIGNMENT);
			SPECTRICE_ASSUME_ALIGNED(BfInvLap,  SPECTRICE_BUFFER_ALIGNMENT);
			SPECTRICE_ASSUME_ALIGNED(BfFwdLap,  SPECTRICE_BUFFER_ALIGNMENT);
			SPECTRICE_ASSUME_ALIGNED(BfAbs,     SPECTRICE_BUFFER_ALIGNMENT);
			SPECTRICE_ASSUME_ALIGNED(BfArg,     SPECTRICE_BUFFER_ALIGNMENT);
			SPECTRICE_ASSUME_ALIGNED(BfArgOld,  SPECTRICE_BUFFER_ALIGNMENT);
			SPECTRICE_ASSUME_ALIGNED(BfArgStep, SPECTRICE_BUFFER_ALIGNMENT);

			//! Apply DFT
			float *BfDFT = BfTemp;
			for(n=0;n<BlockSize/2;n++) {
				BfDFT[            n] = Window[n] * BfFwdLap[n];
				BfDFT[BlockSize-1-n] = Window[n] * BfFwdLap[BlockSize-1-n];
			}
			Fourier_FFTReCenter(BfDFT, BfDFT+BlockSize, BlockSize);

			//! Get crossfade mix ratio
			float MixRatio; {
				float Idx = ((float)State->BlockIdx + (float)Hop / (float)nHops) * (float)BlockSize;
				float Beg = (float)State->FreezeStart;
				float End = (float)State->FreezePoint;
				MixRatio  = (Idx >= End) ? 1.0f : ((Idx-Beg) / (End-Beg));
				MixRatio *= State->FreezeFactor;
				MixRatio  = (MixRatio < 0.0f) ? 0.0f : (MixRatio > 1.0f) ? 1.0f : MixRatio;
			}

			//! Convert to Amp+Phase, apply freezing, convert back to Re+Im
			for(n=0;n<BlockSize/2;n++) {
				//! Convert Re,Im to Abs,Arg
				//! NOTE: Pre-divide Arg by 2Pi to simplify things.
				float Re  = BfDFT[n*2+0];
				float Im  = BfDFT[n*2+1];
				float Abs = sqrtf(SQR(Re) + SQR(Im));
				float Arg = atan2f(Im, Re) * (float)(1.0 / (2*M_PI));

				//! Freeze amplitude
				if(State->FreezeAmp) {
					Abs = MixRatio*BfAbs[n] + (1.0f-MixRatio)*Abs;
					if(!State->HaveSnapshot) BfAbs[n] = Abs;
				}

				//! Freeze phase step
				if(State->FreezePhase) {
					float dArg = Arg - BfArgOld[n]; BfArgOld[n] = Arg;
					dArg += (float)n / nHops;
					dArg -= truncf(dArg);
					dArg += 1.0f * (dArg < 0.0f);
					dArg  = BfArgStep[n] = MixRatio*BfArgStep[n] + (1.0f-MixRatio)*dArg;
					dArg -= (float)n / nHops;
					BfArg[n] += dArg;
					BfArg[n] -= truncf(BfArg[n]);
					Arg = BfArg[n];
				}

				//! Convert back to Re,Im
				Re = Abs * cosf(Arg * (float)(2*M_PI));
				Im = Abs * sinf(Arg * (float)(2*M_PI));
				BfDFT[n*2+0] = Re;
				BfDFT[n*2+1] = Im;
			}

			//! Do iDFT and accumulate
			Fourier_iFFTReCenter(BfDFT, BfDFT+BlockSize, BlockSize);
			for(n=0;n<BlockSize/2;n++) {
				BfInvLap[            n] += Window[n] * BfDFT[n];
				BfInvLap[BlockSize-1-n] += Window[n] * BfDFT[BlockSize-1-n];
			}

			//! Shift samples into/out of buffers
			if(Output) {
				for(n=0;n<HopSize;n++) Output[(Hop*HopSize+n)*nChan+Chan] = BfInvLap[n];
			}
			for(n=HopSize;n<BlockSize;n++) {
				BfFwdLap[n-HopSize] = BfFwdLap[n];
				BfInvLap[n-HopSize] = BfInvLap[n];
			}
			for(n=0;n<HopSize;n++) {
				BfFwdLap[BlockSize-HopSize+n] = Input[(Hop*HopSize+n)*nChan+Chan];
				BfInvLap[BlockSize-HopSize+n] = 0.0f;
			}
		}

		//! Next channel
		BfInvLap  += BlockSize;
		BfFwdLap  += BlockSize;
		BfAbs     += BlockSize/2;
		BfArg     += BlockSize/2;
		BfArgOld  += BlockSize/2;
		BfArgStep += BlockSize/2;
	}
	State->BlockIdx++;
}

/**************************************/
//! EOF
/**************************************/
