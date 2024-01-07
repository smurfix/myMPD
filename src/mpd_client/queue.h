/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_MPD_CLIENT_QUEUE_H
#define MYMPD_MPD_CLIENT_QUEUE_H

#include "src/lib/mympd_state.h"

bool mpd_client_queue_play_newly_inserted(struct t_partition_state *partition_state, sds *error);
bool mpd_client_queue_check_start_play(struct t_partition_state *partition_state, bool play, sds *error);
bool mpd_client_queue_clear(struct t_partition_state *partition_state, sds *error);
sds mpd_client_queue_status(struct t_partition_state *partition_state, sds buffer);

#endif
