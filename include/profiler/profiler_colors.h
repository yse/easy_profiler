/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin

This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef EASY_PROFILER__COLORS__H_______
#define EASY_PROFILER__COLORS__H_______

#include <stdint.h>

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

namespace profiler {

    //////////////////////////////////////////////////////////////////////

	typedef uint8_t color_t; // One-byte RGB color format: RRR-GGG-BB
    typedef uint32_t rgb32_t; // Standard four-byte RGB color format

    //////////////////////////////////////////////////////////////////////

	namespace colors {

        ///< Extracts [0 .. 224] Red value from one-byte RGB format. Possible values are: [0x0, 0x20, 0x40, 0x60, 0x80, 0xa0, 0xe0].
        inline rgb32_t get_red(color_t color) { return color & 0xe0; }

        ///< Extracts [0 .. 224] Green value from one-byte RGB format. Possible values are: [0x0, 0x20, 0x40, 0x60, 0x80, 0xa0, 0xe0].
        inline rgb32_t get_green(color_t color) { return (color & 0x1c) << 3; }

        ///< Extracts [0 .. 192] Blue value from one-byte RGB format. Possible values are: [0x0, 0x40, 0x80, 0xc0]
        inline rgb32_t get_blue(color_t color) { return (color & 3) << 6; }


        ///< Extracts [0 .. 255] Red value from four-byte RGB format.
        inline rgb32_t rgb_red(rgb32_t color) { return (color & 0x00ff0000) >> 16; }

        ///< Extracts [0 .. 255] Green value from four-byte RGB format.
        inline rgb32_t rgb_green(rgb32_t color) { return (color & 0x0000ff00) >> 8; }

        ///< Extracts [0 .. 255] Blue value from four-byte RGB format.
        inline rgb32_t rgb_blue(rgb32_t color) { return color & 0x000000ff; }

        ///< Unpacks one-byte RGB value into standard four-byte RGB value.
        inline rgb32_t convert_to_rgb(color_t color) { return (get_red(color) << 16) | ((color & 0x1c) << 11) | get_blue(color); }

        ///< Packs standard four-byte RGB value into one-byte RGB value. R & G values packed with 0x20 (32) step, B value is packed with 0x40 (64) step.
        inline color_t from_rgb(rgb32_t color) { return (rgb_red(color) & 0xe0) | (((color & 0x0000ff00) >> 11) & 0x1c) | (rgb_blue(color) >> 6); }

        ///< Packs standard four-byte RGB value into one-byte RGB value. R & G values packed with 0x20 (32) step, B value is packed with 0x40 (64) step.
        inline color_t from_rgb(color_t red, color_t green, color_t blue) { return (red & 0xe0) | ((green >> 3) & 0x1c) | (blue >> 6); }


        const color_t Black        = 0x00; // 0x00000000
		const color_t Random       = Black; // Black // Currently GUI interprets Black color as permission to select random color for block
		const color_t Lightgray    = 0x6E; // 0x00606080
		const color_t Darkgray     = 0x25; // 0x00202040
		const color_t White        = 0xFF; // 0x00E0E0C0
		const color_t Red          = 0xE0; // 0x00E00000
		const color_t Green        = 0x1C; // 0x0000E000
		const color_t Blue         = 0x03; // 0x000000C0
		const color_t Magenta      = (Red   | Blue);  // 0x00E000C0
		const color_t Cyan         = (Green | Blue);  // 0x0000E0C0
		const color_t Yellow       = (Red   | Green); // 0x00E0E000
		const color_t Darkred      = 0x60; // 0x00600000
		const color_t Darkgreen    = 0x0C; // 0x00006000
		const color_t Darkblue     = 0x01; // 0x00000040
		const color_t Darkmagenta  = (Darkred   | Darkblue);  // 0x00600040
		const color_t Darkcyan     = (Darkgreen | Darkblue);  // 0x00006040
		const color_t Darkyellow   = (Darkred   | Darkgreen); // 0x00606000
		const color_t Navy         = 0x02; // 0x00000080
		const color_t Teal         = 0x12; // 0x00008080
		const color_t Maroon       = 0x80; // 0x00800000
		const color_t Purple       = 0x82; // 0x00800080
		const color_t Olive        = 0x90; // 0x00808000
		const color_t Grey         = 0x92; // 0x00808080
		const color_t Silver       = 0xDB; // 0x00C0C0C0
        const color_t Orange       = 0xF4; // 0x00E0A000
        const color_t Coral        = 0xF6; // 0x00E0A080
        const color_t Brick        = 0xED; // 0x00E06040
        const color_t Clay         = 0xD6; // 0x00C0A080
        const color_t Skin         = 0xFA; // 0x00E0C080
        const color_t Palegold     = 0xFE; // 0x00E0E080

	} // END of namespace colors.

    const color_t DefaultBlockColor = colors::Clay;

    //////////////////////////////////////////////////////////////////////
	
} // END of namespace profiler.

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__COLORS__H_______
