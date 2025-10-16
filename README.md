# TinySoundFontPlayer

TinySoundFontPlayer is an [iPlug2](https://github.com/iPlug2/iPlug2) based SoundFont player, made with a modified version of [TinySoundFont](https://github.com/NeRdTheNed/TinySoundFont/tree/temp2). These modifications are a combination of the work done by [firodj](https://github.com/firodj/tsf) and [atsushieno](https://github.com/atsushieno/TinySoundFont/tree/split-render), plus a few bodge fixes and enhancements by me.

Currently only the AudioUnit build has been tested on ARM macOS, so please report any bugs you find!

## Features

- Able to load and play SF2 and SF3 files
- SoundFont modulators are supported
- Configurable sample interpolation:
  - None
  - Linear (default)
  - 4 point Watte, cubic Hermite, Lagrange, bspline
  - 6 point cubic Hermite, Lagrange, bspline
- Pseudo-oversampling support (none, 2x, 4x, 8x, 16x)
- Responds well to standard MIDI control messages

## Known issues

- MIDI pan / modulators which influence panning are not currently working due to bugs.
- TinySoundFont has some known issues related to attenuation levels and envelope behaviour. Notable current issues are schellingb/TinySoundFont#95, schellingb/TinySoundFont#96, schellingb/TinySoundFont#97.

## License

TinySoundFontPlayer is licensed under the MIT license. A copy of this license can be found in the file LICENSE.txt. Additionally, a complete list of licenses and credits for all libraries used in this project can be found in the file CREDITS.txt.
