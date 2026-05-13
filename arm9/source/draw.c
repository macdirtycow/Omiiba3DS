/*
*   This file is part of Omiiba3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

/*
*   Code to print to the screen by mid-kid @CakesFW
*   https://github.com/mid-kid/CakesForeveryWan
*/

#include "draw.h"
#include "memory.h"
#include "screen.h"
#include "utils.h"
#include "fs.h"
#include "fmt.h"
#include "font.h"
#include "config.h"
#include "buttons.h"

bool loadSplash(void)
{
    static const char *topSplashFile = "splash.bin",
                      *bottomSplashFile = "splashbottom.bin";

    bool isTopSplashValid = getFileSize(topSplashFile) == SCREEN_TOP_FBSIZE,
         isBottomSplashValid = getFileSize(bottomSplashFile) == SCREEN_BOTTOM_FBSIZE;

    //Don't delay boot nor init the screens if no splash images or invalid splash images are on the SD
    if(!isTopSplashValid && !isBottomSplashValid) return false;

    initScreens();

    if(isTopSplashValid) isTopSplashValid = fileRead(fbs[1].top_left, topSplashFile, SCREEN_TOP_FBSIZE) == SCREEN_TOP_FBSIZE;
    if(isBottomSplashValid) isBottomSplashValid = fileRead(fbs[1].bottom, bottomSplashFile, SCREEN_BOTTOM_FBSIZE) == SCREEN_BOTTOM_FBSIZE;

    if(!isTopSplashValid && !isBottomSplashValid) return false;

    swapFramebuffers(true);

    wait(configData.splashDurationMsec);

    return true;
}

void drawCharacter(bool isTopScreen, u32 posX, u32 posY, u32 color, char character)
{
    u8 *select = isTopScreen ? fbs[0].top_left : fbs[0].bottom;

    for(u32 y = 0; y < 8; y++)
    {
        char charPos = font[character * 8 + y];

        for(u32 x = 0; x < 8; x++)
            if(((charPos >> (7 - x)) & 1) == 1)
            {
                u32 screenPos = (posX * SCREEN_HEIGHT * 3 + (SCREEN_HEIGHT - y - posY - 1) * 3) + x * 3 * SCREEN_HEIGHT;

                select[screenPos] = color >> 16;
                select[screenPos + 1] = color >> 8;
                select[screenPos + 2] = color;
            }
    }
}

u32 drawString(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *string)
{
    for(u32 i = 0, line_i = 0; i < strlen(string); i++)
        switch(string[i])
        {
            case '\n':
                posY += SPACING_Y;
                line_i = 0;
                break;

            case '\t':
                line_i += 2;
                break;

            default:
                //Make sure we never get out of the screen
                if(line_i >= ((isTopScreen ? SCREEN_TOP_WIDTH : SCREEN_BOTTOM_WIDTH) - posX) / SPACING_X)
                {
                    posY += SPACING_Y;
                    line_i = 1; //Little offset so we know the same string continues
                    if(string[i] == ' ') break; //Spaces at the start look weird
                }

                drawCharacter(isTopScreen, posX + line_i * SPACING_X, posY, color, string[i]);

                line_i++;
                break;
        }

    return posY;
}

u32 drawFormattedString(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *fmt, ...)
{
    char buf[DRAW_MAX_FORMATTED_STRING_SIZE + 1];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    return drawString(isTopScreen, posX, posY, color, buf);
}

// Quasi-bold text by overdrawing the same string 1px to the right and 1px down.
static void drawBoldString(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *s)
{
    drawString(isTopScreen, posX,     posY,     color, s);
    drawString(isTopScreen, posX + 1, posY,     color, s);
    drawString(isTopScreen, posX,     posY + 1, color, s);
    drawString(isTopScreen, posX + 1, posY + 1, color, s);
}

// Same 8x8 font, but each font pixel is rendered as scale x scale block, so the
// effective glyph is (8*scale) x (8*scale). Used for the boot-splash logo.
static void drawCharacterScaled(bool isTopScreen, u32 posX, u32 posY, u32 color, char character, u32 scale)
{
    u8 *select = isTopScreen ? fbs[0].top_left : fbs[0].bottom;

    for (u32 y = 0; y < 8; y++)
    {
        char charPos = font[character * 8 + y];

        for (u32 x = 0; x < 8; x++)
        {
            if (((charPos >> (7 - x)) & 1) == 0)
                continue;

            for (u32 sy = 0; sy < scale; sy++)
            {
                for (u32 sx = 0; sx < scale; sx++)
                {
                    u32 px = posX + x * scale + sx;
                    u32 py = posY + y * scale + sy;
                    u32 o  = px * SCREEN_HEIGHT * 3 + (SCREEN_HEIGHT - py - 1) * 3;
                    select[o]     = color >> 16;
                    select[o + 1] = color >> 8;
                    select[o + 2] = color;
                }
            }
        }
    }
}

