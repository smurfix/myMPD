/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

/*! \file
 * \brief Jsonrpc implementation
 */

#ifndef MYMPD_JSONRPC_H
#define MYMPD_JSONRPC_H

#include "dist/sds/sds.h"
#include "src/lib/api.h"
#include "src/lib/fields.h"
#include "src/lib/list.h"
#include "src/lib/validate.h"

#include <stdbool.h>

/**
 * Response types for error messages
 */
enum jsonrpc_response_types {
    RESPONSE_TYPE_JSONRPC_RESPONSE = 0,
    RESPONSE_TYPE_JSONRPC_NOTIFY,
    RESPONSE_TYPE_PLAIN,
    RESPONSE_TYPE_NONE
};

/**
 * Jsonrpc severities
 */
enum jsonrpc_severities {
    JSONRPC_SEVERITY_INFO = 0,
    JSONRPC_SEVERITY_WARN,
    JSONRPC_SEVERITY_ERROR,
    JSONRPC_SEVERITY_MAX
};

/**
 * Jsonrpc facilities
 */
enum jsonrpc_facilities {
    JSONRPC_FACILITY_DATABASE = 0,
    JSONRPC_FACILITY_GENERAL,
    JSONRPC_FACILITY_HOME,
    JSONRPC_FACILITY_JUKEBOX,
    JSONRPC_FACILITY_LYRICS,
    JSONRPC_FACILITY_MPD,
    JSONRPC_FACILITY_PLAYLIST,
    JSONRPC_FACILITY_PLAYER,
    JSONRPC_FACILITY_QUEUE,
    JSONRPC_FACILITY_SESSION,
    JSONRPC_FACILITY_SCRIPT,
    JSONRPC_FACILITY_STICKER,
    JSONRPC_FACILITY_TIMER,
    JSONRPC_FACILITY_TRIGGER,
    JSONRPC_FACILITY_MAX
};

/**
 * Jsonrpc events
 */
enum jsonrpc_events {
    JSONRPC_EVENT_MPD_CONNECTED = 0,
    JSONRPC_EVENT_MPD_DISCONNECTED,
    JSONRPC_EVENT_NOTIFY,
    JSONRPC_EVENT_UPDATE_DATABASE,
    JSONRPC_EVENT_UPDATE_FINISHED,
    JSONRPC_EVENT_UPDATE_HOME,
    JSONRPC_EVENT_UPDATE_JUKEBOX,
    JSONRPC_EVENT_UPDATE_LAST_PLAYED,
    JSONRPC_EVENT_UPDATE_OPTIONS,
    JSONRPC_EVENT_UPDATE_OUTPUTS,
    JSONRPC_EVENT_UPDATE_QUEUE,
    JSONRPC_EVENT_UPDATE_STARTED,
    JSONRPC_EVENT_UPDATE_STATE,
    JSONRPC_EVENT_UPDATE_STORED_PLAYLIST,
    JSONRPC_EVENT_UPDATE_VOLUME,
    JSONRPC_EVENT_WELCOME,
    JSONRPC_EVENT_UPDATE_CACHE_STARTED,
    JSONRPC_EVENT_UPDATE_CACHE_FINISHED,
    JSONRPC_EVENT_MAX
};

/**
 * Struct that holds the validation errors for the json_get_* functions
 */
struct t_jsonrpc_parse_error {
    sds message;  //!< the error message
    sds path;     //!< the json path of the invalid value
};

/**
 * Iteration callback definition
 */
typedef bool (*iterate_callback) (const char *, sds, sds, int, validate_callback, void *, struct t_jsonrpc_parse_error *);

void send_jsonrpc_notify(enum jsonrpc_facilities facility, enum jsonrpc_severities severity, const char *partition, const char *message);
void send_jsonrpc_notify_client(enum jsonrpc_facilities facility, enum jsonrpc_severities severity, unsigned request_id, const char *message);
void send_jsonrpc_event(enum jsonrpc_events event, const char *partition);
sds jsonrpc_event(sds buffer, enum jsonrpc_events event);
sds jsonrpc_notify(sds buffer, enum jsonrpc_facilities facility, enum jsonrpc_severities severity, const char *message);
sds jsonrpc_notify_phrase(sds buffer, enum jsonrpc_facilities facility, enum jsonrpc_severities severity, const char *message, int count, ...);
sds jsonrpc_notify_start(sds buffer, enum jsonrpc_events event);

sds jsonrpc_respond_start(sds buffer, enum mympd_cmd_ids cmd_id, unsigned request_id);
sds jsonrpc_end(sds buffer);
sds jsonrpc_respond_ok(sds buffer, enum mympd_cmd_ids cmd_id, unsigned request_id, enum jsonrpc_facilities facility);
sds jsonrpc_respond_with_ok_or_error(sds buffer, enum mympd_cmd_ids cmd_id, unsigned request_id,
        bool rc, enum jsonrpc_facilities facility, const char *error);
