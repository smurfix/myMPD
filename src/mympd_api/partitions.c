/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"
#include "src/mympd_api/partitions.h"

#include "src/lib/api.h"
#include "src/lib/filehandler.h"
#include "src/lib/jsonrpc.h"
#include "src/lib/mympd_state.h"
#include "src/lib/sds_extras.h"
#include "src/lib/utility.h"
#include "src/mpd_client/connection.h"
#include "src/mpd_client/errorhandler.h"
#include "src/mpd_client/partitions.h"

/**
 * Lists partitions
 * @param mympd_state pointer to central myMPD state
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc request id
 * @return pointer to buffer
 */
sds mympd_api_partition_list(struct t_mympd_state *mympd_state, sds buffer, long request_id) {
    enum mympd_cmd_ids cmd_id = MYMPD_API_PARTITION_LIST;
    struct t_partition_state *partition_state = mympd_state->partition_state;
    buffer = jsonrpc_respond_start(buffer, cmd_id, request_id);
    buffer = sdscat(buffer, "\"data\":[");
    long entity_count = 0;
    while (partition_state != NULL) {
        if (entity_count++) {
            buffer = sdscatlen(buffer, ",", 1);
        }
        buffer = sdscatlen(buffer, "{", 1);
        buffer = tojson_char(buffer, "name", partition_state->name, true);
        buffer = tojson_char(buffer, "highlightColor", partition_state->highlight_color, false);
        buffer = sdscatlen(buffer, "}", 1);
        partition_state = partition_state->next;
    }

    buffer = sdscatlen(buffer, "],", 2);
    buffer = tojson_long(buffer, "totalEntities", entity_count, true);
    buffer = tojson_long(buffer, "returnedEntities", entity_count, false);
    buffer = jsonrpc_end(buffer);
    return buffer;
}

/**
 * Disconnects and removes a partition.
 * Assigned outputs are moved to the default partition: https://github.com/MusicPlayerDaemon/MPD/discussions/1611
 * @param partition_state pointer to partition state for default partition
 * @param buffer already allocated sds string to append the response
 * @param request_id jsonrpc request id
 * @param partition partition to remove
 * @return pointer to buffer
 */
sds mympd_api_partition_rm(struct t_partition_state *partition_state, sds buffer, long request_id, sds partition) {
    enum mympd_cmd_ids cmd_id = MYMPD_API_PARTITION_RM;
    struct t_partition_state *partition_to_remove = partitions_get_by_name(partition_state->mympd_state, partition);
    if (partition_to_remove == NULL) {
        buffer = jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_MPD, JSONRPC_SEVERITY_ERROR, "Partition not found");
        return buffer;
    }
    if (partition_to_remove->conn_state != MPD_CONNECTED) {
        buffer = jsonrpc_respond_message(buffer, cmd_id, request_id, JSONRPC_FACILITY_MPD, JSONRPC_SEVERITY_ERROR, "Partition must be connected");
        return buffer;
    }
    //get assigned outputs
    struct t_list outputs;
    list_init(&outputs);
    bool rc = mpd_send_noidle(partition_to_remove->conn);
    mpd_response_finish(partition_to_remove->conn);
    if (mympd_check_rc_error_and_recover_respond(partition_to_remove, &buffer, cmd_id, request_id, rc, "mpd_send_noidle") == false) {
        return buffer;
    }
    rc = mpd_send_outputs(partition_to_remove->conn);
    if (mympd_check_rc_error_and_recover_respond(partition_to_remove, &buffer, cmd_id, request_id, rc, "mpd_send_outputs") == false) {
        return buffer;
    }
    struct mpd_output *output;
    while ((output = mpd_recv_output(partition_to_remove->conn)) != NULL) {
        list_push(&outputs, mpd_output_get_name(output), 0, NULL, NULL);
        mpd_output_free(output);
    }
    mpd_response_finish(partition_to_remove->conn);
    if (mympd_check_error_and_recover_respond(partition_to_remove, &buffer, cmd_id, request_id) == false) {
        return buffer;
    }
    //disconnect partition
    mpd_client_disconnect(partition_to_remove, MPD_DISCONNECTED);
    //move outputs
    struct t_list_node *current = outputs.head;
    while (current != NULL) {
        rc = mpd_run_move_output(partition_state->conn, current->key);
        if (mympd_check_rc_error_and_recover_respond(partition_state, &buffer, cmd_id, request_id, rc, "mpd_run_move_output") == false) {
            list_clear(&outputs);
            return buffer;
        }
        current = current->next;
    }
    list_clear(&outputs);
    //wait
    my_msleep(100);
    //delete the partition
    rc = mpd_run_delete_partition(partition_state->conn, partition);
    bool result = false;
    buffer = mympd_respond_with_error_or_ok(partition_state, buffer, cmd_id, request_id, rc, "mpd_run_delete_partition", &result);
    if (result == true) {
        //partition was removed
        partition_to_remove->conn_state = MPD_REMOVED;
        sds dirpath = sdscatfmt(sdsempty(),"%S/state/%S",partition_state->mympd_state->config->workdir, partition);
        clean_rm_directory(dirpath);
        FREE_SDS(dirpath);
    }
    return buffer;
}
