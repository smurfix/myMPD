/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"
#include "mympd_api_utility.h"

#include "../lib/jsonrpc.h"
#include "../lib/log.h"
#include "../lib/mem.h"
#include "../lib/mympd_configuration.h"
#include "../lib/sds_extras.h"
#include "../lib/utility.h"
#include "../lib/validate.h"
#include "../mpd_shared.h"
#include "../mpd_shared/mpd_shared_sticker.h"
#include "../mpd_shared/mpd_shared_tags.h"
#include "mympd_api_timer.h"

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//private definitons
static void _detect_extra_files(struct t_mympd_state *mympd_state, const char *uri, sds *booklet_path, struct t_list *images, bool is_dirname);

//public functions
sds get_extra_files(struct t_mympd_state *mympd_state, sds buffer, const char *uri, bool is_dirname) {
    struct t_list images;
    list_init(&images);
    sds booklet_path = sdsempty();
    if (is_streamuri(uri) == false &&
        mympd_state->mpd_state->feat_mpd_library == true)
    {
        _detect_extra_files(mympd_state, uri, &booklet_path, &images, is_dirname);
    }
    buffer = tojson_char(buffer, "bookletPath", booklet_path, true);
    buffer = sdscat(buffer, "\"images\": [");
    struct t_list_node *current = images.head;
    while (current != NULL) {
        if (current != images.head) {
            buffer = sdscatlen(buffer, ",", 1);
        }
        buffer = sds_catjson(buffer, current->key, sdslen(current->key));
        current = current->next;
    }
    buffer = sdscatlen(buffer, "]", 1);
    list_clear(&images);
    FREE_SDS(booklet_path);
    return buffer;
}

bool is_smartpls(const char *workdir, sds playlist) {
    bool smartpls = false;
    if (vcb_isfilename(playlist) == true) {
        sds smartpls_file = sdscatfmt(sdsempty(), "%s/smartpls/%s", workdir, playlist);
        if (access(smartpls_file, F_OK ) != -1) { /* Flawfinder: ignore */
            smartpls = true;
        }
        FREE_SDS(smartpls_file);
    }
    return smartpls;
}

bool is_streamuri(const char *uri) {
    if (uri != NULL && strstr(uri, "://") != NULL) {
        return true;
    }
    return false;
}

bool mympd_api_set_binarylimit(struct t_mympd_state *mympd_state) {
    bool rc = true;
    if (mympd_state->mpd_state->feat_mpd_binarylimit == true) {
        MYMPD_LOG_INFO("Setting binarylimit to %u", mympd_state->mpd_state->mpd_binarylimit);
        rc = mpd_run_binarylimit(mympd_state->mpd_state->conn, mympd_state->mpd_state->mpd_binarylimit);
        sds message = sdsempty();
        rc = check_rc_error_and_recover(mympd_state->mpd_state, &message, NULL, 0, true, rc, "mpd_run_binarylimit");
        if (sdslen(message) > 0) {
            ws_notify(message);
            rc = false;
        }
        FREE_SDS(message);
    }
    return rc;
}

//replacement for deprecated mpd_status_get_elapsed_time
unsigned mympd_api_get_elapsed_seconds(struct mpd_status *status) {
    return mpd_status_get_elapsed_ms(status) / 1000;
}

