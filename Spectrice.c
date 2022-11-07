/**************************************/
//! Spectrice: Spectral Freezing Tool
//! Copyright (C) 2022, Ruben Nunez (Aikku; aik AT aol DOT com DOT au)
//! Refer to the project README file for license terms.
/**************************************/
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/**************************************/
#include "Spectrice.h"
/**************************************/
#define BUFFER_ALIGNMENT 64u //! Always align memory to 64-byte boundaries (preparation for AVX-512)
/**************************************/
#define BUFFER_SIZE_INPUT  65536 //! In samples per channel
#define BUFFER_SIZE_OUTPUT 65536 //! In samples per channel
/**************************************/

struct SampleBuffer_State_t {
	FILE  *File;
	float *Buf;
	char  *RawBuf;
	int    BufIdx;
	int    BufLen;
};

/**************************************/

#define SAMPLEBUFFER_RD 0
#define SAMPLEBUFFER_WR 1
int SampleBuffer_Init(struct SampleBuffer_State_t *State, int BufLen, int PrePad, const char *Filename, int FileMode) {
	static const char *FileModes[] = {"rb","wb"};
	State->File = fopen(Filename, FileModes[FileMode]);
	if(!State->File) return 0;
	char *Buf = malloc(BufLen*sizeof(float) + BUFFER_ALIGNMENT-1);
	if(!Buf) { fclose(State->File); return 0; }

	State->Buf    = (float*)(Buf + (-(uintptr_t)Buf % BUFFER_ALIGNMENT));
	State->RawBuf = Buf;
	State->BufIdx = (FileMode == SAMPLEBUFFER_RD) ? (BufLen - PrePad) : PrePad;
	State->BufLen = BufLen;
	return 1;
}

void SampleBuffer_Destroy(struct SampleBuffer_State_t *State) {
	free(State->RawBuf);
	fclose(State->File);
}

void SampleBuffer_Deinterleave(float *Buf, float *Temp, int N) {
	int n;
	for(n=0;n<N/2;n++) {
		Temp[n] = Buf[n*2+1];
		Buf[n]  = Buf[n*2+0];
	}
	for(n=0;n<N/2;n++) {
		Buf[N/2+n] = Temp[n];
	}
}

void SampleBuffer_Interleave(float *Buf, float *Temp, int N) {
	int n;
	for(n=N/2-1;n>=0;n--) {
		Temp[n] = Buf[N/2+n];
	}
	for(n=N/2-1;n>=0;n--) {
		Buf[n*2+1] = Temp[n];
		Buf[n*2+0] = Buf[n];
	}
}

//! NOTE: nSamples must be <= BufLen
//! NOTE: Returned data at Ret[0..nSamples-1] will be discarded
//! in future calls and may be safely destroyed after this call.
float *SampleBuffer_Fetch(struct SampleBuffer_State_t *State, int nSamples) {
	int n, BufIdx = State->BufIdx, BufLen = State->BufLen;
	float *Buf = State->Buf;

	//! Need to refill for this request?
	if(BufIdx+nSamples > BufLen) {
		int SmpToRead = BufIdx;

		//! Shift samples from end to start
		const float *Src = Buf + BufIdx;
		for(n=0;n<BufLen-SmpToRead;n++) Buf[n] = Src[n];

		//! Read samples
		//! NOTE: Aliasing, so the source data is pushed to the end
		//! of the buffer prior to unpacking/widening to 32bit.
		int16_t *Buf16 = (int16_t*)(Buf + BufLen) - SmpToRead;
		int SmpRead = fread(Buf16, sizeof(int16_t), SmpToRead, State->File);
		for(n=SmpRead;n<SmpToRead;n++) Buf16[n] = 0.0f; //! Zero-pad on EOF

		//! Unpack to float
		float *BufDst = Buf + BufLen - SmpToRead;
		for(n=0;n<SmpToRead;n++) BufDst[n] = (float)Buf16[n];
		BufIdx = 0;
	}

	//! Update samples-remaining count and return pointer to data
	State->BufIdx = BufIdx + nSamples;
	return State->Buf + BufIdx;
}

