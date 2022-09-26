/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_STICKER_CACHE_H
#define MYMPD_STICKER_CACHE_H

#include "../lib/mympd_state.h"

#include <stdbool.h>

/**
 * MPD sticker values
 *
 */
struct t_sticker {
    long play_count;      //!< number how often the song was played
    long skip_count;      //!< number how often the song was skipped
    time_t last_played;   //!< timestamp when the song was played the last time
    time_t last_skipped;  //!< timestamp when the song was skipped the last time
    long like;            //!< hate/neutral/love value
};

struct t_sticker *get_sticker_from_cache(struct t_cache *sticker_cache, const char *uri);
void sticker_cache_free(struct t_cache *sticker_cache);

bool sticker_inc_play_count(struct t_list *sticker_queue, const char *uri);
bool sticker_inc_skip_count(struct t_list *sticker_queue, const char *uri);
bool sticker_set_like(struct t_list *sticker_queue, const char *uri, int value);
bool sticker_set_last_played(struct t_list *sticker_queue, const char *uri, time_t song_start_time);
bool sticker_set_last_skipped(struct t_list *sticker_queue, const char *uri);

bool sticker_dequeue(struct t_list *sticker_queue, struct t_cache *sticker_cache, struct t_partition_state *partition_state);

#endif
