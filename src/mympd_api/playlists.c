/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"
#include "src/mympd_api/playlists.h"

#include "dist/utf8/utf8.h"
#include "src/lib/api.h"
#include "src/lib/filehandler.h"
#include "src/lib/jsonrpc.h"
#include "src/lib/log.h"
#include "src/lib/mem.h"
#include "src/lib/rax_extras.h"
#include "src/lib/sds_extras.h"
#include "src/lib/smartpls.h"
#include "src/lib/sticker_cache.h"
#include "src/lib/utility.h"
#include "src/mpd_client/errorhandler.h"
#include "src/mpd_client/playlists.h"
#include "src/mpd_client/search_local.h"
#include "src/mpd_client/tags.h"
#include "src/mympd_api/sticker.h"

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

/**
 * Simple struct to save playlist data
 */
struct t_pl_data {
    time_t last_modified;      //!< last modified time
    enum playlist_types type;  //!< playlist type (smartpls or not)
    sds name;                  //!< playlistname
};

/**
 * Frees the t_pl_data struct used as callback for rax_free_data
 * @param data void pointer to a t_sticker_value struct
 */
static void free_t_pl_data(void *data) {
    struct t_pl_data *pl_data = (struct t_pl_data *)data;
    FREE_SDS(pl_data->name);
    FREE_PTR(pl_data);
}

/**
 * Lists mpd playlists and myMPD smart playlists
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc request id
 * @param offset list offset
 * @param limit maximum number of entries to print
 * @param searchstr string to search in the playlist name
 * @param type playlist type to list
 * @return pointer to buffer
 */
