/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"
#include "web_server_webradiodb.h"

#include "../lib/jsonrpc.h"
#include "../lib/log.h"
#include "../lib/sds_extras.h"
#include "../lib/utility.h"
#include "web_server_proxy.h"
#include "web_server_utility.h"

//private definitions
static bool webradiodb_send(struct mg_connection *nc, struct mg_connection *backend_nc,
        enum mympd_cmd_ids cmd_id, const char *request);
static void webradiodb_handler(struct mg_connection *nc, int ev, void *ev_data, void *fn_data);
static sds webradiodb_cache_check(sds cachedir, const char *cache_file);
static bool webradiodb_cache_write(sds cachedir, const char *cache_file, const char *data, size_t data_len);

//public functions

void webradiodb_api(struct mg_connection *nc, struct mg_connection *backend_nc,
    enum mympd_cmd_ids cmd_id, sds body, int id)
{
    (void) body;
    sds error = sdsempty();
    sds uri = sdsempty();
    const char *cmd = get_cmd_id_method_name(cmd_id);
    sds data = NULL;
    struct t_mg_user_data *mg_user_data = (struct t_mg_user_data *) nc->mgr->userdata;
    struct t_config *config = mg_user_data->config;

    switch(cmd_id) {
        case MYMPD_API_CLOUD_WEBRADIODB_COMBINED_GET:
            data = webradiodb_cache_check(config->cachedir, "webradiodb-combined.min.json");
            uri = sdscat(uri, "/webradiodb/db/index/webradiodb-combined.min.json");
            break;
        default:
            error = sdscat(error, "Invalid API request");
    }

    if (sdslen(error) > 0) {
        sds response = jsonrpc_respond_message(sdsempty(), cmd, id, true,
            "general", "error", error);
        MYMPD_LOG_ERROR("Error processing method \"%s\"", cmd);
        webserver_send_data(nc, response, sdslen(response), "Content-Type: application/json\r\n");
        FREE_SDS(response);
    }
    else if (data != NULL) {
        sds result = sdsempty();
        result = jsonrpc_result_start(result, cmd, 0);
        result = sdscat(result, "\"data\":");
        result = sdscatsds(result, data);
        result = jsonrpc_result_end(result);
        webserver_send_data(nc, result, sdslen(result), "Content-Type: application/json\r\n");
        sdsfree(result);
    }
    else {
        bool rc = webradiodb_send(nc, backend_nc, cmd_id, uri);
        if (rc == false) {
            sds response = jsonrpc_respond_message(sdsempty(), cmd, id, true,
                "general", "error", "Error connection to radio-browser.info");
            webserver_send_data(nc, response, sdslen(response), "Content-Type: application/json\r\n");
            FREE_SDS(response);
        }
    }
    FREE_SDS(error);
    FREE_SDS(uri);
    FREE_SDS(data);
}

//private functions

static sds webradiodb_cache_check(sds cachedir, const char *cache_file) {
    sds filepath = sdscatfmt(sdsempty(), "%S/webradiodb/%s", cachedir, cache_file);
    struct stat status;
    if (stat(filepath, &status) == 0) {
        //cache it one day
        time_t expire_time = time(NULL) - (long)(24 * 60 * 60);
        if (status.st_mtime < expire_time) {
            MYMPD_LOG_DEBUG("Cache outdated file \"%s\": %ld", filepath, status.st_mtime);
            errno = 0;
            if (unlink(filepath) != 0) {
                MYMPD_LOG_ERROR("Error removing file \"%s\"", filepath);
                MYMPD_LOG_ERRNO(errno);
            }
        }
        else {
            FILE *fp = fopen(filepath, OPEN_FLAGS_READ);
            if (fp == NULL) {
                MYMPD_LOG_ERROR("Cant open cache file \"%s\"", filepath);
                FREE_SDS(filepath);
                return NULL;
            }
            sds data = sdsempty();
            sds_getfile(&data, fp, 1000000);
            fclose(fp);
            MYMPD_LOG_DEBUG("Found cached file \"%s\"", filepath);
            sdsfree(filepath);
            return data;
        }
    }
    sdsfree(filepath);
    return NULL;
}