void mympd_state_default(struct t_mympd_state *mympd_state) {
    mympd_state->music_directory = sdsnew("auto");
    mympd_state->music_directory_value = sdsempty();
    mympd_state->playlist_directory = sdsnew("/var/lib/mpd/playlists");
    mympd_state->jukebox_mode = JUKEBOX_OFF;
    mympd_state->jukebox_playlist = sdsnew("Database");
    mympd_state->jukebox_unique_tag.len = 1;
    mympd_state->jukebox_unique_tag.tags[0] = MPD_TAG_ARTIST;
    mympd_state->jukebox_last_played = 24;
    mympd_state->jukebox_queue_length = 1;
    mympd_state->jukebox_enforce_unique = true;
    mympd_state->coverimage_names = sdsnew("folder,cover");
    mympd_state->tag_list_search = sdsnew("Artist,Album,AlbumArtist,Title,Genre");
    mympd_state->tag_list_browse = sdsnew("Artist,Album,AlbumArtist,Genre");
    mympd_state->smartpls_generate_tag_list = sdsnew("Genre");
    mympd_state->last_played_count = 200;
    mympd_state->smartpls = true;
    mympd_state->smartpls_sort = sdsempty();
    mympd_state->smartpls_prefix = sdsnew("myMPDsmart");
    mympd_state->smartpls_interval = 14400;
    mympd_state->booklet_name = sdsnew("booklet.pdf");
    mympd_state->auto_play = false;
    mympd_state->cols_queue_current = sdsnew("[\"Pos\",\"Title\",\"Artist\",\"Album\",\"Duration\"]");
    mympd_state->cols_search = sdsnew("[\"Title\",\"Artist\",\"Album\",\"Duration\"]");
    mympd_state->cols_browse_database_detail = sdsnew("[\"Track\",\"Title\",\"Duration\"]");
    mympd_state->cols_browse_playlists_detail = sdsnew("[\"Pos\",\"Title\",\"Artist\",\"Album\",\"Duration\"]");
    mympd_state->cols_browse_filesystem = sdsnew("[\"Pos\",\"Title\",\"Artist\",\"Album\",\"Duration\"]");
    mympd_state->cols_playback = sdsnew("[\"Artist\",\"Album\"]");
    mympd_state->cols_queue_last_played = sdsnew("[\"Pos\",\"Title\",\"Artist\",\"Album\",\"LastPlayed\"]");
    mympd_state->cols_queue_jukebox = sdsnew("[\"Pos\",\"Title\",\"Artist\",\"Album\"]");
    mympd_state->volume_min = 0;
    mympd_state->volume_max = 100;
    mympd_state->volume_step = 5;
    mympd_state->mpd_stream_port = 8080;
    mympd_state->webui_settings = sdsnew("{}");
    mympd_state->lyrics_uslt_ext = sdsnew("txt");
    mympd_state->lyrics_sylt_ext = sdsnew("lrc");
    mympd_state->lyrics_vorbis_uslt = sdsnew("LYRICS");
    mympd_state->lyrics_vorbis_sylt = sdsnew("SYNCEDLYRICS");
    mympd_state->covercache_keep_days = 7;
    reset_t_tags(&mympd_state->tag_types_search);
    reset_t_tags(&mympd_state->tag_types_browse);
    reset_t_tags(&mympd_state->smartpls_generate_tag_types);
    //init last played songs list
    list_init(&mympd_state->last_played);
    //init sticker queue
    list_init(&mympd_state->sticker_queue);
    //sticker cache
    mympd_state->sticker_cache_building = false;
    mympd_state->sticker_cache = NULL;
    //album cache
    mympd_state->album_cache_building = false;
    mympd_state->album_cache = NULL;
    //jukebox queue
    list_init(&mympd_state->jukebox_queue);
    list_init(&mympd_state->jukebox_queue_tmp);
    //mpd state
    mympd_state->mpd_state = (struct t_mpd_state *)malloc_assert(sizeof(struct t_mpd_state));
    mpd_shared_default_mpd_state(mympd_state->mpd_state);
    //triggers;
    list_init(&mympd_state->triggers);
    //home icons
    list_init(&mympd_state->home_list);
    //timer
    mympd_api_timer_timerlist_init(&mympd_state->timer_list);
}

