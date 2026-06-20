# SpinTool

Introduction
-----

Spintool is a fork of Eggplant891 project with fixes and news opts. It is a utility created with the intention of research and doing modifications on your own Sonic Spinball binary.

This project was tested successfully with the game from Steam Version of "SEGA Mega Drive and Genesis Classics"

Suggestions, ideas, bugs reports and fixes are welcome ! Thanks in advance ! 

Changes
-----

CORE : 
- Fixed segfault & random crash issues : Works on Linux and Windows
- Settings : Font size Zoom level added
- "roms" folder will contain your original game to use it as reference. After you select it, it will see if you have a modified version in "rom_export" :
  - if yes, this file will be loaded immediately
  - if no, a copy of the file will be created and loaded
  
TOOLS - SPRITE NAVIGATOR :
- Updated elements / windows gui
- Bonus Level Sprites support added : SSC support added
- Main Sprites and Bonus Level Sprites can be import and export easily : you have just to choose correctly the palette
- After sprites modifications, modifications are saved directly on the file in "rom_export" folder

TOOLS - TILE LAYOUT VIEWER :
- Fixed crash when Lava Powerhouse level is loaded
  - Added secure loads in this tool
- Added Views for Intro
- Added Bonus Views
- After levels modificationss, modifications are saved directly on the file in "rom_export" folder

Prerequisites
-----

The following libraries are required to build and run Spintool:
- SDL 3 (https://github.com/libsdl-org/SDL)
- SDL_Image (https://github.com/libsdl-org/SDL_image)

On linux : mkdir build && cmake .. && make -j8

See worklows actions about the commands used to compile a Linux native app and a Windows native app with a Linux Environment System
