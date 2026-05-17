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

#define _GNU_SOURCE // for strchrnul

#include <assert.h>
#include <strings.h>
#include "config.h"
#include "memory.h"
#include "fs.h"
#include "utils.h"
#include "screen.h"
#include "draw.h"
#include "emunand.h"
#include "buttons.h"
#include "pin.h"
#include "i2c.h"
#include "ini.h"
#include "firm.h"
#include "fatfs/ff.h"

#include "config_template_ini.h" // note that it has an extra NUL byte inserted

#define MAKE_OMIIBA_VERSION_MCU(major, minor, build) (u16)(((major) & 0xFF) << 8 | ((minor) & 0x1F) << 5 | ((build) & 7))

#define FLOAT_CONV_MULT 100000000ll
#define FLOAT_CONV_PRECISION 8u

CfgData configData;
ConfigurationStatus needConfig;
static CfgData oldConfig;

static CfgDataMcu configDataMcu;
static_assert(sizeof(CfgDataMcu) > 0, "wrong data size");

// INI parsing
// ===========================================================

static const char *singleOptionIniNamesBoot[] = {
    "autoboot_emunand",
    "enable_external_firm_and_modules",
    "enable_game_patching",
    "app_syscore_threads_on_core_2",
    "show_system_settings_string",
    "show_gba_boot_screen",
};

static const char *singleOptionIniNamesMisc[] = {
    "use_dev_unitinfo",
    "enable_dsi_external_filter",
    "disable_arm11_exception_handlers",
    "enable_safe_firm_rosalina",
};

static const char *keyNames[] = {
    "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L", "X", "Y",
    "?", "?",
    "ZL", "ZR",
    "?", "?", "?", "?",
    "Touch",
    "?", "?", "?",
    "CStick Right", "CStick Left", "CStick Up", "CStick Down",
    "CPad Right", "CPad Left", "CPad Up", "CPad Down",
};

static int parseBoolOption(bool *out, const char *val)
{
    *out = false;
    if (strlen(val) != 1) {
        return -1;
    }

    if (val[0] == '0') {
        return 0;
    } else if (val[0] == '1') {
        *out = true;
        return 0;
    } else {
        return -1;
    }
}

static int parseDecIntOptionImpl(s64 *out, const char *val, size_t numDigits, s64 minval, s64 maxval)
{
    *out = 0;
    s64 res = 0;
    size_t i = 0;

    s64 sign = 1;
    if (numDigits >= 2) {
        if (val[0] == '+') {
            ++i;
        } else if (val[0] == '-') {
            sign = -1;
            ++i;
        }
    }

    for (; i < numDigits; i++) {
        u64 n = (u64)(val[i] - '0');
        if (n > 9) {
            return -1;
        }

        res = 10*res + n;
    }

    res *= sign;
    if (res <= maxval && res >= minval) {
        *out = res;
        return 0;
    } else {
        return -1;
    }
}

static int parseDecIntOption(s64 *out, const char *val, s64 minval, s64 maxval)
{
    return parseDecIntOptionImpl(out, val, strlen(val), minval, maxval);
}

static int parseDecFloatOption(s64 *out, const char *val, s64 minval, s64 maxval)
{
    s64 sign = 1;// intPart < 0 ? -1 : 1;

    switch (val[0]) {
        case '\0':
            return -1;
        case '+':
            ++val;
            break;
        case '-':
            sign = -1;
            ++val;
            break;
        default:
            break;
    }

    // Reject "-" and "+"
    if (val[0] == '\0') {
        return -1;
    }

    char *point = strchrnul(val, '.');

    // Parse integer part, then fractional part
    s64 intPart = 0;
    s64 fracPart = 0;
    int rc = 0;

    if (point == val) {
        // e.g. -.5
        if (val[1] == '\0')
            return -1;
    }
    else {
        rc = parseDecIntOptionImpl(&intPart, val, point - val, INT64_MIN, INT64_MAX);
    }

    if (rc != 0) {
        return -1;
    }

    s64 intPartAbs = sign == -1 ? -intPart : intPart;
    s64 res = 0;
    bool of = __builtin_mul_overflow(intPartAbs, FLOAT_CONV_MULT, &res);

    if (of) {
        return -1;
    }

    s64 mul = FLOAT_CONV_MULT / 10;

    // Check if there's a fractional part
    if (point[0] != '\0' && point[1] != '\0') {
        for (char *pos = point + 1; *pos != '\0' && mul > 0; pos++) {
            if (*pos < '0' || *pos > '9') {
                return -1;
            }

            res += (*pos - '0') * mul;
            mul /= 10;
        }
    }


    res = sign * (res + fracPart);

    if (res <= maxval && res >= minval && !of) {
        *out = res;
        return 0;
    } else {
        return -1;
    }
}

static int parseHexIntOption(u64 *out, const char *val, u64 minval, u64 maxval)
{
    *out = 0;
    size_t numDigits = strlen(val);
    u64 res = 0;

    for (size_t i = 0; i < numDigits; i++) {
        char c = val[i];
        if ((u64)(c - '0') <= 9) {
            res = 16*res + (u64)(c - '0');
        } else if ((u64)(c - 'a') <= 5) {
            res = 16*res + (u64)(c - 'a' + 10);
        } else if ((u64)(c - 'A') <= 5) {
            res = 16*res + (u64)(c - 'A' + 10);
        } else {
            return -1;
        }
    }

    if (res <= maxval && res >= minval) {
        *out = res;
        return 0;
    } else {
        return -1;
    }
}

static int parseKeyComboOption(u32 *out, const char *val)
{
    const char *startpos = val;
    const char *endpos;

    *out = 0;
    u32 keyCombo = 0;
    do {
        // Copy the button name (note that 16 chars is longer than any of the key names)
        char name[17];
        endpos = strchr(startpos, '+');
        size_t n = endpos == NULL ? 16 : endpos - startpos;
        n = n > 16 ? 16 : n;
        strncpy(name, startpos, n);
        name[n] = '\0';

        if (strcmp(name, "?") == 0) {
            // Lol no, bail out
            return -1;
        }

        bool found = false;
        for (size_t i = 0; i < sizeof(keyNames)/sizeof(keyNames[0]); i++) {
            if (strcasecmp(keyNames[i], name) == 0) {
                found = true;
                keyCombo |= 1u << i;
            }
        }

        if (!found) {
            return -1;
        }

        if (endpos != NULL) {
            startpos = endpos + 1;
        }
    } while(endpos != NULL && *startpos != '\0');

    if (*startpos == '\0') {
        // Trailing '+'
        return -1;
    } else {
        *out = keyCombo;
        return 0;
    }
}

static void menuComboToString(char *out, u32 combo)
{
    char *outOrig = out;
    out[0] = 0;
    for(int i = 31; i >= 0; i--)
    {
        if(combo & (1 << i))
        {
            strcpy(out, keyNames[i]);
            out += strlen(keyNames[i]);
            *out++ = '+';
        }
    }

    if (out != outOrig)
        out[-1] = 0;
}

static int encodedFloatToString(char *out, s64 val)
{
    s64 sign = val >= 0 ? 1 : -1;

    s64 intPart = (sign * val) / FLOAT_CONV_MULT;
    s64 fracPart = (sign * val) % FLOAT_CONV_MULT;

    while (fracPart % 10 != 0) {
        // Remove trailing zeroes
        fracPart /= 10;
    }

    int n = sprintf(out, "%lld", sign * intPart);
    if (fracPart != 0) {
        n += sprintf(out + n, ".%0*lld", (int)FLOAT_CONV_PRECISION, fracPart);

        // Remove trailing zeroes
        int n2 = n - 1;
        while (out[n2] == '0') {
            out[n2--] = '\0';
        }

        n = n2;
    }

    return n;
}

static bool hasIniParseError = false;
static int iniParseErrorLine = 0;

#define CHECK_PARSE_OPTION(res) do { if((res) < 0) { hasIniParseError = true; iniParseErrorLine = lineno; return 0; } } while(false)

static int configIniHandler(void* user, const char* section, const char* name, const char* value, int lineno)
{
    CfgData *cfg = (CfgData *)user;
    if (strcmp(section, "meta") == 0) {
        if (strcmp(name, "config_version_major") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 0xFFFF));
            cfg->formatVersionMajor = (u16)opt;
            return 1;
        } else if (strcmp(name, "config_version_minor") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 0xFFFF));
            cfg->formatVersionMinor = (u16)opt;
            return 1;
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "boot") == 0) {
        // Simple options displayed on the Omiiba3DS boot screen
        for (size_t i = 0; i < sizeof(singleOptionIniNamesBoot)/sizeof(singleOptionIniNamesBoot[0]); i++) {
            if (strcmp(name, singleOptionIniNamesBoot[i]) == 0) {
                bool opt;
                CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
                cfg->config |= (u32)opt << i;
                return 1;
            }
        }

        // Multi-choice options displayed on the Omiiba3DS boot screen

        if (strcmp(name, "default_emunand_number") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 1, 4));
            cfg->multiConfig |= (opt - 1) << (2 * (u32)DEFAULTEMU);
            return 1;
        } else if (strcmp(name, "brightness_level") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 1, 4));
            cfg->multiConfig |= (4 - opt) << (2 * (u32)BRIGHTNESS);
            return 1;
        } else if (strcmp(name, "splash_position") == 0) {
            if (strcasecmp(value, "off") == 0) {
                cfg->multiConfig |= 0 << (2 * (u32)SPLASH);
                return 1;
            } else if (strcasecmp(value, "before payloads") == 0) {
                cfg->multiConfig |= 1 << (2 * (u32)SPLASH);
                return 1;
            } else if (strcasecmp(value, "after payloads") == 0) {
                cfg->multiConfig |= 2 << (2 * (u32)SPLASH);
                return 1;
            } else {
                CHECK_PARSE_OPTION(-1);
            }
        } else if (strcmp(name, "splash_duration_ms") == 0) {
            // Not displayed in the menu anymore, but more configurable
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 0xFFFFFFFFu));
            cfg->splashDurationMsec = (u32)opt;
            return 1;
        }
        else if (strcmp(name, "pin_lock_num_digits") == 0) {
            s64 opt;
            u32 encodedOpt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 8));
            // Only allow for 0 (off), 4, 6 or 8 'digits'
            switch (opt) {
                case 0: encodedOpt = 0; break;
                case 4: encodedOpt = 1; break;
                case 6: encodedOpt = 2; break;
                case 8: encodedOpt = 3; break;
                default: {
                    CHECK_PARSE_OPTION(-1);
                }
            }
            cfg->multiConfig |= encodedOpt << (2 * (u32)PIN);
            return 1;
        } else if (strcmp(name, "app_launch_new_3ds_cpu") == 0) {
            if (strcasecmp(value, "off") == 0) {
                cfg->multiConfig |= 0 << (2 * (u32)NEWCPU);
                return 1;
            } else if (strcasecmp(value, "clock") == 0) {
                cfg->multiConfig |= 1 << (2 * (u32)NEWCPU);
                return 1;
            } else if (strcasecmp(value, "l2") == 0) {
                cfg->multiConfig |= 2 << (2 * (u32)NEWCPU);
                return 1;
            } else if (strcasecmp(value, "clock+l2") == 0) {
                cfg->multiConfig |= 3 << (2 * (u32)NEWCPU);
                return 1;
            } else {
                CHECK_PARSE_OPTION(-1);
            }
        } else if (strcmp(name, "autoboot_mode") == 0) {
            if (strcasecmp(value, "off") == 0) {
                cfg->multiConfig |= 0 << (2 * (u32)AUTOBOOTMODE);
                return 1;
            } else if (strcasecmp(value, "3ds") == 0) {
                cfg->multiConfig |= 1 << (2 * (u32)AUTOBOOTMODE);
                return 1;
            } else if (strcasecmp(value, "dsi") == 0) {
                cfg->multiConfig |= 2 << (2 * (u32)AUTOBOOTMODE);
                return 1;
            } else {
                CHECK_PARSE_OPTION(-1);
            }
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "rosalina") == 0) {
        // Rosalina options
        if (strcmp(name, "hbldr_3dsx_titleid") == 0) {
            u64 opt;
            CHECK_PARSE_OPTION(parseHexIntOption(&opt, value, 0, 0xFFFFFFFFFFFFFFFFull));
            cfg->hbldr3dsxTitleId = opt;
            return 1;
        } else if (strcmp(name, "rosalina_menu_combo") == 0) {
            u32 opt;
            CHECK_PARSE_OPTION(parseKeyComboOption(&opt, value));
            cfg->rosalinaMenuCombo = opt;
            return 1;
        } else if (strcmp(name, "plugin_loader_enabled") == 0) {
            bool opt;
            CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
            cfg->pluginLoaderFlags = opt ? cfg->pluginLoaderFlags | 1 : cfg->pluginLoaderFlags & ~1;
            return 1;
        } else if (strcmp(name, "ntp_tz_offset_min") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, -779, 899));
            cfg->ntpTzOffetMinutes = (s16)opt;
            return 1;
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "screen_filters") == 0) {
        if (strcmp(name, "screen_filters_top_cct") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 1000, 25100));
            cfg->topScreenFilter.cct = (u32)opt;
            return 1;
        } else if (strcmp(name, "screen_filters_top_gamma") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, 0, 8 * FLOAT_CONV_MULT));
            cfg->topScreenFilter.gammaEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_top_contrast") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, 0, 255 * FLOAT_CONV_MULT));
            cfg->topScreenFilter.contrastEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_top_brightness") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, -1 * FLOAT_CONV_MULT, 1 * FLOAT_CONV_MULT));
            cfg->topScreenFilter.brightnessEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_top_invert") == 0) {
            bool opt;
            CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
            cfg->topScreenFilter.invert = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_top_color_curve_adj") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 2));
            cfg->topScreenFilter.colorCurveCorrection = (u8)opt;
            return 1;
        } else if (strcmp(name, "screen_filters_bot_cct") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 1000, 25100));
            cfg->bottomScreenFilter.cct = (u32)opt;
            return 1;
        } else if (strcmp(name, "screen_filters_bot_gamma") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, 0, 8 * FLOAT_CONV_MULT));
            cfg->bottomScreenFilter.gammaEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_bot_contrast") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, 0, 255 * FLOAT_CONV_MULT));
            cfg->bottomScreenFilter.contrastEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_bot_brightness") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, -1 * FLOAT_CONV_MULT, 1 * FLOAT_CONV_MULT));
            cfg->bottomScreenFilter.brightnessEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_bot_invert") == 0) {
            bool opt;
            CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
            cfg->bottomScreenFilter.invert = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_bot_color_curve_adj") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 2));
            cfg->bottomScreenFilter.colorCurveCorrection = (u8)opt;
            return 1;
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "autoboot") == 0) {
        if (strcmp(name, "autoboot_dsi_titleid") == 0) {
            u64 opt;
            CHECK_PARSE_OPTION(parseHexIntOption(&opt, value, 0, 0xFFFFFFFFFFFFFFFFull));
            cfg->autobootTwlTitleId = opt;
            return 1;
        } else if (strcmp(name, "autoboot_3ds_app_mem_type") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 4));
            cfg->autobootCtrAppmemtype = (u8)opt;
            return 1;
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "misc") == 0) {
        for (size_t i = 0; i < sizeof(singleOptionIniNamesMisc)/sizeof(singleOptionIniNamesMisc[0]); i++) {
            if (strcmp(name, singleOptionIniNamesMisc[i]) == 0) {
                bool opt;
                CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
                cfg->config |= (u32)opt << (i + (u32)PATCHUNITINFO);
                return 1;
            }
        }

        if (strcmp(name, "force_audio_output") == 0) {
            if (strcasecmp(value, "off") == 0) {
                cfg->multiConfig |= 0 << (2 * (u32)FORCEAUDIOOUTPUT);
                return 1;
            } else if (strcasecmp(value, "headphones") == 0) {
                cfg->multiConfig |= 1 << (2 * (u32)FORCEAUDIOOUTPUT);
                return 1;
            } else if (strcasecmp(value, "speakers") == 0) {
                cfg->multiConfig |= 2 << (2 * (u32)FORCEAUDIOOUTPUT);
                return 1;
            } else {
                CHECK_PARSE_OPTION(-1);
            }
        } else if (strcmp(name, "volume_slider_override") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, -1, 100));
            cfg->volumeSliderOverride = (s8)opt;
            return 1;
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else {
        CHECK_PARSE_OPTION(-1);
    }
}

