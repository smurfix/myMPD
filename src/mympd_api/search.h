/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

/*! \file
 * \brief myMPD search API
 */

#ifndef MYMPD_API_SEARCH_H
#define MYMPD_API_SEARCH_H

#include "src/lib/mympd_state.h"

sds mympd_api_search_songs(struct t_partition_state *partition_state, struct t_stickerdb_state *stickerdb, 
        sds buffer, unsigned request_id, const char *expression, const char *sort, bool sortdesc,
        unsigned offset, unsigned limit, const struct t_fields *tagcols, bool *result);

#endif
