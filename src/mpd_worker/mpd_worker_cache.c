/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"
#include "mpd_worker_cache.h"

#include "../lib/jsonrpc.h"
#include "../lib/log.h"
#include "../lib/mem.h"
#include "../lib/sds_extras.h"
#include "../mpd_shared.h"
#include "../mpd_shared/mpd_shared_sticker.h"
#include "../mpd_shared/mpd_shared_tags.h"
#include "../mympd_api/mympd_api_utility.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//privat definitions
static bool _cache_init(struct t_mpd_worker_state *mpd_worker_state, rax *album_cache, rax *sticker_cache);

//public functions
bool mpd_worker_cache_init(struct t_mpd_worker_state *mpd_worker_state) {
    rax *album_cache = NULL;
    if (mpd_worker_state->mpd_state->feat_mpd_tags == true) {
        album_cache = raxNew();
    }
    rax *sticker_cache = NULL;
    if (mpd_worker_state->mpd_state->feat_mpd_stickers == true) {
        sticker_cache = raxNew();
    }

    bool rc = true;
    if (mpd_worker_state->mpd_state->feat_mpd_tags == true ||
        mpd_worker_state->mpd_state->feat_mpd_stickers == true)
    {
        rc =_cache_init(mpd_worker_state, album_cache, sticker_cache);
    }

    //push album cache building response to mpd_client thread
    if (mpd_worker_state->mpd_state->feat_mpd_tags == true) {
        if (rc == true) {
            struct t_work_request *request = create_request(-1, 0, INTERNAL_API_ALBUMCACHE_CREATED, NULL);
            request->data = sdscatlen(request->data, "}}", 2);
            request->extra = (void *) album_cache;
            mympd_queue_push(mympd_api_queue, request, 0);
            send_jsonrpc_notify("database", "info", "Updated album cache");
        }
        else {
            album_cache_free(&album_cache);
            send_jsonrpc_notify("database", "error", "Update of album cache failed");
        }
    }
    else {
        MYMPD_LOG_INFO("Skipped album cache creation, tags are disabled");
    }

    //push sticker cache building response to mpd_client thread
    if (mpd_worker_state->mpd_state->feat_mpd_stickers == true) {
        if (rc == true) {
            struct t_work_request *request2 = create_request(-1, 0, INTERNAL_API_STICKERCACHE_CREATED, NULL);
            request2->data = sdscatlen(request2->data, "}}", 2);
            request2->extra = (void *) sticker_cache;
            mympd_queue_push(mympd_api_queue, request2, 0);
            send_jsonrpc_notify("database", "info", "Updated sticker cache");
        }
        else {
            sticker_cache_free(&sticker_cache);
            send_jsonrpc_notify("database", "error", "Update of sticker cache failed");
        }
    }
    else {
        MYMPD_LOG_INFO("Skipped sticker cache creation, stickers are disabled");
    }
    return rc;
}

