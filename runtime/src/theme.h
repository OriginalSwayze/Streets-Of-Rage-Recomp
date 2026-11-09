/* Each game's look for the launcher, the pause menu and its pages: a street at night, its
 * signs in two neon colours (neon.h draws them). Streets of Rage in hot pink and teal,
 * 2 in electric blue (its logo's) and pink, 3 in flame orange and violet.
 *
 * C_YELLOW is the main neon (the highlight, the game's name, a focused value), C_AMBER a
 * deeper shade of it, C_RED the second neon (rules, the selected row, the primary button),
 * C_DEEP the dark glow under lettering, C_VALUE an option's value. The names stay from the
 * HUD look before this one, so every page takes the new colours. */
#ifndef THEME_H
#define THEME_H

#include "game.h"
#include "ui.h"

#if GAME_SOR == 3
#define C_YELLOW rgb_hex(0xFFA23C)
#define C_AMBER  rgb_hex(0xE0601E)
#define C_RED    rgb_hex(0xB264FF)
#define C_DEEP   rgb_hex(0x3A1406)
#define C_VALUE  rgb_hex(0xFFD6A8)
#define GAME_NAME "STREETS OF RAGE 3"
#elif GAME_SOR == 2
#define C_YELLOW rgb_hex(0x5CD8FF)
#define C_AMBER  rgb_hex(0x2A8CE0)
#define C_RED    rgb_hex(0xFF3FA4)
#define C_DEEP   rgb_hex(0x0A2A50)
#define C_VALUE  rgb_hex(0xC4ECFF)
#define GAME_NAME "STREETS OF RAGE 2"
#else
#define C_YELLOW rgb_hex(0xFF4FB0)
#define C_AMBER  rgb_hex(0xC81E7A)
#define C_RED    rgb_hex(0x3CF0D2)
#define C_DEEP   rgb_hex(0x4A0A2A)
#define C_VALUE  rgb_hex(0xFFC8E6)
#define GAME_NAME "STREETS OF RAGE"
#endif
#define C_INK    rgb_hex(0x0A0C1A)      /* navy glass */
#define C_LINE   rgb_hex(0x262B52)      /* a panel's thin edge */
#define C_TEXT   rgb_hex(0xE6E8F4)
#define C_DIM    rgb_hex(0x7178A0)

/* Lettering: the game's name stays in its heavy condensed face; signs and labels in
 * Windows' Bahnschrift, a street-sign face (Windows 10 and later). */
#define FONT_SIGN  "Bahnschrift SemiBold Condensed"
#define FONT_UI    "Bahnschrift"
#define FONT_VALUE "Bahnschrift SemiBold"

#endif
