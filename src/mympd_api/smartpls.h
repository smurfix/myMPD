/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

/*! \file
 * \brief myMPD smart playlist API
 */

#ifndef MYMPD_API_SMARTPLS_H
#define MYMPD_API_SMARTPLS_H

#include "dist/sds/sds.h"

sds mympd_api_smartpls_get(sds workdir, sds buffer, unsigned request_id, const char *playlist);

#endif
