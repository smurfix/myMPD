/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_STICKER_CACHE_H
#define MYMPD_STICKER_CACHE_H

#include "src/lib/mympd_state.h"

#include <stdbool.h>
#include <time.h>

/**
 * myMPD sticker types
 */
enum mympd_sticker_types {
    STICKER_UNKNOWN = -1,
    STICKER_PLAY_COUNT,
    STICKER_SKIP_COUNT,
    STICKER_LIKE,
    STICKER_LAST_PLAYED,
    STICKER_LAST_SKIPPED,
    STICKER_ELAPSED,
    STICKER_COUNT
};

/**
 * Valid values for like sticker
 */
enum sticker_like {
    STICKER_LIKE_HATE = 0,
    STICKER_LIKE_NEUTRAL = 1,
    STICKER_LIKE_LOVE = 2
};

/**
 * Wrapper struct for sticker types
 */
struct t_sticker_type {
    enum mympd_sticker_types sticker_type;
};

/**
 * MPD sticker values
 */
struct t_sticker {
    long play_count;      //!< number how often the song was played
    long skip_count;      //!< number how often the song was skipped
    time_t last_played;   //!< timestamp when the song was played the last time
    time_t last_skipped;  //!< timestamp when the song was skipped the last time
    time_t elapsed;       //!< recent song position
    long like;            //!< hate/neutral/love value
};

bool sticker_cache_remove(sds workdir);
bool sticker_cache_write(struct t_cache *sticker_cache, sds workdir, bool free_data);
bool sticker_cache_read(struct t_cache *sticker_cache, sds workdir);

const char *sticker_name_lookup(enum mympd_sticker_types sticker);
enum mympd_sticker_types sticker_name_parse(const char *name);

struct t_sticker *get_sticker_from_cache(struct t_cache *sticker_cache, const char *uri);
void sticker_cache_free(struct t_cache *sticker_cache);

bool sticker_set_elapsed(struct t_list *sticker_queue, const char *uri, time_t elapsed);
bool sticker_inc_play_count(struct t_list *sticker_queue, const char *uri);
bool sticker_inc_skip_count(struct t_list *sticker_queue, const char *uri);
bool sticker_set_like(struct t_list *sticker_queue, const char *uri, int value);
bool sticker_set_last_played(struct t_list *sticker_queue, const char *uri, time_t song_start_time);
bool sticker_set_last_skipped(struct t_list *sticker_queue, const char *uri);

bool sticker_dequeue(struct t_list *sticker_queue, struct t_cache *sticker_cache, struct t_partition_state *partition_state);

#endif
