/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

/*! \file
 * \brief myMPD status API
 */

#include "compile_time.h"
#include "src/mympd_api/status.h"

#include "src/lib/datetime.h"
#include "src/lib/jsonrpc.h"
#include "src/lib/log.h"
#include "src/lib/mympd_state.h"
#include "src/lib/sds_extras.h"
#include "src/lib/timer.h"
#include "src/lib/utility.h"
#include "src/mpd_client/errorhandler.h"
#include "src/mpd_client/jukebox.h"
#include "src/mpd_client/shortcuts.h"
#include "src/mpd_client/tags.h"
#include "src/mpd_client/volume.h"
#include "src/mympd_api/extra_media.h"
#include "src/mympd_api/sticker.h"
#include "src/mympd_api/webradio.h"

/**
 * Private definitions
 */

static const char *get_playstate_name(enum mpd_state play_state);

/**
 * Array to resolv the mpd state to a string
 */
static const char *playstate_names[] = {
    [MPD_STATE_UNKNOWN] = "unknown",
    [MPD_STATE_STOP] = "stop",
    [MPD_STATE_PLAY] = "play",
    [MPD_STATE_PAUSE] = "pause"
};

/**
 * Public functions
 */

/**
 * Replacement for deprecated mpd_status_get_elapsed_time
 * @param status pointer to mpd_status struct
 * @return elapsed seconds
 */
unsigned mympd_api_get_elapsed_seconds(struct mpd_status *status) {
    return mpd_status_get_elapsed_ms(status) / 1000;
}

/**
 * Prints the mpd_status as jsonrpc object string
 * @param partition_state pointer to partition state
 * @param album_cache pointer to album cache
 * @param buffer already allocated sds string to append the response
 * @param status pointer to mpd_status struct
 * @return pointer to buffer
 */
sds mympd_api_status_print(struct t_partition_state *partition_state, struct t_cache *album_cache, sds buffer, struct mpd_status *status) {
    enum mpd_state playstate = mpd_status_get_state(status);

    buffer = tojson_char(buffer, "state", get_playstate_name(playstate), true);
    buffer = tojson_int(buffer, "volume", mpd_status_get_volume(status), true);
    buffer = tojson_int(buffer, "songPos", mpd_status_get_song_pos(status), true);
    buffer = tojson_uint(buffer, "elapsedTime", mympd_api_get_elapsed_seconds(status), true);
    buffer = tojson_uint(buffer, "totalTime", mpd_status_get_total_time(status), true);
    buffer = tojson_int(buffer, "currentSongId", mpd_status_get_song_id(status), true);
    buffer = tojson_uint(buffer, "kbitrate", mpd_status_get_kbit_rate(status), true);
    buffer = tojson_uint(buffer, "queueLength", mpd_status_get_queue_length(status), true);
    buffer = tojson_uint(buffer, "queueVersion", mpd_status_get_queue_version(status), true);
    buffer = tojson_int(buffer, "nextSongPos", mpd_status_get_next_song_pos(status), true);
    buffer = tojson_int(buffer, "nextSongId", mpd_status_get_next_song_id(status), true);
    buffer = tojson_int(buffer, "lastSongId", (partition_state->last_song_id ?
        partition_state->last_song_id : -1), true);
    if (partition_state->mpd_state->feat.partitions == true) {
        buffer = tojson_char(buffer, "partition", mpd_status_get_partition(status), true);
    }
    const struct mpd_audio_format *audioformat = mpd_status_get_audio_format(status);
    buffer = printAudioFormat(buffer, audioformat);
    buffer = sdscatlen(buffer, ",", 1);
    buffer = tojson_uint(buffer, "updateState", mpd_status_get_update_id(status), true);
    buffer = tojson_bool(buffer, "updateCacheState", album_cache->building, true);
    buffer = tojson_char(buffer, "lastError", mpd_status_get_error(status), true);
    buffer = tojson_sds(buffer, "lastJukeboxError", partition_state->jukebox.last_error, false);
    return buffer;
}

/**
 * Gets the update database status from mpd as jsonrpc notification
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @return pointer to buffer
 */