sds jsonrpc_respond_with_message_or_error(sds buffer, enum mympd_cmd_ids cmd_id, unsigned request_id,
        bool rc, enum jsonrpc_facilities facility, const char *message, const char *error);
sds jsonrpc_respond_message(sds buffer, enum mympd_cmd_ids cmd_id, unsigned request_id,
        enum jsonrpc_facilities facility, enum jsonrpc_severities severity, const char *message);
sds jsonrpc_respond_message_phrase(sds buffer, enum mympd_cmd_ids cmd_id, unsigned request_id,
        enum jsonrpc_facilities facility, enum jsonrpc_severities severity, const char *message, int count, ...);

sds json_comma(sds buffer);
sds tojson_raw(sds buffer, const char *key, const char *value, bool comma);
sds tojson_sds(sds buffer, const char *key, sds value, bool comma);
sds tojson_char(sds buffer, const char *key, const char *value, bool comma);
sds tojson_char_len(sds buffer, const char *key, const char *value, size_t len, bool comma);
sds tojson_bool(sds buffer, const char *key, bool value, bool comma);
sds tojson_int(sds buffer, const char *key, int value, bool comma);
sds tojson_uint(sds buffer, const char *key, unsigned value, bool comma);
sds tojson_time(sds buffer, const char *key, time_t value, bool comma);
sds tojson_float(sds buffer, const char *key, float value, bool comma);
sds tojson_int64(sds buffer, const char *key, int64_t value, bool comma);
sds tojson_uint64(sds buffer, const char *key, uint64_t value, bool comma);

void jsonrpc_parse_error_init(struct t_jsonrpc_parse_error *parse_error);
void jsonrpc_parse_error_clear(struct t_jsonrpc_parse_error *parse_error);

bool json_get_bool(sds s, const char *path, bool *result, struct t_jsonrpc_parse_error *error);
bool json_get_int_max(sds s, const char *path, int *result, struct t_jsonrpc_parse_error *error);
bool json_get_int(sds s, const char *path, int min, int max, int *result, struct t_jsonrpc_parse_error *error);
bool json_get_time_max(sds s, const char *path, time_t *result, struct t_jsonrpc_parse_error *error);
bool json_get_int64_max(sds s, const char *path, int64_t *result, struct t_jsonrpc_parse_error *error);
bool json_get_int64(sds s, const char *path, int64_t min, int64_t max, int64_t *result, struct t_jsonrpc_parse_error *error);
bool json_get_uint_max(sds s, const char *path, unsigned *result, struct t_jsonrpc_parse_error *error);
bool json_get_uint(sds s, const char *path, unsigned min, unsigned max, unsigned *result, struct t_jsonrpc_parse_error *error);
bool json_get_string_max(sds s, const char *path, sds *result, validate_callback vcb, struct t_jsonrpc_parse_error *error);
bool json_get_string(sds s, const char *path, size_t min, size_t max, sds *result, validate_callback vcb, struct t_jsonrpc_parse_error *error);
bool json_get_string_cmp(sds s, const char *path, size_t min, size_t max, const char *cmp, sds *result, struct t_jsonrpc_parse_error *error);
bool json_get_array_string(sds s, const char *path, struct t_list *l, validate_callback vcb, int max_elements, struct t_jsonrpc_parse_error *error);
bool json_get_array_int64(sds s, const char *path, struct t_list *l, int max_elements, struct t_jsonrpc_parse_error *error);
bool json_get_object_string(sds s, const char *path, struct t_list *l, validate_callback vcb_key,
    validate_callback vcb_value, int max_elements, struct t_jsonrpc_parse_error *error);
bool json_iterate_object(sds s, const char *path, iterate_callback icb, void *icb_userdata,
    validate_callback vcb_key, validate_callback vcb_value, int max_elements, struct t_jsonrpc_parse_error *error);
bool json_get_fields(sds s, const char *path, struct t_fields *tags, int max_elements, struct t_jsonrpc_parse_error *error);
bool json_get_tag_values(sds s, const char *path, struct mpd_song *song, validate_callback vcb, int max_elements, struct t_jsonrpc_parse_error *error);

bool json_find_key(sds s, const char *path);
sds json_get_key_as_sds(sds s, const char *path);

const char *get_mjson_toktype_name(int vtype);
sds list_to_json_array(sds s, struct t_list *l);
bool json_get_fields_as_string(sds s, sds *fields, struct t_jsonrpc_parse_error *error);

#endif
