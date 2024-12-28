/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

/*! \file
 * \brief MPD volume wrapper
 */

#ifndef MYMPD_MPD_CLIENT_VOLUME_H
#define MYMPD_MPD_CLIENT_VOLUME_H

#include "src/lib/mympd_state.h"

int mpd_client_get_volume(struct t_partition_state *partition_state);
#endif