sds mympd_api_playlist_list(struct t_partition_state *partition_state, sds buffer, long request_id,
        long offset, long limit, sds searchstr, enum playlist_types type)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_PLAYLIST_LIST;
    bool rc = mpd_send_list_playlists(partition_state->conn);
    if (mympd_check_rc_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, rc, "mpd_send_list_playlists") == false) {
        return buffer;
    }
    size_t search_len = sdslen(searchstr);

    struct mpd_playlist *pl;
    rax *entity_list = raxNew();

    long real_limit = offset + limit;
    sds key = sdsempty();
    while ((pl = mpd_recv_playlist(partition_state->conn)) != NULL) {
        const char *plpath = mpd_playlist_get_path(pl);
        bool smartpls = is_smartpls(partition_state->mympd_state->config->workdir, plpath);
        if ((search_len == 0 || utf8casestr(plpath, searchstr) != NULL) &&
            (type == PLTYPE_ALL || (type == PLTYPE_STATIC && smartpls == false) || (type == PLTYPE_SMART && smartpls == true)))
        {
            struct t_pl_data *data = malloc_assert(sizeof(struct t_pl_data));
            data->last_modified = mpd_playlist_get_last_modified(pl);
            data->type = smartpls == true ? PLTYPE_SMART : PLTYPE_STATIC;
            data->name = sdsnew(plpath);
            sdsclear(key);
            key = sdscatsds(key, data->name);
            sds_utf8_tolower(key);
            while (raxTryInsert(entity_list, (unsigned char *)key, sdslen(key), data, NULL) == 0) {
                //duplicate - add chars until it is uniq
                key = sdscatlen(key, ":", 1);
            }
        }
        mpd_playlist_free(pl);
    }
    mpd_response_finish(partition_state->conn);
    if (mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id) == false) {
        //free result
        rax_free_data(entity_list, free_t_pl_data);
        //return error message
        return buffer;
    }

    //add empty smart playlists
    if (type != PLTYPE_STATIC) {
        sds smartpls_path = sdscatfmt(sdsempty(), "%S/smartpls", partition_state->mympd_state->config->workdir);
        errno = 0;
        DIR *smartpls_dir = opendir(smartpls_path);
        if (smartpls_dir != NULL) {
            struct dirent *next_file;
            while ((next_file = readdir(smartpls_dir)) != NULL ) {
                if (next_file->d_type == DT_REG &&
                    (search_len == 0 || utf8casestr(next_file->d_name, searchstr) != NULL)
                ) {
                    struct t_pl_data *data = malloc_assert(sizeof(struct t_pl_data));
                    data->last_modified = smartpls_get_mtime(partition_state->mympd_state->config->workdir, next_file->d_name);
                    data->type = PLTYPE_SMARTPLS_ONLY;
                    data->name = sdsnew(next_file->d_name);
                    sdsclear(key);
                    key = sdscat(key, next_file->d_name);
                    sds_utf8_tolower(key);
                    if (raxTryInsert(entity_list, (unsigned char *)key, sdslen(key), data, NULL) == 0) {
                        //smart playlist already added
                        FREE_SDS(data->name);
                        FREE_PTR(data);
                    }
                }
            }
            closedir(smartpls_dir);
        }
        else {
            MYMPD_LOG_ERROR("Can not open smartpls dir \"%s\"", smartpls_path);
            MYMPD_LOG_ERRNO(errno);
        }
        FREE_SDS(smartpls_path);
    }
    FREE_SDS(key);
    buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
    buffer = sdscat(buffer,"\"data\":[");

    long entity_count = 0;
    long entities_returned = 0;
    raxIterator iter;
    raxStart(&iter, entity_list);
    raxSeek(&iter, "^", NULL, 0);
    while (raxNext(&iter)) {
        struct t_pl_data *data = (struct t_pl_data *)iter.data;
        if (entity_count >= offset &&
            entity_count < real_limit)
        {
            if (entities_returned++) {
                buffer = sdscatlen(buffer, ",", 1);
            }
            buffer = sdscatlen(buffer, "{", 1);
            buffer = tojson_char(buffer, "Type", (data->type == PLTYPE_STATIC ? "plist" : "smartpls"), true);
            buffer = tojson_sds(buffer, "uri", data->name, true);
            buffer = tojson_sds(buffer, "name", data->name, true);
            buffer = tojson_llong(buffer, "lastModified", data->last_modified, true);
            buffer = tojson_bool(buffer, "smartplsOnly", data->type == PLTYPE_SMARTPLS_ONLY ? true : false, false);
            buffer = sdscatlen(buffer, "}", 1);
        }
        entity_count++;
        FREE_SDS(data->name);
        FREE_PTR(data);
    }
    raxStop(&iter);
    buffer = sdscatlen(buffer, "],", 2);
    buffer = tojson_sds(buffer, "searchstr", searchstr, true);
    buffer = tojson_llong(buffer, "totalEntities", (long long)entity_list->numele, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_long(buffer, "offset", offset, false);
    buffer = jsonrpc_end(buffer);
    raxFree(entity_list);
    return buffer;
}

/**
 * Lists the content of a mpd playlist
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc request id
 * @param plist playlist name to list contents
 * @param offset list offset
 * @param limit maximum number of entries to print
 * @param searchstr string to search in the playlist name
 * @param tagcols columns to print
 * @return pointer to buffer
 */
