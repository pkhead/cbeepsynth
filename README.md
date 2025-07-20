# cbeepsynth
This is a port of the [BeepBox](https://beepbox.co) synthesizers, written as a C library. It also implements some modded additions.

**Implemented:**
- FM synthesizer
- Envelopes

**To be implemented:**
- Instruments:
    - Chip wave
    - Custom chip/sampler
    - FM6 (from mods)
    - Noise
    - Pulse width
    - Harmonics
    - Spectrum
    - Picked string
    - Supersaw
- Instrument effects:
    - Transition type
    - Chord type
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
git clone https://github.com/pkhead/cbeepsynth
# or
git submodule add https://github.com/pkhead/cbeepsynth
```

A Lua 5.1+ interpreter is needed if you wish to regenerate the FM algorithm code. Simply run:
```bash
lua fm_algo_gen.lua
```

### Use as a CMake static library
```cmake
# include the library
set(BEEPBOX_SYNTH_BUILD_STATIC 1)
add_subdirectory(deps/cbeepsynth)

# link with the library
target_link_libraries(your_target PRIVATE beepbox_synth_static)
```

### Create a shared library
```bash
mkdir build
cd build
cmake -DBEEPBOX_SYNTH_BUILD_SHARED=1 ..
cmake --build .
# it will create a shared object named beepboxsynth
```

## Credits
- [BeepBox](https://beepbox.co/), created by John Nesky, as well as community-created mods:
    - [JummBox](https://jummb.us/), which includes expanded instrument configuration and a "custom chip" instrument type.
    - [GoldBox](https://aurysystem.github.io/), which includes a 6-operator FM.
    - [UltraBox](https://ultraabox.github.io/) and [Slarmoo's Box](https://slarmoo.github.io/slarmoosbox), which includes more FM frequency ratios.
