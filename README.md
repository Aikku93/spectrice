# Spectrice
Spectrice is a spectral freezing tool, mostly intended for freezing the spectral character of a sample in preparation for looping.

## Getting started

### Prerequisites
No libraries are required, only a compiler. The Makefile might need adjusting, however, if your target platform is not Windows, or you need to adjust the architectural features (eg. disable AVX, FMA, etc.).

### Installing
After adjusting the Makefile as needed, run ```make all``` to build the tool.

## Usage
Spectrice uses WAV files for input/output, in 8-bit PCM, 16-bit PCM, 24-bit PCM, or 32-bit IEEE floating-point formats.

The processing library itself works on 32-bit floating-point blocks of data, however, and can be used standalone.

### Processing
```spectrice Input.wav Output.wav [Options]```

Options:

| Option            | Effect                                                                               |
| ----------------- | ------------------------------------------------------------------------------------ |
| `-blocksize:X`    | Set transform block size. (Default: 8192, Minimum: 16, Maximum: 65536)               |
| `-nhops:X`        | Set number of hops per transform block. (Default: 8. Minimum depends on window type) |
| `-window:X`       | Set analysis+synthesis window function.                                              |
|                   | Can be any of: `sine`, `hann`, `hamming`, `blackman`, `nuttall`. (Default: Nuttall)  |
| `-freezexfade:X`  | Set number of samples to crossfade/blend prior to the freeze point. (Default: 0)     |
| `-freezepoint:X`  | Set the point at which the freezing effect is at full strength. (Default: 0, but this is useless) |
| `-freezefactor:X` | Set strength of freezing effect. (Default: 1.0)                                      |
| `-nofreezeamp`    | Do not freeze amplitude (useless by itself; combine with `freezephase`.              |
| `-freezephase`    | Freeze the phase step.                                                               |
| `-format:default` | Set the output file format (`PCM8`, `PCM16`, `PCM24`, `FLOAT32`, or `default`).      |

## Possible issues
* Due to limitations in STFT re-synthesis, loops might 'pop' at the boundaries. Increasing the number of hops, or moving the loop to a zero-crossing after freezing might help.
* Using small or overly-large transform sizes might result in strange freezing characteristics. This may be useful for effects, but might not be desired.
* A number of different windows are provided, as they all provide different pros and cons; it may be necessary to experiment with the window type and number of hops.
* Phase step freezing isn't perfect, as this can't capture small deviations caused by off-center frequencies during DFT analysis. In practice, this can result in an "infinitely evolving" waveform.

## High-level overview of algorithm
1. Decompose signal using STFT (using a centered DFT algorithm based on DCT-IV and DST-IV).
2. Transform Re/Im pairs into Amplitude/Phase.
3. Apply freezing:
   * Interpolate amplitude from the last segment based on the freezing ratio and crossfade time (without `-nofreezeamp`).
   * Interpolate phase step from the last segment based on the freezing ratio and crossfade time (with `freezephase`).
4. Transform back to Re/Im pairs.
5. Apply inverse STFT.

## Future plans

* ~~Add WAV file support.~~ Added 2022/12/14
* Add support for freezing only a specified loop section, with correct handling of wrapping behaviour.

## Authors
* **Ruben Nunez** - *Initial work* - [Aikku93](https://github.com/Aikku93)

## License
Spectrice is released under the GPLv3 license. See the LICENSE file for full terms.

## Acknowledgements
* Huge thanks to `musicalman` for testing and providing ideas to implement.
