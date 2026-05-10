/*   This paricular file is licensed under the following terms: */

/*
*   This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable
*   for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it
*   and redistribute it freely, subject to the following restrictions:
*
*    The origin of this software must not be misrepresented; you must not claim that you wrote the original software.
*    If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
*
*    Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
*    This notice may not be removed or altered from any source distribution.
*/

#pragma once

#include <3ds/types.h>

/// Omiiba shared config type (private!).
typedef struct OmiibaSharedConfig {
    u64 hbldr_3dsx_tid;             ///< Title ID to use for 3DSX loading (current).
    u64 selected_hbldr_3dsx_tid;    ///< Title ID to use for 3DSX loading (to be moved to "current" when the current app closes).
    bool use_hbldr;                 ///< Whether or not Loader should use hb:ldr (reset to true).
} OmiibaSharedConfig;

/// Omiiba shared config.
#define Omiiba_SharedConfig ((volatile OmiibaSharedConfig *)(OS_SHAREDCFG_VADDR + 0x800))
