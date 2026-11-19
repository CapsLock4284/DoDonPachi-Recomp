# Name:        CMakeLists.txt
# Description: the build of DoDonPachi's port: the board's runtime (Cave68kSDK, runtime/), the translated program
#              (src/) and, with SDL2, the game in a window
# Version:     0.1.0 (draft)
# Created:     16-11-2026
#
# After DonPachi Recomp's CMakeLists.txt (itself modelled on tmntarcade-decomp's, BSD-3-Clause, Sebastian Toyos).
#
#   cmake -S . -B build/msvc                          (Windows)
#   cmake --build build/msvc --config Release
#   cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release     (Linux)
#   cmake --build build/linux -j
#
# The executable goes to build/bin (Windows) or build/bin-linux (Linux): ddonpach, the game in a window, with SDL2
# (on Windows its development package, SDL2-devel-2.x.x-VC.zip, unpacked in build/downloads/sdl2; on Linux the
# system's, e.g. libsdl2-dev). It reads the player's own ROM set, MAME's ddonpach.zip (--rom PATH, default rom: the
# folder holding it). The translated program comes first: python tools/decompile.py writes src/.
cmake_minimum_required(VERSION 3.20)
# The version: the one place it is set
project(ddonpach VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(WIN32)
	set(DDONPACH_BIN_DEFAULT ${CMAKE_SOURCE_DIR}/build/bin)
else()
	string(TOLOWER "${CMAKE_SYSTEM_NAME}" DDONPACH_SYSTEM)
	set(DDONPACH_BIN_DEFAULT ${CMAKE_SOURCE_DIR}/build/bin-${DDONPACH_SYSTEM})
endif()
set(DDONPACH_BIN_DIR ${DDONPACH_BIN_DEFAULT} CACHE PATH "Where the executables go")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release)    # (a single-configuration build, as on Linux: optimised unless asked otherwise)
endif()
foreach(config IN ITEMS "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${config} ${DDONPACH_BIN_DIR})
endforeach()

if(MSVC)
	# the C runtime built into the executables: a player needs no Visual C++ Redistributable installed
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	add_compile_options(/W3 /utf-8)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# The board (Cave68kSDK): its memory map, the EEPROM, the picture, the sound, the ROM set's reader, the board
# descriptions
add_library(ddonpach_runtime STATIC
	runtime/src/machine.c
	runtime/src/video.c
	runtime/src/rom_set.c
	runtime/src/inflate.c
	runtime/src/cheats.c
	runtime/src/sound.c
	runtime/src/free_play.c
	runtime/src/boards/donpachi.c
	runtime/src/boards/ddonpach.c
)
target_include_directories(ddonpach_runtime PUBLIC runtime/include PRIVATE runtime/src)
if(NOT MSVC)
	target_link_libraries(ddonpach_runtime PUBLIC m)    # (sound.c: pow, floor, tanf)
endif()

# The game: the translated 68000 program (tools/decompile.py writes src/) on its runtime (game_program.c: the
# frames, the interrupts, the program's data and tables from the ROM set; tasks.c: the program's own stack), with
# the board's memories for this game (include/game_board.h)
set(DDONPACH_DECOMPILED_DIR ${CMAKE_SOURCE_DIR}/src CACHE PATH "The game's C (tools/decompile.py)")
file(GLOB DDONPACH_DECOMPILED CONFIGURE_DEPENDS ${DDONPACH_DECOMPILED_DIR}/*.c ${DDONPACH_DECOMPILED_DIR}/*/*.c)
if(NOT DDONPACH_DECOMPILED)
	message(FATAL_ERROR "No translated program in ${DDONPACH_DECOMPILED_DIR}: run python tools/decompile.py first")
endif()
add_library(ddonpach_game STATIC runtime/src/game_program.c runtime/src/tasks.c ${DDONPACH_DECOMPILED})
target_include_directories(ddonpach_game PUBLIC include runtime/include ${DDONPACH_DECOMPILED_DIR})
target_link_libraries(ddonpach_game PUBLIC ddonpach_runtime)
if(MSVC)
	# (large source files; the translator's casts between widths)
	target_compile_options(ddonpach_game PRIVATE /bigobj /wd4244 /wd4146 /wd4018 /wd4554)
else()
	target_compile_options(ddonpach_game PRIVATE -fno-strict-aliasing)
endif()

# The game in a window (ports/sdl2): SDL2 from the system, or on Windows the development package unpacked in
# build/downloads/sdl2, its library copied beside the executable
set(DDONPACH_SDL2_UNPACKED "")
if(WIN32)
	file(GLOB DDONPACH_SDL2_UNPACKED ${CMAKE_SOURCE_DIR}/build/downloads/sdl2/SDL2-*/cmake)
endif()
find_package(SDL2 CONFIG QUIET HINTS ${DDONPACH_SDL2_UNPACKED})
if(SDL2_FOUND)
	set(DDONPACH_SDL2 SDL2::SDL2)
	if(TARGET SDL2::SDL2main)
		list(APPEND DDONPACH_SDL2 SDL2::SDL2main)
	endif()
elseif(NOT WIN32)
	find_package(PkgConfig QUIET)    # (a system with SDL2's pkg-config file but no CMake one)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
		if(SDL2_FOUND)
			set(DDONPACH_SDL2 PkgConfig::SDL2)
		endif()
	endif()
endif()
if(SDL2_FOUND)
	add_executable(ddonpach WIN32 ports/sdl2/main.c ports/sdl2/settings.c ports/sdl2/rom_drop.c ports/sdl2/paths.c
		ports/sdl2/crt.c ports/sdl2/scaler.c ports/sdl2/crash_log.c ports/sdl2/menu.c ports/sdl2/hiscore.c
		ports/sdl2/widgets.c ports/sdl2/training.c ports/sdl2/replay.c)
	target_link_libraries(ddonpach PRIVATE ddonpach_game ${DDONPACH_SDL2})
	target_compile_definitions(ddonpach PRIVATE DDONPACH_VERSION="${PROJECT_VERSION}")
	if(WIN32)
		target_sources(ddonpach PRIVATE ports/sdl2/icon/ddonpach.rc)    # (the .exe's icon)
		add_custom_command(TARGET ddonpach POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:ddonpach>)
	elseif(UNIX AND NOT APPLE)
		set_target_properties(ddonpach PROPERTIES BUILD_RPATH "\$ORIGIN" INSTALL_RPATH "\$ORIGIN")
	endif()
else()
	message(STATUS "No SDL2: the window port (ddonpach) is not built")
endif()