static void drawStringScaled(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *s, u32 scale)
{
    for (u32 i = 0; s[i] != '\0'; i++)
        drawCharacterScaled(isTopScreen, posX + i * SPACING_X * scale, posY, color, s[i], scale);
}

static void drawBoldStringScaled(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *s, u32 scale)
{
    drawStringScaled(isTopScreen, posX,     posY,     color, s, scale);
    drawStringScaled(isTopScreen, posX + 1, posY,     color, s, scale);
    drawStringScaled(isTopScreen, posX,     posY + 1, color, s, scale);
    drawStringScaled(isTopScreen, posX + 1, posY + 1, color, s, scale);
}

static void arm9FillRect24(u8 *fb, u32 width, u32 height, u32 gx0, u32 gy0, u32 gw, u32 gh, u32 bgr)
{
    u8 b = (bgr >> 16) & 0xFF, g = (bgr >> 8) & 0xFF, r = bgr & 0xFF;

    if (gx0 >= width || gy0 >= height)
        return;

    u32 x1 = gx0 + gw;
    u32 y1 = gy0 + gh;
    if (x1 > width)  x1 = width;
    if (y1 > height) y1 = height;

    for (u32 gy = gy0; gy < y1; gy++)
    {
        for (u32 gx = gx0; gx < x1; gx++)
        {
            u32 o = gx * height * 3 + (height - 1 - gy) * 3;
            fb[o]     = b;
            fb[o + 1] = g;
            fb[o + 2] = r;
        }
    }
}

// Optional one-line tagline from /omiiba/boot_message.txt. CR/LF stripped,
// truncated to fit the buffer. Returns true if a non-empty line was loaded.
static bool loadBootMessage(char *dst, u32 bufSize)
{
    if (bufSize < 2) return false;

    u32 rd = fileRead(dst, "boot_message.txt", bufSize - 1);
    if (rd == 0) return false;
    if (rd > bufSize - 1) rd = bufSize - 1;
    dst[rd] = 0;

    for (u32 i = 0; i < rd; i++)
        if (dst[i] == '\r' || dst[i] == '\n')
        {
            dst[i] = 0;
            break;
        }

    return dst[0] != 0;
}

// Returns one tip out of a fixed pool, rotating across boots via a 1-byte
// state file (/omiiba/.cow_tip_state). Read or write failure is harmless: in
// the worst case the user just sees the same tip again.
static const char *pickCowTip(void)
{
    static const char *tips[] = {
        "Hold SELECT to open settings.",
        "Hold START to chainload.",
        "Cow menu combo: L + Down + Select.",
        "Custom splash: /splash.bin on SD.",
        "Cheats live in /omiiba/cheats.",
        "3GX plugins: /omiiba/plugins.",
        "Screenshots: /omiiba/screenshots.",
        "GDB stub ports: 4000-4003.",
        "Hold L at boot for emuNAND.",
        "Edit /omiiba/config.ini to tweak.",
        "Payloads: /omiiba/payloads/.",
        "Tagline: /omiiba/boot_message.txt",
        "Press any key to skip splash.",
        "See NOTICE-OMIIBA.md for changes.",
        "Take screenshots from Cow menu.",
    };
    static const u32 numTips = sizeof(tips) / sizeof(tips[0]);

    u8 idx = 0;
    if (fileRead(&idx, ".cow_tip_state", 1) != 1)
        idx = 0;

    u8 nextIdx = (u8)(idx + 1); // u8 wraps naturally
    fileWrite(&nextIdx, ".cow_tip_state", 1);

    return tips[idx % numTips];
}

