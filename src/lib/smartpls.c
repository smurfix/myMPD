/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"
#include "src/lib/smartpls.h"

#include "src/lib/filehandler.h"
#include "src/lib/jsonrpc.h"
#include "src/lib/msg_queue.h"
#include "src/lib/sds_extras.h"
#include "src/lib/state_files.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

//privat definitions
static bool smartpls_save(sds workdir, const char *smartpltype,
        const char *playlist, const char *expression, int max_entries,
        int timerange, const char *sort, bool sortdesc);

//public functions

/**
 * Saves a sticker based smart playlist
 * @param workdir myMPD working directory
 * @param playlist playlist name
 * @param sticker sticker name
 * @param max_entries maximum number of playlist entries
 * @param min_value minimum integer value of sticker
 * @param sort tag to sort the playlist: empty = no sorting, shuffle or mpd tag
 * @param sortdesc sort descending?
 * @return true on success else false
 */
bool smartpls_save_sticker(sds workdir, const char *playlist, const char *sticker,
    int max_entries, int min_value, const char *sort, bool sortdesc)
{
    return smartpls_save(workdir, "sticker", playlist, sticker, max_entries, min_value, sort, sortdesc);
}

/**
 * Saves a sticker based smart playlist
 * @param workdir myMPD working directory
 * @param playlist playlist name
 * @param timerange number of second since now
 * @param sort tag to sort the playlist: empty = no sorting, shuffle or mpd tag
 * @param sortdesc sort descending?
 * @return true on success else false
 */
bool smartpls_save_newest(sds workdir, const char *playlist, int timerange, const char *sort, bool sortdesc) {
    return smartpls_save(workdir, "newest", playlist, NULL, 0, timerange, sort, sortdesc);
}

/**
 * Saves a sticker based smart playlist
 * @param workdir myMPD working directory
 * @param playlist playlist name
 * @param expression mpd search expression
 * @param sort tag to sort the playlist: empty = no sorting, shuffle or mpd tag
 * @param sortdesc sort descending?
 * @return true on success else false
 */
bool smartpls_save_search(sds workdir, const char *playlist, const char *expression, const char *sort, bool sortdesc) {
    return smartpls_save(workdir, "search", playlist, expression, 0, 0, sort, sortdesc);
}

/**
 * Checks if playlist is a smart playlist
 * @param workdir myMPD working directory
 * @param playlist name of the playlist to check
 * @return true if it is a smart playlist else false
 */
bool is_smartpls(sds workdir, const char *playlist) {
    bool smartpls = false;
    if (strchr(playlist, '/') == NULL) {
        //filename only
        sds smartpls_file = sdscatfmt(sdsempty(), "%S/%s/%s", workdir, DIR_WORK_SMARTPLS, playlist);
        smartpls = testfile_read(smartpls_file);
        FREE_SDS(smartpls_file);
    }
    return smartpls;
}

/**
 * Returns the smart playlist last modification time
 * @param workdir myMPD working directory
 * @param playlist name of the playlist to check
 * @return last modification time
 */
time_t smartpls_get_mtime(sds workdir, const char *playlist) {
    sds plpath = sdscatfmt(sdsempty(), "%S/%s/%s", workdir, DIR_WORK_SMARTPLS, playlist);
    time_t mtime = get_mtime(plpath);
    FREE_SDS(plpath);
    return mtime;
}

/**
 * Creates the default myMPD smart playlists on first startup
 * @param workdir myMPD working directory
 * @return true on success else false
 */
bool smartpls_default(sds workdir) {
    //try to get prefix from state file, fallback to default value
    sds prefix = state_file_rw_string(workdir, DIR_WORK_STATE, "smartpls_prefix", MYMPD_SMARTPLS_PREFIX, vcb_isname, false);

    bool rc = true;
    sds playlist = sdscatfmt(sdsempty(), "%S-bestRated", prefix);
    rc = smartpls_save_sticker(workdir, playlist, "like", 200, 2, "", false);
    if (rc == true) {
        sdsclear(playlist);
        playlist = sdscatfmt(playlist, "%S-mostPlayed", prefix);
        rc = smartpls_save_sticker(workdir, playlist, "playCount", 200, 1, "", false);
    }
    if (rc == true) {
        sdsclear(playlist);
        playlist = sdscatfmt(playlist, "%S-newestSongs", prefix);
        rc = smartpls_save_newest(workdir, playlist, 604800, "", false);
    }
    FREE_SDS(playlist);
    FREE_SDS(prefix);
    return rc;
}

/**
 * Sends a request to the mympd_api_queue to update a smart playlist
 * @param playlist smart playlist to update
 * @return true on success else false
 */
bool smartpls_update(const char *playlist) {
    struct t_work_request *request = create_request(-1, 0, MYMPD_API_SMARTPLS_UPDATE, NULL, MPD_PARTITION_DEFAULT);
    request->data = tojson_char(request->data, "plist", playlist, false);
    request->data = jsonrpc_end(request->data);
    return mympd_queue_push(mympd_api_queue, request, 0);
}

/**
 * Sends a request to the mympd_api_queue to update all smart playlists
 * @return true on success else false
 */
bool smartpls_update_all(void) {
    struct t_work_request *request = create_request(-1, 0, MYMPD_API_SMARTPLS_UPDATE_ALL, NULL, MPD_PARTITION_DEFAULT);
    request->data = jsonrpc_end(request->data);
    return mympd_queue_push(mympd_api_queue, request, 0);
}

//privat functions

/**
 * Saves the smart playlist to disk.
 * @param workdir myMPD working directory
 * @param smartpltype type of the smart playlist: sticker, newest or search
 * @param playlist name of the smart playlist
 * @param expression mpd search expression
 * @param max_entries max entries for the playlist
 * @param timerange timerange for newest smart playlist type
 * @param sort mpd tag to sort or shuffle
 * @param sortdesc sort descending?
 * @return true on success else false
 */
static bool smartpls_save(sds workdir, const char *smartpltype, const char *playlist,
        const char *expression, int max_entries, int timerange, const char *sort, bool sortdesc)
{
    sds line = sdscatlen(sdsempty(), "{", 1);
    line = tojson_char(line, "type", smartpltype, true);
    if (strcmp(smartpltype, "sticker") == 0) {
        line = tojson_char(line, "sticker", expression, true);
        line = tojson_long(line, "maxentries", max_entries, true);
        line = tojson_long(line, "minvalue", timerange, true);
    }
    else if (strcmp(smartpltype, "newest") == 0) {
        line = tojson_long(line, "timerange", timerange, true);
    }
    else if (strcmp(smartpltype, "search") == 0) {
        line = tojson_char(line, "expression", expression, true);
    }
    line = tojson_char(line, "sort", sort, true);
    line = tojson_bool(line, "sortdesc", sortdesc, false);
    line = sdscatlen(line, "}", 1);

    sds pl_file = sdscatfmt(sdsempty(), "%S/smartpls/%s", workdir, playlist);
    bool rc = write_data_to_file(pl_file, line, sdslen(line));
    FREE_SDS(line);
    FREE_SDS(pl_file);
    return rc;
}
