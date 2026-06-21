# SpinTool

## Introduction

Spintool is a fork of Eggplant891 project with fixes and news opts. It is a utility created with the intention of research and doing modifications on your own Sonic Spinball binary.

This project was tested successfully with the game from Steam Version of "SEGA Mega Drive and Genesis Classics"

Suggestions, ideas, bugs reports and fixes are welcome ! Thanks in advance ! 

## Changes

GUI / CORE : 
-----
- Fixed segfault & random crash issues : Works on Linux and Windows
- Settings : Font size Zoom level added
- "roms" folder will contain your original game to use it as reference. After you select it, it will see if you have a modified version in "rom_export" :
  - if yes, this file will be loaded immediately
  - if no, a copy of the file will be created and loaded
  
TOOLS - SPRITE NAVIGATOR :
-----
- Updated elements / windows gui
- Bonus Level and Tails Plane Sprites support added : Optimized LZW support added
- Main Sprites and Bonus Level Sprites can be import and export easily : you have just to choose correctly the palette
- After sprites modifications, modifications are saved directly on the file in "rom_export" folder

TOOLS - TILE LAYOUT VIEWER :
-----
- Fixed crash when Lava Powerhouse level is loaded
  - Added secure loads in this tool
- Added Views for Intro
- Added Bonus Views
- After levels modifications, modifications are saved directly on the file in "rom_export" folder

## NOTES

LZW Compression in SpinTool - June 21th, 2026 :
-----
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


## Prerequisites

The following libraries are required to build and run Spintool:
- SDL 3 (https://github.com/libsdl-org/SDL)
- SDL_Image (https://github.com/libsdl-org/SDL_image)

On linux : mkdir build && cmake .. && make -j8

See worklows actions about the commands used to compile a Linux native app and a Windows native app with a Linux Environment System