sds mympd_api_playlist_content_list(struct t_partition_state *partition_state, sds buffer, long request_id,
        sds plist, long offset, long limit, sds searchstr, const struct t_tags *tagcols)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_PLAYLIST_CONTENT_LIST;
    bool rc = mpd_send_list_playlist_meta(partition_state->conn, plist);
    if (mympd_check_rc_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, rc, "mpd_send_list_playlist_meta") == false) {
        return buffer;
    }

    buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
    buffer = sdscat(buffer,"\"data\":[");

    struct mpd_song *song;
    long entities_returned = 0;
    long entity_count = 0;
    unsigned total_time = 0;
    long real_limit = offset + limit;
    time_t last_played_max = 0;
    sds last_played_song_uri = sdsempty();
    while ((song = mpd_recv_song(partition_state->conn)) != NULL) {
        total_time += mpd_song_get_duration(song);
        if (entity_count >= offset &&
            entity_count < real_limit)
        {
            if (search_mpd_song(song, searchstr, tagcols) == true) {
                if (entities_returned++) {
                    buffer= sdscatlen(buffer, ",", 1);
                }
                buffer = sdscatlen(buffer, "{", 1);
                if (is_streamuri(mpd_song_get_uri(song)) == true) {
                    buffer = tojson_char(buffer, "Type", "stream", true);
                }
                else {
                    buffer = tojson_char(buffer, "Type", "song", true);
                }
                buffer = tojson_long(buffer, "Pos", entity_count, true);
                buffer = get_song_tags(buffer, partition_state->mpd_state->feat_tags, tagcols, song);
                if (partition_state->mpd_state->feat_stickers) {
                    buffer = sdscatlen(buffer, ",", 1);
                    struct t_sticker *sticker = get_sticker_from_cache(&partition_state->mpd_state->sticker_cache, mpd_song_get_uri(song));
                    buffer = mympd_api_sticker_print(buffer, sticker);
                    if (sticker != NULL &&
                        sticker->last_played > last_played_max)
                    {
                        last_played_max = sticker->last_played;
                        last_played_song_uri = sds_replace(last_played_song_uri, mpd_song_get_uri(song));
                    }
                }
                buffer = sdscatlen(buffer, "}", 1);
            }
            else {
                entity_count--;
            }
        }
        mpd_song_free(song);
        entity_count++;
    }

    mpd_response_finish(partition_state->conn);
    if (mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id) == false) {
        FREE_SDS(last_played_song_uri);
        return buffer;
    }

    bool smartpls = is_smartpls(partition_state->mympd_state->config->workdir, plist);

    buffer = sdscatlen(buffer, "],", 2);
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_uint(buffer, "totalTime", total_time, true);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, true);
    buffer = tojson_long(buffer, "offset", offset, true);
    buffer = tojson_sds(buffer, "searchstr", searchstr, true);
    buffer = tojson_sds(buffer, "plist", plist, true);
    buffer = tojson_bool(buffer, "smartpls", smartpls, true);
    buffer = sdscat(buffer, "\"lastPlayedSong\":{");
    buffer = tojson_time(buffer, "time", last_played_max, true);
    buffer = tojson_sds(buffer, "uri", last_played_song_uri, false);
    buffer = sdscatlen(buffer, "}", 1);
    buffer = jsonrpc_end(buffer);

    FREE_SDS(last_played_song_uri);
    return buffer;
}

/**
 * Rename the mpd playlists and the corresponding myMPD smart playlist
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc request id
 * @param old_playlist old playlist name
 * @param new_playlist new playlist name
 * @return pointer to buffer
 */
