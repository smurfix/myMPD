/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"
#include "mympd_api_queue.h"

#include "../lib/jsonrpc.h"
#include "../lib/log.h"
#include "../lib/sds_extras.h"
#include "../lib/utility.h"
#include "../mpd_client/mpd_client_errorhandler.h"
#include "../mpd_client/mpd_client_tags.h"
#include "mympd_api_status.h"
#include "mympd_api_sticker.h"
#include "mympd_api_webradios.h"

#include <string.h>

//private definitions
sds _print_queue_entry(struct t_mympd_state *mympd_state, sds buffer, const struct t_tags *tagcols, struct mpd_song *song);

//public
bool mympd_api_queue_play_newly_inserted(struct t_mympd_state *mympd_state) {
    bool rc = mpd_send_queue_changes_brief(mympd_state->mpd_state->conn, mympd_state->mpd_state->queue_version);
    if (mympd_check_rc_error_and_recover(mympd_state->mpd_state, rc, "mpd_send_queue_changes_brief") == false) {
        return false;
    }
    unsigned song_pos;
    unsigned song_id;
    rc = mpd_recv_queue_change_brief(mympd_state->mpd_state->conn, &song_pos, &song_id);
    mpd_response_finish(mympd_state->mpd_state->conn);
    if (rc == true) {
        rc = mpd_run_play_id(mympd_state->mpd_state->conn, song_id);
    }
    if (mympd_check_rc_error_and_recover(mympd_state->mpd_state, rc, "mpd_run_play_id") == false) {
        return false;
    }
    return rc;
}

bool mympd_api_queue_prio_set(struct t_mympd_state *mympd_state, const unsigned trackid, const unsigned priority) {
    //set priority, priority have only an effect in random mode
    bool rc = mpd_run_prio_id(mympd_state->mpd_state->conn, priority, trackid);
    if (mympd_check_rc_error_and_recover(mympd_state->mpd_state, rc, "mpd_run_prio_id") == false) {
        return false;
    }
    return true;
}

bool mympd_api_queue_prio_set_highest(struct t_mympd_state *mympd_state, const unsigned trackid) {
    //default prio is 0
    unsigned priority = 1;

    //try to get prio of next song
    struct mpd_status *status = mpd_run_status(mympd_state->mpd_state->conn);
    if (status == NULL) {
        mympd_check_error_and_recover(mympd_state->mpd_state);
        return false;
    }
    int next_song_id = mpd_status_get_next_song_id(status);
    mpd_status_free(status);
    if (next_song_id > -1 ) {
        bool rc = mpd_send_get_queue_song_id(mympd_state->mpd_state->conn, (unsigned)next_song_id);
        if (rc == true) {
            struct mpd_song *song = mpd_recv_song(mympd_state->mpd_state->conn);
            if (song != NULL) {
                priority = mpd_song_get_prio(song);
                priority++;
                mpd_song_free(song);
            }
        }
        mpd_response_finish(mympd_state->mpd_state->conn);
        if (mympd_check_rc_error_and_recover(mympd_state->mpd_state, rc, "mpd_send_get_queue_song_id") == false) {
            return false;
        }
    }
    if (priority > MPD_QUEUE_PRIO_MAX) {
        MYMPD_LOG_WARN("MPD queue priority limit reached, setting it to max %d", MPD_QUEUE_PRIO_MAX);
        priority = MPD_QUEUE_PRIO_MAX;
    }
    return mympd_api_queue_prio_set(mympd_state, trackid, priority);
}

bool mympd_api_queue_replace_with_song(struct t_mympd_state *mympd_state, const char *uri) {
    if (mpd_command_list_begin(mympd_state->mpd_state->conn, false)) {
        bool rc = mpd_send_clear(mympd_state->mpd_state->conn);
        if (rc == false) {
            MYMPD_LOG_ERROR("Error adding command to command list mpd_send_clear");
        }
        rc = mpd_send_add(mympd_state->mpd_state->conn, uri);
        if (rc == false) {
            MYMPD_LOG_ERROR("Error adding command to command list mpd_send_add");
        }
        if (mpd_command_list_end(mympd_state->mpd_state->conn) == true) {
            mpd_response_finish(mympd_state->mpd_state->conn);
        }
    }
    if (mympd_check_error_and_recover(mympd_state->mpd_state) == false) {
        return false;
    }
    return true;
}