sds mympd_api_status_updatedb_state(struct t_partition_state *partition_state, sds buffer) {
    unsigned update_id = mympd_api_status_updatedb_id(partition_state);
    if (update_id == UINT_MAX) {
        buffer = jsonrpc_notify(buffer, JSONRPC_FACILITY_MPD, JSONRPC_SEVERITY_ERROR, "Error getting database update id");
    }
    else if (update_id > 0) {
        buffer = jsonrpc_notify_start(buffer, JSONRPC_EVENT_UPDATE_STARTED);
        buffer = tojson_uint(buffer, "jobid", update_id, false);
        buffer = jsonrpc_end(buffer);
    }
    else {
        buffer = jsonrpc_event(buffer, JSONRPC_EVENT_UPDATE_FINISHED);
    }
    return buffer;
}

/**
 * Gets the update database id
 * @param partition_state pointer to partition state
 * @return database update id
 */
unsigned mympd_api_status_updatedb_id(struct t_partition_state *partition_state) {
    unsigned update_id = UINT_MAX;
    struct mpd_status *status = mpd_run_status(partition_state->conn);
    if (status != NULL) {
        update_id = mpd_status_get_update_id(status);
        MYMPD_LOG_NOTICE(partition_state->name, "Update database id: %u", update_id);
        mpd_status_free(status);
    }
    else {
        MYMPD_LOG_ERROR(partition_state->name, "Failure getting database update id");
    }
    mpd_response_finish(partition_state->conn);
    mympd_check_error_and_recover(partition_state, NULL, "mpd_run_status");
    return update_id;
}

/**
 * Gets the mpd status, updates internal myMPD states and returns a jsonrpc notify or response
 * @param partition_state pointer to partition state
 * @param album_cache pointer to album cache
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc request id
 * @param response_type jsonrpc response type: RESPONSE_TYPE_RESPONSE or RESPONSE_TYPE_NOTIFY
 * @return pointer to buffer
 */
