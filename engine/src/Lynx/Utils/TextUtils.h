#pragma once

namespace Lynx
{
    namespace TextUtils
    {
        static uint32_t DecodeNextCodepoint(const std::string& text, size_t& i)
        {
            unsigned char c = (unsigned char)text[i];
            uint32_t codepoint = 0;

            if (c < 0x80) // 1-byte sequence (ASCII)
            {
                codepoint = c;
                i += 1;
            }
            else if ((c & 0xE0) == 0xC0) // 2-byte sequence
            {
                codepoint = ((c & 0x1F) << 6) | ((unsigned char)text[i + 1] & 0x3F);
                i += 2;
            }
            else if ((c & 0xF0) == 0xE0) // 3-byte sequence
            {
                codepoint = ((c & 0x0F) << 12) | (((unsigned char)text[i + 1] & 0x3F) << 6) | ((unsigned char)text[i + 2] & 0x3F);
                i += 3;
            }
            else if ((c & 0xF8) == 0xF0) // 4-byte sequence
            {
                codepoint = ((c & 0x07) << 18) | (((unsigned char)text[i + 1] & 0x3F) << 12) | (((unsigned char)text[i + 2] & 0x3F) << 6) | ((unsigned char)text[i + 3] & 0x3F);
                i += 4;
            }
            else
            {
                // Invalid, skip 1
                i += 1;
            }
            return codepoint;
        }
    }
}