bool mympd_api_queue_replace_with_playlist(struct t_mympd_state *mympd_state, const char *plist) {
    if (mpd_command_list_begin(mympd_state->mpd_state->conn, false)) {
        mpd_send_clear(mympd_state->mpd_state->conn);
        mpd_send_load(mympd_state->mpd_state->conn, plist);
        if (mpd_command_list_end(mympd_state->mpd_state->conn) == true) {
            mpd_response_finish(mympd_state->mpd_state->conn);
        }
    }
    if (mympd_check_error_and_recover(mympd_state->mpd_state) == false) {
        return false;
    }
    return true;
}

sds mympd_api_queue_status(struct t_mympd_state *mympd_state, sds buffer) {
    struct mpd_status *status = mpd_run_status(mympd_state->mpd_state->conn);
    if (status == NULL) {
        mympd_check_error_and_recover(mympd_state->mpd_state);
        return buffer;
    }

    mympd_state->mpd_state->queue_version = mpd_status_get_queue_version(status);
    mympd_state->mpd_state->queue_length = mpd_status_get_queue_length(status);
    mympd_state->mpd_state->crossfade = (time_t)mpd_status_get_crossfade(status);
    mympd_state->mpd_state->state = mpd_status_get_state(status);

    if (buffer != NULL) {
        buffer = jsonrpc_notify_start(buffer, JSONRPC_EVENT_UPDATE_QUEUE);
        buffer = mympd_api_status_print(mympd_state, buffer, status);
        buffer = jsonrpc_respond_end(buffer);
    }
    mpd_status_free(status);
    return buffer;
}

sds mympd_api_queue_crop(struct t_mympd_state *mympd_state, sds buffer, enum mympd_cmd_ids cmd_id, long request_id, bool or_clear) {
    struct mpd_status *status = mpd_run_status(mympd_state->mpd_state->conn);
    if (status == NULL) {
        mympd_check_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id);
        return buffer;
    }
    const unsigned length = mpd_status_get_queue_length(status) - 1;
    unsigned playing_song_pos = (unsigned)mpd_status_get_song_pos(status);
    enum mpd_state state = mpd_status_get_state(status);

    if ((state == MPD_STATE_PLAY || state == MPD_STATE_PAUSE) && length > 1) {
        playing_song_pos++;
        if (playing_song_pos < length) {
            bool rc = mpd_run_delete_range(mympd_state->mpd_state->conn, playing_song_pos, UINT_MAX);
            if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_run_delete_range") == false) {
                return buffer;
            }
        }
        playing_song_pos--;
        if (playing_song_pos > 0) {
            bool rc = mpd_run_delete_range(mympd_state->mpd_state->conn, 0, playing_song_pos--);
            if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_run_delete_range") == false) {
                return buffer;
            }
        }
        buffer = jsonrpc_respond_ok(buffer, cmd_id, request_id, JSONRPC_FACILITY_QUEUE);
    }
    else if (or_clear == true || state == MPD_STATE_STOP) {
        bool rc = mpd_run_clear(mympd_state->mpd_state->conn);
        if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_run_clear") == true) {
            buffer = jsonrpc_respond_ok(buffer, cmd_id, request_id, JSONRPC_FACILITY_QUEUE);
        }
    }
    else {
        buffer = jsonrpc_respond_message(buffer, cmd_id, request_id,
            JSONRPC_FACILITY_QUEUE, JSONRPC_SEVERITY_ERROR, "Can not crop the queue");
        MYMPD_LOG_ERROR("Can not crop the queue");
    }

    mpd_status_free(status);

    return buffer;
}

sds mympd_api_queue_list(struct t_mympd_state *mympd_state, sds buffer, long request_id,
                         long offset, long limit, const struct t_tags *tagcols)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_QUEUE_LIST;
    struct mpd_status *status = mpd_run_status(mympd_state->mpd_state->conn);
    if (status == NULL) {
        mympd_check_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id);
        return buffer;
    }

    mympd_state->mpd_state->queue_version = mpd_status_get_queue_version(status);
    mympd_state->mpd_state->queue_length = (long long)mpd_status_get_queue_length(status);
    mpd_status_free(status);

    if (offset >= mympd_state->mpd_state->queue_length) {
        offset = 0;
    }

    long real_limit = offset + limit;

    bool rc = mpd_send_list_queue_range_meta(mympd_state->mpd_state->conn, (unsigned)offset, (unsigned)real_limit);
    if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_send_list_queue_range_meta") == false) {
        return buffer;
    }

    buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
    buffer = sdscat(buffer, "\"data\":[");
    unsigned total_time = 0;
    long entity_count = 0;
    long entities_returned = 0;
    struct mpd_song *song;
    while ((song = mpd_recv_song(mympd_state->mpd_state->conn)) != NULL) {
        if (entities_returned++) {
            buffer = sdscatlen(buffer, ",", 1);
        }
        buffer = _print_queue_entry(mympd_state, buffer, tagcols, song);
        total_time += mpd_song_get_duration(song);
        mpd_song_free(song);
        entity_count++;
    }

    buffer = sdscatlen(buffer, "],", 2);
    buffer = tojson_uint(buffer, "totalTime", total_time, true);
    buffer = tojson_llong(buffer, "totalEntities", mympd_state->mpd_state->queue_length, true);
    buffer = tojson_long(buffer, "offset", offset, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, false);
    buffer = jsonrpc_respond_end(buffer);

    mpd_response_finish(mympd_state->mpd_state->conn);
    mympd_check_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id);
    return buffer;
}

