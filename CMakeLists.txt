cmake_minimum_required(VERSION 4.1)

project(spintool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# SDL is supplied by the native Linux packages or by vcpkg when
# cross-compiling from Linux to Windows with MinGW-w64.
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)

add_executable(spintool
        external/imgui/backends/imgui_impl_sdl3.cpp
        external/imgui/backends/imgui_impl_sdlrenderer3.cpp
        external/imgui/misc/cpp/imgui_stdlib.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_demo.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/imgui.cpp
        src/editor/editor_brush.cpp
        src/editor/editor_level.cpp
        src/editor/editor_project.cpp
        src/editor/game_obj_manager.cpp
        src/editor/spline_manager.cpp
        src/editor/tile_brush_manager.cpp
        src/editor/tile_layout_manager.cpp
        src/rom/culling_tables/animated_object_culling_table.cpp
        src/rom/culling_tables/game_obj_collision_culling_table.cpp
        src/rom/culling_tables/spline_culling_table.cpp
        src/rom/game_objects/game_object_definition.cpp
        src/rom/game_objects/game_object_flipper.cpp
        src/rom/game_objects/game_object_ring.cpp
        src/rom/animated_object.cpp
        src/rom/animation_sequence.cpp
        src/rom/collision_tile.cpp
        src/rom/colour.cpp
        src/rom/level.cpp
        src/rom/lzss_decompressor.cpp
        src/rom/compressed2_optimizer.cpp
        src/rom/bonus_stage_decoder.cpp
        src/rom/tails_plane_decoder.cpp
        src/rom/title_screen_decoder.cpp
        src/rom/palette.cpp
        src/rom/rom_asset_definitions.cpp
        src/rom/rom_data.cpp
        src/rom/spinball_rom.cpp
        src/rom/sprite.cpp
        src/rom/sprite_tile.cpp
        src/rom/ssc_compressor.cpp
        src/rom/ssc_decompressor.cpp
        src/rom/tile.cpp
        src/rom/tile_brush.cpp
        src/rom/tile_layout.cpp
        src/rom/tileset.cpp
        src/types/blit_settings.cpp
        src/types/bounding_box.cpp
        src/ui/ui_animation_navigator.cpp
        src/ui/ui_editor.cpp
        src/ui/ui_editor_window.cpp
        src/ui/ui_file_selector.cpp
        src/ui/ui_palette.cpp
        src/ui/ui_palette_viewer.cpp
        src/ui/ui_sprite.cpp
        src/ui/ui_sprite_importer.cpp
        src/ui/ui_sprite_navigator.cpp
        src/ui/ui_sprite_viewer.cpp
        src/ui/ui_tile_editor.cpp
        src/ui/ui_tile_layout_viewer.cpp
        src/ui/ui_tile_picker.cpp
        src/ui/ui_tileset_navigator.cpp
        src/main.cpp
        src/render.cpp
        src/rom/metadata/rom_metadata.cpp
        src/serialisation/editor_serialiser.cpp)

target_include_directories(spintool PRIVATE
    external
    external/imgui
    external/imgui/backends
    external/imgui/misc/cpp
    external/imgui/misc/freetype
    external/imgui/misc/single_file
    external/nlohmann
    redist
    redist/editor
    redist/rom
    redist/rom/culling_tables
    redist/rom/game_objects
    redist/types
    redist/ui
)

target_link_libraries(spintool PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
)

if(WIN32)
    target_compile_definitions(spintool PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    set_target_properties(spintool PROPERTIES
        OUTPUT_NAME "spintool"
        SUFFIX ".exe"
    )
else()
    set_target_properties(spintool PROPERTIES
        OUTPUT_NAME "spintool"
    )
endif()

include(GNUInstallDirs)
install(TARGETS spintool
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
