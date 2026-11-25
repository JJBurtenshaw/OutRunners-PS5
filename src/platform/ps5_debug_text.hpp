#pragma once

#include <SDL.h>

namespace ps5
{
    namespace debug
    {
        // Minimal 3x5 bitmap font.
        // No external fonts or SDL_ttf dependency.

        inline const char* glyph(char c)
        {
            switch (c)
            {
                case '0': return "111101101101111";
                case '1': return "010110010010111";
                case '2': return "111001111100111";
                case '3': return "111001111001111";
                case '4': return "101101111001001";
                case '5': return "111100111001111";
                case '6': return "111100111101111";
                case '7': return "111001001001001";
                case '8': return "111101111101111";
                case '9': return "111101111001111";

                case 'F': return "111100110100100";
                case 'P': return "110101110100100";
                case 'S': return "111100111001111";
                case 'M': return "101111111101101";

                // ORUNNERS_FULL_FONT_V1
                case 'A': return "010101111101101";
                case 'B': return "110101110101110";
                case 'C': return "111100100100111";
                case 'D': return "110101101101110";
                case 'E': return "111100110100111";
                case 'G': return "111100101101111";
                case 'H': return "101101111101101";
                case 'I': return "111010010010111";
                case 'J': return "001001001101111";
                case 'K': return "101101110101101";
                case 'L': return "100100100100111";
                case 'N': return "101111111111101";
                case 'O': return "111101101101111";
                case 'Q': return "111101101111001";
                case 'R': return "110101110101101";
                case 'T': return "111010010010010";
                case 'U': return "101101101101111";
                case 'V': return "101101101101010";
                case 'W': return "101101111111101";
                case 'X': return "101101010101101";
                case 'Y': return "101101010010010";
                case 'Z': return "111001010100111";

                case '.': return "000000000000010";
                case '-': return "000000111000000";

                default: return "000000000000000";
            }
        }

        inline void text(
            SDL_Surface* surface,
            int x,
            int y,
            const char* value,
            int scale,
            Uint32 colour)
        {
            if (!surface || !value || scale <= 0)
                return;

            for (; *value; ++value)
            {
                const char* bits = glyph(*value);

                for (int row = 0; row < 5; ++row)
                {
                    for (int col = 0; col < 3; ++col)
                    {
                        if (bits[row * 3 + col] != '1')
                            continue;

                        SDL_Rect pixel{
                            x + col * scale,
                            y + row * scale,
                            scale,
                            scale
                        };

                        SDL_FillRect(
                            surface,
                            &pixel,
                            colour
                        );
                    }
                }

                x += scale * 4;
            }
        }
    }
}
