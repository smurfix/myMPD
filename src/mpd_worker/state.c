/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"
#include "src/mpd_worker/state.h"

#include "src/lib/mem.h"
#include "src/lib/sds_extras.h"

/**
 * Frees the mpd_worker_state struct
 * @param mpd_worker_state pointer to the t_mpd_worker_state struct
 */
void *mpd_worker_state_free(struct t_mpd_worker_state *mpd_worker_state) {
    FREE_SDS(mpd_worker_state->smartpls_sort);
    FREE_SDS(mpd_worker_state->smartpls_prefix);
    // mpd state
    mpd_state_free(mpd_worker_state->mpd_state);
    partition_state_free(mpd_worker_state->partition_state);
    // stickerdb
    mpd_state_free(mpd_worker_state->stickerdb->mpd_state);
    partition_state_free(mpd_worker_state->stickerdb);
    FREE_PTR(mpd_worker_state);
    return NULL;
}