sds mympd_api_status_get(struct t_partition_state *partition_state, struct t_cache *album_cache,
        sds buffer, unsigned request_id, enum jsonrpc_response_types response_type)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_PLAYER_STATE;
    struct mpd_status *status = mpd_run_status(partition_state->conn);
    int song_id = -1;
    bool song_changed = false;
    if (status != NULL) {
        time_t now = time(NULL);
        song_id = mpd_status_get_song_id(status);
        if (partition_state->song_id != song_id) {
            //song has changed, save old state
            song_changed = true;
            partition_state->last_song_id = partition_state->song_id;
            partition_state->last_song_end_time = partition_state->song_end_time;
            partition_state->last_song_start_time = partition_state->song_start_time;
        }

        const char *player_error = mpd_status_get_error(status);
        partition_state->player_error = player_error == NULL || player_error[0] == '\0'
            ? false
            : true;
        partition_state->play_state = mpd_status_get_state(status);
        partition_state->song_id = song_id;
        partition_state->song_pos = mpd_status_get_song_pos(status);
        partition_state->song_duration = (time_t)mpd_status_get_total_time(status);
        partition_state->next_song_id = mpd_status_get_next_song_id(status);
        partition_state->queue_version = mpd_status_get_queue_version(status);
        partition_state->queue_length = mpd_status_get_queue_length(status);
        partition_state->crossfade = (time_t)mpd_status_get_crossfade(status);

        time_t elapsed_time = (time_t)mympd_api_get_elapsed_seconds(status);
        partition_state->song_start_time = now - elapsed_time;
        partition_state->song_end_time = partition_state->song_duration == 0
            ? 0
            : now + partition_state->song_duration - elapsed_time;

        if (partition_state->play_state == MPD_STATE_PLAY) {
            //scrobble time is half length of song or SCROBBLE_TIME_MAX (4 minutes) whatever is shorter
            time_t scrobble_offset = partition_state->song_duration > SCROBBLE_TIME_TOTAL
                ? SCROBBLE_TIME_MAX - elapsed_time
                : (partition_state->song_duration / 2) - elapsed_time;
            if (scrobble_offset > 0) {
                MYMPD_LOG_DEBUG(partition_state->name, "Setting scrobble timer");
                mympd_timer_set(partition_state->timer_fd_scrobble, (int)scrobble_offset, 0);
            }
            else {
                MYMPD_LOG_DEBUG(partition_state->name, "Disabling scrobble timer");
                mympd_timer_set(partition_state->timer_fd_scrobble, 0, 0);
            }
        }
        else {
            MYMPD_LOG_DEBUG(partition_state->name, "Disabling scrobble timer");
            mympd_timer_set(partition_state->timer_fd_scrobble, 0, 0);
        }

        if (partition_state->jukebox.mode == JUKEBOX_OFF ||
            partition_state->play_state != MPD_STATE_PLAY)
        {
            jukebox_disable(partition_state);
        }
        else {
            //jukebox add time is crossfade + 10s before song end time
            time_t add_offset = partition_state->song_duration - (elapsed_time + partition_state->crossfade + JUKEBOX_ADD_SONG_OFFSET);
            if (add_offset > 0) {
                MYMPD_LOG_DEBUG(partition_state->name, "Setting jukebox timer");
                mympd_timer_set(partition_state->timer_fd_jukebox, (int)add_offset, 0);
            }
            else {
                jukebox_disable(partition_state);
            }
        }

        #ifdef MYMPD_DEBUG 
            char fmt_time_now[32];
            readable_time(fmt_time_now, now);
            char fmt_time_start[32];
            readable_time(fmt_time_start, partition_state->song_start_time);
            char fmt_time_end[32];
            readable_time(fmt_time_end, partition_state->song_end_time);
            MYMPD_LOG_DEBUG(partition_state->name, "Now %s, start time %s, end time %s",
                fmt_time_now, fmt_time_start, fmt_time_end);
        #endif

        if (response_type == RESPONSE_TYPE_JSONRPC_NOTIFY) {
            buffer = jsonrpc_notify_start(buffer, JSONRPC_EVENT_UPDATE_STATE);
        }
        else {
            buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
        }
        buffer = mympd_api_status_print(partition_state, album_cache, buffer, status);
        buffer = jsonrpc_end(buffer);

        mpd_status_free(status);
    }
    mpd_response_finish(partition_state->conn);
    if (response_type == RESPONSE_TYPE_JSONRPC_NOTIFY) {
        mympd_check_error_and_recover_notify(partition_state, &buffer, "mpd_run_status");
    }
    else {
        mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, "mpd_run_status");
    }
    //update song uri if song has changed
    if (song_changed == true) {
        struct mpd_song *song = mpd_run_current_song(partition_state->conn);
        if (song != NULL) {
            if (partition_state->last_song != NULL) {
                mpd_song_free(partition_state->last_song);
            }
            partition_state->last_song = partition_state->song;
            partition_state->song = song;
        }
        else {
            if (partition_state->song != NULL) {
                mpd_song_free(partition_state->song);
                partition_state->song = NULL;
            }
        }
        mpd_response_finish(partition_state->conn);
    }
    if (response_type == RESPONSE_TYPE_JSONRPC_NOTIFY) {
        mympd_check_error_and_recover_notify(partition_state, &buffer, "mpd_run_status");
    }
    else {
        mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, "mpd_run_status");
    }
    return buffer;
}

/**
 * Clears the mpd player error returned by the status command.
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the error response
 * @param cmd_id jsonrpc method
 * @param request_id jsonrpc request id
 * @return true on success, else false
 */
bool mympd_api_status_clear_error(struct t_partition_state *partition_state, sds *buffer,
        enum mympd_cmd_ids cmd_id, unsigned request_id)
{
    bool rc = true;
    if (partition_state->player_error == true) {
        mpd_run_clearerror(partition_state->conn);
        rc = mympd_check_error_and_recover_respond(partition_state, buffer, cmd_id, request_id, "mpd_run_clearerror");
        if (rc == true) {
            partition_state->player_error = false;
        }
    }
    return rc;
}

/**
 * Gets the mpd volume as jsonrpc notify
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc request id
 * @param response_type jsonrpc response type: RESPONSE_TYPE_RESPONSE or RESPONSE_TYPE_NOTIFY
 * @return pointer to buffer
 */
