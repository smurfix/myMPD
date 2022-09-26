/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_API_STATS_H
#define MYMPD_API_STATS_H

#include "../lib/mympd_state.h"

sds mympd_api_stats_get(struct t_partition_state *partition_state, sds buffer, long request_id);
#endif
