cmake_minimum_required(VERSION 3.20)
project(StreetsOfRageCollection C CXX)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- The game: games/<GAME>/ ---------------------------------------------------
# Configure once per game, each in its own folder:
#   cmake -S . -B build/<game> -G Ninja -DGAME=<game> -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
if(NOT GAME)
  file(GLOB GAME_DIRS RELATIVE ${CMAKE_SOURCE_DIR}/games ${CMAKE_SOURCE_DIR}/games/*)
  message(FATAL_ERROR "Pass -DGAME=<name>, one of: ${GAME_DIRS}")
endif()
set(GAME_DIR ${CMAKE_SOURCE_DIR}/games/${GAME})
set(GAME_GEN ${CMAKE_SOURCE_DIR}/generated/${GAME})
set(GAME_CONFIG ${CMAKE_SOURCE_DIR}/build/${GAME}/config)
set(GAME_ENV ${CMAKE_COMMAND} -E env GAME=${GAME})
execute_process(COMMAND ${GAME_ENV} node ${CMAKE_SOURCE_DIR}/tools/game-config.mjs
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} RESULT_VARIABLE GAME_CONFIG_FAILED)
if(GAME_CONFIG_FAILED)
  message(FATAL_ERROR "tools/game-config.mjs failed for ${GAME}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GAME_DIR}/game.json)
include(${GAME_CONFIG}/game.cmake)

# ---- SDL2 (static) ----------------------------------------------------------
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/sdl2 EXCLUDE_FROM_ALL)

# ---- Musashi 68000 interpreter (fallback for code not yet recompiled) --------
set(MUSASHI_DIR ${CMAKE_SOURCE_DIR}/third_party/musashi)
set(MUSASHI_GEN ${CMAKE_BINARY_DIR}/musashi_gen)
add_executable(m68kmake ${MUSASHI_DIR}/m68kmake.c)
add_custom_command(
  OUTPUT ${MUSASHI_GEN}/m68kops.c ${MUSASHI_GEN}/m68kops.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${MUSASHI_GEN}
  COMMAND m68kmake ${MUSASHI_GEN} ${MUSASHI_DIR}/m68k_in.c
  DEPENDS m68kmake ${MUSASHI_DIR}/m68k_in.c)
add_library(musashi STATIC
  ${MUSASHI_DIR}/m68kcpu.c
  ${MUSASHI_DIR}/m68kdasm.c
  ${MUSASHI_DIR}/softfloat/softfloat.c
  ${MUSASHI_GEN}/m68kops.c)
target_include_directories(musashi PUBLIC ${MUSASHI_DIR} ${MUSASHI_GEN} ${CMAKE_SOURCE_DIR}/runtime/src ${GAME_CONFIG})
target_compile_definitions(musashi PUBLIC "MUSASHI_CNF=\"musashi_conf.h\"")
target_compile_options(musashi PRIVATE -w)

# ---- Sound: Z80 core and YM2612 --------------------------------------------------
add_library(z80core STATIC third_party/z80/z80.c)
target_include_directories(z80core PUBLIC third_party/z80)
target_compile_options(z80core PRIVATE -w)
add_library(ymfm STATIC third_party/ymfm/src/ymfm_opn.cpp third_party/ymfm/src/ymfm_ssg.cpp
  third_party/ymfm/src/ymfm_adpcm.cpp third_party/ymfm/src/ymfm_misc.cpp)
target_include_directories(ymfm PUBLIC third_party/ymfm/src)
target_compile_options(ymfm PRIVATE -w)

# ---- Recompiled game code (generated from the user's ROMs) ------------------------
# One folder per edition (GAME_EDITIONS, from game.json); an edition whose ROM is not set
# up in local.json gets empty files, so the list of sources is always the same.
set(RECOMP_SOURCES ${GAME_GEN}/editions.c)
foreach(edition ${GAME_EDITIONS})
  list(APPEND RECOMP_SOURCES ${GAME_GEN}/${edition}/edition.c)
  foreach(i RANGE 0 15)
    if(i LESS 10)
      set(id "0${i}")
    else()
      set(id "${i}")
    endif()
    list(APPEND RECOMP_SOURCES ${GAME_GEN}/${edition}/recomp_${id}.c)
  endforeach()
endforeach()
file(GLOB RECOMP_TOOLS ${CMAKE_SOURCE_DIR}/tools/*.mjs ${CMAKE_SOURCE_DIR}/tools/lib/*.mjs)
set(RECOMP_DEPENDS ${RECOMP_TOOLS} ${GAME_DIR}/game.json ${GAME_DIR}/patches.json)
foreach(f entries.json coverage.txt editions.json ../../local.json)
  if(EXISTS ${GAME_DIR}/${f})
    list(APPEND RECOMP_DEPENDS ${GAME_DIR}/${f})                # code found by tracing or play; mapped addresses; a ROM added or moved
  endif()
endforeach()
add_custom_command(
  OUTPUT ${RECOMP_SOURCES}
  COMMAND ${GAME_ENV} node ${CMAKE_SOURCE_DIR}/tools/recomp.mjs
  DEPENDS ${RECOMP_DEPENDS}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Recompiling the ROM to C")
add_library(recomp STATIC ${RECOMP_SOURCES})
target_include_directories(recomp PRIVATE runtime/src ${GAME_CONFIG})
target_link_libraries(recomp PRIVATE musashi)
target_compile_options(recomp PRIVATE -O1 -w)

# ---- Runtime ------------------------------------------------------------------
# WIN32: a windowed program, so no console window opens with the game (main.c attaches
# to the terminal it was started from, for the test output).
add_executable(${GAME_EXE} WIN32
  runtime/src/main.c
  runtime/src/console.c
  runtime/src/machine.c
  runtime/src/widescreen.c
  runtime/src/state.c
  runtime/src/rewind.c
  runtime/src/png.c
  runtime/src/text.c
  runtime/src/launcher.c
  runtime/src/launcher_scene.c
  runtime/src/foil.c
  runtime/src/ui.c
  runtime/src/menu.c
  runtime/src/savestate.c
  runtime/src/cheats.c
  runtime/src/hd.c
  runtime/src/motion.c
  runtime/src/ost.c
  runtime/src/filter.c
  runtime/src/input.c
  runtime/src/pads.c
  runtime/src/combat.c
  runtime/src/automap.c
  runtime/src/controls.c
  runtime/src/outfit.c
  runtime/src/preview.c
  runtime/src/fx.c
  runtime/src/texpack.c
  runtime/src/bus.c
  runtime/src/vdp.c
  runtime/src/io.c
  runtime/src/settings.c
  runtime/src/z80bus.c
  runtime/src/ai.c
  runtime/src/fighters.c
  runtime/src/music.c
  runtime/src/third.c
  runtime/src/sound.cpp)
target_link_libraries(${GAME_EXE} PRIVATE recomp musashi z80core ymfm SDL2::SDL2main SDL2::SDL2-static)
target_compile_options(${GAME_EXE} PRIVATE -Wall -Wextra -Wno-unused-parameter)
if(WIN32)
  target_link_options(${GAME_EXE} PRIVATE -static)
  target_link_libraries(${GAME_EXE} PRIVATE gdi32 comdlg32 mfplat mfreadwrite mfuuid ole32)
endif()

# ---- The collection's front end (built with the first game) -----------------------
if(GAME STREQUAL "sor1")
  add_executable(Collection WIN32
    runtime/src/collection.c
    runtime/src/update.c
    runtime/src/ui.c
    runtime/src/text.c
    runtime/src/png.c)
  set_target_properties(Collection PROPERTIES OUTPUT_NAME "Streets of Rage Collection")
  target_link_libraries(Collection PRIVATE SDL2::SDL2main SDL2::SDL2-static)
  target_compile_options(Collection PRIVATE -Wall -Wextra -Wno-unused-parameter)
  if(WIN32)
    target_link_options(Collection PRIVATE -static)
    target_link_libraries(Collection PRIVATE gdi32 winhttp bcrypt)
  endif()
  # The installer (setup/setup.c); the release appends the source to it.
  add_executable(Setup WIN32
    setup/setup.c
    setup/unzip.c
    runtime/src/ui.c
    runtime/src/text.c
    runtime/src/png.c)
  target_include_directories(Setup PRIVATE runtime/src)
  target_link_libraries(Setup PRIVATE SDL2::SDL2main SDL2::SDL2-static)
  target_compile_options(Setup PRIVATE -Wall -Wextra -Wno-unused-parameter)
  if(WIN32)
    target_link_options(Setup PRIVATE -static)
    target_link_libraries(Setup PRIVATE gdi32 comdlg32 bcrypt winhttp ole32 shell32 uuid)
  endif()
endif()
