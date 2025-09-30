# TinySoundFontPlayer

TinySoundFontPlayer is an [iPlug2](https://github.com/iPlug2/iPlug2) based SoundFont player, made with a modified version of [TinySoundFont](https://github.com/NeRdTheNed/TinySoundFont/tree/temp). These modifications are a combination of the work done by [firodj](https://github.com/firodj/tsf) and [atsushieno](https://github.com/atsushieno/TinySoundFont/tree/split-render), plus a few bodge fixes and enhancements by me.

Currently only the AudioUnit build has been tested on ARM macOS, so please report any bugs you find!

## Features

- Able to load and play SF2 and SF3 files
- SoundFont modulators are supported
- Cubic interpolation ("4-point, 3rd-order Hermite")
- Responds well to standard MIDI control messages

## Known issues

- MIDI pan / modulators which influence panning are not currently working due to bugs.
- SoundFont filter features / modulators which influence SoundFont filter are not currently working due to bugs.
- UI may sometimes not show current preset

## License

TinySoundFontPlayer is licensed under the MIT license.