sds mympd_api_playlist_rename(struct t_partition_state *partition_state, sds buffer,
        long request_id, const char *old_playlist, const char *new_playlist)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_PLAYLIST_RENAME;
    //first handle smart playlists
    sds old_pl_file = sdscatfmt(sdsempty(), "%S/smartpls/%s", partition_state->mympd_state->config->workdir, old_playlist);
    sds new_pl_file = sdscatfmt(sdsempty(), "%S/smartpls/%s", partition_state->mympd_state->config->workdir, new_playlist);
    //link old name to new name
    if (testfile_read(old_pl_file) == true) {
        //smart playlist file exists
        errno = 0;
        if (link(old_pl_file, new_pl_file) == -1) {
            //handle new smart playlist name exists already
            if (errno == EEXIST) {
                MYMPD_LOG_ERROR("A playlist with name \"%s\" already exists", new_pl_file);
                buffer = jsonrpc_respond_message(buffer, cmd_id, request_id,
                    JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "A smart playlist with this name already exists");
                FREE_SDS(old_pl_file);
                FREE_SDS(new_pl_file);
                return buffer;
            }
            //other errors
            MYMPD_LOG_ERROR("Renaming smart playlist from \"%s\" to \"%s\" failed", old_pl_file, new_pl_file);
            MYMPD_LOG_ERRNO(errno);
            buffer = jsonrpc_respond_message(buffer, cmd_id, request_id,
                JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Renaming playlist failed");
            FREE_SDS(old_pl_file);
            FREE_SDS(new_pl_file);
            return buffer;
        }
        //remove old smart playlist
        if (rm_file(old_pl_file) == false) {
            //try to remove new smart playlist to prevent duplicates
            try_rm_file(new_pl_file);
            buffer = jsonrpc_respond_message(buffer, cmd_id, request_id,
                JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Renaming playlist failed");
            FREE_SDS(old_pl_file);
            FREE_SDS(new_pl_file);
            return buffer;
        }
    }
    FREE_SDS(old_pl_file);
    FREE_SDS(new_pl_file);
    //rename mpd playlist
    bool rc = mpd_run_rename(partition_state->conn, old_playlist, new_playlist);
    if (mympd_check_rc_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, rc, "mpd_run_rename") == true) {
        buffer = jsonrpc_respond_message(buffer, cmd_id, request_id,
            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Successfully renamed playlist");
    }
    return buffer;
}

/**
 * Deletes the mpd playlists and the myMPD smart playlist
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc request id
 * @param playlist playlist to delete
 * @param smartpls_only true to delete only the smart playlist definition
 * @return pointer to buffer
 */
sds mympd_api_playlist_delete(struct t_partition_state *partition_state, sds buffer,
        long request_id, const char *playlist, bool smartpls_only)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_PLAYLIST_RM;
    //remove smart playlist
    sds pl_file = sdscatfmt(sdsempty(), "%S/smartpls/%s", partition_state->mympd_state->config->workdir, playlist);
    if (try_rm_file(pl_file) == RM_FILE_ERROR) {
        //ignores none existing smart playlist
        buffer = jsonrpc_respond_message(buffer, cmd_id, request_id,
            JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_ERROR, "Deleting smart playlist failed");
        MYMPD_LOG_ERROR("Deleting smart playlist \"%s\" failed", playlist);
        MYMPD_LOG_ERRNO(errno);
        FREE_SDS(pl_file);
        return buffer;
    }
    FREE_SDS(pl_file);
    if (smartpls_only == true) {
        send_jsonrpc_event(JSONRPC_EVENT_UPDATE_STORED_PLAYLIST, partition_state->name);
        buffer = jsonrpc_respond_ok(buffer, cmd_id, request_id, JSONRPC_FACILITY_PLAYLIST);
        return buffer;
    }
    //remove mpd playlist
    bool rc = mpd_run_rm(partition_state->conn, playlist);
    if (mympd_check_rc_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, rc, "mpd_run_rm") == true) {
        buffer = jsonrpc_respond_ok(buffer, cmd_id, request_id, JSONRPC_FACILITY_PLAYLIST);
    }
    return buffer;
}

/**
 * Parses the playlist delete criteria from string to enum
 * @param str string to parse
 * @return parsed criteria as enum
 */
enum plist_delete_criterias parse_plist_delete_criteria(const char *str) {
    if (strcmp(str, "deleteEmptyPlaylists") == 0) {
        return PLAYLIST_DELETE_EMPTY;
    }
    if (strcmp(str, "deleteSmartPlaylists") == 0) {
        return PLAYLIST_DELETE_SMARTPLS;
    }
    if (strcmp(str, "deleteAllPlaylists") == 0) {
        return PLAYLIST_DELETE_ALL;
    }
    return PLAYLIST_DELETE_UNKNOWN;
}

/**
 * Deletes the mpd playlists and the myMPD smart playlist
 * @param partition_state pointer to partition state
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc request id
 * @param criteria deletion criteria
 * @return pointer to buffer
 */