static size_t saveOmiibaIniConfigToStr(char *out)
{
    const CfgData *cfg = &configData;

    char omiibaVerStr[64];
    char omiibaRevSuffixStr[16];
    char rosalinaMenuComboStr[128];

    const char *splashPosStr;
    const char *n3dsCpuStr;
    const char *autobootModeStr;
    const char *forceAudioOutputStr;

    switch (MULTICONFIG(SPLASH)) {
        default: case 0: splashPosStr = "off"; break;
        case 1: splashPosStr = "before payloads"; break;
        case 2: splashPosStr = "after payloads"; break;
    }

    switch (MULTICONFIG(NEWCPU)) {
        default: case 0: n3dsCpuStr = "off"; break;
        case 1: n3dsCpuStr = "clock"; break;
        case 2: n3dsCpuStr = "l2"; break;
        case 3: n3dsCpuStr = "clock+l2"; break;
    }

    switch (MULTICONFIG(AUTOBOOTMODE)) {
        default: case 0: autobootModeStr = "off"; break;
        case 1: autobootModeStr = "3ds"; break;
        case 2: autobootModeStr = "dsi"; break;
    }

    switch (MULTICONFIG(FORCEAUDIOOUTPUT)) {
        default: case 0: forceAudioOutputStr = "off"; break;
        case 1: forceAudioOutputStr = "headphones"; break;
        case 2: forceAudioOutputStr = "speakers"; break;
    }

    if (VERSION_BUILD != 0) {
        sprintf(omiibaVerStr, "Omiiba3DS v%d.%d.%d", (int)VERSION_MAJOR, (int)VERSION_MINOR, (int)VERSION_BUILD);
    } else {
        sprintf(omiibaVerStr, "Omiiba3DS v%d.%d", (int)VERSION_MAJOR, (int)VERSION_MINOR);
    }

    if (ISRELEASE) {
        strcpy(omiibaRevSuffixStr, "");
    } else {
        sprintf(omiibaRevSuffixStr, "-%08lx", (u32)COMMIT_HASH);
    }

    menuComboToString(rosalinaMenuComboStr, cfg->rosalinaMenuCombo);

    static const int pinOptionToDigits[] = { 0, 4, 6, 8 };
    int pinNumDigits = pinOptionToDigits[MULTICONFIG(PIN)];

    char topScreenFilterGammaStr[32];
    char topScreenFilterContrastStr[32];
    char topScreenFilterBrightnessStr[32];
    encodedFloatToString(topScreenFilterGammaStr, cfg->topScreenFilter.gammaEnc);
    encodedFloatToString(topScreenFilterContrastStr, cfg->topScreenFilter.contrastEnc);
    encodedFloatToString(topScreenFilterBrightnessStr, cfg->topScreenFilter.brightnessEnc);

    char bottomScreenFilterGammaStr[32];
    char bottomScreenFilterContrastStr[32];
    char bottomScreenFilterBrightnessStr[32];
    encodedFloatToString(bottomScreenFilterGammaStr, cfg->bottomScreenFilter.gammaEnc);
    encodedFloatToString(bottomScreenFilterContrastStr, cfg->bottomScreenFilter.contrastEnc);
    encodedFloatToString(bottomScreenFilterBrightnessStr, cfg->bottomScreenFilter.brightnessEnc);

    int n = sprintf(
        out, (const char *)config_template_ini,
        omiibaVerStr, omiibaRevSuffixStr,

        (int)CONFIG_VERSIONMAJOR, (int)CONFIG_VERSIONMINOR,
        (int)CONFIG(AUTOBOOTEMU), (int)CONFIG(LOADEXTFIRMSANDMODULES),
        (int)CONFIG(PATCHGAMES), (int)CONFIG(REDIRECTAPPTHREADS),
        (int)CONFIG(PATCHVERSTRING), (int)CONFIG(SHOWGBABOOT),

        1 + (int)MULTICONFIG(DEFAULTEMU), 4 - (int)MULTICONFIG(BRIGHTNESS),
        splashPosStr, (unsigned int)cfg->splashDurationMsec,
        pinNumDigits, n3dsCpuStr,
        autobootModeStr,

        cfg->hbldr3dsxTitleId, rosalinaMenuComboStr, (int)(cfg->pluginLoaderFlags & 1),
        (int)cfg->ntpTzOffetMinutes,

        (int)cfg->topScreenFilter.cct, (int)cfg->bottomScreenFilter.cct,
        (int)cfg->topScreenFilter.colorCurveCorrection, (int)cfg->bottomScreenFilter.colorCurveCorrection,
        topScreenFilterGammaStr, bottomScreenFilterGammaStr,
        topScreenFilterContrastStr, bottomScreenFilterContrastStr,
        topScreenFilterBrightnessStr, bottomScreenFilterBrightnessStr,
        (int)cfg->topScreenFilter.invert, (int)cfg->bottomScreenFilter.invert,

        cfg->autobootTwlTitleId, (int)cfg->autobootCtrAppmemtype,

        forceAudioOutputStr,
        cfg->volumeSliderOverride,

        (int)CONFIG(PATCHUNITINFO), (int)CONFIG(ENABLEDSIEXTFILTER),
        (int)CONFIG(DISABLEARM11EXCHANDLERS), (int)CONFIG(ENABLESAFEFIRMROSALINA)
    );

    return n < 0 ? 0 : (size_t)n;
}

static char tmpIniBuffer[0x2000 + 0x400]; // eyeballed. TODO use #embed

static bool readOmiibaIniConfig(void)
{
    u32 rd = fileRead(tmpIniBuffer, "config.ini", sizeof(tmpIniBuffer) - 1);
    if (rd == 0) return false;

    tmpIniBuffer[rd] = '\0';

    return ini_parse_string(tmpIniBuffer, &configIniHandler, &configData) >= 0 && !hasIniParseError;
}

static bool writeOmiibaIniConfig(void)
{
    size_t n = saveOmiibaIniConfigToStr(tmpIniBuffer);

    // FIXME: this is UB we should port snprintf sometime (as well as fix other tech debt)
    if (n + 1 >= sizeof(tmpIniBuffer)) {
        error("Configuration data buffer overflow, please report this issue");
        __builtin_unreachable();
    }

    return n != 0 && fileWrite(tmpIniBuffer, "config.ini", n);
}

// ===========================================================

static void writeConfigMcu(void)
{
    u8 data[sizeof(CfgDataMcu)];

    // Set Omiiba3DS version
    configDataMcu.omiibaVersion = MAKE_OMIIBA_VERSION_MCU(VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);

    // Set bootconfig from CfgData
    configDataMcu.bootCfg = configData.bootConfig;

    memcpy(data, &configDataMcu, sizeof(CfgDataMcu));

    // Fix checksum
    u8 checksum = 0;
    for (u32 i = 0; i < sizeof(CfgDataMcu) - 1; i++)
        checksum += data[i];
    checksum = ~checksum;
    data[sizeof(CfgDataMcu) - 1] = checksum;
    configDataMcu.checksum = checksum;

    I2C_writeReg(I2C_DEV_MCU, 0x60, 200 - sizeof(CfgDataMcu));
    I2C_writeRegBuf(I2C_DEV_MCU, 0x61, data, sizeof(CfgDataMcu));
}

static bool readConfigMcu(void)
{
    u8 data[sizeof(CfgDataMcu)];
    u16 curVer = MAKE_OMIIBA_VERSION_MCU(VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);

    // Select free reg id, then access the data regs
    I2C_writeReg(I2C_DEV_MCU, 0x60, 200 - sizeof(CfgDataMcu));
    I2C_readRegBuf(I2C_DEV_MCU, 0x61, data, sizeof(CfgDataMcu));
    memcpy(&configDataMcu, data, sizeof(CfgDataMcu));

    u8 checksum = 0;
    for (u32 i = 0; i < sizeof(CfgDataMcu) - 1; i++)
        checksum += data[i];
    checksum = ~checksum;

    if (checksum != configDataMcu.checksum || configDataMcu.omiibaVersion < MAKE_OMIIBA_VERSION_MCU(10, 3, 0))
    {
        // Invalid data stored in MCU...
        memset(&configDataMcu, 0, sizeof(CfgDataMcu));
        configData.bootConfig = 0;
        // Perform upgrade process (ignoring failures)
        doOmiibaUpgradeProcess();
        writeConfigMcu();

        return false;
    }

    if (configDataMcu.omiibaVersion < curVer)
    {
        // Perform upgrade process (ignoring failures)
        doOmiibaUpgradeProcess();
        writeConfigMcu();
    }

    return true;
}

