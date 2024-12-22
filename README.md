Sonic Spintool
=====

Introduction
-----

Sonic Spintool is a utility created with the intention of research and hacking Sonic Spinball binaries. Hacks include modifications of assets such as:

- Sprites
- Level tiles
- Levels & Level layouts
- Music / SFX
- Game logic and other tweakables

The original motivation of any implementations is to serve the purposes of research, but the ability to also create new mods & hacks is part of this goal.

Prerequisites
-----

*(this should be managed by cmake in future)*

The source code currently requires the use of Visual Studio 2022 (with C++ 17 toolchains)

The following libraries are required to build and run Sonic Spintool:
- SDL 3 (https://github.com/libsdl-org/SDL)
- SDL_Image (https://github.com/libsdl-org/SDL_image)

.lib files both of these libraries must be placed in `\lib\x64\`
The binaries for these libraries must also be build and accessible by the built executable *(i.e. by placing them in the same directory as the executable file)*

