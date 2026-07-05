# Sonic Spintool
## Table of Contents
- [Introduction](#introduction)
- [Pre-requisites](#pre-requisites)
- [Compiling](#compiling)
  - [Linux](#linux)
- [Notes](#notes)
  - [Compatibility](#compatibility)
  - [LZW Compression](#lzw-compression)

## Introduction

Sonic Spintool is a utility created with the intention of research and hacking Sonic Spinball binaries. Hacks include modifications of assets such as:

- Sprites
- Level tiles
- Levels & Level layouts
- Music / SFX
- Game logic and other tweakables

The original motivation of any implementations is to serve the purposes of research, but the ability to also create new mods & hacks is part of this goal.

## Pre-requisites
The following libraries are required to build and run Spintool:
- SDL 3 (https://github.com/libsdl-org/SDL)
- SDL_Image (https://github.com/libsdl-org/SDL_image)

## Compiling
### Linux
```
mkdir build && cmake .. && make -j8
```

See workflows actions about the commands used to compile a Linux native app and a Windows native app with a Linux Environment System

Suggestions, ideas, bugs reports and fixes are welcome ! Thanks in advance ! 

# Notes

Compatibility
---
This project was tested using the ROM from Steam Version of "SEGA Mega Drive and Genesis Classics"

LZW Compression
-----
_June 21th, 2026_
Sonic Spinball stores some graphical data ( for example Bonus Stages, Tails' Plane ) using an LZW-based compression format (called on SpinTool as `Compressed2`).

When a PNG is imported, SpinTool converts the image back into Mega Drive tile data and recompresses the complete graphics block before writing it to the ROM.

Changing only a few pixels or colors does not change the number of tiles, but it can still change the compressed size. LZW compression depends on repeated byte sequences. A small graphical modification may reduce the number of repeated patterns, causing the compressed data to become larger.

SpinTool uses an optimized LZW compressor that tries several compatible dictionary-reset strategies and keeps the smallest valid result. The generated data remains fully compatible with the original game decompressor.

**The original graphics block has a fixed amount of space available in the ROM. SpinTool does not relocate or enlarge this block to avoid ROM corruption.**

The import follows these rules:
* If the new compressed data fits within the original capacity, the import is accepted.
* If the compressed data is smaller, the unused bytes are safely cleared.
* If the smallest possible result is still larger than the original capacity, the import is refused.
* The ROM is not modified when an import is refused.
* If the imported PNG produces exactly the same tile data as the current frame, SpinTool keeps the original compressed stream and performs no write.

An image can therefore have exactly the same dimensions as the original and still exceed the available capacity. The limitation is not related to the PNG size or the sprite dimensions. It is determined only by the size of the resulting compressed tile data.
