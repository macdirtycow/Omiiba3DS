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

static void drawHLine(bool isTopScreen, u32 startX, u32 endX, u32 posY, u32 color, char ch)
{
    for (u32 x = startX; x + SPACING_X <= endX; x += SPACING_X)
        drawCharacter(isTopScreen, x, posY, color, ch);
}

void omiibaBootSplash(void)
{
    initScreens();

    // Both screens to black.
    memset(fbs[0].top_left, 0, SCREEN_TOP_FBSIZE);
    memset(fbs[0].bottom,   0, SCREEN_BOTTOM_FBSIZE);

    // Top screen layout (400 x 240, landscape).
    static const char *title    = "O M I I B A 3 D S";
    static const char *subtitle = "Cow Edition  -  Custom Firmware";
    static const char *fork     = "Educational fork of Luma3DS";
    static const char *hint     = "SELECT: settings    START: chainloader";

    u32 titleX    = (SCREEN_TOP_WIDTH - (u32)strlen(title)    * SPACING_X) / 2;
    u32 subX      = (SCREEN_TOP_WIDTH - (u32)strlen(subtitle) * SPACING_X) / 2;
    u32 forkX     = (SCREEN_TOP_WIDTH - (u32)strlen(fork)     * SPACING_X) / 2;
    u32 hintX     = (SCREEN_TOP_WIDTH - (u32)strlen(hint)     * SPACING_X) / 2;

    drawHLine(true, 50, SCREEN_TOP_WIDTH - 50,  85, COLOR_TITLE, '=');
    drawBoldString(true, titleX,  110, COLOR_TITLE, title);
    drawString    (true, subX,    140, COLOR_WHITE, subtitle);
    drawString    (true, forkX,   158, COLOR_GREEN, fork);
    drawHLine(true, 50, SCREEN_TOP_WIDTH - 50, 175, COLOR_TITLE, '=');
    drawString    (true, hintX,   210, COLOR_WHITE, hint);

    // Bottom screen layout (320 x 240).
    static const char *botTop = "Cow menu in-game:";
    static const char *botBtn = "L + Down + Select";
    u32 botTopX = (SCREEN_BOTTOM_WIDTH - (u32)strlen(botTop) * SPACING_X) / 2;
    u32 botBtnX = (SCREEN_BOTTOM_WIDTH - (u32)strlen(botBtn) * SPACING_X) / 2;
    drawString    (false, botTopX, 100, COLOR_WHITE, botTop);
    drawBoldString(false, botBtnX, 120, COLOR_TITLE, botBtn);

    // Hold ~1.4s, but bail out instantly when the user presses anything (so power
    // users that always mash SELECT/START don't feel slowed down).
    for (u32 i = 0; i < 56 && (HID_PAD == 0); i++)
        wait(25);
}
