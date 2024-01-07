/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_STICKER_H
#define MYMPD_STICKER_H

#include "src/lib/list.h"

#include <time.h>

/**
 * Valid values for like sticker
 */
enum sticker_like {
    STICKER_LIKE_HATE = 0,
    STICKER_LIKE_NEUTRAL = 1,
    STICKER_LIKE_LOVE = 2
};

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
 * MPD sticker values
 */
struct t_sticker {
    long long mympd[STICKER_COUNT];  //!< array of myMPD stickers
    struct t_list user;              //!< list of user defined stickers
};

const char *sticker_name_lookup(enum mympd_sticker_types sticker);
enum mympd_sticker_types sticker_name_parse(const char *name);
void sticker_struct_init(struct t_sticker *sticker);
void sticker_struct_clear(struct t_sticker *sticker);

#endif