bool readConfig(void)
{
    bool retMcu, ret;

    retMcu = readConfigMcu();
    ret = readOmiibaIniConfig();
    if(!retMcu || !ret ||
       configData.formatVersionMajor != CONFIG_VERSIONMAJOR ||
       configData.formatVersionMinor != CONFIG_VERSIONMINOR)
    {
        memset(&configData, 0, sizeof(CfgData));
        configData.formatVersionMajor = CONFIG_VERSIONMAJOR;
        configData.formatVersionMinor = CONFIG_VERSIONMINOR;
        configData.config |= 1u << PATCHVERSTRING;
        configData.splashDurationMsec = 3000;
        configData.volumeSliderOverride = -1;
        configData.hbldr3dsxTitleId = HBLDR_DEFAULT_3DSX_TID;
        configData.rosalinaMenuCombo = 1u << 9 | 1u << 7 | 1u << 2; // L+Start+Select
        configData.topScreenFilter.cct = 6500; // default temp, no-op
        configData.topScreenFilter.gammaEnc = 1 * FLOAT_CONV_MULT; // 1.0f
        configData.topScreenFilter.contrastEnc = 1 * FLOAT_CONV_MULT; // 1.0f
        configData.bottomScreenFilter = configData.topScreenFilter;
        configData.autobootTwlTitleId = AUTOBOOT_DEFAULT_TWL_TID;
        ret = false;
    }
    else
        ret = true;

    configData.bootConfig = configDataMcu.bootCfg;
    oldConfig = configData;

    return ret;
}

void writeConfig(bool isConfigOptions)
{
    bool updateMcu, updateIni;

    if (needConfig == CREATE_CONFIGURATION)
    {
        updateMcu = !isConfigOptions; // We've already committed it once (if it wasn't initialized)
        updateIni = isConfigOptions;
        needConfig = MODIFY_CONFIGURATION;
    }
    else
    {
        updateMcu = !isConfigOptions && configData.bootConfig != oldConfig.bootConfig;
        updateIni = isConfigOptions && (configData.config != oldConfig.config || configData.multiConfig != oldConfig.multiConfig);
    }

    if (updateMcu)
        writeConfigMcu();

    if(updateIni && !writeOmiibaIniConfig())
        error("Error writing the configuration file");
}

static void waitForBootHubBack(void)
{
    u32 pressed;

    do
    {
        pressed = waitInput(true) & (BUTTON_A | BUTTON_B | BUTTON_START);
    }
    while(!pressed);
}

static void drawBootHubMessage(const char *title, const char *message)
{
    clearScreens(false);
    drawString(true, 10, 10, COLOR_TITLE, "Omiiba Boot Hub");
    drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, title);
    drawString(true, 10, 10 + 3 * SPACING_Y, COLOR_WHITE, message);
    drawString(false, 10, 10, COLOR_WHITE, "Press A/B/START to return.");
}

static bool askBootHubYesNo(const char *title, const char *message, const char *yesText, const char *noText)
{
    clearScreens(false);
    drawString(true, 10, 10, COLOR_TITLE, "Omiiba setup wizard");
    drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, title);
    drawString(true, 10, 10 + 3 * SPACING_Y, COLOR_WHITE, message);
    drawFormattedString(false, 10, 10, COLOR_WHITE, "A: %s\nB: %s", yesText, noText);

    u32 pressed;
    do
    {
        pressed = waitInput(true) & (BUTTON_A | BUTTON_B);
    }
    while(!pressed);

    return (pressed & BUTTON_A) != 0;
}

static bool askBootHubConfirm(const char *title, const char *message, const char *yesText, const char *noText)
{
    clearScreens(false);
    drawString(true, 10, 10, COLOR_TITLE, "Omiiba Boot Hub");
    drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, title);
    drawString(true, 10, 10 + 3 * SPACING_Y, COLOR_WHITE, message);
    drawFormattedString(false, 10, 10, COLOR_WHITE, "A: %s\nB: %s", yesText, noText);

    u32 pressed;
    do
    {
        pressed = waitInput(true) & (BUTTON_A | BUTTON_B);
    }
    while(!pressed);

    return (pressed & BUTTON_A) != 0;
}

static const char *findGodMode9Payload(void)
{
    static const char *paths[] = {
        "payloads/GodMode9.firm",
        "payloads/godmode9.firm",
        "payloads/GODMODE9.firm",
    };

    for(u32 i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
        if(getFileSize(paths[i]) > 0)
            return paths[i];

    return NULL;
}

static const char *findOpenAgbFirmPayload(void)
{
    static const char *paths[] = {
        "payloads/open_agb_firm.firm",
        "payloads/open_agb.firm",
        "payloads/Open_AGB_Firm.firm",
        "payloads/OPEN_AGB_FIRM.firm",
    };

    for(u32 i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
        if(getFileSize(paths[i]) > 0)
            return paths[i];

    return NULL;
}

static bool directoryExists(const char *path)
{
    DIR dir;
    bool exists = f_opendir(&dir, path) == FR_OK;

    if(exists)
        f_closedir(&dir);

    return exists;
}

static u32 countFirmPayloads(void)
{
    DIR dir;
    FILINFO info;
    u32 count = 0;

    if(f_opendir(&dir, "payloads") != FR_OK)
        return 0;

    while(f_readdir(&dir, &info) == FR_OK && info.fname[0] != 0)
    {
        u32 nameLength = strlen(info.fname);
        if(nameLength >= 6 && memcmp(info.fname + nameLength - 5, ".firm", 5) == 0)
            count++;
    }

    f_closedir(&dir);
    return count;
}

static u32 listFirmPayloads(char payloadList[][49], u32 maxPayloads)
{
    DIR dir;
    FILINFO info;
    u32 count = 0;

    if(f_opendir(&dir, "payloads") != FR_OK)
        return 0;

    while(f_readdir(&dir, &info) == FR_OK && info.fname[0] != 0 && count < maxPayloads)
    {
        if(info.fname[0] == '.')
            continue;

        u32 nameLength = strlen(info.fname);
        if(nameLength < 6 || nameLength > 52)
            continue;

        if(memcmp(info.fname + nameLength - 5, ".firm", 5) != 0)
            continue;

        nameLength -= 5;
        memcpy(payloadList[count], info.fname, nameLength);
        payloadList[count][nameLength] = 0;
        count++;
    }

    f_closedir(&dir);
    return count;
}

static const char *onOff(bool value)
{
    return value ? "on" : "off";
}

static void launchGodMode9Tools(void)
{
    static const char *items[] = {
        "Start GodMode9",
        "System save dump script",
        "Back to Boot Hub",
    };

    static const char *descriptions[] = {
        "Launch GodMode9 from /omiiba/payloads.",
        "Launch GodMode9, then run:\n\n"
        "HOME -> Scripts -> Omiiba_System_Save_Dump\n\n"
        "The release zip installs this script to\n"
        "SD:/gm9/scripts/.",
        "Return to the Omiiba Boot Hub.",
    };

    const u32 itemAmount = sizeof(items) / sizeof(items[0]);
    u32 selectedItem = 0;

    while(true)
    {
        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "GodMode9 tools");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: select    B: Boot Hub");

        for(u32 i = 0; i < itemAmount; i++)
            drawString(true, 10, 10 + (3 + i) * SPACING_Y, i == selectedItem ? COLOR_RED : COLOR_WHITE, items[i]);

        drawString(false, 10, 10, COLOR_WHITE, descriptions[selectedItem]);

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return;

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedItem = !selectedItem ? itemAmount - 1 : selectedItem - 1;
                    break;
                case BUTTON_DOWN:
                    selectedItem = selectedItem == itemAmount - 1 ? 0 : selectedItem + 1;
                    break;
                case BUTTON_LEFT:
                    selectedItem = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedItem = itemAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            const char *path = findGodMode9Payload();

            if(selectedItem == 2)
                return;

            if(path != NULL)
                loadHomebrewFirmPath(path, true);

            drawBootHubMessage("GodMode9 tools",
                               "GodMode9 was not found.\n\n"
                               "Place GodMode9.firm at:\n"
                               "SD:/omiiba/payloads/GodMode9.firm\n\n"
                               "Then reopen this menu to launch it.");
            waitForBootHubBack();
        }
    }
}

typedef struct
{
    const char *name;
    const char *description;
    const char *scaler;
    const char *colorProfile;
    const char *contrast;
    const char *brightness;
    const char *saturation;
    u32 backlight;
    bool showWarning;
} OpenAgbDisplayPreset;

static char openAgbConfigBuffer[1024];

static const OpenAgbDisplayPreset openAgbDisplayPresets[] = {
    {
        "Original 1:1",
        "No scaling and no color correction.\n\n"
        "Best for sharp native-size output.",
        "none", "none", "1.0", "0.0", "1.0", 56, false,
    },
    {
        "Balanced matrix",
        "Matrix scaler with GBA color profile.\n\n"
        "Recommended starting point for most games.",
        "matrix", "gba", "1.0", "0.0", "0.95", 64, false,
    },
    {
        "Soft bilinear",
        "Bilinear scaler with SP101-style colors.\n\n"
        "Softer fullscreen look with brighter color.",
        "bilinear", "gba_sp101", "1.0", "0.0", "1.0", 64, false,
    },
    {
        "GBA color",
        "Matrix scaler with classic GBA color feel.\n\n"
        "Slightly lower saturation for older games.",
        "matrix", "gba", "0.96", "-0.02", "0.86", 60, false,
    },
    {
        "SP101 bright",
        "Matrix scaler with AGS-101-style color.\n\n"
        "Brighter backlight; may use more battery.",
        "matrix", "gba_sp101", "1.04", "0.02", "1.0", 82, true,
    },
    {
        "Vivid emulator",
        "Matrix scaler with VBA-style colors.\n\n"
        "More saturated, less hardware-authentic.",
        "matrix", "vba", "1.08", "0.02", "1.18", 74, true,
    },
    {
        "Battery saver",
        "Matrix scaler with color correction disabled.\n\n"
        "Lower backlight for longer play sessions.",
        "matrix", "none", "1.0", "0.0", "1.0", 36, false,
    },
    {
        "Restore safe default config",
        "Write a complete safe config.ini.\n\n"
        "This does not delete or empty the file.",
        "matrix", "none", "1.0", "0.0", "1.0", 64, false,
    },
};

static bool writeOpenAgbDisplayPreset(const OpenAgbDisplayPreset *preset)
{
    f_mkdir("sdmc:/3ds");
    f_mkdir("sdmc:/3ds/open_agb_firm");

    int n = sprintf(openAgbConfigBuffer,
                    "# Generated by Omiiba3DS GBA Labs.\n"
                    "# Preset: %s\n"
                    "# Edit in open_agb_firm if you want per-game tuning.\n\n"
                    "[general]\n"
                    "backlight=%lu\n"
                    "backlightSteps=5\n"
                    "directBoot=false\n"
                    "useGbaDb=true\n"
                    "useSavesFolder=true\n\n"
                    "[video]\n"
                    "scaler=%s\n"
                    "colorProfile=%s\n"
                    "contrast=%s\n"
                    "brightness=%s\n"
                    "saturation=%s\n",
                    preset->name,
                    (unsigned long)preset->backlight,
                    preset->scaler,
                    preset->colorProfile,
                    preset->contrast,
                    preset->brightness,
                    preset->saturation);

    return n > 0 && (u32)n < sizeof(openAgbConfigBuffer) &&
           fileWrite(openAgbConfigBuffer, "sdmc:/3ds/open_agb_firm/config.ini", (u32)n);
}

static void applyOpenAgbDisplayPreset(const OpenAgbDisplayPreset *preset)
{
    if(preset->showWarning &&
       !askBootHubConfirm("GBA display preset",
                          "This preset uses brighter/heavier color\n"
                          "processing and may reduce battery life.\n\n"
                          "Apply it anyway?",
                          "apply", "cancel"))
        return;

    if(writeOpenAgbDisplayPreset(preset))
    {
        drawBootHubMessage("GBA display preset",
                           "open_agb_firm config.ini was written.\n\n"
                           "Path:\n"
                           "SD:/3ds/open_agb_firm/config.ini\n\n"
                           "You can now start open_agb_firm.");
        waitForBootHubBack();
    }
    else
    {
        drawBootHubMessage("GBA display preset",
                           "Failed to write open_agb_firm config.\n\n"
                           "Check that the SD card is present and\n"
                           "not write-protected.");
        waitForBootHubBack();
    }
}

static void writeVcPatchHelpFile(void)
{
    static const char helpText[] =
        "Omiiba3DS VC Patch Helper\n"
        "=========================\n\n"
        "Enable game patching in Omiiba/Luma settings first.\n\n"
        "Per-title patch root:\n"
        "  SD:/omiiba/titles/<16-digit TITLEID>/\n\n"
        "Supported files/folders:\n"
        "  code.ips       IPS code patch\n"
        "  code.bps       BPS code patch\n"
        "  code.bin       decompressed replacement ExeFS code\n"
        "  exheader.bin   decrypted replacement exheader\n"
        "  locale.txt     region/language override\n"
        "  romfs/         LayeredFS file replacement folder\n\n"
        "Virtual Console notes:\n"
        "  NES/SNES/GB/GBC VC titles can use normal title patching.\n"
        "  GBA VC runs under AGB_FIRM; display scaling/color patches need\n"
        "  separate AGB_FIRM research and are not enabled by this helper.\n";

    f_mkdir("sdmc:/omiiba");
    fileWrite(helpText, "sdmc:/omiiba/VC_PATCH_HELP.txt", sizeof(helpText) - 1);
}