//private functions
static bool _cache_init(struct t_mpd_worker_state *mpd_worker_state, rax *album_cache, rax *sticker_cache) {
    MYMPD_LOG_INFO("Creating caches");
    long start = 0;
    long end = start + MPD_RESULTS_MAX;
    long i = 0;
    long album_count = 0;
    long song_count = 0;
    long skipped = 0;
    //get first song from each album
    do {
        bool rc = mpd_search_db_songs(mpd_worker_state->mpd_state->conn, false);
        if (check_rc_error_and_recover(mpd_worker_state->mpd_state, NULL, NULL, 0, false, rc, "mpd_search_db_songs") == false) {
            MYMPD_LOG_ERROR("Cache update failed");
            mpd_search_cancel(mpd_worker_state->mpd_state->conn);
            return false;
        }
        rc = mpd_search_add_uri_constraint(mpd_worker_state->mpd_state->conn, MPD_OPERATOR_DEFAULT, "");
        if (check_rc_error_and_recover(mpd_worker_state->mpd_state, NULL, NULL, 0, false, rc, "mpd_search_add_uri_constraint") == false) {
            MYMPD_LOG_ERROR("Cache update failed");
            mpd_search_cancel(mpd_worker_state->mpd_state->conn);
            return false;
        }
        rc = mpd_search_add_window(mpd_worker_state->mpd_state->conn, start, end);
        if (check_rc_error_and_recover(mpd_worker_state->mpd_state, NULL, NULL, 0, false, rc, "mpd_search_add_window") == false) {
            MYMPD_LOG_ERROR("Cache update failed");
            mpd_search_cancel(mpd_worker_state->mpd_state->conn);
            return false;
        }
        rc = mpd_search_commit(mpd_worker_state->mpd_state->conn);
        if (check_rc_error_and_recover(mpd_worker_state->mpd_state, NULL, NULL, 0, false, rc, "mpd_search_commit") == false) {
            MYMPD_LOG_ERROR("Cache update failed");
            return false;
        }
        struct mpd_song *song;
        sds album = sdsempty();
        sds artist = sdsempty();
        sds key = sdsempty();
        while ((song = mpd_recv_song(mpd_worker_state->mpd_state->conn)) != NULL) {
            //sticker cache
            if (mpd_worker_state->mpd_state->feat_mpd_stickers == true) {
                const char *uri = mpd_song_get_uri(song);
                struct t_sticker *sticker = (struct t_sticker *) malloc_assert(sizeof(struct t_sticker));
                raxInsert(sticker_cache, (unsigned char*)uri, strlen(uri), (void *)sticker, NULL);
                song_count++;
            }
            //album cache
            if (mpd_worker_state->mpd_state->feat_mpd_tags == true) {
                album = mpd_shared_get_tag_value_string(song, MPD_TAG_ALBUM, album);
                artist = mpd_shared_get_tag_value_string(song, MPD_TAG_ALBUM_ARTIST, artist);
                if (sdslen(album) > 0 && sdslen(artist) > 0) {
                    sdsclear(key);
                    key = sdscatfmt(key, "%s::%s", album, artist);
                    if (raxTryInsert(album_cache, (unsigned char*)key, sdslen(key), (void *)song, NULL) == 0) {
                        //discard song data if key exists
                        mpd_song_free(song);
                    }
                    else {
                        album_count++;
                    }
                }
                else {
                    MYMPD_LOG_DEBUG("Albumcache, skipping \"%s\"", mpd_song_get_uri(song));
                    skipped++;
                    mpd_song_free(song);
                }
            }
            i++;
        }
        FREE_SDS(album);
        FREE_SDS(artist);
        FREE_SDS(key);
        mpd_response_finish(mpd_worker_state->mpd_state->conn);
        if (check_error_and_recover2(mpd_worker_state->mpd_state, NULL, NULL, 0, false) == false) {
            MYMPD_LOG_ERROR("Cache update failed");
            return false;
        }
        start = end;
        end = end + MPD_RESULTS_MAX;
    } while (i >= start);
    //get sticker values
    if (mpd_worker_state->mpd_state->feat_mpd_stickers == true) {
        raxIterator iter;
        raxStart(&iter, sticker_cache);
        raxSeek(&iter, "^", NULL, 0);
        sds uri = sdsempty();
        while (raxNext(&iter)) {
            uri = sds_replacelen(uri, (char *)iter.key, iter.key_len);
            mpd_shared_get_sticker(mpd_worker_state->mpd_state, uri, (struct t_sticker *)iter.data);
        }
        FREE_SDS(uri);
        raxStop(&iter);
    }
    MYMPD_LOG_INFO("Added %ld albums to album cache", album_count);
    MYMPD_LOG_INFO("Skipped %ld albums", skipped);
    MYMPD_LOG_INFO("Added %ld songs to sticker cache", song_count);
    MYMPD_LOG_INFO("Cache updated successfully");
    return true;
}
