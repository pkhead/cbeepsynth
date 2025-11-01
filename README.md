# cbeepsynth
This is a port of the [BeepBox](https://beepbox.co) synthesizers/effects and song player, written as a C library. It also implements some modded additions.

- **bpbxsyn:** The synthesizer library, located under the `synth/` directory.
- **beepbox:** The song player library, located under the `song/` directory.

This project is structured such that the synth library can be used independently of the song player library.

## To Be Implemented:
### Synth
- Instruments:
    - Custom chip
    - FM6 (from mods)
    - Noise
    - Picked string
    - Supersaw

### Song
- Loading BeepBox/JummBox song URL data into a song structure.
- Song playback

## Building/Usage
Prerequisites:
- C99-compliant compiler
- CMake

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

### Synth
#### Use as a CMake static library
```cmake
# include the library
set(BEEPBOX_SYNTH_BUILD_STATIC TRUE)
add_subdirectory(deps/cbeepsynth)

# link with the library
target_link_libraries(your_target PRIVATE beepbox_synth_static)
```

#### Create a shared library
```bash
mkdir build
cd build
cmake -DBEEPBOX_SYNTH_BUILD_SHARED=1 ..
cmake --build .
# it will create a shared object/DLL named bpbxsynth
```

### Song
#### Use as a CMake static library
```cmake
# there are three ways to link bpbxsynth with the beepbox song player:

# 1. build bpbxsynth as a static library
set(BEEPBOX_SYNTH_BUILD_STATIC TRUE)
set(BEEPBOX_BUILD_STATIC TRUE)
set(BEEPBOX_SYNTH_LIB beepbox_synth_static)

# 2. build bpbxsynth as a dynamic library
set(BEEPBOX_SYNTH_BUILD_SHARED TRUE)
set(BEEPBOX_BUILD_STATIC TRUE)
set(BEEPBOX_SYNTH_LIB beepbox_synth_shared)

# 3. find the bpbxsynth package on your system, and link with it ?
find_package(BeepBoxSynth REQUIRED)
set(BEEPBOX_BUILD_STATIC TRUE)
set(BEEPBOX_SYNTH_LIB BeepBoxSynth::LibName)

add_subdirectory(deps/cbeepsynth)

# link with the library
target_link_libraries(your_target PRIVATE beepbox_static)
```

#### Create a shared library
```bash
mkdir build
cd build
cmake -DBEEPBOX_BUILD_SHARED=1 -DBEEPBOX_SYNTH_BUILD_STATIC=1 -DBEEPBOX_SYNTH_LIB=beepbox_synth_static ..
cmake --build .
# it will create a shared object/DLL named beepbox
```

## Credits
- [BeepBox](https://beepbox.co/), created by John Nesky, as well as community-created mods:
    - [JummBox](https://jummb.us/), which includes expanded instrument configuration and a "custom chip" instrument type.
    - [GoldBox](https://aurysystem.github.io/), which includes a 6-operator FM.
    - [UltraBox](https://ultraabox.github.io/) and [Slarmoo's Box](https://slarmoo.github.io/slarmoosbox), which includes more FM frequency ratios.