static void createVcPatchFolders(void)
{
    f_mkdir("sdmc:/omiiba");
    f_mkdir("sdmc:/omiiba/titles");
    f_mkdir("sdmc:/omiiba/titles/0004000000000000");
    f_mkdir("sdmc:/omiiba/titles/0004000000000000/romfs");
    writeVcPatchHelpFile();
}

static void launchVcPatchHelper(void)
{
    static const char *items[] = {
        "VC patch status",
        "Create patch folders",
        "Patch file layout",
        "GBA VC warning",
        "Back to Boot Hub",
    };

    static const char *descriptions[] = {
        "Check whether game patching is enabled\n"
        "and whether /omiiba/titles exists.",
        "Create a safe template at:\n\n"
        "SD:/omiiba/titles/0004000000000000/romfs/\n\n"
        "Rename the title ID folder to the real\n"
        "16-digit Virtual Console title ID.",
        "Omiiba title patch layout:\n\n"
        "code.ips / code.bps / code.bin\n"
        "exheader.bin / locale.txt / romfs/",
        "GBA VC injects run under AGB_FIRM.\n\n"
        "Normal title patches can affect the title,\n"
        "but display scaling/color needs separate\n"
        "AGB_FIRM patch research.",
        "Return to the Omiiba Boot Hub.",
    };

    const u32 itemAmount = sizeof(items) / sizeof(items[0]);
    u32 selectedItem = 0;

    while(true)
    {
        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "VC Patch Helper");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: select    B: Boot Hub");
        drawString(true, 10, 10 + 2 * SPACING_Y, COLOR_WHITE, "LayeredFS / IPS / BPS setup helper");

        for(u32 i = 0; i < itemAmount; i++)
            drawString(true, 10, 10 + (4 + i) * SPACING_Y, i == selectedItem ? COLOR_RED : COLOR_WHITE, items[i]);

        drawString(false, 10, 10, COLOR_WHITE, descriptions[selectedItem]);

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return;

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedItem = !selectedItem ? itemAmount - 1 : selectedItem - 1;
                    break;
                case BUTTON_DOWN:
                    selectedItem = selectedItem == itemAmount - 1 ? 0 : selectedItem + 1;
                    break;
                case BUTTON_LEFT:
                    selectedItem = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedItem = itemAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            if(selectedItem == 0)
            {
                clearScreens(false);
                drawString(true, 10, 10, COLOR_TITLE, "VC patch status");
                drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "Read-only title patch checks");

                drawFormattedString(true, 10, 10 + 3 * SPACING_Y, COLOR_WHITE,
                                    "Game patching: %s", onOff(CONFIG(PATCHGAMES)));
                drawFormattedString(true, 10, 10 + 4 * SPACING_Y, COLOR_WHITE,
                                    "Patch root: %s", directoryExists("sdmc:/omiiba/titles") ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 5 * SPACING_Y, COLOR_WHITE,
                                    "Template folder: %s", directoryExists("sdmc:/omiiba/titles/0004000000000000") ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 6 * SPACING_Y, COLOR_WHITE,
                                    "Template romfs: %s", directoryExists("sdmc:/omiiba/titles/0004000000000000/romfs") ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 7 * SPACING_Y, COLOR_WHITE,
                                    "Help file: %s", getFileSize("sdmc:/omiiba/VC_PATCH_HELP.txt") > 0 ? "found" : "missing");

                drawString(false, 10, 10, COLOR_WHITE,
                           "Use a real 16-digit title ID folder for\n"
                           "actual patches. The template folder is\n"
                           "only a safe starting point.\n\n"
                           "Press A/B/START to return.");
                waitForBootHubBack();
            }
            else if(selectedItem == 1)
            {
                createVcPatchFolders();
                drawBootHubMessage("VC Patch Helper",
                                   "Created/checked patch helper files:\n\n"
                                   "SD:/omiiba/titles/\n"
                                   "SD:/omiiba/titles/0004000000000000/romfs/\n"
                                   "SD:/omiiba/VC_PATCH_HELP.txt\n\n"
                                   "Rename the template folder to a real\n"
                                   "16-digit Virtual Console title ID.");
                waitForBootHubBack();
            }
            else if(selectedItem == itemAmount - 1)
                return;
            else
            {
                drawBootHubMessage("VC Patch Helper", descriptions[selectedItem]);
                waitForBootHubBack();
            }
        }
    }
}

static void launchOpenAgbDisplayPresets(void)
{
    const u32 presetAmount = sizeof(openAgbDisplayPresets) / sizeof(openAgbDisplayPresets[0]);
    const u32 itemAmount = presetAmount + 2;
    u32 selectedItem = 0;

    while(true)
    {
        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "GBA display presets");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: apply    B: GBA Labs");

        for(u32 i = 0; i < presetAmount; i++)
            drawString(true, 10, 10 + (3 + i) * SPACING_Y,
                       i == selectedItem ? COLOR_RED : COLOR_WHITE,
                       openAgbDisplayPresets[i].name);

        drawString(true, 10, 10 + (3 + presetAmount) * SPACING_Y,
                   presetAmount == selectedItem ? COLOR_RED : COLOR_WHITE,
                   "Apply and start open_agb_firm");
        drawString(true, 10, 10 + (4 + presetAmount) * SPACING_Y,
                   presetAmount + 1 == selectedItem ? COLOR_RED : COLOR_WHITE,
                   "Back to GBA Labs");

        if(selectedItem < presetAmount)
            drawString(false, 10, 10, COLOR_WHITE, openAgbDisplayPresets[selectedItem].description);
        else if(selectedItem == presetAmount)
            drawString(false, 10, 10, COLOR_WHITE,
                       "Apply the selected preset first if needed,\n"
                       "then launch the bundled open_agb_firm.");
        else
            drawString(false, 10, 10, COLOR_WHITE, "Return to the GBA Labs menu.");

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return;

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedItem = !selectedItem ? itemAmount - 1 : selectedItem - 1;
                    break;
                case BUTTON_DOWN:
                    selectedItem = selectedItem == itemAmount - 1 ? 0 : selectedItem + 1;
                    break;
                case BUTTON_LEFT:
                    selectedItem = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedItem = itemAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            if(selectedItem < presetAmount)
                applyOpenAgbDisplayPreset(&openAgbDisplayPresets[selectedItem]);
            else if(selectedItem == presetAmount)
            {
                const char *path = findOpenAgbFirmPayload();

                if(path != NULL)
                    loadHomebrewFirmPath(path, true);

                drawBootHubMessage("GBA display presets",
                                   "open_agb_firm was not found.\n\n"
                                   "Expected bundled path:\n"
                                   "SD:/omiiba/payloads/open_agb_firm.firm");
                waitForBootHubBack();
            }
            else
                return;
        }
    }
}

static void writeGbaVcResearchFile(void)
{
    static const char researchText[] =
        "Omiiba3DS GBA VC Patch Research\n"
        "===============================\n\n"
        "Current safe AGB_FIRM patch point:\n"
        "  Show GBA boot screen in patched AGB_FIRM\n\n"
        "Not enabled yet:\n"
        "  Forced GBA VC scaling filters\n"
        "  Forced GBA VC color/brightness presets\n\n"
        "Reason:\n"
        "  GBA Virtual Console injects run under AGB_FIRM legacy mode.\n"
        "  Omiiba currently has patchAgbFirm() and patchAgbBootSplash(),\n"
        "  but no verified AgbBg/display-driver patch point for color or\n"
        "  scaling. Enabling blind byte patches here could black-screen.\n\n"
        "Safe current recommendation:\n"
        "  Use open_agb_firm for SD-card GBA games and Omiiba display presets.\n"
        "  Keep VC inject display patching in alpha research until verified.\n";

    f_mkdir("sdmc:/omiiba");
    f_mkdir("sdmc:/omiiba/agb_vc_research");
    fileWrite(researchText, "sdmc:/omiiba/agb_vc_research/README.txt", sizeof(researchText) - 1);
}

static void launchGbaVcPatchResearch(void)
{
    static const char *items[] = {
        "Patch point status",
        "Create research notes",
        "Display patch warning",
        "Back to GBA Labs",
    };

    static const char *descriptions[] = {
        "Show the safe AGB_FIRM patch status\n"
        "that Omiiba can currently verify.",
        "Create SD:/omiiba/agb_vc_research/README.txt\n"
        "with current GBA VC research notes.",
        "GBA VC scaling/color patches are not\n"
        "enabled until a verified AgbBg/display\n"
        "patch point is known for hardware tests.",
        "Return to GBA Labs.",
    };

    const u32 itemAmount = sizeof(items) / sizeof(items[0]);
    u32 selectedItem = 0;

    while(true)
    {
        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "GBA VC research");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: select    B: GBA Labs");
        drawString(true, 10, 10 + 2 * SPACING_Y, COLOR_WHITE, "AGB_FIRM display patch prototype area");

        for(u32 i = 0; i < itemAmount; i++)
            drawString(true, 10, 10 + (4 + i) * SPACING_Y, i == selectedItem ? COLOR_RED : COLOR_WHITE, items[i]);

        drawString(false, 10, 10, COLOR_WHITE, descriptions[selectedItem]);

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return;

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedItem = !selectedItem ? itemAmount - 1 : selectedItem - 1;
                    break;
                case BUTTON_DOWN:
                    selectedItem = selectedItem == itemAmount - 1 ? 0 : selectedItem + 1;
                    break;
                case BUTTON_LEFT:
                    selectedItem = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedItem = itemAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            if(selectedItem == 0)
            {
                clearScreens(false);
                drawString(true, 10, 10, COLOR_TITLE, "AGB patch status");
                drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "Read-only research checks");

                drawFormattedString(true, 10, 10 + 3 * SPACING_Y, COLOR_WHITE,
                                    "GBA boot splash patch: %s", onOff(CONFIG(SHOWGBABOOT)));
                drawString(true, 10, 10 + 4 * SPACING_Y, COLOR_WHITE,
                           "AGB display patch: research only");
                drawFormattedString(true, 10, 10 + 5 * SPACING_Y, COLOR_WHITE,
                                    "Research folder: %s", directoryExists("sdmc:/omiiba/agb_vc_research") ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 6 * SPACING_Y, COLOR_WHITE,
                                    "Research notes: %s", getFileSize("sdmc:/omiiba/agb_vc_research/README.txt") > 0 ? "found" : "missing");

                drawString(false, 10, 10, COLOR_WHITE,
                           "This screen deliberately does not patch\n"
                           "GBA VC display registers yet.\n\n"
                           "It tracks the alpha research state so\n"
                           "we avoid unsafe black-screen builds.\n\n"
                           "Press A/B/START to return.");
                waitForBootHubBack();
            }
            else if(selectedItem == 1)
            {
                writeGbaVcResearchFile();
                drawBootHubMessage("GBA VC research",
                                   "Created research notes at:\n\n"
                                   "SD:/omiiba/agb_vc_research/README.txt\n\n"
                                   "Display scaling/color patching remains\n"
                                   "research until a patch point is verified.");
                waitForBootHubBack();
            }
            else if(selectedItem == itemAmount - 1)
                return;
            else
            {
                drawBootHubMessage("GBA VC research", descriptions[selectedItem]);
                waitForBootHubBack();
            }
        }
    }
}

