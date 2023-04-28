/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_API_WEBRADIOS_H
#define MYMPD_API_WEBRADIOS_H

#include "dist/sds/sds.h"

#include <stdbool.h>

sds get_webradio_from_uri(sds workdir, const char *uri);
bool mympd_api_webradio_save(sds workdir, sds name, sds uri, sds uri_old,
        sds genre, sds picture, sds homepage, sds country, sds language, sds codec, int bitrate, sds description);
bool mympd_api_webradio_delete(sds workdir, const char *filename);
sds mympd_api_webradio_get(sds workdir, sds buffer, long request_id, sds filename);
sds mympd_api_webradio_list(sds workdir, sds buffer, long request_id, sds searchstr, long offset, long limit);

#endif
