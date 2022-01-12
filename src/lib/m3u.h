/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_M3U_H
#define MYMPD_M3U_H

#include "../../dist/sds/sds.h"

sds m3u_to_json(sds buffer, const char *filename, sds *plname);
sds m3u_get_field(sds buffer, const char *field, const char *filename);

#endif