sds mympd_api_status_volume_get(struct t_partition_state *partition_state, sds buffer, unsigned request_id, enum jsonrpc_response_types response_type) {
    enum mympd_cmd_ids cmd_id = MYMPD_API_PLAYER_VOLUME_GET;
    int volume = mpd_client_get_volume(partition_state);
    if (response_type == RESPONSE_TYPE_JSONRPC_NOTIFY) {
        buffer = jsonrpc_notify_start(buffer, JSONRPC_EVENT_UPDATE_VOLUME);
    }
    else {
        buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
    }
    buffer = tojson_int(buffer, "volume", volume, false);
    buffer = jsonrpc_end(buffer);
    return buffer;
}

/**
 * Gets the current playing song as jsonrpc response
 * @param mympd_state pointer to mympd state
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc request id
 * @return pointer to buffer
 */
sds mympd_api_status_current_song(struct t_mympd_state *mympd_state, struct t_partition_state *partition_state,
        sds buffer, unsigned request_id)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_PLAYER_CURRENT_SONG;
    if (mpd_command_list_begin(partition_state->conn, true)) {
        if (mpd_send_status(partition_state->conn) == false) {
            mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_status");
        }
        if (mpd_send_current_song(partition_state->conn) == false) {
            mympd_set_mpd_failure(partition_state, "Error adding command to command list mpd_send_current_song");
        }
        mpd_client_command_list_end_check(partition_state);
    }

    struct mpd_status *status = mpd_recv_status(partition_state->conn);
    struct mpd_song *song = NULL;
    if (mpd_response_next(partition_state->conn)) {
        song = mpd_recv_song(partition_state->conn);
    }
    if (status != NULL &&
        song != NULL)
    {
        const char *uri = mpd_song_get_uri(song);
        buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
        buffer = tojson_uint(buffer, "pos", mpd_song_get_pos(song), true);
        buffer = tojson_int(buffer, "currentSongId", partition_state->song_id, true);
        buffer = print_song_tags(buffer, partition_state->mpd_state, &partition_state->mpd_state->tags_mympd, song);
        buffer = sdscatlen(buffer, ",", 1);
        if (partition_state->mpd_state->feat.stickers == true) {
            struct t_stickers sticker;
            stickers_reset(&sticker);
            stickers_enable_all(&sticker, STICKER_TYPE_SONG);
            buffer = mympd_api_sticker_get_print(buffer, mympd_state->stickerdb, STICKER_TYPE_SONG, uri, &sticker);
        }
        buffer = json_comma(buffer);
        buffer = mympd_api_get_extra_media(buffer, partition_state->mpd_state, mympd_state->booklet_name, mympd_state->info_txt_name, uri, false);
        if (is_streamuri(uri) == true) {
            sds webradio = mympd_api_webradio_from_uri_tojson(mympd_state, uri);
            if (sdslen(webradio) > 0) {
                buffer = sdscat(buffer, ",\"webradio\":");
                buffer = sdscatsds(buffer, webradio);
            }
            FREE_SDS(webradio);
        }
        time_t start_time = time(NULL) - (time_t)mympd_api_get_elapsed_seconds(status);
        buffer = sdscatlen(buffer, ",", 1);
        buffer = tojson_time(buffer, "startTime", start_time, false);
        buffer = jsonrpc_end(buffer);
    }
    if (status != NULL) {
        mpd_status_free(status);
    }
    if (song != NULL) {
        mpd_song_free(song);
    }
    mpd_response_finish(partition_state->conn);
    if (song == NULL &&
        mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, "mpd_run_current_song") == true)
    {
        return jsonrpc_respond_message(buffer, cmd_id, request_id,
            JSONRPC_FACILITY_PLAYER, JSONRPC_SEVERITY_INFO, "No current song");
    }
    return buffer;
}

/**
 * Private functions
 */

/**
 * Resolves the mpd_state enum to a string
 * @param play_state mpd_state
 * @return play state as string
 */
static const char *get_playstate_name(enum mpd_state play_state) {
    if ((unsigned)play_state >= 4) {
        return playstate_names[MPD_STATE_UNKNOWN];
    }
    return playstate_names[play_state];
}
