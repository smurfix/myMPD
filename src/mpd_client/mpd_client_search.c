/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"
#include "mpd_client_search.h"

#include "../lib/jsonrpc.h"
#include "../lib/log.h"
#include "../lib/sds_extras.h"
#include "../mympd_api/mympd_api_sticker.h"
#include "mpd_client_errorhandler.h"
#include "mpd_client_tags.h"

#include <string.h>

//private definitions

static sds _mpd_client_search(struct t_mpd_state *mpd_state, sds buffer, enum mympd_cmd_ids cmd_id, long request_id,
        const char *expression, const char *sort, const bool sortdesc,
        const char *plist, unsigned to, unsigned whence,
        const unsigned offset, unsigned limit, const struct t_tags *tagcols,
        struct t_cache *sticker_cache, bool *result);

//public functions

/**
 * Searches the mpd database for songs by expression and returns a jsonrpc result
 * @param mpd_state pointer to struct mpd_state
 * @param buffer already allocated sds string to append the result
 * @param request_id jsonrpc request id
 * @param expression mpd search expression
 * @param sort tag to sort
 * @param sortdesc false = ascending, true = descending
 * @param offset result offset
 * @param limit max number of results to return
 * @param tagcols tags to return
 * @param sticker_cache pointer to sticker cache
 * @param result pointer to bool to set returncode
 * @return pointer to buffer
 */
sds mpd_client_search_response(struct t_mpd_state *mpd_state, sds buffer, long request_id,
        const char *expression, const char *sort, const bool sortdesc, const unsigned offset, const unsigned limit,
        const struct t_tags *tagcols, struct t_cache *sticker_cache, bool *result)
{
    return _mpd_client_search(mpd_state, buffer, MYMPD_API_DATABASE_SEARCH, request_id,
            expression, sort, sortdesc, NULL, 0, MPD_POSITION_ABSOLUTE, offset, limit,
            tagcols, sticker_cache, result);
}

/**
 * Searches the mpd database for songs by expression and adds the result to a playlist
 * @param mpd_state pointer to struct mpd_state
 * @param expression mpd search expression
 * @param plist playlist to create or add the result
 * @param to position to insert the songs, UINT_MAX to append
 * @return true on success else false
 */
bool mpd_client_search_add_to_plist(struct t_mpd_state *mpd_state, const char *expression,
        const char *plist, unsigned to)
{
    bool result = false;
    _mpd_client_search(mpd_state, NULL, MYMPD_API_DATABASE_SEARCH, 0,
            expression, NULL, false, plist, to, MPD_POSITION_ABSOLUTE, 0, 0,
            NULL, NULL, &result);
    return result;
}

/**
 * Searches the mpd database for songs by expression and adds the result to the queue
 * @param mpd_state pointer to struct mpd_state
 * @param expression mpd search expression
 * @param to position to insert the songs, UINT_MAX to append
 * @param whence enum mpd_position_whence:
 *               0 = MPD_POSITION_ABSOLUTE
 *               1 = MPD_POSITION_AFTER_CURRENT
 *               2 = MPD_POSITION_BEFORE_CURRENT
 * @return true on success else false
 */
bool mpd_client_search_add_to_queue(struct t_mpd_state *mpd_state, const char *expression,
        unsigned to, enum mpd_position_whence whence)
{
    bool result = false;
    _mpd_client_search(mpd_state, NULL, MYMPD_API_DATABASE_SEARCH, 0,
            expression, NULL, false, "queue", to, whence, 0, 0,
            NULL, NULL, &result);
    return result;
}

/**
 * Escapes a mpd search expression
 * @param buffer already allocated sds string to append
 * @param tag search tag
 * @param operator search operator
 * @param value search value
 * @return pointer to buffer
 */
sds escape_mpd_search_expression(sds buffer, const char *tag, const char *operator, const char *value) {
    buffer = sdscatfmt(buffer, "(%s %s '", tag, operator);
    for (size_t i = 0;  i < strlen(value); i++) {
        if (value[i] == '\\' || value[i] == '\'') {
            buffer = sdscatlen(buffer, "\\", 1);
        }
        buffer = sds_catchar(buffer, value[i]);
    }
    buffer = sdscatlen(buffer, "')", 2);
    return buffer;
}

