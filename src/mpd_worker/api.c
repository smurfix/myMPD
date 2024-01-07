/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"
#include "src/mpd_worker/api.h"

#include "src/lib/jsonrpc.h"
#include "src/lib/log.h"
#include "src/lib/sds_extras.h"
#include "src/mpd_client/playlists.h"
#include "src/mpd_worker/cache.h"
#include "src/mpd_worker/smartpls.h"
#include "src/mpd_worker/song.h"

/**
 * Handler for mpd worker api requests
 * @param mpd_worker_state pointer to mpd_worker_state struct
 */
void mpd_worker_api(struct t_mpd_worker_state *mpd_worker_state) {
    struct t_work_request *request = mpd_worker_state->request;
    bool rc;
    bool bool_buf1;
    bool async = false;
    sds sds_buf1 = NULL;
    sds sds_buf2 = NULL;
    sds sds_buf3 = NULL;
    sds error = sdsempty();

    struct t_jsonrpc_parse_error parse_error;
    jsonrpc_parse_error_init(&parse_error);

    const char *method = get_cmd_id_method_name(request->cmd_id);
    MYMPD_LOG_INFO(NULL, "MPD WORKER API request (%lld)(%ld) %s: %s", request->conn_id, request->id, method, request->data);
    //create response struct
    struct t_work_response *response = create_response(request);
    //some shortcuts
    struct t_partition_state *partition_state = mpd_worker_state->partition_state;

    switch(request->cmd_id) {
        case MYMPD_API_SONG_FINGERPRINT:
            if (json_get_string(request->data, "$.params.uri", 1, FILEPATH_LEN_MAX, &sds_buf1, vcb_ispathfilename, &parse_error) == true) {
                response->data = mpd_worker_song_fingerprint(partition_state, response->data, request->id, sds_buf1);
            }
            break;
        case MYMPD_API_CACHES_CREATE:
            if (json_get_bool(request->data, "$.params.force", &bool_buf1, &parse_error) == true) {
                response->data = jsonrpc_respond_ok(response->data, request->cmd_id, request->id, JSONRPC_FACILITY_DATABASE);
                push_response(response, request->id, request->conn_id);
                mpd_worker_cache_init(mpd_worker_state, bool_buf1);
                async = true;
            }
            break;
        case MYMPD_API_PLAYLIST_CONTENT_SHUFFLE:
            if (json_get_string(request->data, "$.params.plist", 1, FILENAME_LEN_MAX, &sds_buf1, vcb_isfilename, &parse_error) == true) {
                rc = mpd_client_playlist_shuffle(partition_state, sds_buf1, &error);
                response->data = rc == true
                    ? jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Shuffled playlist %{plist} successfully", 2, "plist", sds_buf1)
                    : jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Shuffling playlist %{plist} failed: %{error}", 4, "plist", sds_buf1, "error", error);
            }
            break;
        case MYMPD_API_PLAYLIST_CONTENT_SORT:
            if (json_get_string(request->data, "$.params.plist", 1, FILENAME_LEN_MAX, &sds_buf1, vcb_isfilename, &parse_error) == true &&
                json_get_string(request->data, "$.params.tag", 1, NAME_LEN_MAX, &sds_buf2, vcb_ismpdtag, &parse_error) == true &&
                json_get_bool(request->data, "$.params.sortdesc", &bool_buf1, NULL) == true)
            {
                rc = mpd_client_playlist_sort(partition_state, sds_buf1, sds_buf2, bool_buf1, &error);
                response->data = rc == true
                    ? jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                          JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Sorted playlist %{plist} successfully", 2, "plist", sds_buf1)
                    : jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                           JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Sorting playlist %{plist} failed: %{error}", 4, "plist", sds_buf1, "error", error);
            }
            break;
        case MYMPD_API_PLAYLIST_CONTENT_VALIDATE:
            if (json_get_string(request->data, "$.params.plist", 1, FILENAME_LEN_MAX, &sds_buf1, vcb_isfilename, &parse_error) == true &&
                json_get_bool(request->data, "$.params.remove", &bool_buf1, &parse_error) == true)
            {
                long result = mpd_client_playlist_validate(partition_state, sds_buf1, bool_buf1, &error);
                if (result == -1) {
                    response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id, JSONRPC_FACILITY_PLAYLIST,
                        JSONRPC_SEVERITY_ERROR, "Validation of playlist %{plist} failed: %{error}", 4, "plist", sds_buf1, "error", error);
                }
                else if (result == 0) {
                    response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id, JSONRPC_FACILITY_PLAYLIST,
                        JSONRPC_SEVERITY_INFO, "Content of playlist %{plist} is valid", 2, "plist", sds_buf1);
                }
                else {
                    sds_buf2 = sdsfromlonglong((long long)result);
                    if (bool_buf1 == true) {
                        response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id, JSONRPC_FACILITY_PLAYLIST,
                            JSONRPC_SEVERITY_WARN, "Removed %{count} entries from playlist %{plist}", 4, "count", sds_buf2, "plist", sds_buf1);
                    }
                    else {
                        response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id, JSONRPC_FACILITY_PLAYLIST,
                            JSONRPC_SEVERITY_WARN, "%{count} invalid entries in playlist %{plist}", 4, "count", sds_buf2, "plist", sds_buf1);
                    }
                }
            }
            break;
        case MYMPD_API_PLAYLIST_CONTENT_VALIDATE_ALL:
            if (json_get_bool(request->data, "$.params.remove", &bool_buf1, &parse_error) == true) {
                response->data = jsonrpc_respond_message(response->data, request->cmd_id, request->id,
                    JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Playlists validation started");
                push_response(response, request->id, request->conn_id);
                long result = mpd_client_playlist_validate_all(partition_state, bool_buf1, &error);
                if (result == -1) {
                    sds_buf1 = jsonrpc_notify_phrase(sdsempty(), JSONRPC_FACILITY_PLAYLIST,
                        JSONRPC_SEVERITY_ERROR, "Validation of all playlists failed: %{error}", 2, "error", error);
                }
                else if (result == 0) {
                    sds_buf1 = jsonrpc_notify(sdsempty(), JSONRPC_FACILITY_PLAYLIST,
                        JSONRPC_SEVERITY_INFO, "Content of all playlists are valid");
                }
                else {
                    sds_buf2 = sdsfromlonglong((long long)result);
                    if (bool_buf1 == true) {
                        sds_buf1 = jsonrpc_notify_phrase(sdsempty(), JSONRPC_FACILITY_PLAYLIST,
                            JSONRPC_SEVERITY_WARN, "Removed %{count} entries from playlist %{plist}", 2, "count", sds_buf2);
                    }
                    else {
                        sds_buf1 = jsonrpc_notify_phrase(sdsempty(), JSONRPC_FACILITY_PLAYLIST,
                            JSONRPC_SEVERITY_WARN, "%{count} invalid entries in playlists", 2, "count", sds_buf2);
                    }
                }
                ws_notify(sds_buf1, MPD_PARTITION_ALL);
                async = true;
            }
            break;
        case MYMPD_API_PLAYLIST_CONTENT_DEDUP:
            if (json_get_string(request->data, "$.params.plist", 1, FILENAME_LEN_MAX, &sds_buf1, vcb_isfilename, &parse_error) == true &&
                json_get_bool(request->data, "$.params.remove", &bool_buf1, &parse_error) == true)
            {
                long result = mpd_client_playlist_dedup(partition_state, sds_buf1, bool_buf1, &error);
                if (result == -1) {
                    response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Deduplication of playlist %{plist} failed: %{error}", 4, "plist", sds_buf1, "error", error);
                }
                else if (result == 0) {
                    response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Content of playlist %{plist} is uniq", 2, "plist", sds_buf1);
                }
                else {
                    sds_buf2 = sdsfromlonglong((long long)result);
                    if (bool_buf1 == true) {
                        response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_WARN, "Removed %{count} entries from playlist %{plist}", 4, "count", sds_buf2, "plist", sds_buf1);
                    }
                    else {
                        response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_WARN, "%{count} duplicate entries in playlist %{plist}", 4, "count", sds_buf2, "plist", sds_buf1);
                    }
                }
            }
            break;
        case MYMPD_API_PLAYLIST_CONTENT_DEDUP_ALL:
            if (json_get_bool(request->data, "$.params.remove", &bool_buf1, &parse_error) == true) {
                response->data = jsonrpc_respond_message(response->data, request->cmd_id, request->id,
                    JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Playlists deduplication started");
                push_response(response, request->id, request->conn_id);
                sds buffer;
                long result = mpd_client_playlist_dedup_all(partition_state, bool_buf1, &error);
                if (result == -1) {
                    buffer = jsonrpc_notify_phrase(sdsempty(),
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Deduplication of all playlists failed: %{error}", 2, "error", error);
                }
                else if (result == 0) {
                    buffer = jsonrpc_notify(sdsempty(),
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Content of all playlists are uniq");
                }
                else {
                    sds_buf2 = sdsfromlonglong((long long)result);
                    if (bool_buf1 == true) {
                        buffer = jsonrpc_notify_phrase(sdsempty(),
                            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_WARN, "Removed %{count} entries from playlists", 2, "count", sds_buf2);
                    }
                    else {
                        buffer = jsonrpc_notify_phrase(sdsempty(),
                            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_WARN, "%{count} duplicate entries in playlists", 2, "count", sds_buf2);
                    }
                }
                ws_notify(buffer, MPD_PARTITION_ALL);
                FREE_SDS(buffer);
                async = true;
            }
            break;
        case MYMPD_API_PLAYLIST_CONTENT_VALIDATE_DEDUP:
            if (json_get_string(request->data, "$.params.plist", 1, FILENAME_LEN_MAX, &sds_buf1, vcb_isfilename, &parse_error) == true &&
                json_get_bool(request->data, "$.params.remove", &bool_buf1, &parse_error) == true)
            {
                long result1 = mpd_client_playlist_validate(partition_state, sds_buf1, bool_buf1, &error);
                if (result1 == -1) {
                    response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Validation of playlist %{plist} failed: %{error}", 4, "plist", sds_buf1, "error", error);
                    break;
                }
                long result2 = mpd_client_playlist_dedup(partition_state, sds_buf1, bool_buf1, &error);
                if (result2 == -1) {
                    response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Deduplication of playlist %{plist} failed: %{error}", 4, "plist", sds_buf1, "error", error);
                    break;
                }
                
                if (result1 + result2 == 0) {
                    response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Content of playlist %{plist} is valid and uniq", 2, "plist", sds_buf1);
                }
                else {
                    sds_buf2 = sdsfromlonglong((long long)result1);
                    sds_buf3 = sdsfromlonglong((long long)result2);
                    if (bool_buf1 == true) {
                        response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_WARN, "Removed %{count1} invalid and %{count2} duplicate entries from playlist %{plist}",
                            6, "count1", sds_buf2, "count2", sds_buf3, "plist", sds_buf1);
                    }
                    else {
                        response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_WARN, "%{count1} invalid and %{count2} duplicate entries in playlist %{plist}",
                            6, "count1", sds_buf2, "count2", sds_buf3, "plist", sds_buf1);
                    }
                }
            }
            break;
        case MYMPD_API_PLAYLIST_CONTENT_VALIDATE_DEDUP_ALL:
            if (json_get_bool(request->data, "$.params.remove", &bool_buf1, &parse_error) == true) {
                response->data = jsonrpc_respond_message(response->data, request->cmd_id, request->id,
                    JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Playlists validation and deduplication started");
                push_response(response, request->id, request->conn_id);
                long result1 = mpd_client_playlist_validate_all(partition_state, bool_buf1, &error);
                long result2 = -1;
                if (result1 > -1) {
                    result2 = mpd_client_playlist_dedup_all(partition_state, bool_buf1, &error);
                }
                if (result1 == -1) {
                    sds_buf1 = jsonrpc_notify_phrase(sdsempty(),
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Validation of all playlists failed: %{error}", 2, "error", error);
                }
                else if (result2 == -1) {
                    sds_buf1 = jsonrpc_notify_phrase(sdsempty(),
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Deduplication of all playlists failed: %{error}", 2, "error", error);
                }
                else if (result1 + result2 == 0) {
                    sds_buf1 = jsonrpc_notify(sdsempty(),
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Content of all playlists are valid and uniq");
                }
                else {
                    sds_buf2 = sdsfromlonglong((long long)result1);
                    sds_buf3 = sdsfromlonglong((long long)result2);
                    if (bool_buf1 == true) {
                        sds_buf1 = jsonrpc_notify_phrase(sdsempty(),
                            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_WARN, "Removed %{count1} invalid and %{count2} duplicate entries from playlists",
                            4, "count1", sds_buf2, "count2", sds_buf3);
                    }
                    else {
                        sds_buf1 = jsonrpc_notify_phrase(sdsempty(),
                            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_WARN, "%{count1} invalid and %{count2} duplicate entries in playlists",
                            4, "count1", sds_buf2, "count2", sds_buf3);
                    }
                }
                ws_notify(sds_buf1, MPD_PARTITION_ALL);
                async = true;
            }
            break;
        case MYMPD_API_SMARTPLS_UPDATE:
            if (mpd_worker_state->smartpls == false) {
                response->data = jsonrpc_respond_message(response->data, request->cmd_id, request->id,
                    JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Smart playlists are disabled");
                break;
            }
            if (json_get_string(request->data, "$.params.plist", 1, 200, &sds_buf1, vcb_isfilename, &parse_error) == true) {
                rc = mpd_worker_smartpls_update(mpd_worker_state, sds_buf1);
                if (rc == true) {
                    response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Smart playlist %{playlist} updated", 2, "playlist", sds_buf1);
                    //notify client
                    //send mpd event manually as fallback if mpd playlist is not created (no songs are found)
                    send_jsonrpc_event(JSONRPC_EVENT_UPDATE_STORED_PLAYLIST, MPD_PARTITION_ALL);
                }
                else {
                    response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Updating smart playlist %{playlist} failed", 2, "playlist", sds_buf1);
                }
            }
            break;
        case MYMPD_API_SMARTPLS_UPDATE_ALL:
            if (mpd_worker_state->smartpls == false) {
                response->data = jsonrpc_respond_message(response->data, request->cmd_id, request->id,
                    JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Smart playlists are disabled");
                break;
            }
            if (json_get_bool(request->data, "$.params.force", &bool_buf1, &parse_error) == true) {
                response->data = jsonrpc_respond_message(response->data, request->cmd_id, request->id,
                    JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Smart playlists update started");
                push_response(response, request->id, request->conn_id);
                rc = mpd_worker_smartpls_update_all(mpd_worker_state, bool_buf1);
                if (rc == true) {
                    send_jsonrpc_notify(JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, MPD_PARTITION_ALL, "Smart playlists updated");
                }
                else {
                    send_jsonrpc_notify(JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, MPD_PARTITION_ALL, "Smart playlists update failed");
                }
                async = true;
            }
            break;
        default:
            response->data = jsonrpc_respond_message(response->data, request->cmd_id, request->id,
                JSONRPC_FACILITY_GENERAL, JSONRPC_SEVERITY_ERROR, "Unknown request");
            MYMPD_LOG_ERROR(MPD_PARTITION_DEFAULT, "Unknown API request: %.*s", (int)sdslen(request->data), request->data);
    }
    FREE_SDS(sds_buf1);
    FREE_SDS(sds_buf2);
    FREE_SDS(sds_buf3);

    if (async == true) {
        //already responded
        free_request(request);
        FREE_SDS(error);
        jsonrpc_parse_error_clear(&parse_error);
        return;
    }

    //sync request handling
    if (sdslen(response->data) == 0) {
        // error handling
        if (parse_error.message != NULL) {
            // jsonrpc parsing error
            response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                JSONRPC_FACILITY_GENERAL, JSONRPC_SEVERITY_ERROR, "Parsing error: %{message}", 4, "message", parse_error.message, "path", parse_error.path);
        }
        else if (sdslen(error) > 0) {
            // error from api function
            response->data = jsonrpc_respond_message(response->data, request->cmd_id, request->id,
                JSONRPC_FACILITY_GENERAL, JSONRPC_SEVERITY_ERROR, error);
            MYMPD_LOG_ERROR(MPD_PARTITION_DEFAULT, "Error processing method \"%s\"", method);
        }
        else {
            // no response and no error - this should not occur
            response->data = jsonrpc_respond_message_phrase(response->data, request->cmd_id, request->id,
                JSONRPC_FACILITY_GENERAL, JSONRPC_SEVERITY_ERROR, "No response for method %{method}", 2, "method", method);
            MYMPD_LOG_ERROR(MPD_PARTITION_DEFAULT, "No response for method \"%s\"", method);
        }
    }
    push_response(response, request->id, request->conn_id);
    free_request(request);
    FREE_SDS(error);
    jsonrpc_parse_error_clear(&parse_error);
}