static void launchGbaLabs(void)
{
    static const char *items[] = {
        "Start open_agb_firm",
        "Display presets",
        "GBA VC patch research",
        "open_agb_firm setup help",
        "GBA display notes",
        "AGB_FIRM scaling filters: research",
        "AGB_FIRM color presets: research",
        "Back to Boot Hub",
    };

    static const char *descriptions[] = {
        "Launch open_agb_firm from /omiiba/payloads.\n\n"
        "This is the recommended stable path for\n"
        "SD-card GBA ROM booting.",
        "Write /3ds/open_agb_firm/config.ini\n"
        "with scaler, color, brightness and battery\n"
        "presets before launching open_agb_firm.",
        "Prototype area for GBA Virtual Console\n"
        "patch-point tracking and research notes.\n\n"
        "No unsafe display byte patches are applied.",
        "Place open_agb_firm at:\n\n"
        "SD:/omiiba/payloads/open_agb_firm.firm\n\n"
        "v1.4.4 release zips bundle it together\n"
        "with /3ds/open_agb_firm/gba_db.bin.",
        "AGB_FIRM VC injects run in legacy GBA mode.\n\n"
        "Rosalina-style live filters are not expected\n"
        "to work there without deeper FIRM patches.",
        "Experimental research only.\n\n"
        "TWL has patchTwlBg(), but AGB currently\n"
        "only has patchAgbBootSplash() here.",
        "Experimental research only.\n\n"
        "VC inject color/gamma work needs a verified\n"
        "AgbBg/display-driver patch point first.",
        "Return to the Omiiba Boot Hub.",
    };

    const u32 itemAmount = sizeof(items) / sizeof(items[0]);
    u32 selectedItem = 0;

    while(true)
    {
        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "GBA Labs");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: select    B: Boot Hub");
        drawString(true, 10, 10 + 2 * SPACING_Y, COLOR_WHITE, "Experimental GBA tools and research");

        for(u32 i = 0; i < itemAmount; i++)
            drawString(true, 10, 10 + (4 + i) * SPACING_Y, i == selectedItem ? COLOR_RED : COLOR_WHITE, items[i]);

        drawString(false, 10, 10, COLOR_WHITE, descriptions[selectedItem]);

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return;

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedItem = !selectedItem ? itemAmount - 1 : selectedItem - 1;
                    break;
                case BUTTON_DOWN:
                    selectedItem = selectedItem == itemAmount - 1 ? 0 : selectedItem + 1;
                    break;
                case BUTTON_LEFT:
                    selectedItem = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedItem = itemAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            if(selectedItem == 0)
            {
                const char *path = findOpenAgbFirmPayload();

                if(path != NULL)
                    loadHomebrewFirmPath(path, true);

                drawBootHubMessage("GBA Labs",
                                   "open_agb_firm was not found.\n\n"
                                   "Place it at:\n"
                                   "SD:/omiiba/payloads/open_agb_firm.firm\n\n"
                                   "Then reopen GBA Labs to launch it.");
                waitForBootHubBack();
            }
            else if(selectedItem == 1)
                launchOpenAgbDisplayPresets();
            else if(selectedItem == 2)
                launchGbaVcPatchResearch();
            else if(selectedItem == itemAmount - 1)
                return;
            else
            {
                drawBootHubMessage("GBA Labs",
                                   descriptions[selectedItem]);
                waitForBootHubBack();
            }
        }
    }
}

static void launchDsLabs(void)
{
    static const char *items[] = {
        "DS setup status",
        "Create DS folders",
        "Bundled TWiLight help",
        "TWL filter help",
        "DS ROM folder notes",
        "Back to Boot Hub",
    };

    static const char *descriptions[] = {
        "Check common DS/TWiLight folders and files.\n\n"
        "This is read-only and does not change SD.",
        "Create recommended DS folders on SD:\n\n"
        "SD:/roms/\n"
        "SD:/roms/nds/\n\n"
        "This does not install the TWiLight CIA.",
        "The release zip bundles TWiLight Menu++:\n\n"
        "SD:/TWiLight Menu.cia\n"
        "SD:/BOOT.NDS\n"
        "SD:/_nds/\n\n"
        "Install the CIA with FBI, then put DS ROMs\n"
        "in SD:/roms/nds/.",
        "Omiiba can patch TWL_FIRM's upscaling filter\n"
        "when Advanced setting Enable DSi external\n"
        "filter is on and this file exists:\n\n"
        "SD:/omiiba/twl_upscaling_filter.bin",
        "Recommended DS ROM folder:\n\n"
        "SD:/roms/nds/\n\n"
        "TWiLight Menu++ uses a file browser, so this\n"
        "is guidance rather than a hard requirement.",
        "Return to the Omiiba Boot Hub.",
    };

    const u32 itemAmount = sizeof(items) / sizeof(items[0]);
    u32 selectedItem = 0;

    while(true)
    {
        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "DS Labs");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: select    B: Boot Hub");
        drawString(true, 10, 10 + 2 * SPACING_Y, COLOR_WHITE, "TWiLight/nds-bootstrap setup help");

        for(u32 i = 0; i < itemAmount; i++)
            drawString(true, 10, 10 + (4 + i) * SPACING_Y, i == selectedItem ? COLOR_RED : COLOR_WHITE, items[i]);

        drawString(false, 10, 10, COLOR_WHITE, descriptions[selectedItem]);

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return;

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedItem = !selectedItem ? itemAmount - 1 : selectedItem - 1;
                    break;
                case BUTTON_DOWN:
                    selectedItem = selectedItem == itemAmount - 1 ? 0 : selectedItem + 1;
                    break;
                case BUTTON_LEFT:
                    selectedItem = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedItem = itemAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            if(selectedItem == 0)
            {
                clearScreens(false);
                drawString(true, 10, 10, COLOR_TITLE, "DS setup status");
                drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "Read-only DS/TWL checks");

                drawFormattedString(true, 10, 10 + 3 * SPACING_Y, COLOR_WHITE,
                                    "DS ROM folder: %s", directoryExists("sdmc:/roms/nds") ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 4 * SPACING_Y, COLOR_WHITE,
                                    "TWiLight CIA: %s", getFileSize("sdmc:/TWiLight Menu.cia") > 0 ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 5 * SPACING_Y, COLOR_WHITE,
                                    "BOOT.NDS: %s", getFileSize("sdmc:/BOOT.NDS") > 0 ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 6 * SPACING_Y, COLOR_WHITE,
                                    "TWiLight _nds: %s", directoryExists("sdmc:/_nds") ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 7 * SPACING_Y, COLOR_WHITE,
                                    "TWiLightMenu: %s", directoryExists("sdmc:/_nds/TWiLightMenu") ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 8 * SPACING_Y, COLOR_WHITE,
                                    "nds-bootstrap: %s",
                                    getFileSize("sdmc:/_nds/nds-bootstrap-release.nds") > 0 ||
                                    getFileSize("sdmc:/_nds/nds-bootstrap.nds") > 0 ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 9 * SPACING_Y, COLOR_WHITE,
                                    "Cheat DB: %s", getFileSize("sdmc:/_nds/TWiLightMenu/extras/usrcheat.dat") > 0 ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 10 * SPACING_Y, COLOR_WHITE,
                                    "TWL filter bin: %s", getFileSize("twl_upscaling_filter.bin") > 0 ? "found" : "missing");
                drawFormattedString(true, 10, 10 + 11 * SPACING_Y, COLOR_WHITE,
                                    "TWL external filter: %s", onOff(CONFIG(ENABLEDSIEXTFILTER)));

                drawString(false, 10, 10, COLOR_WHITE,
                           "Recommended DS ROM folder:\n"
                           "SD:/roms/nds/\n\n"
                           "Install SD:/TWiLight Menu.cia with FBI\n"
                           "to launch TWiLight from HOME Menu.\n\n"
                           "Press A/B/START to return.");
                waitForBootHubBack();
            }
            else if(selectedItem == 1)
            {
                f_mkdir("sdmc:/roms");
                f_mkdir("sdmc:/roms/nds");
                drawBootHubMessage("DS Labs",
                                   "Created/checked DS folders:\n\n"
                                   "SD:/roms/\n"
                                   "SD:/roms/nds/\n\n"
                                   "Put .nds ROMs in SD:/roms/nds/.");
                waitForBootHubBack();
            }
            else if(selectedItem == itemAmount - 1)
                return;
            else
            {
                drawBootHubMessage("DS Labs",
                                   descriptions[selectedItem]);
                waitForBootHubBack();
            }
        }
    }
}

static void showBootHubDiagnostics(void)
{
    FirmwareSource emuNandType = FIRMWARE_EMUNAND;
    u32 emuIndex = 0;
    if(isSdMode)
        locateEmuNand(&emuNandType, &emuIndex, false);
    else
        emuNandType = FIRMWARE_SYSNAND;

    const char *splashMode;
    switch(MULTICONFIG(SPLASH))
    {
        default:
        case 0:
            splashMode = "off";
            break;
        case 1:
            splashMode = "before payloads";
            break;
        case 2:
            splashMode = "after payloads";
            break;
    }

    clearScreens(false);
    drawString(true, 10, 10, COLOR_TITLE, "Omiiba diagnostics");
    drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "Read-only system checks");

    drawFormattedString(true, 10, 10 + 3 * SPACING_Y, COLOR_WHITE,
                        "Storage: %s", isSdMode ? "SD:/omiiba" : "CTRNAND:/rw/omiiba");
    drawFormattedString(true, 10, 10 + 4 * SPACING_Y, COLOR_WHITE,
                        "config.ini: %s", getFileSize("config.ini") > 0 ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 5 * SPACING_Y, COLOR_WHITE,
                        "GodMode9: %s", findGodMode9Payload() != NULL ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 6 * SPACING_Y, COLOR_WHITE,
                        "open_agb_firm: %s", findOpenAgbFirmPayload() != NULL ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 7 * SPACING_Y, COLOR_WHITE,
                        "GBA database: %s", getFileSize("sdmc:/3ds/open_agb_firm/gba_db.bin") > 0 ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 8 * SPACING_Y, COLOR_WHITE,
                        "GBA config: %s", getFileSize("sdmc:/3ds/open_agb_firm/config.ini") > 0 ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 9 * SPACING_Y, COLOR_WHITE,
                        "GBA ROM folder: %s", directoryExists("sdmc:/gba") ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 10 * SPACING_Y, COLOR_WHITE,
                        "DS ROM folder: %s", directoryExists("sdmc:/roms/nds") ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 11 * SPACING_Y, COLOR_WHITE,
                        "TWiLight CIA: %s", getFileSize("sdmc:/TWiLight Menu.cia") > 0 ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 12 * SPACING_Y, COLOR_WHITE,
                        "TWiLight _nds: %s", directoryExists("sdmc:/_nds") ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 13 * SPACING_Y, COLOR_WHITE,
                        "TWL filter bin: %s", getFileSize("twl_upscaling_filter.bin") > 0 ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 14 * SPACING_Y, COLOR_WHITE,
                        "GM9 save script: %s", getFileSize("sdmc:/gm9/scripts/Omiiba_System_Save_Dump.gm9") > 0 ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 15 * SPACING_Y, COLOR_WHITE,
                        "Payloads: %lu .firm file(s)", countFirmPayloads());
    drawFormattedString(true, 10, 10 + 16 * SPACING_Y, COLOR_WHITE,
                        "SD boot.firm: %s", getFileSize("sdmc:/boot.firm") > 0 ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 17 * SPACING_Y, COLOR_WHITE,
                        "CTRNAND boot.firm: %s", getFileSize("nand:/boot.firm") > 0 ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 18 * SPACING_Y, COLOR_WHITE,
                        "EmuNAND: %s", emuNandType == FIRMWARE_EMUNAND ? "detected" : "not detected");
    drawFormattedString(true, 10, 10 + 19 * SPACING_Y, COLOR_WHITE,
                        "Splash: %s", splashMode);
    drawFormattedString(true, 10, 10 + 20 * SPACING_Y, COLOR_WHITE,
                        "Game patching: %s", onOff(CONFIG(PATCHGAMES)));
    drawFormattedString(true, 10, 10 + 21 * SPACING_Y, COLOR_WHITE,
                        "VC patch root: %s", directoryExists("sdmc:/omiiba/titles") ? "found" : "missing");
    drawFormattedString(true, 10, 10 + 22 * SPACING_Y, COLOR_WHITE,
                        "GBA VC research: %s", getFileSize("sdmc:/omiiba/agb_vc_research/README.txt") > 0 ? "found" : "missing");

    drawString(false, 10, 10, COLOR_WHITE,
               "Diagnostics only reads files, folders and\n"
               "existing config values. It does not change NAND.\n\n"
               "Missing items are guidance, not always errors.\n"
               "For example, CTRNAND boot.firm and GodMode9\n"
               "are optional but recommended for recovery.\n\n"
               "Press A/B/START to return.");
    waitForBootHubBack();
}

static bool choosePayloadHotkey(const char **prefix, const char **label)
{
    static const char *items[] = {
        "X",
        "Y",
        "B",
        "A while holding L",
        "START while holding L",
        "SELECT while holding L",
        "Back",
    };

    static const char *prefixes[] = {
        "x",
        "y",
        "b",
        "a",
        "start",
        "select",
    };

    const u32 itemAmount = sizeof(items) / sizeof(items[0]);
    u32 selectedItem = 0;

    while(true)
    {
        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "Payload hotkey copy");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: choose    B: cancel");

        for(u32 i = 0; i < itemAmount; i++)
            drawString(true, 10, 10 + (3 + i) * SPACING_Y, i == selectedItem ? COLOR_RED : COLOR_WHITE, items[i]);

        drawString(false, 10, 10, COLOR_WHITE,
                   "Creates a copy named like x_name.firm.\n\n"
                   "This does not rename or delete the original\n"
                   "payload, so it is safe to undo manually.");

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return false;

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedItem = !selectedItem ? itemAmount - 1 : selectedItem - 1;
                    break;
                case BUTTON_DOWN:
                    selectedItem = selectedItem == itemAmount - 1 ? 0 : selectedItem + 1;
                    break;
                case BUTTON_LEFT:
                    selectedItem = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedItem = itemAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            if(selectedItem >= itemAmount - 1)
                return false;

            *prefix = prefixes[selectedItem];
            *label = items[selectedItem];
            return true;
        }
    }
}