sds mympd_api_queue_search(struct t_mympd_state *mympd_state, sds buffer, long request_id,
                            const char *mpdtagtype, const long offset, const long limit, const char *searchstr, const struct t_tags *tagcols)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_QUEUE_SEARCH;
    bool rc = mpd_search_queue_songs(mympd_state->mpd_state->conn, false);
    if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_queue_songs") == false) {
        mpd_search_cancel(mympd_state->mpd_state->conn);
        return buffer;
    }
    if (mpd_tag_name_parse(mpdtagtype) != MPD_TAG_UNKNOWN) {
        rc = mpd_search_add_tag_constraint(mympd_state->mpd_state->conn, MPD_OPERATOR_DEFAULT, mpd_tag_name_parse(mpdtagtype), searchstr);
        if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_tag_constraint") == false) {
            mpd_search_cancel(mympd_state->mpd_state->conn);
            return buffer;
        }
    }
    else {
        rc = mpd_search_add_any_tag_constraint(mympd_state->mpd_state->conn, MPD_OPERATOR_DEFAULT, searchstr);
        if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_any_tag_constraint") == false) {
            mpd_search_cancel(mympd_state->mpd_state->conn);
            return buffer;
        }
    }

    rc = mpd_search_commit(mympd_state->mpd_state->conn);
    if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_commit") == false) {
        return buffer;
    }

    buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
    buffer = sdscat(buffer, "\"data\":[");
    struct mpd_song *song;
    unsigned total_time = 0;
    long entity_count = 0;
    long entities_returned = 0;
    long real_limit = offset + limit;
    while ((song = mpd_recv_song(mympd_state->mpd_state->conn)) != NULL) {
        if (entity_count >= offset && entity_count < real_limit) {
            if (entities_returned++) {
                buffer= sdscatlen(buffer, ",", 1);
            }
            buffer = _print_queue_entry(mympd_state, buffer, tagcols, song);
            total_time += mpd_song_get_duration(song);
        }
        mpd_song_free(song);
        entity_count++;
    }

    buffer = sdscatlen(buffer, "],", 2);
    buffer = tojson_uint(buffer, "totalTime", total_time, true);
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "offset", offset, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_char(buffer, "mpdtagtype", mpdtagtype, false);
    buffer = jsonrpc_respond_end(buffer);

    mpd_response_finish(mympd_state->mpd_state->conn);
    if (mympd_check_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id) == false) {
        return buffer;
    }

    return buffer;
}

