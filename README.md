## MikPlayer

### Overview
MikPlayer is a 32-bit protected mode DOS application that plays tracker modules (MOD, S3M, XM, IT) using the MikMod library. It features a colorful text-mode user interface, providing real-time playback visualization and interactive controls.

#### Technical architecture
- Compiler: Open Watcom C/C++ (wcl386)
- Target: 32-bit DOS with DPMI (DOS/4GW or DOS32A extender)
- Audio Library: libMikMod 3.3.13 (statically linked)
- Memory Model: Flat 32-bit addressing
- Optimizations: Pentium-class optimizations (-5s), inline math functions (-om), FPU support (-fp5 -fpi87)
- Zero dependencies: Single statically-linked executable
- Real-time VU meter: Based on actual voice volumes via Voice_GetVolume()
- Smart channel detection: Uses Voice_Stopped() to track active voices
- Precision timing: clock()-based update intervals with sub-second accuracy
- Compiler Flags: -5s -fp5 -fpi87 -ox -om -s -ot
- Compiler Flags 386 build: -3s -fp3 -fpi87 -ox -om -s -ot

#### Audio
- Mixing: Software mixing via libMikMod (DMODE_SOFT_MUSIC)
- Sample Rate: 22050 Hz default (11025/44100 Hz configurable)
- Channels: Stereo (mono on 386 for performance)
- Voice Allocation: Up to 64 virtual voices (8 on 386, 16 on 486)
- Smart Loading: Files <6MB loaded to RAM, larger files streamed from disk

#### Display
- Direct VRAM writing to 0xB8000 for flicker-free updates
- IBM CP437 art (218, 191, 196, 179, etc.) for a sweet nostalgia UI
- BIOS color attributes - 8-bit combined foreground/background
- Text Mode: 80x25
- Refresh Rate: 8 updates per second (configurable)

#### CPU-specific profiles
- 386 Mode: 11kHz mono, 8 voices, 1 update/sec
- 486 Mode: 22kHz stereo, 16 voices
- Hi-Fi Mode: 44kHz stereo, 64 voices

#### Memory
- Threshold-based loading: Files <6MB loaded to memory via Player_LoadMem()
- Streaming playback: Larger files use Player_Load() for disk streaming
- Reduces memory footprint on systems with limited RAM

#### Supported formats (libMikMod)
- MOD - ProTracker/NoiseTracker (Amiga classic, 4 channels)
- S3M - Scream Tracker 3 (up to 32 channels)
- XM - FastTracker 2 (up to 32 channels)
- IT - Impulse Tracker (up to 64 channels, NNA support)
- Even more, but I havent tested yet: MTM, ULT, FAR, MED, 669, STM, and more

#### Compile MikMod
Inside the libmikmod folder, there is a *dos\* folder with Makefiles for DJGPP and WATCOMC.
```
cd libmikmod-3.3.13
cd dos
wmake -f Makefile.wat
```

#### Compile MikPlay
Using the dos32a extender and optimizations:
```
wcl386 -l=dos32a -5s -bt=dos -fp5 -fpi87 -mf -oeatxh -w4 -ei -zp8 -zq -dMIKMOD_STATIC=1 -i..\libmikmod-3.3.13\include\ mikplay.c ..\libmikmod-3.3.13\dos\mikmod.lib
```

#### Thanks to beta-testers
@philip_petev
@mbgaudio