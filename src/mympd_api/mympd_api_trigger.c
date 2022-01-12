/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"
#include "mympd_api_trigger.h"

#include "../lib/api.h"
#include "../lib/jsonrpc.h"
#include "../lib/log.h"
#include "../lib/mem.h"
#include "../lib/mympd_configuration.h"
#include "../lib/sds_extras.h"

#include <errno.h>
#include <stdlib.h>

//private definitions
void _trigger_execute(const char *script, struct t_list *arguments);

static const char *const mpd_trigger_names[] = {
    "mpd_database",
    "mpd_stored_playlist",
    "mpd_playlist",
    "mpd_player",
    "mpd_mixer",
    "mpd_output",
    "mpd_options",
    "mpd_update",
    "mpd_sticker",
    "mpd_subscription",
    "mpd_message",
    "mpd_partition",
    "mpd_neighbor",
    "mpd_mount",
    NULL
};

static const char *const mympd_trigger_names[] = {
    "mympd_scrobble",
    "mympd_start",
    "mympd_stop",
    "mympd_connected",
    "mympd_disconnected",
    NULL
};

//public functions
const char *mympd_api_trigger_name(long event) {
    if (event < 0) {
        for (int i = 0; mympd_trigger_names[i] != NULL; ++i) {
            if (event == (-1 - i)) {
                return mympd_trigger_names[i];
            }
        }
        return NULL;
    }
    for (int i = 0; mpd_trigger_names[i] != NULL; ++i) {
        if (event == (1 << i)) {
            return mpd_trigger_names[i];
        }
    }
    return NULL;
}

sds mympd_api_trigger_print_trigger_list(sds buffer) {
    for (int i = 0; mympd_trigger_names[i] != NULL; ++i) {
        buffer = tojson_long(buffer, mympd_trigger_names[i], (-1 -i), true);
    }

    for (int i = 0; mpd_trigger_names[i] != NULL; ++i) {
        if (i > 0) {
            buffer = sdscatlen(buffer, ",", 1);
        }
        buffer = tojson_long(buffer, mpd_trigger_names[i], (1 << i), false);
    }
    return buffer;
}

void mympd_api_trigger_execute(struct t_mympd_state *mympd_state, enum trigger_events event) {
    MYMPD_LOG_DEBUG("Trigger event: %s (%d)", mympd_api_trigger_name(event), event);
    struct t_list_node *current = mympd_state->triggers.head;
    while (current != NULL) {
        if (current->value_i == event) {
            MYMPD_LOG_NOTICE("Executing script %s for trigger %s (%d)", current->value_p,
                mympd_api_trigger_name(event), event);
            _trigger_execute(current->value_p, (struct t_list *)current->user_data);
        }
        current = current->next;
    }
}

sds mympd_api_trigger_list(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id) {
    buffer = jsonrpc_result_start(buffer, method, request_id);
    buffer = sdscat(buffer, "\"data\":[");
    int entities_returned = 0;
    struct t_list_node *current = mympd_state->triggers.head;
    int j = 0;
    while (current != NULL) {
        if (entities_returned++) {
            buffer = sdscatlen(buffer, ",", 1);
        }
        buffer = sdscatlen(buffer, "{", 1);
        buffer = tojson_long(buffer, "id", j, true);
        buffer = tojson_char(buffer, "name", current->key, true);
        buffer = tojson_long(buffer, "event", current->value_i, true);
        buffer = tojson_char(buffer, "eventName", mympd_api_trigger_name(current->value_i), true);
        buffer = tojson_char(buffer, "script", current->value_p, true);
        buffer = sdscat(buffer, "\"arguments\": {");
        struct t_list *arguments = ( struct t_list *)current->user_data;
        struct t_list_node *argument = arguments->head;
        int i = 0;
        while (argument != NULL) {
            if (i++) {
                buffer = sdscatlen(buffer, ",", 1);
            }
            buffer = tojson_char(buffer, argument->key, argument->value_p, false);
            argument = argument->next;
        }
        buffer = sdscatlen(buffer, "}}", 2);
        current = current->next;
        j++;
    }

    buffer = sdscatlen(buffer, "],", 2);
    buffer = tojson_long(buffer, "returnedEntities", entities_returned, false);
    buffer = jsonrpc_result_end(buffer);
    return buffer;
}

sds mympd_api_trigger_get(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id, long id) {
    struct t_list_node *current = list_node_at(&mympd_state->triggers, id);
    if (current != NULL) {
        buffer = jsonrpc_result_start(buffer, method, request_id);
        buffer = tojson_long(buffer, "id", id, true);
        buffer = tojson_char(buffer, "name", current->key, true);
        buffer = tojson_long(buffer, "event", current->value_i, true);
        buffer = tojson_char(buffer, "script", current->value_p, true);
        buffer = sdscat(buffer, "\"arguments\": {");
        struct t_list *arguments = (struct t_list *)current->user_data;
        struct t_list_node *argument = arguments->head;
        int i = 0;
        while (argument != NULL) {
            if (i++) {
                buffer = sdscatlen(buffer, ",", 1);
            }
            buffer = tojson_char(buffer, argument->key, argument->value_p, false);
            argument = argument->next;
        }
        buffer = sdscatlen(buffer, "}", 1);
        buffer = jsonrpc_result_end(buffer);
    }
    else {
        buffer = jsonrpc_respond_message(buffer, method, request_id, false, "trigger", "warn", "Trigger not found");
    }

    return buffer;
}