static void copyPayloadAsHotkey(const char *payloadName)
{
    const char *prefix, *label;
    char src[10 + 49 + 5];
    char dst[10 + 49 + 5 + 8];
    static u8 copyBuffer[0x10000];

    if(!choosePayloadHotkey(&prefix, &label))
        return;

    sprintf(src, "payloads/%s.firm", payloadName);
    sprintf(dst, "payloads/%s_%s.firm", prefix, payloadName);

    clearScreens(false);
    drawString(true, 10, 10, COLOR_TITLE, "Payload hotkey copy");
    drawFormattedString(true, 10, 10 + 2 * SPACING_Y, COLOR_WHITE,
                        "Copy:\n%s\n\nto:\n%s\n\nHotkey: %s", src, dst, label);
    drawString(false, 10, 10, COLOR_WHITE,
               "A: create copy\nB: cancel\n\n"
               "Existing destination files are not overwritten.");

    u32 pressed;
    do
    {
        pressed = waitInput(true) & (BUTTON_A | BUTTON_B);
    }
    while(!pressed);

    if(!(pressed & BUTTON_A))
        return;

    if(fileCopy(src, dst, false, copyBuffer, sizeof(copyBuffer)))
        drawBootHubMessage("Payload copied",
                           "Hotkey copy created.\n\n"
                           "If the file already existed, no overwrite\n"
                           "was performed.");
    else
        drawBootHubMessage("Payload copy failed",
                           "Could not create the hotkey copy.\n\n"
                           "Check free SD space and whether a file\n"
                           "with that hotkey name already exists.");

    waitForBootHubBack();
}

static void showPayloadHotkeyGuide(void)
{
    drawBootHubMessage("Payload hotkeys",
                       "Supported names:\n"
                       "x_name.firm / y_name.firm / b_name.firm\n"
                       "a_name.firm boots with L + A\n"
                       "start_name.firm boots with L + START\n"
                       "select_name.firm boots with L + SELECT\n\n"
                       "START alone opens the chainloader menu.");
    waitForBootHubBack();
}

static void runPayloadManager(void)
{
    char payloadList[20][49];
    u32 selectedPayload = 0;

    while(true)
    {
        u32 payloadAmount = listFirmPayloads(payloadList, 20);

        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "Payload manager");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: menu    B: Boot Hub");

        if(payloadAmount == 0)
        {
            drawString(true, 10, 10 + 3 * SPACING_Y, COLOR_WHITE,
                       "No .firm payloads found in /omiiba/payloads.");
            drawString(false, 10, 10, COLOR_WHITE,
                       "Put GodMode9.firm or other payloads in:\n"
                       "SD:/omiiba/payloads/\n\n"
                       "Press A for hotkey naming help,\n"
                       "or B to return.");
        }
        else
        {
            for(u32 i = 0; i < payloadAmount; i++)
                drawString(true, 10, 10 + (3 + i) * SPACING_Y, i == selectedPayload ? COLOR_RED : COLOR_WHITE, payloadList[i]);

            drawFormattedString(false, 10, 10, COLOR_WHITE,
                                "Selected: %s.firm\n\n"
                                "A: actions\nB: return\n\n"
                                "Actions: launch, copy as hotkey,\n"
                                "hotkey guide.", payloadList[selectedPayload]);
        }

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return;

        if(payloadAmount == 0)
        {
            if(pressed & BUTTON_A)
                showPayloadHotkeyGuide();
            continue;
        }

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedPayload = !selectedPayload ? payloadAmount - 1 : selectedPayload - 1;
                    break;
                case BUTTON_DOWN:
                    selectedPayload = selectedPayload == payloadAmount - 1 ? 0 : selectedPayload + 1;
                    break;
                case BUTTON_LEFT:
                    selectedPayload = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedPayload = payloadAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            static const char *actions[] = {
                "Launch selected payload",
                "Copy selected as hotkey",
                "Hotkey guide",
                "Back",
            };
            u32 selectedAction = 0;

            while(true)
            {
                clearScreens(false);
                drawString(true, 10, 10, COLOR_TITLE, "Payload actions");
                drawFormattedString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "%s.firm", payloadList[selectedPayload]);

                for(u32 i = 0; i < sizeof(actions) / sizeof(actions[0]); i++)
                    drawString(true, 10, 10 + (3 + i) * SPACING_Y, i == selectedAction ? COLOR_RED : COLOR_WHITE, actions[i]);

                drawString(false, 10, 10, COLOR_WHITE,
                           "Launch uses the same chainloader as START.\n\n"
                           "Copy as hotkey creates a duplicate with\n"
                           "x_/y_/b_/a_/start_/select_ prefix.");

                do
                {
                    pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
                }
                while(!pressed);

                if(pressed & BUTTON_B)
                    break;

                if(pressed & DPAD_BUTTONS)
                {
                    switch(pressed & DPAD_BUTTONS)
                    {
                        case BUTTON_UP:
                            selectedAction = !selectedAction ? 3 : selectedAction - 1;
                            break;
                        case BUTTON_DOWN:
                            selectedAction = selectedAction == 3 ? 0 : selectedAction + 1;
                            break;
                        case BUTTON_LEFT:
                            selectedAction = 0;
                            break;
                        case BUTTON_RIGHT:
                            selectedAction = 3;
                            break;
                        default:
                            break;
                    }
                }
                else if(pressed & BUTTON_A)
                {
                    char path[10 + 49 + 5];
                    sprintf(path, "payloads/%s.firm", payloadList[selectedPayload]);

                    switch(selectedAction)
                    {
                        case 0:
                            loadHomebrewFirmPath(path, true);
                            break;
                        case 1:
                            copyPayloadAsHotkey(payloadList[selectedPayload]);
                            break;
                        case 2:
                            showPayloadHotkeyGuide();
                            break;
                        default:
                            break;
                    }

                    break;
                }
            }
        }
    }
}

static const char *bootThemeName(u8 idx)
{
    static const char *names[] = {
        "Omiiba amber",
        "Midnight blue",
        "Pasture green",
        "Berry purple",
    };

    return names[idx % (sizeof(names) / sizeof(names[0]))];
}

static u8 readBootThemeIndex(void)
{
    u8 idx = 0;
    if(fileRead(&idx, ".boot_theme", 1) != 1)
        idx = 0;

    return idx % 4;
}

static void writeBootThemeIndex(u8 idx)
{
    idx %= 4;
    fileWrite(&idx, ".boot_theme", 1);
}

static void chooseBootTheme(void)
{
    const u32 themeAmount = 4;
    u32 selectedTheme = readBootThemeIndex();

    while(true)
    {
        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "Boot splash theme");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: save theme    B: cancel");

        for(u32 i = 0; i < themeAmount; i++)
            drawString(true, 10, 10 + (3 + i) * SPACING_Y, i == selectedTheme ? COLOR_RED : COLOR_WHITE, bootThemeName((u8)i));

        drawFormattedString(false, 10, 10, COLOR_WHITE,
                            "Current choice: %s\n\n"
                            "This changes the cold-boot splash\n"
                            "background, panels, muted text and accent.\n\n"
                            "It is stored in /omiiba/.boot_theme.",
                            bootThemeName((u8)selectedTheme));

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return;

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedTheme = !selectedTheme ? themeAmount - 1 : selectedTheme - 1;
                    break;
                case BUTTON_DOWN:
                    selectedTheme = selectedTheme == themeAmount - 1 ? 0 : selectedTheme + 1;
                    break;
                case BUTTON_LEFT:
                    selectedTheme = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedTheme = themeAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            writeBootThemeIndex((u8)selectedTheme);
            drawBootHubMessage("Theme saved",
                               "Boot splash theme saved.\n\n"
                               "You will see it on the next cold boot\n"
                               "when the Omiiba splash is shown.");
            waitForBootHubBack();
            return;
        }
    }
}

static void runThemeSettings(void)
{
    static const char *items[] = {
        "Choose boot splash theme",
        "Boot message help",
        "Reset Cow tip rotation",
        "Back to Boot Hub",
    };

    static const char *descriptions[] = {
        "Select one of the built-in Omiiba splash palettes.",
        "Custom one-line boot tagline:\n\n"
        "SD:/omiiba/boot_message.txt\n\n"
        "Only the first line is shown.",
        "Delete /omiiba/.cow_tip_state so the\n"
        "rotating tips start from the first tip again.",
        "Return to the Omiiba Boot Hub.",
    };

    const u32 itemAmount = sizeof(items) / sizeof(items[0]);
    u32 selectedItem = 0;

    while(true)
    {
        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "Theme/settings");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: select    B: Boot Hub");

        for(u32 i = 0; i < itemAmount; i++)
            drawString(true, 10, 10 + (3 + i) * SPACING_Y, i == selectedItem ? COLOR_RED : COLOR_WHITE, items[i]);

        drawString(false, 10, 10, COLOR_WHITE, descriptions[selectedItem]);

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return;

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedItem = !selectedItem ? itemAmount - 1 : selectedItem - 1;
                    break;
                case BUTTON_DOWN:
                    selectedItem = selectedItem == itemAmount - 1 ? 0 : selectedItem + 1;
                    break;
                case BUTTON_LEFT:
                    selectedItem = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedItem = itemAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            switch(selectedItem)
            {
                case 0:
                    chooseBootTheme();
                    break;
                case 1:
                    drawBootHubMessage("Boot message",
                                       "Create this text file:\n"
                                       "SD:/omiiba/boot_message.txt\n\n"
                                       "The first line is shown on the\n"
                                       "top-screen boot splash.");
                    waitForBootHubBack();
                    break;
                case 2:
                    fileDelete(".cow_tip_state");
                    drawBootHubMessage("Cow tips reset",
                                       "Tip rotation state was reset.\n\n"
                                       "The next splash starts from the\n"
                                       "first built-in Cow tip again.");
                    waitForBootHubBack();
                    break;
                default:
                    return;
            }
        }
    }
}

typedef enum
{
    BOOT_HUB_BACK_TO_SETTINGS = 0,
    BOOT_HUB_CONTINUE_BOOT,
    BOOT_HUB_SAVE_SETTINGS_BOOT,
    BOOT_HUB_RUN_SETUP_WIZARD,
    BOOT_HUB_RUN_PROFILES,
} BootHubResult;

typedef struct
{
    u32 posXs[4];
    u32 posY;
    u32 enabled;
    bool visible;
} MultiOptionState;

typedef struct
{
    u32 posY;
    bool enabled;
    bool visible;
} SingleOptionState;

static BootHubResult omiibaBootHub(void)
{
    static const char *items[] = {
        "Continue normal boot",
        "Save settings and boot",
        "Setup wizard",
        "Boot chainloader",
        "GodMode9 tools",
        "GBA Labs",
        "DS Labs",
        "VC Patch Helper",
        "Diagnostics",
        "Profiles",
        "Payload manager",
        "Theme/settings",
        "Advanced boot settings",
    };

    static const char *descriptions[] = {
        "Continue booting Omiiba3DS.\n\n"
        "If no settings changed, this skips config.ini\n"
        "writing for a faster exit.",
        "Write current pending settings to config.ini,\n"
        "then continue booting Omiiba3DS.",
        "Run a guided first setup for splash,\nbrightness, game patching and GodMode9.",
        "Open the standard payload chainloader.\n\nEquivalent to holding START at boot.",
        "Launch GodMode9 from /omiiba/payloads.\n\nLow-level dumping remains delegated to GodMode9.",
        "Launch open_agb_firm and view experimental\nGBA display research notes.",
        "Check TWiLight/nds-bootstrap folders,\nDS ROM path and TWL filter setup.",
        "Create /omiiba/titles patch folders\nand view VC LayeredFS/IPS guidance.",
        "Show safe read-only checks for Omiiba setup health.",
        "Apply named safe setting groups:\nDefault, Safe, Performance, Modding, Dev.",
        "List payloads, launch selected .firm files\nand create safe hotkey copies.",
        "Choose boot splash palettes, view boot\nmessage help and reset rotating Cow tips.",
        "Open the classic advanced boot settings\n"
        "menu for low-level Luma/Omiiba toggles.",
    };

    const u32 itemAmount = sizeof(items) / sizeof(items[0]);
    u32 selectedItem = 0;

    while(true)
    {
        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "Omiiba Boot Hub");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: select    B: advanced settings");

        for(u32 i = 0; i < itemAmount; i++)
            drawString(true, 10, 10 + (3 + i) * SPACING_Y, i == selectedItem ? COLOR_RED : COLOR_WHITE, items[i]);

        drawString(false, 10, 10, COLOR_WHITE, descriptions[selectedItem]);

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return BOOT_HUB_BACK_TO_SETTINGS;

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedItem = !selectedItem ? itemAmount - 1 : selectedItem - 1;
                    break;
                case BUTTON_DOWN:
                    selectedItem = selectedItem == itemAmount - 1 ? 0 : selectedItem + 1;
                    break;
                case BUTTON_LEFT:
                    selectedItem = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedItem = itemAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            switch(selectedItem)
            {
                case 0:
                    return BOOT_HUB_CONTINUE_BOOT;
                case 1:
                    return BOOT_HUB_SAVE_SETTINGS_BOOT;
                case 2:
                    return BOOT_HUB_RUN_SETUP_WIZARD;
                case 3:
                    loadHomebrewFirm(0);
                    break;
                case 4:
                    launchGodMode9Tools();
                    break;
                case 5:
                    launchGbaLabs();
                    break;
                case 6:
                    launchDsLabs();
                    break;
                case 7:
                    launchVcPatchHelper();
                    break;
                case 8:
                    showBootHubDiagnostics();
                    break;
                case 9:
                    return BOOT_HUB_RUN_PROFILES;
                case 10:
                    runPayloadManager();
                    break;
                case 11:
                    runThemeSettings();
                    break;
                default:
                    return BOOT_HUB_BACK_TO_SETTINGS;
            }
        }
    }
}