sds mympd_api_queue_search_adv(struct t_mympd_state *mympd_state, sds buffer, long request_id,
        sds expression, sds sort, bool sortdesc, unsigned offset, unsigned limit,
        const struct t_tags *tagcols)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_QUEUE_SEARCH_ADV;
    bool rc = mpd_search_queue_songs(mympd_state->mpd_state->conn, false);
    if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_queue_songs") == false) {
        mpd_search_cancel(mympd_state->mpd_state->conn);
        return buffer;
    }

    if (sdslen(expression) == 0) {
        //search requires an expression
        rc = mpd_search_add_expression(mympd_state->mpd_state->conn, "(base '')");
    }
    else {
        rc = mpd_search_add_expression(mympd_state->mpd_state->conn, expression);
    }
    if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_expression") == false) {
        mpd_search_cancel(mympd_state->mpd_state->conn);
        return buffer;
    }

    enum mpd_tag_type sort_tag = mpd_tag_name_parse(sort);
    if (sort_tag != MPD_TAG_UNKNOWN) {
        sort_tag = get_sort_tag(sort_tag);
        rc = mpd_search_add_sort_tag(mympd_state->mpd_state->conn, sort_tag, sortdesc);
        if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_sort_tag") == false) {
            mpd_search_cancel(mympd_state->mpd_state->conn);
            return buffer;
        }
    }
    else if (strcmp(sort, "LastModified") == 0) {
        rc = mpd_search_add_sort_name(mympd_state->mpd_state->conn, "Last-Modified", sortdesc);
        if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_sort_name") == false) {
            mpd_search_cancel(mympd_state->mpd_state->conn);
            return buffer;
        }
    }
    else if (strcmp(sort, "Priority") == 0) {
        rc = mpd_search_add_sort_name(mympd_state->mpd_state->conn, "prio", sortdesc);
        if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_sort_name") == false) {
            mpd_search_cancel(mympd_state->mpd_state->conn);
            return buffer;
        }
    }
    else {
        MYMPD_LOG_WARN("Unknown sort tag: %s", sort);
    }

    unsigned real_limit = limit == 0 ? offset + MPD_PLAYLIST_LENGTH_MAX : offset + limit;
    rc = mpd_search_add_window(mympd_state->mpd_state->conn, offset, real_limit);
    if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_window") == false) {
        mpd_search_cancel(mympd_state->mpd_state->conn);
        return buffer;
    }

    rc = mpd_search_commit(mympd_state->mpd_state->conn);
    if (mympd_check_rc_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_commit") == false) {
        return buffer;
    }

    buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
    buffer = sdscat(buffer, "\"data\":[");
    struct mpd_song *song;
    unsigned total_time = 0;
    long entities_returned = 0;
    while ((song = mpd_recv_song(mympd_state->mpd_state->conn)) != NULL) {
        if (entities_returned++) {
            buffer= sdscatlen(buffer, ",", 1);
        }
        buffer = _print_queue_entry(mympd_state, buffer, tagcols, song);
        total_time += mpd_song_get_duration(song);
        mpd_song_free(song);
    }

    buffer = sdscatlen(buffer, "],", 2);
    buffer = tojson_uint(buffer, "totalTime", total_time, true);
    if (sdslen(expression) == 0) {
        buffer = tojson_llong(buffer, "totalEntities", mympd_state->mpd_state->queue_length, true);
    }
    else {
        buffer = tojson_long(buffer, "totalEntities", -1, true);
    }
    buffer = tojson_uint(buffer, "offset", offset, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, false);
    buffer = jsonrpc_respond_end(buffer);

    mpd_response_finish(mympd_state->mpd_state->conn);
    if (mympd_check_error_and_recover_respond(mympd_state->mpd_state, &buffer, cmd_id, request_id) == false) {
        return buffer;
    }

    return buffer;
}

//private functions

sds _print_queue_entry(struct t_mympd_state *mympd_state, sds buffer, const struct t_tags *tagcols, struct mpd_song *song) {
    buffer = sdscatlen(buffer, "{", 1);
    buffer = tojson_uint(buffer, "id", mpd_song_get_id(song), true);
    buffer = tojson_uint(buffer, "Pos", mpd_song_get_pos(song), true);
    buffer = tojson_uint(buffer, "Priority", mpd_song_get_prio(song), true);
    const struct mpd_audio_format *audioformat = mpd_song_get_audio_format(song);
    buffer = printAudioFormat(buffer, audioformat);
    buffer = sdscatlen(buffer, ",", 1);
    buffer = get_song_tags(buffer, mympd_state->mpd_state, tagcols, song);
    const char *uri = mpd_song_get_uri(song);
    buffer = sdscatlen(buffer, ",", 1);
    if (is_streamuri(uri) == true) {
        sds webradio = get_webradio_from_uri(mympd_state->config->workdir, uri);
        if (sdslen(webradio) > 0) {
            buffer = sdscat(buffer, "\"webradio\":{");
            buffer = sdscatsds(buffer, webradio);
            buffer = sdscatlen(buffer, "},", 2);
            buffer = tojson_char(buffer, "type", "webradio", false);
        }
        else {
            buffer = tojson_char(buffer, "type", "stream", false);
        }
        FREE_SDS(webradio);
    }
    else {
        buffer = tojson_char(buffer, "type", "song", false);
    }
    if (mympd_state->mpd_state->feat_mpd_stickers == true) {
        buffer = sdscatlen(buffer, ",", 1);
        buffer = mympd_api_sticker_list(buffer, &mympd_state->sticker_cache, uri);
    }
    buffer = sdscatlen(buffer, "}", 1);
    return buffer;
}