bool mympd_api_trigger_delete(struct t_mympd_state *mympd_state, long idx) {
    struct t_list_node *toremove = list_node_at(&mympd_state->triggers, idx);
    if (toremove != NULL) {
        list_clear((struct t_list *)toremove->user_data);
        return list_shift(&mympd_state->triggers, idx);
    }
    return false;
}

void mympd_api_trigerlist_free_arguments(struct t_mympd_state *mympd_state) {
    struct t_list_node *current = mympd_state->triggers.head;
    while (current != NULL) {
        list_clear((struct t_list *)current->user_data);
        current = current->next;
    }
}

bool mympd_api_trigger_file_read(struct t_mympd_state *mympd_state) {
    sds trigger_file = sdscatfmt(sdsempty(), "%s/state/trigger_list", mympd_state->config->workdir);
    errno = 0;
    FILE *fp = fopen(trigger_file, OPEN_FLAGS_READ);
    if (fp == NULL) {
        MYMPD_LOG_DEBUG("Can not open file \"%s\"", trigger_file);
        if (errno != ENOENT) {
            MYMPD_LOG_ERRNO(errno);
        }
        FREE_SDS(trigger_file);
        return false;
    }
    int i = 0;
    sds line = sdsempty();
    while (sds_getline(&line, fp, 1000) == 0) {
        if (i > LIST_TRIGGER_MAX) {
            MYMPD_LOG_WARN("Too many triggers defined");
            break;
        }
        if (validate_json(line) == false) {
            MYMPD_LOG_ERROR("Invalid line");
            break;
        }
        sds name = NULL;
        sds script = NULL;
        int event;
        struct t_list *arguments = list_new();
        if (json_get_string(line, "$.name", 1, 200, &name, vcb_isfilename, NULL) == true &&
            json_get_string(line, "$.script", 0, 200, &script, vcb_isfilename, NULL) == true &&
            json_get_int_max(line, "$.event", &event, NULL) == true &&
            json_get_object_string(line, "$.arguments", arguments, vcb_isname, 10, NULL))
        {
            list_push(&mympd_state->triggers, name, event, script, arguments);
        }
        else {
            list_clear(arguments);
            FREE_PTR(arguments);
        }
        FREE_SDS(name);
        FREE_SDS(script);
        i++;
    }
    FREE_SDS(line);
    fclose(fp);
    MYMPD_LOG_INFO("Read %d triggers(s) from disc", mympd_state->triggers.length);
    FREE_SDS(trigger_file);
    return true;
}

bool mympd_api_trigger_file_save(struct t_mympd_state *mympd_state) {
    MYMPD_LOG_INFO("Saving triggers to disc");
    sds tmp_file = sdscatfmt(sdsempty(), "%s/state/trigger_list.XXXXXX", mympd_state->config->workdir);
    errno = 0;
    int fd = mkstemp(tmp_file);
    if (fd < 0) {
        MYMPD_LOG_ERROR("Can not open file \"%s\" for write", tmp_file);
        MYMPD_LOG_ERRNO(errno);
        FREE_SDS(tmp_file);
        return false;
    }
    FILE *fp = fdopen(fd, "w");
    struct t_list_node *current = mympd_state->triggers.head;
    sds buffer = sdsempty();
    while (current != NULL) {
        buffer = sds_replace(buffer, "{");
        buffer = tojson_char(buffer, "name", current->key, true);
        buffer = tojson_long(buffer, "event", current->value_i, true);
        buffer = tojson_char(buffer, "script", current->value_p, true);
        buffer = sdscat(buffer, "\"arguments\":{");
        struct t_list *arguments = (struct t_list *)current->user_data;
        struct t_list_node *argument = arguments->head;
        int i = 0;
        while (argument != NULL) {
            if (i++) {
                buffer = sdscatlen(buffer, ",", 1);
            }
            buffer = tojson_char(buffer, argument->key, argument->value_p, false);
            argument = argument->next;
        }
        buffer = sdscatlen(buffer, "}}\n", 3);
        fputs(buffer, fp);
        current = current->next;
    }
    fclose(fp);
    FREE_SDS(buffer);
    sds trigger_file = sdscatfmt(sdsempty(), "%s/state/trigger_list", mympd_state->config->workdir);
    errno = 0;
    if (rename(tmp_file, trigger_file) == -1) {
        MYMPD_LOG_ERROR("Renaming file from %s to %s failed", tmp_file, trigger_file);
        MYMPD_LOG_ERRNO(errno);
        FREE_SDS(tmp_file);
        FREE_SDS(trigger_file);
        return false;
    }
    FREE_SDS(tmp_file);
    FREE_SDS(trigger_file);
    return true;
}

//private functions
void _trigger_execute(const char *script, struct t_list *arguments) {
    struct t_work_request *request = create_request(-1, 0, MYMPD_API_SCRIPT_EXECUTE, NULL);
    request->data = tojson_char(request->data, "script", script, true);
    request->data = sdscat(request->data, "\"arguments\": {");
    struct t_list_node *argument = arguments->head;
    int i = 0;
    while (argument != NULL) {
        if (i++) {
            request->data = sdscatlen(request->data, ",", 1);
        }
        request->data = tojson_char(request->data, argument->key, argument->value_p, false);
        argument = argument->next;
    }
    request->data = sdscatlen(request->data, "}}}", 3);
    mympd_queue_push(mympd_api_queue, request, 0);
}
