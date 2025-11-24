COMPILER := g++
COMPILER_FLAGS := -std=c++17 -D IMGUI_DEFINE_MATH_OPERATORS -lSDL3 -lSDL3_image

OBJ_DIR = intermediate/obj
DEP_DIR = intermediate/deps
INCLUDE_DIRS := \
-I $(realpath ./redist) \
-I $(realpath ./external) \
-I $(realpath ./external/imgui) \
-I $(realpath ./external/SDL3) \
-I $(realpath ./external/nlohmann)

CPP_FILES := $(wildcard src/*.cpp) $(wildcard src/*/*.cpp) $(wildcard src/*/*/*.cpp) $(wildcard external/imgui/*.cpp) $(wildcard external/imgui/backends/*.cpp)

OUTPUT_EXECUTABLE := SonicSpintool

$(OUTPUT_EXECUTABLE):
	@echo --- Begin Compilation ---
	$(COMPILER) $(INCLUDE_DIRS) -o $(OUTPUT_EXECUTABLE) $(CPP_FILES) $(COMPILER_FLAGS)
	@echo -- End Compilation ---
