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
#include "MiniRIFF.h"
#include "WavIO.h"
/**************************************/

//! Possible output formats
#define FORMAT_PCM8    0
#define FORMAT_PCM16   1
#define FORMAT_PCM24   2
#define FORMAT_FLOAT32 3
#define FORMAT_DEFAULT 4

/************************************************/

//! Read gain in linear form, or dB form
static double ReadGain(const char *Str) {
	double Gain;
	int IsDecibel = 0;
	int nArg = sscanf(Str, "%lf %*1[dD]%*1[bB]%n", &Gain, &IsDecibel);
	if(nArg == 0) return NAN;
	return IsDecibel ? pow(10.0, Gain/20.0) : Gain;
}

/**************************************/

int main(int argc, const char *argv[]) {
	int   ExitCode = 0;
	char *AllocBuffer;
	struct WAV_State_t FileIn;
	struct WAV_State_t FileOut;
	struct Spectrice_t State;

	//! Check arguments
	if(argc < 3) {
		printf(
			"spectrice - Spectral Freezing Tool\n"
			"Usage:\n"
			" spectrice Input.wav Output.wav [Opt]\n"
			"Options:\n"
			" -blocksize:1024 - Set number of coefficients per block (must be a power of 2).\n"
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
			"                   If this parameter is not provided, then the freeze point will\n"
			"                   become the waveform's loop start point (and if no loop is\n"
			"                   found, the application will exit with an error).\n"
			" -freezefactor:1.0 - Amount of freezing to apply. 0.0 = No change, 1.0 = Freeze.\n"
			" -nofreezeamp      - Don't freeze amplitude.\n"
			" -freezephase      - Freeze phase step.\n"
			" -snapshot:n       - Capture a snapshot of the amplitude at some arbitrary\n"
			"                     position, and use this for blending with cross-fading.\n"
			"                     Can be 'n' to disable this feature, or a sample position\n"
			"                     from which to capture the snapshot.\n"
			" -snapshotgain:1.0 - Set gain of snapshot. Can be specified in linear form, or\n"
			"                     in dB (eg. 1.0 == 0.0dB).\n"
			" -format:default   - Set output format (default, PCM8, PCM16, PCM24, FLOAT32).\n"
			"                     `default` will use the same format as the input file.\n"
			" -loops:y          - Enable(y) or disable(n) loop handling. When enabled, any\n"
			"                     data past the loop end point will \"wrap around\" back to\n"
			"                     the loop start point.\n"
		);
		return 1;
	}

	//! Parse parameters
	int   BlockSize    = 1024;
	int   nHops        = 8;
	int   FreezeAmp    = 1;
	int   FreezePhase  = 0;
	int   WindowType   = SPECTRICE_WINDOW_TYPE_NUTTALL;
	int   FreezeXFade  = 0;
	int   FreezePoint  = 0;
	int   SnapshotPos  = -1;
	float SnapshotGain = 1.0f;
	int   LoopEnd      = 0;
	int   LoopLen      = 0;
	int   LoopProcess  = 1;
	float FreezeFactor = 1.0f;
	int   FormatType   = FORMAT_DEFAULT;
	{
		int n;
		for(n=3;n<argc;n++) {
			if(!memcmp(argv[n], "-blocksize:", 11)) {
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

			else if(!memcmp(argv[n], "-snapshot:", 10)) {
				char x = argv[n][10];
				if(x == 'n' || x == 'N') SnapshotPos = -1;
				else SnapshotPos = atoi(argv[n] + 10);
			}

			else if(!memcmp(argv[n], "-snapshotgain:", 14)) {
				const char *Str = argv[n] + 14;
				double x = ReadGain(Str);
				if(x == NAN) printf("WARNING: Ignoring invalid parameter to snapshot gain (%s)\n", Str);
				else SnapshotGain = (float)x;
			}

			else if(!memcmp(argv[n], "-loops:", 7)) {
				char x = argv[n][7];
				     if(x == 'y' || x == 'Y') LoopProcess = 1;
				else if(x == 'n' || x == 'N') LoopProcess = 0;
				else printf("WARNING: Ignoring invalid parameter to loop processing (%c)\n", x);
			}

			else if(!memcmp(argv[n], "-format:", 8)) {
				const char *FmtStr = argv[n] + 8;
				     if(!strcmp(FmtStr, "PCM8")    || !strcmp(FmtStr, "pcm8"))
					FormatType = FORMAT_PCM8;
				else if(!strcmp(FmtStr, "PCM16")   || !strcmp(FmtStr, "pcm16"))
					FormatType = FORMAT_PCM16;
				else if(!strcmp(FmtStr, "PCM24")   || !strcmp(FmtStr, "pcm24"))
					FormatType = FORMAT_PCM24;
				else if(!strcmp(FmtStr, "FLOAT32") || !strcmp(FmtStr, "float32"))
					FormatType = FORMAT_FLOAT32;
				else if(!strcmp(FmtStr, "DEFAULT") || !strcmp(FmtStr, "default"))
					FormatType = FORMAT_DEFAULT;
				else {
					printf("ERROR: Invalid output format (%s).\n", FmtStr);
					ExitCode = -1; goto Exit_BadArgs;
				}
			}

			else printf("WARNING: Ignoring unknown argument (%s)\n", argv[n]);
		}
	}

	//! Open input file
	{
		int Error = WAV_OpenR(&FileIn, argv[1]);
		if(Error < 0) {
			printf("ERROR: Unable to open input file (%s); error %s.\n", argv[1], WAV_ErrorCodeToString(Error));
			ExitCode = -1; goto Exit_FailOpenInFile;
		}
	}

	//! Ensure file is at last as long the block size
	if((int)FileIn.nSamplePoints < BlockSize) {
		printf("ERROR: Input file has less sample points than BlockSize.\n");
		ExitCode = -1; goto Exit_FailFileLength;
	}

	//! Ensure snapshot position is valid
	if(SnapshotPos >= 0 && SnapshotPos >= (int)FileIn.nSamplePoints - BlockSize) {
		printf("WARNING: Snapshot position too close to end of file; moving to last block.\n");
		SnapshotPos = FileIn.nSamplePoints - BlockSize;
	}

	//! Read loop points
	{
		//! Find a smpl chunk
		const struct WAV_Chunk_t *Ck = FileIn.Chunks;
		while(Ck) {
			if(Ck->CkType == RIFF_FOURCC("smpl")) {
				struct WAVE_smpl_t *CkData = malloc(Ck->CkSize);
				if(CkData) {
					fseek(FileIn.File, Ck->FileOffs, SEEK_SET);
					fread(CkData, Ck->CkSize, 1, FileIn.File);

					//! Now find the first loop point and assign
					uint32_t i;
					for(i=0;i<CkData->cSampleLoops;i++) {
						struct WAVE_smpl_loop_t *CkLoop = &CkData->loopPoints[i];
						if(CkLoop->dwType == WAVE_SMPL_LOOP_TYPE_FOWARD) {
							//! dwEnd is inclusive, but we need exclusive, so add 1
							LoopEnd = CkLoop->dwEnd+1;
							LoopLen = LoopEnd - CkLoop->dwStart;
							break;
						}
					}
					free(CkData);
				}
				break;
			} else Ck = Ck->Next;
		}

		//! If we have no loops, disable loop processing
		if(!LoopLen) LoopProcess = 0;
	}

	//! If we don't have a freeze point, set it now
	if(FreezePoint == 0) {
		if(LoopLen) {
			FreezePoint = LoopEnd - LoopLen;
		} else {
			printf("ERROR: Unable to find freeze point.\n");
			ExitCode = -1; goto Exit_FailGetFreezePoint;
		}
	}
	int FreezeStart = FreezePoint - FreezeXFade;

	//! Verify that FreezeStart occurs after at least one block of data
	//! NOTE: Further shift by BlockSize/2 to account for OLA structure.
	int XformPrimingLength = BlockSize + BlockSize/2;
	if(FreezeStart < XformPrimingLength) {
		printf("WARNING: Freeze start point too early; moving to %d.\n", XformPrimingLength);
		FreezeStart = XformPrimingLength;
		if(FreezePoint < FreezeStart) FreezePoint = FreezeStart;
	}

	//! Create output file
	{
		struct WAVE_fmt_t fmt, *fmtSrc = FileIn.fmt;
		if(FormatType == FORMAT_DEFAULT) {
			memcpy(&fmt, fmtSrc, sizeof(fmt));
		} else {
			int BytesPerSmp = 0;
			switch(FormatType) {
				case FORMAT_PCM8:    BytesPerSmp =  8 / 8; break;
				case FORMAT_PCM16:   BytesPerSmp = 16 / 8; break;
				case FORMAT_PCM24:   BytesPerSmp = 24 / 8; break;
				case FORMAT_FLOAT32: BytesPerSmp = 32 / 8; break;
			}
			fmt.wFormatTag      = (FormatType == FORMAT_FLOAT32) ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
			fmt.nChannels       = fmtSrc->nChannels;
			fmt.nSamplesPerSec  = fmtSrc->nSamplesPerSec;
			fmt.nAvgBytesPerSec = BytesPerSmp * fmtSrc->nChannels * fmtSrc->nSamplesPerSec;
			fmt.nBlockAlign     = BytesPerSmp * fmtSrc->nChannels;
			fmt.wBitsPerSample  = BytesPerSmp * 8;
		}
		int Error = WAV_OpenW(&FileOut, argv[2], &fmt);
		if(Error < 0) {
			printf("ERROR: Unable to create output file (%s); error %s.\n", argv[1], WAV_ErrorCodeToString(Error));
			ExitCode = -1; goto Exit_FailCreateOutFile;
		}

		//! Copy all chunks from source file
		const struct WAV_Chunk_t *SrcCk = FileIn.Chunks;
		      struct WAV_Chunk_t *Prev  = NULL;
		while(SrcCk) {
			//! Ensure to exclude fmt and data
			if(SrcCk->CkType != RIFF_FOURCC("fmt ") && SrcCk->CkType != RIFF_FOURCC("data")) {
				//! Allocate memory for chunk header and data
				struct WAV_Chunk_t *DstCk = malloc(sizeof(struct WAV_Chunk_t) + SrcCk->CkSize);
				if(DstCk) {
					//! Fille out new chunk and read from source file
					DstCk->CkType = SrcCk->CkType;
					DstCk->CkSize = SrcCk->CkSize;
					DstCk->Prev   = Prev;
					DstCk->Next   = NULL;
					if(Prev) Prev->Next     = DstCk;
					else     FileOut.Chunks = DstCk;
					fseek(FileIn.File, SrcCk->FileOffs, SEEK_SET);
					fread(DstCk+1, SrcCk->CkSize, 1, FileIn.File);
					Prev = DstCk;
				}
			}
			SrcCk = SrcCk->Next;
		}
	}

	//! Allocate reading buffer
	AllocBuffer = malloc(2 * sizeof(float)*BlockSize*FileIn.fmt->nChannels);
	if(!AllocBuffer) {
		printf("ERROR: Couldn't allocate reading buffer.\n");
		ExitCode = -1; goto Exit_FailCreateAllocBuffer;
	}
	float *ReadBuffer = (float*)AllocBuffer;
	float *OutBuffer  = ReadBuffer + BlockSize*FileIn.fmt->nChannels;

	//! Because the freeze start point might not be block-aligned, we copy
	//! samples directly until one block before the freeze start point; we
	//! then use this block to prime the processor.
	{
		int nSmpRem = FreezeStart - XformPrimingLength;
		while(nSmpRem) {
			int N = nSmpRem;
			if(N > BlockSize) N = BlockSize;
			nSmpRem -= N;
			WAV_ReadAsFloat(&FileIn, ReadBuffer, N);
			WAV_WriteFromFloat(&FileOut, ReadBuffer, N);
		}
		WAV_ReadAsFloat(&FileIn, ReadBuffer, BlockSize);
		LoopEnd -= FreezeStart;
	}

	//! If we need to capture a snapshot, do so now and put it in OutBuffer
	if(SnapshotPos >= 0) {
		uint32_t OldPos = FileIn.SamplePosition;
		FileIn.SamplePosition = SnapshotPos;
		WAV_ReadAsFloat(&FileIn, OutBuffer, BlockSize);
		FileIn.SamplePosition = OldPos;

		//! Apply gain
		int n;
		if(SnapshotGain != 1.0f) {
			for(n=0;n<BlockSize*FileIn.fmt->nChannels;n++) OutBuffer[n] *= SnapshotGain;
		}
	}

	//! Initialize state
	State.nChan        = FileIn.fmt->nChannels;
	State.BlockSize    = BlockSize;
	State.nHops        = nHops;
	State.FreezeStart  = BlockSize;
	State.FreezePoint  = BlockSize + FreezePoint - FreezeStart;
	State.FreezeFactor = FreezeFactor;
	State.FreezeAmp    = FreezeAmp;
	State.FreezePhase  = FreezePhase;
	if(!Spectrice_Init(&State, WindowType, ReadBuffer, (SnapshotPos >= 0) ? OutBuffer : NULL)) {
		printf("ERROR: Unable to initialize processor.\n");
		ExitCode = -1; goto Exit_FailInitSpectrice;
	}

	//! Begin processing
	int nSamplesRem = FileIn.nSamplePoints - FreezeStart + XformPrimingLength;
	int nLoopSamplesRem = LoopEnd;
	int Block, nBlocks = (nSamplesRem - 1) / BlockSize + 1;
	for(Block=0;Block<nBlocks;Block++) {
		printf("\rBlock %u/%u (%.2f%%)", Block+1, nBlocks, Block*100.0f/nBlocks);

		int nOutputSmp = nSamplesRem;
		if(nOutputSmp > BlockSize) nOutputSmp = BlockSize;
		nSamplesRem -= nOutputSmp;

		//! Make sure to wrap around at the loop point
		int nReadSmpRem = nOutputSmp;
		float *NextDst = ReadBuffer;
		for(;;) {
			if(LoopProcess && !nLoopSamplesRem) {
				//! Rewind to loop start
				FileIn.SamplePosition -= LoopLen;
				nLoopSamplesRem       += LoopLen;
			}

			int nSmpThisRun = nReadSmpRem;
			if(LoopProcess && nSmpThisRun > nLoopSamplesRem) nSmpThisRun = nLoopSamplesRem;
			WAV_ReadAsFloat(&FileIn, NextDst, nSmpThisRun);

			nReadSmpRem     -= nSmpThisRun;
			nLoopSamplesRem -= nSmpThisRun;
			NextDst         += nSmpThisRun * FileIn.fmt->nChannels;
			if(!nReadSmpRem) break;
		}
		{
			//! Clear end of buffer if needed
			int n, N = BlockSize - nOutputSmp;
			for(n=0;n<N*FileIn.fmt->nChannels;n++) *NextDst++ = 0.0f;
		}
		Spectrice_Process(&State, OutBuffer, ReadBuffer);
		WAV_WriteFromFloat(&FileOut, OutBuffer, nOutputSmp);
	}
	printf("\nOk.");

	//! Exit points
Exit_FailInitSpectrice:
	free(AllocBuffer);
Exit_FailCreateAllocBuffer:
	{
		//! Save pointer to chunks data and close file
		struct WAV_Chunk_t *Ck = FileOut.Chunks;
		WAV_Close(&FileOut);

		//! Delete output file chunks
		while(Ck) {
			struct WAV_Chunk_t *Next = Ck->Next;
			free(Ck);
			Ck = Next;
		}
	}
Exit_FailCreateOutFile:
Exit_FailGetFreezePoint:
Exit_FailFileLength:
	WAV_Close(&FileIn);
Exit_FailOpenInFile:
Exit_BadArgs:
	return ExitCode;
}

/**************************************/
//! EOF
/**************************************/
