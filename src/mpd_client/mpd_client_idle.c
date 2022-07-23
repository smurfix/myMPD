/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"
#include "mpd_client_idle.h"

#include "../lib/jsonrpc.h"
#include "../lib/log.h"
#include "../lib/sds_extras.h"
#include "../lib/utility.h"
#include "../mpd_worker/mpd_worker.h"
#include "../mympd_api/mympd_api_handler.h"
#include "../mympd_api/mympd_api_queue.h"
#include "../mympd_api/mympd_api_status.h"
#include "../mympd_api/mympd_api_stats.h"
#include "../mympd_api/mympd_api_sticker.h"
#include "../mympd_api/mympd_api_timer.h"
#include "../mympd_api/mympd_api_timer_handlers.h"
#include "../mympd_api/mympd_api_trigger.h"
#include "mpd_client_connection.h"
#include "mpd_client_errorhandler.h"
#include "mpd_client_features.h"
#include "mpd_client_jukebox.h"
#include "mpd_client_tags.h"

#include <poll.h>
#include <string.h>

//private definitions
static bool update_mympd_caches(struct t_mympd_state *mympd_state, time_t timeout);

//public functions
void mpd_client_parse_idle(struct t_mympd_state *mympd_state, unsigned idle_bitmask) {
    for (unsigned j = 0;; j++) {
        enum mpd_idle idle_event = 1 << j;
        const char *idle_name = mpd_idle_name(idle_event);
        if (idle_name == NULL) {
            break;
        }
        if (idle_bitmask & idle_event) {
            MYMPD_LOG_INFO("MPD idle event: %s", idle_name);
            sds buffer = sdsempty();
            switch(idle_event) {
                case MPD_IDLE_DATABASE:
                    //database has changed
                    MYMPD_LOG_INFO("MPD database has changed");
                    buffer = jsonrpc_event(buffer, "update_database");
                    //add timer for cache updates
                    update_mympd_caches(mympd_state, 10);
                    break;
                case MPD_IDLE_STORED_PLAYLIST:
                    buffer = jsonrpc_event(buffer, "update_stored_playlist");
                    break;
                case MPD_IDLE_QUEUE: {
                    unsigned old_queue_version = mympd_state->mpd_state->queue_version;
                    buffer = mympd_api_queue_status(mympd_state, buffer);
                    if (mympd_state->mpd_state->queue_version == old_queue_version) {
                        //ignore this idle event, queue version has not changed
                        //idle event is not from current partition
                        sdsclear(buffer);
                        MYMPD_LOG_DEBUG("Queue version has not changed, ignoring idle event MPD_IDLE_QUEUE");
                        break;
                    }
                    //jukebox enabled
                    if (mympd_state->jukebox_mode != JUKEBOX_OFF &&
                        mympd_state->mpd_state->queue_length < mympd_state->jukebox_queue_length)
                    {
                        MYMPD_LOG_DEBUG("Jukebox mode: %u", mympd_state->jukebox_mode);
                        mpd_client_jukebox(mympd_state);
                    }
                    //autoPlay enabled
                    if (mympd_state->auto_play == true &&
                        mympd_state->mpd_state->queue_length > 0)
                    {
                        if (mympd_state->mpd_state->state != MPD_STATE_PLAY) {
                            MYMPD_LOG_INFO("AutoPlay enabled, start playing");
                            if (mpd_run_play(mympd_state->mpd_state->conn) == false) {
                                check_error_and_recover(mympd_state->mpd_state, NULL, NULL, 0);
                            }
                        }
                        else {
                            MYMPD_LOG_DEBUG("Autoplay enabled, already playing");
                        }
                    }
                    break;
                }
                case MPD_IDLE_PLAYER:
                    //get and put mpd state
                    buffer = mympd_api_status_get(mympd_state, buffer, NULL, 0);
                    //check if song has changed
                    if (mympd_state->mpd_state->song_id != mympd_state->mpd_state->last_song_id &&
                        mympd_state->mpd_state->last_skipped_id != mympd_state->mpd_state->last_song_id &&
                        mympd_state->mpd_state->last_song_uri != NULL)
                    {
                        time_t now = time(NULL);
                        if (mympd_state->mpd_state->feat_mpd_stickers == true &&          //stickers enabled
                            mympd_state->mpd_state->last_song_set_song_played_time > now) //time in the future
                        {
                            //last song skipped
                            time_t elapsed = now - mympd_state->mpd_state->last_song_start_time;
                            if (elapsed > 10 &&
                                mympd_state->mpd_state->last_song_start_time > 0 &&
                                sdslen(mympd_state->mpd_state->last_song_uri) > 0)
                            {
                                MYMPD_LOG_DEBUG("Song \"%s\" skipped", mympd_state->mpd_state->last_song_uri);
                                mympd_api_sticker_inc_skip_count(mympd_state, mympd_state->mpd_state->last_song_uri);
                                mympd_api_sticker_last_skipped(mympd_state, mympd_state->mpd_state->last_song_uri);
                                mympd_state->mpd_state->last_skipped_id = mympd_state->mpd_state->last_song_id;
                            }
                        }
                    }
                    break;
                case MPD_IDLE_MIXER:
                    buffer = mympd_api_status_volume_get(mympd_state, buffer, NULL, 0);
                    break;
                case MPD_IDLE_OUTPUT:
                    buffer = jsonrpc_event(buffer, "update_outputs");
                    break;
                case MPD_IDLE_OPTIONS:
                    mympd_api_queue_status(mympd_state, NULL);
                    buffer = jsonrpc_event(buffer, "update_options");
                    break;
                case MPD_IDLE_UPDATE:
                    buffer = mympd_api_status_updatedb_state(mympd_state, buffer);
                    break;
                default: {
                    //other idle events not used
                }
            }
            mympd_api_trigger_execute(&mympd_state->trigger_list, (enum trigger_events)idle_event);
            if (sdslen(buffer) > 0) {
                ws_notify(buffer);
            }
            FREE_SDS(buffer);
        }
    }
}