static void runOmiibaSetupWizard(MultiOptionState *multiOptions, SingleOptionState *singleOptions)
{
    drawBootHubMessage("Setup wizard",
                       "This wizard changes only existing safe\n"
                       "Omiiba settings.\n\n"
                       "You can review them in the settings menu\n"
                       "before saving.");
    waitForBootHubBack();

    if(askBootHubYesNo("Splash screen",
                       "Show the Omiiba splash before payloads?\n\n"
                       "Recommended if you use payload hotkeys.",
                       "enable", "skip"))
        multiOptions[SPLASH].enabled = 1;

    if(askBootHubYesNo("Screen brightness",
                       "Use bright boot/menu screens?\n\n"
                       "You can still change this later.",
                       "bright", "keep"))
    {
        multiOptions[BRIGHTNESS].enabled = 0;
        updateBrightness(multiOptions[BRIGHTNESS].enabled);
    }

    if(askBootHubYesNo("Game patching",
                       "Enable game patching for LayeredFS,\n"
                       "IPS patches, plugins and modding?\n\n"
                       "Recommended for Omiiba modding setups.",
                       "enable", "skip"))
        singleOptions[PATCHGAMES].enabled = true;

    if(askBootHubYesNo("External FIRMs/modules",
                       "Enable the Advanced setting:\n"
                       "loading external FIRMs and modules?\n\n"
                       "Most users do not need this. Enable only\n"
                       "if your setup uses external system files.",
                       "enable", "skip"))
        singleOptions[LOADEXTFIRMSANDMODULES].enabled = true;

    drawBootHubMessage("GodMode9",
                       "For maintenance tools, place GodMode9 at:\n"
                       "SD:/omiiba/payloads/GodMode9.firm\n\n"
                       "System save dumps are handled by the\n"
                       "included GM9 script, not by Omiiba itself.");
    waitForBootHubBack();

    static const char setupDone[] = "1\n";
    fileWrite(setupDone, ".setup_wizard_done", sizeof(setupDone) - 1);
}

static void applyDefaultProfile(MultiOptionState *multiOptions, SingleOptionState *singleOptions)
{
    multiOptions[SPLASH].enabled = 1;       // before payloads
    multiOptions[BRIGHTNESS].enabled = 1;   // level 3
    multiOptions[NEWCPU].enabled = 0;

    singleOptions[PATCHGAMES].enabled = true;
    singleOptions[REDIRECTAPPTHREADS].enabled = false;
}

static void applySafeProfile(MultiOptionState *multiOptions, SingleOptionState *singleOptions)
{
    multiOptions[SPLASH].enabled = 1;
    multiOptions[BRIGHTNESS].enabled = 1;
    multiOptions[NEWCPU].enabled = 0;

    singleOptions[PATCHGAMES].enabled = false;
    singleOptions[REDIRECTAPPTHREADS].enabled = false;
}

static void applyPerformanceProfile(MultiOptionState *multiOptions, SingleOptionState *singleOptions)
{
    multiOptions[SPLASH].enabled = 1;
    multiOptions[BRIGHTNESS].enabled = 0;
    if(ISN3DS)
        multiOptions[NEWCPU].enabled = 1; // Clock only, safer than Clock+L2.

    singleOptions[PATCHGAMES].enabled = true;
    singleOptions[REDIRECTAPPTHREADS].enabled = ISN3DS;
}

static void applyModdingProfile(MultiOptionState *multiOptions, SingleOptionState *singleOptions)
{
    multiOptions[SPLASH].enabled = 1;
    multiOptions[BRIGHTNESS].enabled = 0;

    singleOptions[PATCHGAMES].enabled = true;
    singleOptions[REDIRECTAPPTHREADS].enabled = false;
}

static void applyDeveloperProfile(MultiOptionState *multiOptions, SingleOptionState *singleOptions)
{
    multiOptions[SPLASH].enabled = 1;
    multiOptions[BRIGHTNESS].enabled = 0;
    if(ISN3DS)
        multiOptions[NEWCPU].enabled = 1;

    singleOptions[PATCHGAMES].enabled = true;
    singleOptions[REDIRECTAPPTHREADS].enabled = ISN3DS;
}

static void runOmiibaProfiles(MultiOptionState *multiOptions, SingleOptionState *singleOptions)
{
    static const char *items[] = {
        "Default",
        "Safe",
        "Performance",
        "Plugin/Game Modding",
        "Developer/GDB",
        "Back to Boot Hub",
    };

    static const char *descriptions[] = {
        "Balanced Omiiba defaults:\n"
        "- Splash before payloads\n"
        "- Brightness level 3\n"
        "- Game patching on\n"
        "- CPU tweaks off",

        "Maximum compatibility:\n"
        "- Splash before payloads\n"
        "- Game patching off\n"
        "- CPU/thread tweaks off",

        "Performance-oriented:\n"
        "- Brightest boot/menu screen\n"
        "- Game patching on\n"
        "- New 3DS clock mode (N3DS only)\n"
        "- App syscore redirect (N3DS only)",

        "Modding/plugin setup:\n"
        "- Bright splash before payloads\n"
        "- Game patching on\n"
        "- CPU/thread tweaks off",

        "Developer setup:\n"
        "- Game patching on\n"
        "- New 3DS clock mode (N3DS only)\n"
        "- App syscore redirect (N3DS only)",

        "Return without applying a profile.",
    };

    const u32 itemAmount = sizeof(items) / sizeof(items[0]);
    u32 selectedItem = 0;

    while(true)
    {
        clearScreens(false);
        drawString(true, 10, 10, COLOR_TITLE, "Omiiba profiles");
        drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "A: preview/apply    B: Boot Hub");

        for(u32 i = 0; i < itemAmount; i++)
            drawString(true, 10, 10 + (3 + i) * SPACING_Y, i == selectedItem ? COLOR_RED : COLOR_WHITE, items[i]);

        drawString(false, 10, 10, COLOR_WHITE, descriptions[selectedItem]);

        u32 pressed;
        do
        {
            pressed = waitInput(true) & (MENU_BUTTONS | BUTTON_B);
        }
        while(!pressed);

        if(pressed & BUTTON_B)
            return;

        if(pressed & DPAD_BUTTONS)
        {
            switch(pressed & DPAD_BUTTONS)
            {
                case BUTTON_UP:
                    selectedItem = !selectedItem ? itemAmount - 1 : selectedItem - 1;
                    break;
                case BUTTON_DOWN:
                    selectedItem = selectedItem == itemAmount - 1 ? 0 : selectedItem + 1;
                    break;
                case BUTTON_LEFT:
                    selectedItem = 0;
                    break;
                case BUTTON_RIGHT:
                    selectedItem = itemAmount - 1;
                    break;
                default:
                    break;
            }
        }
        else if(pressed & BUTTON_A)
        {
            if(selectedItem == itemAmount - 1)
                return;

            if(!askBootHubYesNo("Apply profile?", descriptions[selectedItem], "apply", "cancel"))
                continue;

            switch(selectedItem)
            {
                case 0:
                    applyDefaultProfile(multiOptions, singleOptions);
                    break;
                case 1:
                    applySafeProfile(multiOptions, singleOptions);
                    break;
                case 2:
                    applyPerformanceProfile(multiOptions, singleOptions);
                    break;
                case 3:
                    applyModdingProfile(multiOptions, singleOptions);
                    break;
                case 4:
                    applyDeveloperProfile(multiOptions, singleOptions);
                    break;
                default:
                    break;
            }

            updateBrightness(multiOptions[BRIGHTNESS].enabled);
            drawBootHubMessage("Profile applied",
                               "Profile applied to pending settings.\n\n"
                               "Use Save and exit or Continue normal\n"
                               "boot to write config.ini.");
            waitForBootHubBack();
            return;
        }
    }
}

void configMenu(bool oldPinStatus, u32 oldPinMode)
{
    static const char *multiOptionsText[]  = { "Default EmuNAND: 1( ) 2( ) 3( ) 4( )",
                                               "Screen brightness: 4( ) 3( ) 2( ) 1( )",
                                               "Splash: Off( ) Before( ) After( ) payloads",
                                               "PIN lock: Off( ) 4( ) 6( ) 8( ) digits",
                                               "New 3DS CPU: Off( ) Clock( ) L2( ) Clock+L2( )",
                                               "Hbmenu autoboot: Off( ) 3DS( ) DSi( )",
                                             };

    static const char *singleOptionsText[] = { "( ) Autoboot EmuNAND",
                                               "( ) Enable loading external FIRMs and modules",
                                               "( ) Enable game patching",
                                               "( ) Redirect app. syscore threads to core2",
                                               "( ) Show NAND or user string in System Settings",
                                               "( ) Show GBA boot screen in patched AGB_FIRM",

                                               // Should always be the last 3 entries
                                               "\nBack to Omiiba Boot Hub",
                                               "\nBoot chainloader",
                                               "Save and exit"
                                             };

    static const char *optionsDescription[]  = { "Select the default EmuNAND.\n\n"
                                                 "It will be booted when no directional\n"
                                                 "pad buttons are pressed (Up/Right/Down\n"
                                                 "/Left equal EmuNANDs 1/2/3/4).",

                                                 "Select the screen brightness.",

                                                 "Enable splash screen support.\n\n"
                                                 "\t* 'Before payloads' displays it\n"
                                                 "before booting payloads\n"
                                                 "(intended for splashes that display\n"
                                                 "button hints).\n\n"
                                                 "\t* 'After payloads' displays it\n"
                                                 "afterwards.\n\n"
                                                 "Edit the duration in config.ini (3s\n"
                                                 "default).",

                                                 "Activate a PIN lock.\n\n"
                                                 "The PIN will be asked each time\n"
                                                 "Omiiba3DS boots.\n\n"
                                                 "4, 6 or 8 digits can be selected.\n\n"
                                                 "The ABXY buttons and the directional\n"
                                                 "pad buttons can be used as keys.\n\n"
                                                 "A message can also be displayed\n"
                                                 "(refer to the wiki for instructions).",

                                                 "Select the New 3DS CPU mode.\n\n"
                                                 "This won't apply to\n"
                                                 "New 3DS exclusive/enhanced games.\n\n"
                                                 "'Clock+L2' can cause issues with some\n"
                                                 "games.",

                                                 "Enable autobooting into homebrew menu,\n"
                                                 "either into 3DS or DSi mode.\n\n"
                                                 "Autobooting into a gamecard title is\n"
                                                 "not supported.\n\n"
                                                 "Refer to the \"autoboot\" section in the\n"
                                                 "configuration file to configure\n"
                                                 "this feature.",

                                                 "If enabled, an EmuNAND\n"
                                                 "will be launched on boot.\n\n"
                                                 "Otherwise, SysNAND will.\n\n"
                                                 "Hold L on boot to switch NAND.\n\n"
                                                 "To use a different EmuNAND from the\n"
                                                 "default, hold a directional pad button\n"
                                                 "(Up/Right/Down/Left equal EmuNANDs\n"
                                                 "1/2/3/4).",

                                                 "Enable loading external FIRMs and\n"
                                                 "system modules.\n\n"
                                                 "This isn't needed in most cases.\n\n"
                                                 "Refer to the wiki for instructions.",

                                                 "Enable overriding the region and\n"
                                                 "language configuration and the usage\n"
                                                 "of patched code binaries, exHeaders,\n"
                                                 "IPS code patches and LayeredFS\n"
                                                 "for specific games.\n\n"
                                                 "Also makes certain DLCs for out-of-\n"
                                                 "region games work.\n\n"
                                                 "Refer to the wiki for instructions.",

                                                 "Redirect app. threads that would spawn\n"
                                                 "on core1, to core2 (which is an extra\n"
                                                 "CPU core for applications that usually\n"
                                                 "remains unused).\n\n"
                                                 "This improves the performance of very\n"
                                                 "demanding games (like Pok\x82mon US/UM)\n" // CP437
                                                 "by about 10%. Can break some games\n"
                                                 "and other applications.\n",

                                                 "Enable showing the current NAND:\n\n"
                                                 "\t* Sys  = SysNAND\n"
                                                 "\t* Emu  = EmuNAND 1\n"
                                                 "\t* EmuX = EmuNAND X\n\n"
                                                 "or a user-defined custom string in\n"
                                                 "System Settings.\n\n"
                                                 "Refer to the wiki for instructions.",

                                                 "Enable showing the GBA boot screen\n"
                                                 "when booting GBA games.",

                                               // Should always be the last 3 entries
                                               "Return to Omiiba's main Boot Hub.\n\n"
                                               "Use this after changing advanced\n"
                                               "settings if you want to continue\n"
                                               "through the Hub.",

                                               "Boot to the Omiiba3DS chainloader menu.",

                                                 "Save the changes and exit. To discard\n"
                                                 "any changes press the POWER button.\n"
                                                 "Use START as a shortcut to this entry."
                                               };

    FirmwareSource nandType = FIRMWARE_SYSNAND;
    if(isSdMode)
    {
        // Check if there is at least one emuNAND
        u32 emuIndex = 0;
        nandType = FIRMWARE_EMUNAND;
        locateEmuNand(&nandType, &emuIndex, false);
    }

    MultiOptionState multiOptions[] = {
        { .visible = nandType == FIRMWARE_EMUNAND },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = ISN3DS },
        { .visible = true },
        // { .visible = true }, audio rerouting, hidden
    };

    SingleOptionState singleOptions[] = {
        { .visible = nandType == FIRMWARE_EMUNAND },
        { .visible = true },
        { .visible = true },
        { .visible = ISN3DS },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true },
    };

    //Calculate the amount of the various kinds of options and pre-select the first single one
    u32 multiOptionsAmount = sizeof(multiOptions) / sizeof(MultiOptionState),
        singleOptionsAmount = sizeof(singleOptions) / sizeof(SingleOptionState),
        totalIndexes = multiOptionsAmount + singleOptionsAmount - 1,
        selectedOption = 0,
        singleSelected = 0;
    bool isMultiOption = false;
    bool settingsDirty = needConfig == CREATE_CONFIGURATION;
    bool forceSaveAndBoot = false;
    bool skipConfigWrite = false;

    //Parse the existing options
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        //Detect the positions where the "x" should go
        u32 optionNum = 0;
        for(u32 j = 0; optionNum < 4 && j < strlen(multiOptionsText[i]); j++)
            if(multiOptionsText[i][j] == '(') multiOptions[i].posXs[optionNum++] = j + 1;
        while(optionNum < 4) multiOptions[i].posXs[optionNum++] = 0;

        multiOptions[i].enabled = MULTICONFIG(i);
    }
    for(u32 i = 0; i < singleOptionsAmount; i++)
        singleOptions[i].enabled = CONFIG(i);

    initScreens();

    if(needConfig == CREATE_CONFIGURATION && getFileSize(".setup_wizard_done") == 0)
    {
        runOmiibaSetupWizard(multiOptions, singleOptions);
        settingsDirty = true;
    }