static void SampleBuffer_WriteFlush(struct SampleBuffer_State_t *State) {
	int n, BufIdx = State->BufIdx;
	float *Buf = State->Buf;
	int SmpToWrite = BufIdx; if(!SmpToWrite) return;
	int16_t *Buf16 = (int16_t*)Buf;
	for(n=0;n<SmpToWrite;n++) {
		int v = lrintf(Buf[n]);
		if(v < -32768) v = -32768;
		if(v > +32767) v = +32767;
		Buf16[n] = (int16_t)v;
	}
	fwrite(Buf16, sizeof(int16_t), SmpToWrite, State->File);
	State->BufIdx = 0;
}
void SampleBuffer_Write(struct SampleBuffer_State_t *State, int nSamples, const float *Src) {
	int n, BufIdx = State->BufIdx, BufLen = State->BufLen;
	float *Buf = State->Buf;

	//! Still have samples to skip?
	if(BufIdx < 0) {
		int N = -BufIdx; if(N > nSamples) N = nSamples;
		nSamples -= N;
		Src      += N;
		BufIdx   += N;
	}

	//! Keep cycling samples through buffer
	while(nSamples) {
		//! Buffer is full?
		if(BufIdx == BufLen) {
			State->BufIdx = BufIdx;
			SampleBuffer_WriteFlush(State);
			BufIdx = /*State->BufIdx*/0;
		}

		//! Fill the buffer as much as we can
		int N = BufLen - BufIdx; if(N > nSamples) N = nSamples;
		float *Dst = Buf + BufIdx;
		for(n=0;n<N;n++) *Dst++ = *Src++;
		BufIdx   += N;
		nSamples -= N;
	}
	State->BufIdx = BufIdx;
}

/**************************************/