void mpd_client_idle(struct t_mympd_state *mympd_state) {
    sds buffer = sdsempty();
    long mympd_api_queue_length = 0;
    switch (mympd_state->mpd_state->conn_state) {
        case MPD_WAIT: {
            time_t now = time(NULL);
            if (now > mympd_state->mpd_state->reconnect_time) {
                mympd_state->mpd_state->conn_state = MPD_DISCONNECTED;
            }
            //mympd_api_api error response
            mympd_api_queue_length = mympd_queue_length(mympd_api_queue, 50);
            if (mympd_api_queue_length > 0) {
                //Handle request
                MYMPD_LOG_DEBUG("Handle request (mpd disconnected)");
                struct t_work_request *request = mympd_queue_shift(mympd_api_queue, 50, 0);
                if (request != NULL) {
                    if (is_mympd_only_api_method(request->cmd_id) == true) {
                        //reconnect instantly on change of mpd host
                        if (request->cmd_id == MYMPD_API_CONNECTION_SAVE) {
                            mympd_state->mpd_state->conn_state = MPD_DISCONNECTED;
                        }
                        mympd_api_handler(mympd_state, request);
                    }
                    else {
                        //other requests not allowed
                        if (request->conn_id > -1) {
                            struct t_work_result *response = create_result(request);
                            response->data = jsonrpc_respond_message(response->data, request->method, request->id, true, "mpd", "error", "MPD disconnected");
                            MYMPD_LOG_DEBUG("Send http response to connection %lld: %s", request->conn_id, response->data);
                            mympd_queue_push(web_server_queue, response, 0);
                        }
                        free_request(request);
                    }
                }
            }
            if (now < mympd_state->mpd_state->reconnect_time) {
                //pause 100ms to prevent high cpu usage
                my_msleep(100);
            }
            break;
        }
        case MPD_DISCONNECTED:
            //try to connect
            if (mpd_client_connect(mympd_state->mpd_state) == false) {
                break;
            }
            //check version
            if (mpd_connection_cmp_server_version(mympd_state->mpd_state->conn, 0, 21, 0) < 0) {
                MYMPD_LOG_EMERG("MPD version too old, myMPD supports only MPD version >= 0.21.0");
                mympd_state->mpd_state->conn_state = MPD_TOO_OLD;
                s_signal_received = 1;
            }
            //we are connected
            buffer = jsonrpc_event(buffer, "mpd_connected");
            ws_notify(buffer);
            //reset reconnection intervals
            mympd_state->mpd_state->reconnect_interval = 0;
            mympd_state->mpd_state->reconnect_time = 0;
            //reset list of supported tags
            reset_t_tags(&mympd_state->mpd_state->tag_types_mpd);
            //get mpd features
            mpd_client_mpd_features(mympd_state);
            //set binarylimit
            mpd_client_set_binarylimit(mympd_state->mpd_state);
            //initiate cache updates
            update_mympd_caches(mympd_state, 2);
            //set timer for smart playlist update
            mympd_api_timer_replace(&mympd_state->timer_list, 30, (int)mympd_state->smartpls_interval, timer_handler_by_id, TIMER_ID_SMARTPLS_UPDATE, NULL);
            //jukebox
            if (mympd_state->jukebox_mode != JUKEBOX_OFF) {
                mpd_client_jukebox(mympd_state);
            }
            if (!mpd_send_idle(mympd_state->mpd_state->conn)) {
                MYMPD_LOG_ERROR("Entering idle mode failed");
                mympd_state->mpd_state->conn_state = MPD_FAILURE;
            }
            mympd_api_trigger_execute(&mympd_state->trigger_list, TRIGGER_MYMPD_CONNECTED);
            break;
        case MPD_FAILURE:
            MYMPD_LOG_ERROR("MPD connection failed");
            buffer = jsonrpc_event(buffer, "mpd_disconnected");
            ws_notify(buffer);
            mympd_api_trigger_execute(&mympd_state->trigger_list, TRIGGER_MYMPD_DISCONNECTED);
            // fall through
        case MPD_DISCONNECT:
        case MPD_DISCONNECT_INSTANT:
        case MPD_RECONNECT:
            if (mympd_state->mpd_state->conn != NULL) {
                mpd_connection_free(mympd_state->mpd_state->conn);
            }
            mympd_state->mpd_state->conn = NULL;
            if (mympd_state->mpd_state->conn_state != MPD_DISCONNECT_INSTANT) {
                //set wait time for next connection attempt
                mympd_state->mpd_state->conn_state = MPD_WAIT;
                if (mympd_state->mpd_state->reconnect_interval < 20) {
                    mympd_state->mpd_state->reconnect_interval += 2;
                }
                mympd_state->mpd_state->reconnect_time = time(NULL) + mympd_state->mpd_state->reconnect_interval;
                MYMPD_LOG_INFO("Waiting %lld seconds before reconnection", (long long)mympd_state->mpd_state->reconnect_interval);
            }
            else {
                mympd_state->mpd_state->conn_state = MPD_DISCONNECTED;
                mympd_state->mpd_state->reconnect_interval = 0;
                mympd_state->mpd_state->reconnect_time = 0;
            }
            break;
        case MPD_CONNECTED: {
            struct pollfd fds[1];
            fds[0].fd = mpd_connection_get_fd(mympd_state->mpd_state->conn);
            fds[0].events = POLLIN;
            int pollrc = poll(fds, 1, 50);
            bool jukebox_add_song = false;
            bool set_played = false;
            mympd_api_queue_length = mympd_queue_length(mympd_api_queue, 50);
            //handle jukebox and last played only in mpd play state
            if (mympd_state->mpd_state->state == MPD_STATE_PLAY) {
                time_t now = time(NULL);
                //check if we should set the played state of current song
                if (now > mympd_state->mpd_state->set_song_played_time &&
                    mympd_state->mpd_state->set_song_played_time > 0 &&
                    mympd_state->mpd_state->last_last_played_id != mympd_state->mpd_state->song_id)
                {
                    MYMPD_LOG_DEBUG("Song has played half: %lld", (long long)mympd_state->mpd_state->set_song_played_time);
                    set_played = true;
                }
                //check if the jukebox should add a song
                if (mympd_state->jukebox_mode != JUKEBOX_OFF) {
                    //add time is crossfade + 10s before song end time
                    time_t add_time = mympd_state->mpd_state->song_end_time - (mympd_state->mpd_state->crossfade + 10);
                    if (now > add_time &&
                        add_time > 0 &&
                        mympd_state->mpd_state->queue_length <= mympd_state->jukebox_queue_length)
                    {
                        MYMPD_LOG_DEBUG("Jukebox should add song");
                        jukebox_add_song = true;
                    }
                }
            }
            //check if we need to exit the idle mode
            if (pollrc > 0 ||                          //idle event waiting
                mympd_api_queue_length > 0 ||          //api was called
                jukebox_add_song == true ||            //jukebox trigger
                set_played == true ||                  //playstat of song must be set
                mympd_state->sticker_queue.length > 0) //we must set waiting stickers
            {
                MYMPD_LOG_DEBUG("Leaving mpd idle mode");
                if (!mpd_send_noidle(mympd_state->mpd_state->conn)) {
                    check_error_and_recover(mympd_state->mpd_state, NULL, NULL, 0);
                    mympd_state->mpd_state->conn_state = MPD_FAILURE;
                    break;
                }
                if (pollrc > 0) {
                    //Handle idle events
                    MYMPD_LOG_DEBUG("Checking for idle events");
                    enum mpd_idle idle_bitmask = mpd_recv_idle(mympd_state->mpd_state->conn, false);
                    mpd_client_parse_idle(mympd_state, idle_bitmask);
                }
                else {
                    mpd_response_finish(mympd_state->mpd_state->conn);
                }
                //set song played state
                if (set_played == true) {
                    mympd_state->mpd_state->last_last_played_id = mympd_state->mpd_state->song_id;

                    if (mympd_state->last_played_count > 0) {
                        mympd_api_stats_last_played_add_song(mympd_state, mympd_state->mpd_state->song_id);
                    }
                    if (mympd_state->mpd_state->feat_mpd_stickers == true) {
                        mympd_api_sticker_inc_play_count(mympd_state, mympd_state->mpd_state->song_uri);
                        mympd_api_sticker_last_played(mympd_state, mympd_state->mpd_state->song_uri);
                    }
                    mympd_api_trigger_execute(&mympd_state->trigger_list, TRIGGER_MYMPD_SCROBBLE);
                }
                //trigger jukebox
                if (jukebox_add_song == true) {
                    mpd_client_jukebox(mympd_state);
                }
                //an api request is there
                if (mympd_api_queue_length > 0) {
                    //Handle request
                    MYMPD_LOG_DEBUG("Handle request");
                    struct t_work_request *request = mympd_queue_shift(mympd_api_queue, 50, 0);
                    if (request != NULL) {
                        mympd_api_handler(mympd_state, request);
                    }
                }
                //process sticker queue
                if (mympd_state->sticker_queue.length > 0) {
                    mympd_api_sticker_dequeue(mympd_state);
                }
                //reenter idle mode
                MYMPD_LOG_DEBUG("Entering mpd idle mode");
                if (!mpd_send_idle(mympd_state->mpd_state->conn)) {
                    check_error_and_recover(mympd_state->mpd_state, NULL, NULL, 0);
                    mympd_state->mpd_state->conn_state = MPD_FAILURE;
                }
            }
            break;
        }
        default:
            MYMPD_LOG_ERROR("Invalid mpd connection state");
    }
    FREE_SDS(buffer);
}

//private functions

/**
 * Checks if we should create the caches and adds a one-shot timer
 * @param mympd_state pointer to the mympd_state struct
 * @param timeout seconds after the timer triggers
 * @return true on success else false
 */
static bool update_mympd_caches(struct t_mympd_state *mympd_state, time_t timeout) {
    if (mympd_state->mpd_state->feat_mpd_stickers == false &&
        mympd_state->mpd_state->feat_mpd_tags == false)
    {
        MYMPD_LOG_DEBUG("Caches are disabled");
        return true;
    }
    MYMPD_LOG_DEBUG("Adding timer to update the caches");
    return mympd_api_timer_replace(&mympd_state->timer_list, timeout, TIMER_ONE_SHOT_REMOVE, timer_handler_by_id, TIMER_ID_CACHES_CREATE, NULL);
}
