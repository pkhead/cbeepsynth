# cbeepsynth
This is a port of the [BeepBox](https://beepbox.co) synthesizers, written as a C library. It also implements some modded additions.

**Implemented:**
- FM synthesizer
- Envelopes

**To be implemented:**
- Instruments:
    - Chip wave
    - FM6 (from mods)
    - Noise
    - Pulse width
    - Harmonics
    - Spectrum
    - Picked string
    - Supersaw
- Instrument effects:
    - Transition type
    - Chord type?
    - Pitch shift, detune
    - Vibrato
    - Note filter
- Audio effects:
    - Distortion
    - Bitcrusher
    - Panning
    - Chorus (or is this an inst effect?)
    - Echo
    - Reverb

## Building/Usage
Clone or get this repository as a Git submodule
```bash
git clone https://github.com/pkhead/cbeepsyn
# or
git submodule add https://github.com/pkhead/cbeepsyn
```

A Lua 5.1+ interpreter is needed if you wish to regenerate the FM algorithm code. Simply run:
```bash
lua fm_algo_gen.lua
```

### Usage as a CMake library
```cmake
# include the library
add_subdirectory(deps/cbeepsyn)

# link with the library
target_link_libraries(your_target PRIVATE beepbox_synth_static)

# or you can do so statically...
target_link_libraries(your_target PRIVATE beepbox_synth_shared)
```

## Credits
- [BeepBox](https://beepbox.co/), created by John Nesky.