//private functions
static sds _mpd_client_search(struct t_mpd_state *mpd_state, sds buffer, enum mympd_cmd_ids cmd_id, long request_id,
        const char *expression, const char *sort, const bool sortdesc, const char *plist,
        unsigned to, unsigned whence, const unsigned offset, const unsigned limit,
        const struct t_tags *tagcols, struct t_cache *sticker_cache, bool *result)
{
    *result = false;
    if (expression[0] == '\0') {
        MYMPD_LOG_ERROR("No search expression defined");
        buffer = jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_MPD,
            JSONRPC_SEVERITY_ERROR, "No search expression defined");
        return buffer;
    }

    if (plist == NULL) {
        //show search results
        bool rc = mpd_search_db_songs(mpd_state->conn, false);
        if (mympd_check_rc_error_and_recover_respond(mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_db_songs") == false) {
            mpd_search_cancel(mpd_state->conn);
            return buffer;
        }
        buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
        buffer = sdscat(buffer, "\"data\":[");
    }
    else if (strcmp(plist, "queue") == 0) {
        //add search to queue
        bool rc = mpd_search_add_db_songs(mpd_state->conn, false);
        if (mympd_check_rc_error_and_recover_respond(mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_db_songs") == false) {
            mpd_search_cancel(mpd_state->conn);
            return buffer;
        }
    }
    else {
        //add search to playlist
        bool rc = mpd_search_add_db_songs_to_playlist(mpd_state->conn, plist);
        if (mympd_check_rc_error_and_recover_respond(mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_db_songs_to_playlist") == false) {
            mpd_search_cancel(mpd_state->conn);
            return buffer;
        }
    }

    bool rc = mpd_search_add_expression(mpd_state->conn, expression);
    if (mympd_check_rc_error_and_recover_respond(mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_expression") == false) {
        mpd_search_cancel(mpd_state->conn);
        return buffer;
    }

    if (plist == NULL ||
        mpd_connection_cmp_server_version(mpd_state->conn, 0, 22, 0) >= 0)
    {
        if (sort != NULL &&
            strcmp(sort, "") != 0 &&
            strcmp(sort, "-") != 0 &&
            mpd_state->feat_mpd_tags == true)
        {
            enum mpd_tag_type sort_tag = mpd_tag_name_parse(sort);
            if (sort_tag != MPD_TAG_UNKNOWN) {
                sort_tag = get_sort_tag(sort_tag);
                rc = mpd_search_add_sort_tag(mpd_state->conn, sort_tag, sortdesc);
                if (mympd_check_rc_error_and_recover_respond(mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_sort_tag") == false) {
                    mpd_search_cancel(mpd_state->conn);
                    return buffer;
                }
            }
            else if (strcmp(sort, "LastModified") == 0) {
                rc = mpd_search_add_sort_name(mpd_state->conn, "Last-Modified", sortdesc);
                if (mympd_check_rc_error_and_recover_respond(mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_sort_name") == false) {
                    mpd_search_cancel(mpd_state->conn);
                    return buffer;
                }
            }
            else {
                MYMPD_LOG_WARN("Unknown sort tag: %s", sort);
            }
        }

        unsigned real_limit = limit == 0 ? offset + MPD_PLAYLIST_LENGTH_MAX : offset + limit;
        rc = mpd_search_add_window(mpd_state->conn, offset, real_limit);
        if (mympd_check_rc_error_and_recover_respond(mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_window") == false) {
            mpd_search_cancel(mpd_state->conn);
            return buffer;
        }
    }

    if (mpd_state->feat_mpd_whence == true &&
        plist != NULL &&
        to < UINT_MAX) //to = UINT_MAX is append
    {
        rc = mpd_search_add_position(mpd_state->conn, to, whence);
        if (mympd_check_rc_error_and_recover_respond(mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_add_position") == false) {
            mpd_search_cancel(mpd_state->conn);
            return buffer;
        }
    }

    rc = mpd_search_commit(mpd_state->conn);
    if (mympd_check_rc_error_and_recover_respond(mpd_state, &buffer, cmd_id, request_id, rc, "mpd_search_commit") == false) {
        return buffer;
    }

    if (plist == NULL) {
        struct mpd_song *song;
        unsigned entities_returned = 0;
        while ((song = mpd_recv_song(mpd_state->conn)) != NULL) {
            if (entities_returned++) {
                buffer = sdscatlen(buffer, ",", 1);
            }
            buffer = sdscatlen(buffer, "{", 1);
            buffer = tojson_char(buffer, "Type", "song", true);
            buffer = get_song_tags(buffer, mpd_state, tagcols, song);
            if (sticker_cache != NULL) {
                buffer = sdscatlen(buffer, ",", 1);
                buffer = mympd_api_sticker_list(buffer, sticker_cache, mpd_song_get_uri(song));
            }
            buffer = sdscatlen(buffer, "}", 1);
            mpd_song_free(song);
        }
        buffer = sdscatlen(buffer, "],", 2);
        if (offset == 0 &&
            entities_returned < limit)
        {
            buffer = tojson_uint(buffer, "totalEntities", entities_returned, true);
        }
        else {
            buffer = tojson_long(buffer, "totalEntities", -1, true);
        }
        buffer = tojson_uint(buffer, "offset", offset, true);
        buffer = tojson_uint(buffer, "returnedEntities", entities_returned, true);
        buffer = tojson_char(buffer, "expression", expression, true);
        buffer = tojson_char(buffer, "sort", sort, true);
        buffer = tojson_bool(buffer, "sortdesc", sortdesc, false);
        buffer = jsonrpc_respond_end(buffer);
    }
    else if (strcmp(plist, "queue") == 0) {
        buffer = jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_QUEUE,
            JSONRPC_SEVERITY_INFO, "Added songs to queue");
    }
    else {
        buffer = jsonrpc_respond_message_phrase(buffer, cmd_id, request_id, JSONRPC_FACILITY_PLAYLIST,
            JSONRPC_SEVERITY_INFO, "Added songs to %{playlist}", 2, "playlist", plist);
    }

    mpd_response_finish(mpd_state->conn);
    if (mympd_check_error_and_recover_respond(mpd_state, &buffer, cmd_id, request_id) == false) {
       return buffer;
    }
    *result = true;
    return buffer;
}