showBootHubScreen:
    {
        BootHubResult hubResult = omiibaBootHub();

        if(hubResult == BOOT_HUB_CONTINUE_BOOT)
        {
            skipConfigWrite = !settingsDirty;
            goto finishConfigMenu;
        }

        if(hubResult == BOOT_HUB_SAVE_SETTINGS_BOOT)
        {
            forceSaveAndBoot = true;
            goto finishConfigMenu;
        }

        if(hubResult == BOOT_HUB_RUN_SETUP_WIZARD)
        {
            runOmiibaSetupWizard(multiOptions, singleOptions);
            settingsDirty = true;
            goto showBootHubScreen;
        }

        if(hubResult == BOOT_HUB_RUN_PROFILES)
        {
            runOmiibaProfiles(multiOptions, singleOptions);
            settingsDirty = true;
            goto showBootHubScreen;
        }
    }

    static const char *bootTypes[] = { "B9S",
                                       "B9S (ntrboot)",
                                       "FIRM0",
                                       "FIRM1" };

    clearScreens(false);
    drawString(true, 10, 10, COLOR_TITLE, CONFIG_TITLE);
    drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "Use the DPAD and A to change settings");
    drawFormattedString(false, 10, SCREEN_HEIGHT - 2 * SPACING_Y, COLOR_YELLOW, "Booted from %s via %s", isSdMode ? "SD" : "CTRNAND", bootTypes[(u32)bootType]);

    //Character to display a selected option
    char selected = 'x';

    u32 endPos = 10 + 2 * SPACING_Y;

    //Display all the multiple choice options in white
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        if(!multiOptions[i].visible) continue;

        multiOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(true, 10, multiOptions[i].posY, COLOR_WHITE, multiOptionsText[i]);
        drawCharacter(true, 10 + multiOptions[i].posXs[multiOptions[i].enabled] * SPACING_X, multiOptions[i].posY, COLOR_WHITE, selected);
    }

    endPos += SPACING_Y / 2;

    //Display all the normal options in white except for the first one
    for(u32 i = 0, color = COLOR_RED; i < singleOptionsAmount; i++)
    {
        if(!singleOptions[i].visible) continue;

        singleOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(true, 10, singleOptions[i].posY, color, singleOptionsText[i]);
        if(singleOptions[i].enabled && singleOptionsText[i][0] == '(') drawCharacter(true, 10 + SPACING_X, singleOptions[i].posY, color, selected);

        if(color == COLOR_RED)
        {
            singleSelected = i;
            selectedOption = i + multiOptionsAmount;
            color = COLOR_WHITE;
        }
    }

    drawString(false, 10, 10, COLOR_WHITE, optionsDescription[selectedOption]);

    bool startPressed = false;
    //Boring configuration menu
    while(true)
    {
        u32 pressed = 0;
        if (!startPressed)
        do
        {
            pressed = waitInput(true) & MENU_BUTTONS;
        }
        while(!pressed);

        // Force the selection of "save and exit" and trigger it.
        if(pressed & BUTTON_START)
        {
            startPressed = true;
            // This moves the cursor to the last entry
            pressed = BUTTON_RIGHT;
        }

        if(pressed & DPAD_BUTTONS)
        {
            //Remember the previously selected option
            u32 oldSelectedOption = selectedOption;

            while(true)
            {
                switch(pressed & DPAD_BUTTONS)
                {
                    case BUTTON_UP:
                        selectedOption = !selectedOption ? totalIndexes : selectedOption - 1;
                        break;
                    case BUTTON_DOWN:
                        selectedOption = selectedOption == totalIndexes ? 0 : selectedOption + 1;
                        break;
                    case BUTTON_LEFT:
                        pressed = BUTTON_DOWN;
                        selectedOption = 0;
                        break;
                    case BUTTON_RIGHT:
                        pressed = BUTTON_UP;
                        selectedOption = totalIndexes;
                        break;
                    default:
                        break;
                }

                if(selectedOption < multiOptionsAmount)
                {
                    if(!multiOptions[selectedOption].visible) continue;

                    isMultiOption = true;
                    break;
                }
                else
                {
                    singleSelected = selectedOption - multiOptionsAmount;

                    if(!singleOptions[singleSelected].visible) continue;

                    isMultiOption = false;
                    break;
                }
            }

            if(selectedOption == oldSelectedOption && !startPressed) continue;

            //The user moved to a different option, print the old option in white and the new one in red. Only print 'x's if necessary
            if(oldSelectedOption < multiOptionsAmount)
            {
                drawString(true, 10, multiOptions[oldSelectedOption].posY, COLOR_WHITE, multiOptionsText[oldSelectedOption]);
                drawCharacter(true, 10 + multiOptions[oldSelectedOption].posXs[multiOptions[oldSelectedOption].enabled] * SPACING_X, multiOptions[oldSelectedOption].posY, COLOR_WHITE, selected);
            }
            else
            {
                u32 singleOldSelected = oldSelectedOption - multiOptionsAmount;
                drawString(true, 10, singleOptions[singleOldSelected].posY, COLOR_WHITE, singleOptionsText[singleOldSelected]);
                if(singleOptions[singleOldSelected].enabled) drawCharacter(true, 10 + SPACING_X, singleOptions[singleOldSelected].posY, COLOR_WHITE, selected);
            }

            if(isMultiOption) drawString(true, 10, multiOptions[selectedOption].posY, COLOR_RED, multiOptionsText[selectedOption]);
            else drawString(true, 10, singleOptions[singleSelected].posY, COLOR_RED, singleOptionsText[singleSelected]);

            drawString(false, 10, 10, COLOR_BLACK, optionsDescription[oldSelectedOption]);
            drawString(false, 10, 10, COLOR_WHITE, optionsDescription[selectedOption]);
        }
        else if (pressed & BUTTON_A || startPressed)
        {
            //The selected option's status changed, print the 'x's accordingly
            if(isMultiOption)
            {
                u32 oldEnabled = multiOptions[selectedOption].enabled;
                drawCharacter(true, 10 + multiOptions[selectedOption].posXs[oldEnabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_BLACK, selected);
                multiOptions[selectedOption].enabled = (oldEnabled == 3 || !multiOptions[selectedOption].posXs[oldEnabled + 1]) ? 0 : oldEnabled + 1;
                settingsDirty = true;

                if(selectedOption == BRIGHTNESS) updateBrightness(multiOptions[BRIGHTNESS].enabled);
            }
            else
            {
                // Save and exit was selected.
                if (singleSelected == singleOptionsAmount - 1)
                {
                    drawString(true, 10, singleOptions[singleSelected].posY, COLOR_GREEN, singleOptionsText[singleSelected]);
                    forceSaveAndBoot = true;
                    startPressed = false;
                    break;
                }
                else if (singleSelected == singleOptionsAmount - 2) {
                    loadHomebrewFirm(0);
                    break;
                }
                else if (singleSelected == singleOptionsAmount - 3)
                {
                    BootHubResult hubResult = omiibaBootHub();

                    if(hubResult == BOOT_HUB_CONTINUE_BOOT)
                    {
                        skipConfigWrite = !settingsDirty;
                        startPressed = false;
                        break;
                    }

                    if(hubResult == BOOT_HUB_SAVE_SETTINGS_BOOT)
                    {
                        forceSaveAndBoot = true;
                        startPressed = false;
                        break;
                    }

                    if(hubResult == BOOT_HUB_RUN_SETUP_WIZARD)
                    {
                        runOmiibaSetupWizard(multiOptions, singleOptions);
                        settingsDirty = true;
                    }

                    if(hubResult == BOOT_HUB_RUN_PROFILES)
                    {
                        runOmiibaProfiles(multiOptions, singleOptions);
                        settingsDirty = true;
                    }

                    goto showBootHubScreen;
                }
                else
                {
                    bool oldEnabled = singleOptions[singleSelected].enabled;
                    singleOptions[singleSelected].enabled = !oldEnabled;
                    settingsDirty = true;
                    if(oldEnabled) drawCharacter(true, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_BLACK, selected);
                }
            }
        }

        //In any case, if the current option is enabled (or a multiple choice option is selected) we must display a red 'x'
        if(isMultiOption) drawCharacter(true, 10 + multiOptions[selectedOption].posXs[multiOptions[selectedOption].enabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_RED, selected);
        else if(singleOptions[singleSelected].enabled && singleOptionsText[singleSelected][0] == '(') drawCharacter(true, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_RED, selected);
    }

finishConfigMenu:
    bool didSaveConfig = !skipConfigWrite || settingsDirty || forceSaveAndBoot;

    if(didSaveConfig)
    {
        //Parse and write the new configuration
        configData.multiConfig = 0;
        for(u32 i = 0; i < multiOptionsAmount; i++)
            configData.multiConfig |= multiOptions[i].enabled << (i * 2);

        configData.config &= ~((1 << (u32)NUMCONFIGURABLE) - 1);
        for(u32 i = 0; i < singleOptionsAmount; i++)
            configData.config |= (singleOptions[i].enabled ? 1 : 0) << i;

        writeConfig(true);

        u32 newPinMode = MULTICONFIG(PIN);

        if(newPinMode != 0) newPin(oldPinStatus && newPinMode == oldPinMode, newPinMode);
        else if(oldPinStatus)
        {
            if(!fileDelete(PIN_FILE))
                error("Unable to delete PIN file");
        }
    }

    if(didSaveConfig)
    {
        while(HID_PAD & PIN_BUTTONS);
        wait(2000ULL);
    }
}
