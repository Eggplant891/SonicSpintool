# Cross-compile a 64-bit Windows executable from Linux with MinGW-w64.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Avoid configure-time attempts to run Windows executables on Linux.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