int main(int argc, const char *argv[]) {
	//! Check arguments
	if(argc < 4) {
		printf(
			"spectrice - Spectral Freezing Tool\n"
			"Usage:\n"
			" spectrice Input Output [Opt]\n"
			"Options:\n"
			" -nc:1           - Set number of channels.\n"
			" -blocksize:8192 - Set number of coefficients per block (must be a power of 2).\n"
			" -nhops:8        - Set number of evenly-divided hops per block (must be 2^n).\n"
			" -window:nuttall - Set the window function. Possible values:\n"
			"                   - sine     (minimum hops: 2)\n"
			"                   - hann     (minimum hops: 4)\n"
			"                   - hamming  (minimum hops: 4)\n"
			"                   - blackman (minimum hops: 8)\n"
			"                   - nuttall  (minimum hops: 8)\n"
			" -freezexfade:0  - Set number of samples to crossfade/blend prior to freezing.\n"
			"                   This will always be rounded to blocks.\n"
			" -freezepoint:X  - Set freezing point. If this is not aligned to BlockSize, the\n"
			"                   data will be padded so that it is and then shifted back on\n"
			"                   output.\n"
			" -freezefactor:1.0 - Amount of freezing to apply. 0.0 = No change, 1.0 = Freeze.\n"
			" -nofreezeamp      - Don't freeze amplitude.\n"
			" -freezephase      - Freeze phase step.\n"
			"Multi-channel data must be interleaved (packed).\n"
		);
		return 1;
	}

	//! Parse parameters
	int   nChan        = 1;
	int   BlockSize    = 8192;
	int   nHops        = 8;
	int   FreezeAmp    = 1;
	int   FreezePhase  = 0;
	int   WindowType   = SPECTRICE_WINDOW_TYPE_NUTTALL;
	int   FreezeXFade  = 0;
	int   FreezePoint  = 0;
	float FreezeFactor = 1.0f;
	{
		int n;
		for(n=3;n<argc;n++) {
			if(!memcmp(argv[n], "-nc:", 4)) {
				int x = atoi(argv[n] + 4);
				if(x > 0 && x <= 2) nChan = x;
				else printf("WARNING: Ignoring invalid parameter to number of channels (%d)\n", x);
			}

			else if(!memcmp(argv[n], "-blocksize:", 11)) {
				int x = atoi(argv[n] + 11);
				if(x >= 16 && x <= 65536 && (x & (-x)) == x) BlockSize = x;
				else printf("WARNING: Ignoring invalid parameter to block size (%d)\n", x);
			}

			else if(!memcmp(argv[n], "-nhops:", 7)) {
				int x = atoi(argv[n] + 7);
				if(x >= 2 && (x & (-x)) == x) nHops = x;
				else printf("WARNING: Ignoring invalid parameter to number of hops (%d)\n", x);
			}

			else if(!memcmp(argv[n], "-window:", 8)) {
				const char *x = argv[n] + 8;
				     if(!strcmp(x, "sine"))     WindowType = SPECTRICE_WINDOW_TYPE_SINE;
				else if(!strcmp(x, "hann"))     WindowType = SPECTRICE_WINDOW_TYPE_HANN;
				else if(!strcmp(x, "hamming"))  WindowType = SPECTRICE_WINDOW_TYPE_HAMMING;
				else if(!strcmp(x, "blackman")) WindowType = SPECTRICE_WINDOW_TYPE_BLACKMAN;
				else if(!strcmp(x, "nuttall"))  WindowType = SPECTRICE_WINDOW_TYPE_NUTTALL;
				else printf("WARNING: Ignoring invalid parameter to window type (%s)\n", x);
			}

			else if(!memcmp(argv[n], "-freezexfade:", 13)) {
				int x = atoi(argv[n] + 13);
				if(x >= 0) FreezeXFade = x;
				else printf("WARNING: Ignoring invalid parameter to freeze crossfade (%d)\n", x);
			}

			else if(!memcmp(argv[n], "-freezepoint:", 13)) {
				int x = atoi(argv[n] + 13);
				if(x > 0) FreezePoint = x;
				else printf("WARNING: Ignoring invalid parameter to freeze point (%d)\n", x);
			}

			else if(!memcmp(argv[n], "-freezefactor:", 14)) {
				float x = atof(argv[n] + 14);
				if(x >= 0.0f && x <= 1.0f) FreezeFactor = x;
				else printf("WARNING: Ignoring invalid parameter to freeze factor (%f)\n", x);
			}

			else if(!strcmp(argv[n], "-nofreezeamp")) {
				FreezeAmp = 0;
			}

			else if(!strcmp(argv[n], "-freezephase")) {
				FreezePhase = 1;
			}

			else printf("WARNING: Ignoring unknown argument (%s)\n", argv[n]);
		}
	}
	int FreezeStart = FreezePoint - FreezeXFade;

	//! Create IO streams
	int InputPrePad  = BlockSize - (FreezePoint % BlockSize);
	int OutputPrePad = -(BlockSize + InputPrePad); //! iSTFT delay, plus input pre-padding
	struct SampleBuffer_State_t InFile, OutFile; {
		if(!SampleBuffer_Init(&InFile, BUFFER_SIZE_INPUT*nChan, InputPrePad*nChan, argv[1], SAMPLEBUFFER_RD)) {
			printf("ERROR: Unable to open input file (%s)\n", argv[1]);
			return -1;
		}
		if(!SampleBuffer_Init(&OutFile, BUFFER_SIZE_OUTPUT*nChan, OutputPrePad*nChan, argv[2], SAMPLEBUFFER_WR)) {
			printf("ERROR: Unable to open output file (%s)\n", argv[2]);
			SampleBuffer_Destroy(&InFile);
			return -1;
		}
	}

	//! Create processing buffer
	float *ProcessBuffer = malloc(BlockSize * nChan * sizeof(float));
	if(!ProcessBuffer) {
		printf("ERROR: Out of memory\n");
		SampleBuffer_Destroy(&OutFile);
		SampleBuffer_Destroy(&InFile);
		return -1;
	}

	//! Create [de]interleaving buffer
	float *InterleaveBuffer = malloc(BlockSize * sizeof(float));
	if(!InterleaveBuffer) {
		printf("ERROR: Out of memory\n");
		free(ProcessBuffer);
		SampleBuffer_Destroy(&OutFile);
		SampleBuffer_Destroy(&InFile);
		return -1;
	}

	//! Init state (and re-align data to freeze point)
	struct Spectrice_t State = {
		.nChan        = nChan,
		.BlockSize    = BlockSize,
		.nHops        = nHops,
		.FreezeStart  = FreezeStart + InputPrePad,
		.FreezePoint  = FreezePoint + InputPrePad,
		.FreezeFactor = FreezeFactor,
		.FreezeAmp    = FreezeAmp,
		.FreezePhase  = FreezePhase,
	};
	if(!Spectrice_Init(&State, WindowType)) {
		printf("ERROR: Unable to initialize state\n");
		SampleBuffer_Destroy(&OutFile);
		SampleBuffer_Destroy(&InFile);
		return -1;
	}

	//! Get number of samples+blocks to process
	int nSamples;
	int nBlocks; {
		FILE *File = InFile.File;
		long int Beg = ftell(File); fseek(File,   0, SEEK_END);
		long int End = ftell(File); fseek(File, Beg, SEEK_SET);
		nSamples = (End - Beg) / (nChan*sizeof(int16_t)) - OutputPrePad; //! Include pre-padding
		nBlocks  = (nSamples - 1) / BlockSize + 1;
	}

	//! Begin processing
	int Block, nSamplesRem = nSamples;
	for(Block=0;Block<nBlocks;Block++) {
		printf("\rBlock %u/%u (%.2f%%)", Block, nBlocks, Block*100.0f/nBlocks);

		float *Buf;
		int nOutSamples = (nSamplesRem < BlockSize) ? nSamplesRem : BlockSize;
		Buf = SampleBuffer_Fetch(&InFile, BlockSize*nChan);
		if(nChan == 2) SampleBuffer_Deinterleave(Buf, InterleaveBuffer, BlockSize*2);
		Spectrice_Process(&State, ProcessBuffer, Buf);
		Buf = ProcessBuffer;
		if(nChan == 2) SampleBuffer_Interleave(Buf, InterleaveBuffer, BlockSize*2);
		SampleBuffer_Write(&OutFile, nOutSamples*nChan, Buf);
		nSamplesRem -= nOutSamples;
	}
	printf("\nOk.");

	//! Destroy state, normal return
	Spectrice_Destroy(&State);
	SampleBuffer_WriteFlush(&OutFile);
	SampleBuffer_Destroy(&OutFile);
	SampleBuffer_Destroy(&InFile);
	return 0;
}

/**************************************/
//! EOF
/**************************************/
