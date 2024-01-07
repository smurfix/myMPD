/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"
#include "src/mympd_api/home.h"

#include "src/lib/filehandler.h"
#include "src/lib/jsonrpc.h"
#include "src/lib/log.h"
#include "src/lib/sds_extras.h"

#include <errno.h>

/**
 * Moves a home icon in the list
 * @param home_list pointer to home list
 * @param from from pos
 * @param to to pos
 * @return true on success, else false
 */
bool mympd_api_home_icon_move(struct t_list *home_list, long from, long to) {
    return list_move_item_pos(home_list, from, to);
}

/**
 * Deletes a home icon in the list
 * @param home_list pointer to home list
 * @param pos position to remove
 * @return true on success, else false
 */
bool mympd_api_home_icon_delete(struct t_list *home_list, long pos) {
    return list_remove_node(home_list, pos);
}

/**
 * Adds/replaces a home icon in the list
 * @param home_list pointer to home list
 * @param replace true to replace the icon at oldpos
 * @param oldpos original pos of the icon
 * @param name name
 * @param ligature ligature
 * @param bgcolor background color
 * @param color color
 * @param image image
 * @param cmd command
 * @param option_list options for the command
 * @return true on success, else false
 */
bool mympd_api_home_icon_save(struct t_list *home_list, bool replace, long oldpos,
    sds name, sds ligature, sds bgcolor, sds color, sds image, sds cmd, struct t_list *option_list)
{
    sds key = sdsnewlen("{", 1);
    key = tojson_sds(key, "name", name, true);
    key = tojson_sds(key, "ligature", ligature, true);
    key = tojson_sds(key, "bgcolor", bgcolor, true);
    key = tojson_sds(key, "color", color, true);
    key = tojson_sds(key, "image", image, true);
    key = tojson_sds(key, "cmd", cmd, true);
    key = sdscat(key, "\"options\":[");
    struct t_list_node *current = option_list->head;
    int i = 0;
    while (current != NULL) {
        if (i++) {
            key = sdscatlen(key, ",", 1);
        }
        key = sds_catjson(key, current->key, sdslen(current->key));
        current = current->next;
    }
    key = sdscatlen(key, "]}", 2);
    bool rc = false;
    if (replace == true) {
        rc = list_replace(home_list, oldpos, key, 0, NULL, NULL);
    }
    else {
        rc = list_push(home_list, key, 0, NULL, NULL);
    }
    FREE_SDS(key);
    return rc;
}

/**
 * Reads the home icons from the filesystem
 * @param home_list pointer to home list
 * @param workdir working directory
 * @return true on success, else false
 */
bool mympd_api_home_file_read(struct t_list *home_list, sds workdir) {
    sds home_file = sdscatfmt(sdsempty(), "%S/%s/%s", workdir, DIR_WORK_STATE, FILENAME_HOME);
    errno = 0;
    FILE *fp = fopen(home_file, OPEN_FLAGS_READ);
    int i = 0;
    if (fp == NULL) {
        //ignore error
        MYMPD_LOG_DEBUG(NULL, "Can not open file \"%s\"", home_file);
        if (errno != ENOENT) {
            MYMPD_LOG_ERRNO(NULL, errno);
        }
        FREE_SDS(home_file);
        return false;
    }

    sds line = sdsempty();
    while (sds_getline(&line, fp, LINE_LENGTH_MAX) >= 0) {
        if (validate_json_object(line) == false) {
            MYMPD_LOG_ERROR(NULL, "Invalid line");
            break;
        }
        list_push(home_list, line, 0, NULL, NULL);
        i++;
        if (i == LIST_HOME_ICONS_MAX) {
            MYMPD_LOG_WARN(NULL, "Too many lines in home_list");
            break;
        }
    }
    FREE_SDS(line);
    (void) fclose(fp);
    FREE_SDS(home_file);
    MYMPD_LOG_INFO(NULL, "Read %ld home icon(s) from disc", home_list->length);
    return true;
}

/**
 * Callback function for mympd_api_home_file_save
 * @param buffer buffer to append the line
 * @param current list node to print
 * @return pointer to buffer
 */
static sds homeicon_to_line_cb(sds buffer, struct t_list_node *current) {
    return sdscatfmt(buffer, "%S\n", current->key);
}

/**
 * Writes the home icons to the filesystem
 * @param home_list pointer to home list
 * @param workdir working directory
 * @return true on success, else false
 */
bool mympd_api_home_file_save(struct t_list *home_list, sds workdir) {
    MYMPD_LOG_INFO(NULL, "Saving home icons to disc");
    sds filepath = sdscatfmt(sdsempty(), "%S/%s/%s", workdir, DIR_WORK_STATE, FILENAME_HOME);
    bool rc = list_write_to_disk(filepath, home_list, homeicon_to_line_cb);
    FREE_SDS(filepath);
    return rc;
}

/**
 * Returns a jsonrpc response with all home icons
 * @param home_list pointer to home list
 * @param buffer buffer to append the response
 * @param request_id jsonrpc request id
 * @return pointer to buffer
 */
sds mympd_api_home_icon_list(struct t_list *home_list, sds buffer, long request_id) {
    enum mympd_cmd_ids cmd_id = MYMPD_API_HOME_ICON_LIST;
    buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
    buffer = sdscat(buffer, "\"data\":[");
    int returned_entities = 0;
    struct t_list_node *current = home_list->head;
    while (current != NULL) {
        if (returned_entities++) {
            buffer = sdscatlen(buffer, ",", 1);
        }
        buffer = sdscat(buffer, current->key);
        current = current->next;
    }
    buffer = sdscatlen(buffer, "],", 2);
    buffer = tojson_long(buffer, "returnedEntities", returned_entities, false);
    buffer = jsonrpc_end(buffer);
    return buffer;
}

/**
 * Returns a jsonrpc response with the home icon details
 * @param home_list pointer to home list
 * @param buffer buffer to append the response
 * @param request_id jsonrpc request id
 * @param pos position of the home icon to get
 * @return pointer to buffer
 */
sds mympd_api_home_icon_get(struct t_list *home_list, sds buffer, long request_id, long pos) {
    enum mympd_cmd_ids cmd_id = MYMPD_API_HOME_ICON_GET;
    struct t_list_node *current = list_node_at(home_list, pos);
    if (current != NULL) {
        buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
        buffer = sdscat(buffer, "\"data\":");
        buffer = sdscat(buffer, current->key);
        buffer = sdscatlen(buffer, ",", 1);
        buffer = tojson_long(buffer, "returnedEntities", 1, false);
        buffer = jsonrpc_end(buffer);
        return buffer;
    }

    MYMPD_LOG_ERROR(NULL, "Can not get home icon at pos %ld", pos);
    buffer = jsonrpc_respond_message(buffer, cmd_id, request_id,
        JSONRPC_FACILITY_HOME, JSONRPC_SEVERITY_ERROR, "Can not get home icon");
    return buffer;
}