sds mympd_api_playlist_delete_all(struct t_partition_state *partition_state, sds buffer,
        long request_id, enum plist_delete_criterias criteria)
{
    enum mympd_cmd_ids cmd_id = MYMPD_API_PLAYLIST_RM_ALL;
    bool rc = mpd_send_list_playlists(partition_state->conn);
    if (mympd_check_rc_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, rc, "mpd_send_list_playlists") == false) {
        return buffer;
    }

    struct t_list playlists;
    list_init(&playlists);
    //get all mpd playlists
    struct mpd_playlist *pl;
    while ((pl = mpd_recv_playlist(partition_state->conn)) != NULL) {
        const char *plpath = mpd_playlist_get_path(pl);
        list_push(&playlists, plpath, 1, NULL, NULL);
        mpd_playlist_free(pl);
    }
    mpd_response_finish(partition_state->conn);
    if (mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id) == false) {
        list_clear(&playlists);
        return buffer;
    }
    //delete each smart playlist file that have no corresponding mpd playlist file
    sds smartpls_path = sdscatfmt(sdsempty(), "%S/smartpls", partition_state->mympd_state->config->workdir);
    errno = 0;
    DIR *smartpls_dir = opendir(smartpls_path);
    if (smartpls_dir != NULL) {
        struct dirent *next_file;
        sds smartpls_file = sdsempty();
        while ((next_file = readdir(smartpls_dir)) != NULL ) {
            if (next_file->d_type == DT_REG) {
                if (list_get_node(&playlists, next_file->d_name) == NULL) {
                    smartpls_file = sdscatfmt(smartpls_file, "%S/smartpls/%s", partition_state->mympd_state->config->workdir, next_file->d_name);
                    if (rm_file(smartpls_file) == true) {
                        MYMPD_LOG_INFO("Removed orphaned smartpls file \"%s\"", smartpls_file);
                    }
                    sdsclear(smartpls_file);
                }
            }
        }
        FREE_SDS(smartpls_file);
        closedir(smartpls_dir);
    }
    else {
        MYMPD_LOG_ERROR("Can not open smartpls dir \"%s\"", smartpls_path);
        MYMPD_LOG_ERRNO(errno);
    }
    FREE_SDS(smartpls_path);

    if (criteria == PLAYLIST_DELETE_EMPTY) {
        struct t_list_node *current = playlists.head;
        while (current != NULL) {
            current->value_i = mpd_client_enum_playlist(partition_state, current->key, true);
            current = current->next;
        }
    }

    if (mpd_command_list_begin(partition_state->conn, false)) {
        struct t_list_node *current;
        while ((current = list_shift_first(&playlists)) != NULL) {
            bool smartpls = false;
            if (criteria == PLAYLIST_DELETE_SMARTPLS) {
                sds smartpls_file = sdscatfmt(sdsempty(), "%S/smartpls/%s", partition_state->mympd_state->config->workdir, current->key);
                if (try_rm_file(smartpls_file) == RM_FILE_OK) {
                    MYMPD_LOG_INFO("Smartpls file %s removed", smartpls_file);
                    smartpls = true;
                }
                FREE_SDS(smartpls_file);
            }
            if (criteria == PLAYLIST_DELETE_ALL ||
                (criteria == PLAYLIST_DELETE_SMARTPLS && smartpls == true) ||
                (criteria == PLAYLIST_DELETE_EMPTY && current->value_i == 0))
            {
                rc = mpd_send_rm(partition_state->conn, current->key);
                if (rc == false) {
                    MYMPD_LOG_ERROR("Error adding command to command list mpd_send_rm");
                    break;
                }
                MYMPD_LOG_INFO("Deleting mpd playlist %s", current->key);
            }
            list_node_free(current);
        }
        if (mpd_command_list_end(partition_state->conn)) {
            mpd_response_finish(partition_state->conn);
        }
        if (mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id) == false) {
            list_clear(&playlists);
            return buffer;
        }
    }
    else if (mympd_check_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id) == false) {
        list_clear(&playlists);
        return buffer;
    }
    list_clear(&playlists);
    buffer = jsonrpc_respond_message(buffer, cmd_id, request_id,
        JSONRPC_FACILITY_PLAYLIST, JSONRPC_SEVERITY_INFO, "Playlists deleted");
    return buffer;
}
