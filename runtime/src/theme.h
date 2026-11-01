/* Each game's look for the launcher, the pause menu and its pages: Streets of Rage's yellow
 * HUD on charcoal, 2's electric blue from its logo, 3's flame orange. C_YELLOW is the
 * highlight, C_AMBER its darker edge, C_RED the bars and the primary button, C_DEEP the
 * shadow under the game's name, C_VALUE an option's value. */
#ifndef THEME_H
#define THEME_H

#include "game.h"
#include "ui.h"

#if GAME_SOR == 3
#define C_YELLOW rgb_hex(0xFFB848)
#define C_AMBER  rgb_hex(0xF07A28)
#define C_RED    rgb_hex(0xC81E10)
#define C_DEEP   rgb_hex(0x5A1406)
#define C_VALUE  rgb_hex(0xFFD8A8)
#define GAME_NAME "STREETS OF RAGE 3"
#elif GAME_SOR == 2
#define C_YELLOW rgb_hex(0x7CCBFF)
#define C_AMBER  rgb_hex(0x2E86E0)
#define C_RED    rgb_hex(0xE02828)
#define C_DEEP   rgb_hex(0x10306A)
#define C_VALUE  rgb_hex(0xBFE2FF)
#define GAME_NAME "STREETS OF RAGE 2"
#else
#define C_YELLOW rgb_hex(0xF8D838)
#define C_AMBER  rgb_hex(0xF0A030)
#define C_RED    rgb_hex(0xE0381C)
#define C_DEEP   rgb_hex(0x8A140A)
#define C_VALUE  rgb_hex(0xE8D49A)
#define GAME_NAME "STREETS OF RAGE"
#endif
#define C_INK    rgb_hex(0x15151A)
#define C_TEXT   rgb_hex(0xE9E5DC)
#define C_DIM    rgb_hex(0x8E8A82)

#endif