void omiibaBootSplash(void)
{
    initScreens();

    arm9FillRect24(fbs[0].top_left,  SCREEN_TOP_WIDTH, SCREEN_HEIGHT, 0, 0, SCREEN_TOP_WIDTH, SCREEN_HEIGHT, COLOR_SPLASH_BG);
    arm9FillRect24(fbs[0].bottom, SCREEN_BOTTOM_WIDTH, SCREEN_HEIGHT, 0, 0, SCREEN_BOTTOM_WIDTH, SCREEN_HEIGHT, COLOR_SPLASH_BG);

    /* Top: subtle panel + accent rails */
    arm9FillRect24(fbs[0].top_left, SCREEN_TOP_WIDTH, SCREEN_HEIGHT, 40, 78, 320, 108, COLOR_SPLASH_PANEL);
    arm9FillRect24(fbs[0].top_left, SCREEN_TOP_WIDTH, SCREEN_HEIGHT, 0, 76, SCREEN_TOP_WIDTH, 2, COLOR_TITLE);
    arm9FillRect24(fbs[0].top_left, SCREEN_TOP_WIDTH, SCREEN_HEIGHT, 0, 186, SCREEN_TOP_WIDTH, 2, COLOR_TITLE);

    static const char *title    = "OMIIBA3DS";
    static const char *subtitle = "Cow Edition  /  custom firmware";
    static const char *fork     = "Educational fork of Luma3DS";
    static const char *hint     = "SELECT  settings   |   START  chainloader";

    static const u32 titleScale = 2; // 8x8 -> 16x16 glyphs
    u32 titleX = (SCREEN_TOP_WIDTH - (u32)strlen(title) * SPACING_X * titleScale) / 2;
    u32 subX   = (SCREEN_TOP_WIDTH - (u32)strlen(subtitle) * SPACING_X) / 2;
    u32 forkX  = (SCREEN_TOP_WIDTH - (u32)strlen(fork) * SPACING_X) / 2;
    u32 hintX  = (SCREEN_TOP_WIDTH - (u32)strlen(hint) * SPACING_X) / 2;

    drawBoldStringScaled(true, titleX, 90, COLOR_TITLE, title, titleScale);
    drawString          (true, subX,  124, COLOR_SPLASH_MUTED, subtitle);
    drawString          (true, forkX, 142, COLOR_SPLASH_MUTED, fork);

    char bootMsg[64];
    if (loadBootMessage(bootMsg, sizeof(bootMsg)))
    {
        u32 msgX = (SCREEN_TOP_WIDTH - (u32)strlen(bootMsg) * SPACING_X) / 2;
        drawString(true, msgX, 164, COLOR_WHITE, bootMsg);
    }

    drawString(true, hintX, 208, COLOR_WHITE, hint);

    /* Bottom: card for in-game combo + tip */
    static const u32 panelX = 12;
    static const u32 panelW = 296; // 12..308, leaves 6px margin on each screen edge
    arm9FillRect24(fbs[0].bottom, SCREEN_BOTTOM_WIDTH, SCREEN_HEIGHT, panelX, 52, panelW, 148, COLOR_SPLASH_PANEL);
    arm9FillRect24(fbs[0].bottom, SCREEN_BOTTOM_WIDTH, SCREEN_HEIGHT, panelX, 50, panelW, 2, COLOR_TITLE);

    static const char *botTop = "In-game overlay";
    static const char *botBtn = "L  +  Down  +  Select";
    static const char *tipLabel = "Tip";
    const char *tip = pickCowTip();

    u32 botTopX = (SCREEN_BOTTOM_WIDTH - (u32)strlen(botTop) * SPACING_X) / 2;
    u32 botBtnX = (SCREEN_BOTTOM_WIDTH - (u32)strlen(botBtn) * SPACING_X) / 2;
    u32 tipLabX = (SCREEN_BOTTOM_WIDTH - (u32)strlen(tipLabel) * SPACING_X) / 2;

    drawString(false, botTopX, 64, COLOR_SPLASH_MUTED, botTop);
    drawBoldString(false, botBtnX, 84, COLOR_TITLE, botBtn);
    drawString(false, tipLabX, 118, COLOR_TITLE, tipLabel);

    // Text must stay inside the panel. Centre when it fits, otherwise wrap to
    // two lines on the last space before the limit (hard-cut as final fallback).
    const u32 textMargin = 8;
    const u32 textMinX = panelX + textMargin;
    const u32 textMaxX = panelX + panelW - textMargin;
    const u32 maxChars = (textMaxX - textMinX) / SPACING_X;

    u32 tipLen = (u32)strlen(tip);
    if (tipLen <= maxChars)
    {
        u32 tipX = (SCREEN_BOTTOM_WIDTH - tipLen * SPACING_X) / 2;
        drawString(false, tipX, 136, COLOR_WHITE, tip);
    }
    else
    {
        u32 wrap = maxChars;
        while (wrap > 0 && tip[wrap] != ' ')
            wrap--;
        if (wrap == 0)
            wrap = maxChars; // no space found, hard-wrap

        char line1[64], line2[64];
        u32 i;
        for (i = 0; i < wrap && i < sizeof(line1) - 1; i++)
            line1[i] = tip[i];
        line1[i] = '\0';

        u32 j = (tip[wrap] == ' ') ? wrap + 1 : wrap;
        u32 k = 0;
        while (tip[j] != '\0' && k < sizeof(line2) - 1)
            line2[k++] = tip[j++];
        line2[k] = '\0';

        u32 l1 = (u32)strlen(line1);
        u32 l2 = (u32)strlen(line2);
        if (l2 > maxChars)
        {
            line2[maxChars] = '\0';
            l2 = maxChars;
        }

        u32 x1 = (SCREEN_BOTTOM_WIDTH - l1 * SPACING_X) / 2;
        u32 x2 = (SCREEN_BOTTOM_WIDTH - l2 * SPACING_X) / 2;
        drawString(false, x1, 130, COLOR_WHITE, line1);
        drawString(false, x2, 142, COLOR_WHITE, line2);
    }

    for (u32 i = 0; i < 56 && (HID_PAD == 0); i++)
        wait(25);
}