void mympd_state_free(struct t_mympd_state *mympd_state) {
    list_clear(&mympd_state->jukebox_queue);
    list_clear(&mympd_state->jukebox_queue_tmp);
    list_clear(&mympd_state->sticker_queue);
    list_clear(&mympd_state->triggers);
    list_clear(&mympd_state->last_played);
    list_clear(&mympd_state->home_list);
    mympd_api_timer_timerlist_free(&mympd_state->timer_list);
    //mpd state
    mpd_shared_free_mpd_state(mympd_state->mpd_state);
    //caches
    sticker_cache_free(&mympd_state->sticker_cache);
    album_cache_free(&mympd_state->album_cache);
    //sds
    FREE_SDS(mympd_state->tag_list_search);
    FREE_SDS(mympd_state->tag_list_browse);
    FREE_SDS(mympd_state->smartpls_generate_tag_list);
    FREE_SDS(mympd_state->jukebox_playlist);
    FREE_SDS(mympd_state->cols_queue_current);
    FREE_SDS(mympd_state->cols_search);
    FREE_SDS(mympd_state->cols_browse_database_detail);
    FREE_SDS(mympd_state->cols_browse_playlists_detail);
    FREE_SDS(mympd_state->cols_browse_filesystem);
    FREE_SDS(mympd_state->cols_playback);
    FREE_SDS(mympd_state->cols_queue_last_played);
    FREE_SDS(mympd_state->cols_queue_jukebox);
    FREE_SDS(mympd_state->coverimage_names);
    FREE_SDS(mympd_state->music_directory);
    FREE_SDS(mympd_state->music_directory_value);
    FREE_SDS(mympd_state->smartpls_sort);
    FREE_SDS(mympd_state->smartpls_prefix);
    FREE_SDS(mympd_state->booklet_name);
    FREE_SDS(mympd_state->navbar_icons);
    FREE_SDS(mympd_state->webui_settings);
    FREE_SDS(mympd_state->playlist_directory);
    FREE_SDS(mympd_state->lyrics_sylt_ext);
    FREE_SDS(mympd_state->lyrics_uslt_ext);
    FREE_SDS(mympd_state->lyrics_vorbis_uslt);
    FREE_SDS(mympd_state->lyrics_vorbis_sylt);
    //struct itself
    FREE_PTR(mympd_state);
}

//private functions
static void _detect_extra_files(struct t_mympd_state *mympd_state, const char *uri, sds *booklet_path, struct t_list *images, bool is_dirname) {
    sds path = sdsnew(uri);
    if (is_dirname == false) {
        dirname(path);
        sdsupdatelen(path);
    }

    if (is_virtual_cuedir(mympd_state->music_directory_value, path)) {
        //fix virtual cue sheet directories
        dirname(path);
        sdsupdatelen(path);
    }
    sds albumpath = sdscatfmt(sdsempty(), "%s/%s", mympd_state->music_directory_value, path);

    MYMPD_LOG_DEBUG("Read extra files from albumpath: \"%s\"", albumpath);
    errno = 0;
    DIR *album_dir = opendir(albumpath);
    if (album_dir != NULL) {
        struct dirent *next_file;
        sds fullpath = sdsempty();
        while ((next_file = readdir(album_dir)) != NULL) {
            const char *ext = strrchr(next_file->d_name, '.');
            if (strcmp(next_file->d_name, mympd_state->booklet_name) == 0) {
                MYMPD_LOG_DEBUG("Found booklet for uri %s", uri);
                *booklet_path = sdscatfmt(*booklet_path, "%s/%s", path, mympd_state->booklet_name);
            }
            else if (ext != NULL) {
                if (strcasecmp(ext, ".webp") == 0 || strcasecmp(ext, ".jpg") == 0 ||
                    strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".png") == 0 ||
                    strcasecmp(ext, ".avif") == 0 || strcasecmp(ext, ".svg") == 0)
                {
                    fullpath = sdscatfmt(fullpath, "%s/%s", path, next_file->d_name);
                    list_push(images, fullpath, 0, NULL, NULL);
                    sdsclear(fullpath);
                }
            }
        }
        closedir(album_dir);
        FREE_SDS(fullpath);
    }
    else {
        MYMPD_LOG_ERROR("Can not open directory \"%s\" to get list of extra files", albumpath);
        MYMPD_LOG_ERRNO(errno);
    }
    FREE_SDS(path);
    FREE_SDS(albumpath);
}