static bool webradiodb_cache_write(sds cachedir, const char *cache_file, const char *data, size_t data_len) {
    sds filepath = sdscatfmt(sdsempty(), "%S/webradiodb/%s", cachedir, cache_file);
    bool rc = write_data_to_file(filepath, data, data_len);
    sdsfree(filepath);
    return rc;
}

static bool webradiodb_send(struct mg_connection *nc, struct mg_connection *backend_nc,
        enum mympd_cmd_ids cmd_id, const char *request)
{
    const char *host = WEBRADIODB_HOST;
    sds uri = sdscatfmt(sdsempty(), "https://%s%s", host, request);
    backend_nc = create_http_backend_connection(nc, backend_nc, uri, webradiodb_handler);
    sdsfree(uri);
    if (backend_nc != NULL) {
        struct backend_nc_data_t *backend_nc_data = (struct backend_nc_data_t *)backend_nc->fn_data;
        backend_nc_data->cmd_id = cmd_id;
        return true;
    }
    return false;
}

static void webradiodb_handler(struct mg_connection *nc, int ev, void *ev_data, void *fn_data) {
    struct t_mg_user_data *mg_user_data = (struct t_mg_user_data *) nc->mgr->userdata;
    struct backend_nc_data_t *backend_nc_data = (struct backend_nc_data_t *)fn_data;
    struct t_config *config = mg_user_data->config;
    switch(ev) {
        case MG_EV_CONNECT: {
            MYMPD_LOG_INFO("Backend HTTP connection \"%lu\" connected", nc->id);
            struct mg_str host = mg_url_host(backend_nc_data->uri);
            struct mg_tls_opts tls_opts = {
                .srvname = host
            };
            mg_tls_init(nc, &tls_opts);
            mg_printf(nc, "GET %s HTTP/1.1\r\n"
                "Host: %.*s\r\n"
                "User-Agent: myMPD/%s\r\n"
                "Connection: close\r\n"
                "\r\n",
                mg_url_uri(backend_nc_data->uri),
                host.len, host.ptr,
                MYMPD_VERSION
            );
            mg_user_data->connection_count++;
            break;
        }
        case MG_EV_ERROR:
            MYMPD_LOG_ERROR("HTTP connection \"%lu\" failed", nc->id);
            break;
        case MG_EV_HTTP_MSG: {
            struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            MYMPD_LOG_DEBUG("Got response from connection \"%lu\": %d bytes", nc->id, hm->body.len);
            const char *cmd = get_cmd_id_method_name(backend_nc_data->cmd_id);
            sds result = sdsempty();
            if (hm->body.len > 0) {
                result = jsonrpc_result_start(result, cmd, 0);
                result = sdscat(result, "\"data\":");
                result = sdscatlen(result, hm->body.ptr, hm->body.len);
                result = jsonrpc_result_end(result);
            }
            else {
                result = jsonrpc_respond_message(result, cmd, 0, true,
                    "database", "error", "Empty response from webradiodb backend");
            }
            webserver_send_data(backend_nc_data->frontend_nc, result, sdslen(result), "Content-Type: application/json\r\n");
            sdsfree(result);
            if (backend_nc_data->cmd_id == MYMPD_API_CLOUD_WEBRADIODB_COMBINED_GET) {
                webradiodb_cache_write(config->cachedir, "webradiodb-combined.min.json", hm->body.ptr, hm->body.len);
            }
            break;
        }
        case MG_EV_CLOSE: {
            //print end of jsonrpc message
            MYMPD_LOG_INFO("Backend HTTP connection \"%lu\" closed", nc->id);
            mg_user_data->connection_count--;
            if (backend_nc_data->frontend_nc != NULL) {
                //remove backend connection pointer from frontend connection
                backend_nc_data->frontend_nc->fn_data = NULL;
                //close frontend connection
                backend_nc_data->frontend_nc->is_draining = 1;
                //free backend_nc_data
                free_backend_nc_data(backend_nc_data);
                free(fn_data);
            }
            break;
        }
    }
}
