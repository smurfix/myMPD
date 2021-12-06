/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"
#include "web_server.h"

#include "lib/api.h"
#include "lib/http_client.h"
#include "lib/jsonrpc.h"
#include "lib/log.h"
#include "lib/mem.h"
#include "lib/mympd_pin.h"
#include "lib/sds_extras.h"
#include "lib/validate.h"
#include "web_server/web_server_albumart.h"
#include "web_server/web_server_sessions.h"
#include "web_server/web_server_tagart.h"

#include <sys/prctl.h>

//private definitions
static bool parse_internal_message(struct t_work_result *response, struct t_mg_user_data *mg_user_data);
static void ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *fn_data);
#ifdef ENABLE_SSL
    static void ev_handler_redirect(struct mg_connection *nc_http, int ev, void *ev_data, void *fn_data);
#endif
static void send_ws_notify(struct mg_mgr *mgr, struct t_work_result *response);
static void send_api_response(struct mg_mgr *mgr, struct t_work_result *response);
static bool handle_api(struct mg_connection *nc, sds body, struct mg_str *auth_header, struct t_mg_user_data *mg_user_data);
static bool handle_script_api(long long conn_id, sds body);
static void mpd_stream_proxy_forward(struct mg_http_message *hm, struct mg_connection *nc);
static void mpd_stream_proxy_ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *fn_data);

//public functions
bool web_server_init(void *arg_mgr, struct t_config *config, struct t_mg_user_data *mg_user_data) {
    struct mg_mgr *mgr = (struct mg_mgr *) arg_mgr;

    //initialize mgr user_data, malloced in main.c
    mg_user_data->config = config;
    mg_user_data->browse_document_root = sdscatfmt(sdsempty(), "%s/empty", config->workdir);
    mg_user_data->pics_document_root = sdscatfmt(sdsempty(), "%s/pics", config->workdir);
    mg_user_data->smartpls_document_root = sdscatfmt(sdsempty(), "%s/smartpls", config->workdir);
    mg_user_data->music_directory = sdsempty();
    mg_user_data->playlist_directory = sdsempty();
    sds default_coverimagename = sdsnew("cover,folder");
    mg_user_data->coverimage_names= webserver_split_coverimage_names(default_coverimagename, mg_user_data->coverimage_names, &mg_user_data->coverimage_names_len);
    FREE_SDS(default_coverimagename);
    mg_user_data->feat_library = false;
    mg_user_data->feat_mpd_albumart = false;
    mg_user_data->connection_count = 0;
    mg_user_data->stream_uri = sdsnew("http://localhost:8000");
    mg_user_data->covercache = true;
    list_init(&mg_user_data->session_list);

    //init monogoose mgr
    mg_mgr_init(mgr);
    mgr->userdata = mg_user_data;
    mgr->product_name = "myMPD "MYMPD_VERSION;
    mgr->directory_listing_css = DIRECTORY_LISTING_CSS;
    //set dns server
    sds dns_uri = get_dnsserver();
    mgr->dns4.url = strdup(dns_uri);
    MYMPD_LOG_DEBUG("Setting dns server to %s", dns_uri);
    FREE_SDS(dns_uri);

    //bind to http_port
    struct mg_connection *nc_http;
    sds http_url = sdscatfmt(sdsempty(), "http://%s:%s", config->http_host, config->http_port);
    #ifdef ENABLE_SSL
    if (config->ssl == true) {
        nc_http = mg_http_listen(mgr, http_url, ev_handler_redirect, NULL);
    }
    else {
    #endif
        nc_http = mg_http_listen(mgr, http_url, ev_handler, NULL);
    #ifdef ENABLE_SSL
    }
    #endif
    FREE_SDS(http_url);
    if (nc_http == NULL) {
        MYMPD_LOG_ERROR("Can't bind to http://%s:%s", config->http_host, config->http_port);
        mg_mgr_free(mgr);
        return false;
    }
    MYMPD_LOG_NOTICE("Listening on http://%s:%s", config->http_host, config->http_port);

    //bind to ssl_port
    #ifdef ENABLE_SSL
    if (config->ssl == true) {
        sds https_url = sdscatfmt(sdsempty(), "https://%s:%s", config->http_host, config->ssl_port);
        struct mg_connection *nc_https = mg_http_listen(mgr, https_url, ev_handler, NULL);
        FREE_SDS(https_url);
        if (nc_https == NULL) {
            MYMPD_LOG_ERROR("Can't bind to https://%s:%s", config->http_host, config->ssl_port);
            mg_mgr_free(mgr);
            return false;
        }
        MYMPD_LOG_NOTICE("Listening on https://%s:%s", config->http_host, config->ssl_port);
    }
    #endif
    MYMPD_LOG_NOTICE("Serving files from \"%s\"", DOC_ROOT);
    return mgr;
}

void web_server_free(void *arg_mgr) {
    struct mg_mgr *mgr = (struct mg_mgr *) arg_mgr;
    mg_mgr_free(mgr);
    mgr = NULL;
}

void *web_server_loop(void *arg_mgr) {
    thread_logname = sds_replace(thread_logname, "webserver");
    prctl(PR_SET_NAME, thread_logname, 0, 0, 0);
    struct mg_mgr *mgr = (struct mg_mgr *) arg_mgr;

    //set mongoose loglevel
    #ifdef DEBUG
    mg_log_set("1");
    #endif

    struct t_mg_user_data *mg_user_data = (struct t_mg_user_data *) mgr->userdata;
    #ifdef ENABLE_SSL
    MYMPD_LOG_DEBUG("Using certificate: %s", mg_user_data->config->ssl_cert);
    MYMPD_LOG_DEBUG("Using private key: %s", mg_user_data->config->ssl_key);
    #endif

    sds last_notify = sdsempty();
    time_t last_time = 0;
    while (s_signal_received == 0) {
        struct t_work_result *response = mympd_queue_shift(web_server_queue, 50, 0);
        if (response != NULL) {
            if (response->conn_id == -1) {
                //internal message
                MYMPD_LOG_DEBUG("Got internal message");
                parse_internal_message(response, mg_user_data);
            }
            else if (response->conn_id == 0) {
                MYMPD_LOG_DEBUG("Got websocket notify");
                //websocket notify from mpd idle
                time_t now = time(NULL);
                if (strcmp(response->data, last_notify) != 0 || last_time < now - 1) {
                    last_notify = sds_replace(last_notify, response->data);
                    last_time = now;
                    send_ws_notify(mgr, response);
                }
                else {
                    free_result(response);
                }
            }
            else {
                MYMPD_LOG_DEBUG("Got API response for id \"%ld\"", response->conn_id);
                //api response
                send_api_response(mgr, response);
            }
        }
        //webserver polling
        mg_mgr_poll(mgr, 50);
    }
    FREE_SDS(thread_logname);
    FREE_SDS(last_notify);
    return NULL;
}

//private functions
static bool parse_internal_message(struct t_work_result *response, struct t_mg_user_data *mg_user_data) {
    bool rc = false;
    if (response->extra != NULL) {
        struct set_mg_user_data_request *new_mg_user_data = (struct set_mg_user_data_request *)response->extra;

        mg_user_data->music_directory = sds_replace(mg_user_data->music_directory, new_mg_user_data->music_directory);
        FREE_SDS(new_mg_user_data->music_directory);

        mg_user_data->playlist_directory = sds_replace(mg_user_data->playlist_directory, new_mg_user_data->playlist_directory);
        FREE_SDS(new_mg_user_data->playlist_directory);

        sdsfreesplitres(mg_user_data->coverimage_names, mg_user_data->coverimage_names_len);
        mg_user_data->coverimage_names = webserver_split_coverimage_names(new_mg_user_data->coverimage_names, mg_user_data->coverimage_names, &mg_user_data->coverimage_names_len);
        FREE_SDS(new_mg_user_data->coverimage_names);

        mg_user_data->feat_library = new_mg_user_data->feat_library;
        mg_user_data->feat_mpd_albumart = new_mg_user_data->feat_mpd_albumart;
        mg_user_data->covercache = new_mg_user_data->covercache;

        sdsclear(mg_user_data->stream_uri);
        if (new_mg_user_data->mpd_stream_port != 0) {
            mg_user_data->stream_uri = sdscatprintf(mg_user_data->stream_uri, "http://%s:%u",
                (strncmp(new_mg_user_data->mpd_host, "/", 1) == 0 ? "127.0.0.1" : new_mg_user_data->mpd_host),
                new_mg_user_data->mpd_stream_port);
        }
        FREE_SDS(new_mg_user_data->mpd_host);
	FREE_PTR(response->extra);
        rc = true;
    }
    else {
        MYMPD_LOG_WARN("Invalid internal message: %s", response->data);
    }
    free_result(response);
    return rc;
}

static void send_ws_notify(struct mg_mgr *mgr, struct t_work_result *response) {
    struct mg_connection *nc = mgr->conns;
    int send_count = 0;
    int conn_count = 0;
    while (nc != NULL) {
        if ((int)nc->is_websocket == 1) {
            MYMPD_LOG_DEBUG("Sending notify to conn_id %lu: %s", nc->id, response->data);
            mg_ws_send(nc, response->data, sdslen(response->data), WEBSOCKET_OP_TEXT);
            send_count++;
        }
        nc = nc->next;
        conn_count++;
    }
    if (send_count == 0) {
        MYMPD_LOG_DEBUG("No websocket client connected, discarding message: %s", response->data);
    }
    free_result(response);
    struct t_mg_user_data *mg_user_data = (struct t_mg_user_data *) mgr->userdata;
    if (conn_count != mg_user_data->connection_count) {
        MYMPD_LOG_DEBUG("Correcting connection count from %d to %d", mg_user_data->connection_count, conn_count);
        mg_user_data->connection_count = conn_count;
    }
}

static void send_api_response(struct mg_mgr *mgr, struct t_work_result *response) {
    struct mg_connection *nc = mgr->conns;
    while (nc != NULL) {
        if ((int)nc->is_websocket == 0 && nc->id == (long unsigned)response->conn_id) {
            MYMPD_LOG_DEBUG("Sending response to conn_id %lu (length: %d): %s", nc->id, sdslen(response->data), response->data);
            if (response->cmd_id == INTERNAL_API_ALBUMART) {
                webserver_albumart_send(nc, response->data, response->binary);
            }
            else {
                webserver_send_data(nc, response->data, sdslen(response->data), "Content-Type: application/json\r\n");
            }
            break;
        }
        nc = nc->next;
    }
    free_result(response);
}

// Reverse proxy
static void mpd_stream_proxy_forward(struct mg_http_message *hm, struct mg_connection *nc) {
    mg_printf(nc, "%.*s\r\n", (int) (hm->proto.ptr + hm->proto.len - hm->message.ptr), hm->message.ptr);
    mg_send(nc, "\r\n", 2);
    mg_send(nc, hm->body.ptr, hm->body.len);
}

static void mpd_stream_proxy_ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *fn_data) {
    struct mg_connection *frontend_nc = fn_data;
    struct t_mg_user_data *mg_user_data = (struct t_mg_user_data *) nc->mgr->userdata;
    switch(ev) {
        case MG_EV_ACCEPT:
            mg_user_data->connection_count++;
            break;
        case MG_EV_READ:
            //forward incoming data from backend to frontend
            mg_send(frontend_nc, nc->recv.buf, nc->recv.len);
            mg_iobuf_del(&nc->recv, 0, nc->recv.len);
            break;
        case MG_EV_CLOSE: {
            MYMPD_LOG_INFO("Backend HTTP connection %lu closed", nc->id);
            mg_user_data->connection_count--;
            if (frontend_nc != NULL) {
                //remove backend connection pointer from frontend connection
                frontend_nc->fn_data = NULL;
                //close frontend connection
                frontend_nc->is_closing = 1;
            }
            break;
        }
    }
	(void) ev_data;
}

//nc->label usage
//0 - connection type: F = frontend connection, B = backend connection
//1 - http method: G = GET, H = HEAD, P = POST
//2 - connection header: C = close, K = keepalive

// Event handler
static void ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *fn_data) {
    //initial connection specific data structure
    struct mg_connection *backend_nc = fn_data;
    struct t_mg_user_data *mg_user_data = (struct t_mg_user_data *) nc->mgr->userdata;
    struct t_config *config = mg_user_data->config;
    switch(ev) {
        case MG_EV_ACCEPT: {
            mg_user_data->connection_count++;
            //check connection count
            if (mg_user_data->connection_count > HTTP_CONNECTIONS_MAX) {
                nc->is_draining = 1;
                MYMPD_LOG_DEBUG("Connections: %d", mg_user_data->connection_count);
                webserver_send_error(nc, 429, "Concurrent connections limit exceeded");
                break;
            }
            //check acl
            if (sdslen(config->acl) > 0 && mg_check_ip_acl(mg_str(config->acl), nc->peer.ip) == false) {
                nc->is_draining = 1;
                webserver_send_error(nc, 403, "Request blocked by ACL");
                break;
            }
            //ssl support
            #ifdef ENABLE_SSL
            if (config->ssl == true) {
                MYMPD_LOG_DEBUG("Init tls with cert \"%s\" and key \"%s\"", config->ssl_cert, config->ssl_key);
                struct mg_tls_opts tls_opts = {
                    .cert = config->ssl_cert,
                    .certkey = config->ssl_key
                };
                mg_tls_init(nc, &tls_opts);
            }
            #endif
            if (loglevel == LOG_DEBUG) {
                char addr_str[INET6_ADDRSTRLEN];
                const char *addr_str_ptr = inet_ntop((nc->peer.is_ip6 == true ? AF_INET6 : AF_INET), &nc->peer.ip, addr_str, INET6_ADDRSTRLEN);
                if (addr_str_ptr == NULL) {
                    webserver_send_error(nc, 500, "Internal error");
                    nc->is_draining = 1;
                    break;
                }
                MYMPD_LOG_DEBUG("New connection id \"%lu\" from %s, connections: %d", nc->id, addr_str_ptr, mg_user_data->connection_count);
            }
            //set labels
            nc->label[0] = 'F';
            nc->label[1] = '-';
            nc->label[2] = '-';
            break;
        }
        case MG_EV_WS_MSG: {
            struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
            MYMPD_LOG_DEBUG("WS message (%lu): %.*s", nc->id, wm->data.len, wm->data.ptr);
            if (strncmp(wm->data.ptr, "ping", wm->data.len) == 0) {
                size_t sent = mg_ws_send(nc, "pong", 4, WEBSOCKET_OP_TEXT);
                if (sent != 6) {
                    MYMPD_LOG_WARN("WS could not reply with pong, closing connection");
                    nc->is_closing = 1;
                }
            }
            break;
        }
        case MG_EV_HTTP_MSG: {
            struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            MYMPD_LOG_INFO("HTTP request (%lu): %.*s %.*s", nc->id, (int)hm->method.len, hm->method.ptr, (int)hm->uri.len, hm->uri.ptr);
            //limit proto to HTTP/1.1
            if (strncmp(hm->proto.ptr, "HTTP/1.1", hm->proto.len) != 0) {
                MYMPD_LOG_ERROR("Invalid http version, only HTTP/1.1 is supported");
                nc->is_closing = 1;
                return;
            }
            //limit allowed http methods
            if (strncmp(hm->method.ptr, "GET", hm->method.len) == 0) {
                nc->label[1] = 'G';
            }
            else if (strncmp(hm->method.ptr, "HEAD", hm->method.len) == 0) {
                nc->label[1] = 'H';
            }
            else if (strncmp(hm->method.ptr, "POST", hm->method.len) == 0) {
                nc->label[1] = 'P';
            }
            else {
                MYMPD_LOG_ERROR("Invalid http method \"%.*s\"", hm->method.len, hm->method.ptr);
                nc->is_closing = 1;
                return;
            }
            //check uri length
            if (hm->uri.len > URI_LENGTH_MAX) {
                MYMPD_LOG_ERROR("Uri is too long, length is %d, maximum length is %d", hm->uri.len, URI_LENGTH_MAX);
                nc->is_closing = 1;
                return;
            }

            //check post requests length
            if (nc->label[1] == 'P' && (hm->body.len == 0 || hm->body.len > BODY_SIZE_MAX)) {
                MYMPD_LOG_ERROR("POST request with body of size %d is out of bounds", hm->body.len);
                nc->is_closing = 1;
                return;
            }
            //respect connection close header
            struct mg_str *connection_hdr = mg_http_get_header(hm, "Connection");
            if (connection_hdr != NULL) {
                if (strncmp(connection_hdr->ptr, "close", connection_hdr->len) == 0) {
                    MYMPD_LOG_DEBUG("Connection: close header found");
                    nc->label[2] = 'C';
                }
                else if (strncmp(connection_hdr->ptr, "close", connection_hdr->len) == 0) {
                    MYMPD_LOG_DEBUG("Connection: keepalive header found");
                    nc->label[2] = 'K';
                }
            }

            if (mg_http_match_uri(hm, "/stream/")) {
                if (sdslen(mg_user_data->stream_uri) == 0) {
                    nc->is_draining = 1;
                    webserver_send_error(nc, 404, "MPD stream port not configured");
                    break;
                }
                if (backend_nc == NULL) {
                    MYMPD_LOG_INFO("Creating new mpd stream proxy backend connection to \"%s\"", mg_user_data->stream_uri);
                    backend_nc = mg_connect(nc->mgr, mg_user_data->stream_uri, mpd_stream_proxy_ev_handler, nc);
                    if (backend_nc == NULL) {
                        //no backend connection, close frontend connection
                        MYMPD_LOG_WARN("Can not create backend connection");
                        nc->is_closing = 1;
                    }
                    else {
                        //save backend connection pointer in frontend connection fn_data
                        nc->fn_data = backend_nc;
                    }
                }
                if (backend_nc != NULL) {
                    //strip path
                    hm->uri.ptr = "/";
                    hm->uri.len = 1;
                    //set labels
                    backend_nc->label[0] = 'B';
                    backend_nc->label[1] = nc->label[1];
                    backend_nc->label[2] = nc->label[2];
                    //forward request
                    MYMPD_LOG_INFO("Forwarding client connection \"%lu\" to backend connection \"%lu\"", nc->id, backend_nc->id);
                    mpd_stream_proxy_forward(hm, backend_nc);
                }
            }
            else if (mg_http_match_uri(hm, "/ws/")) {
                mg_ws_upgrade(nc, hm, NULL);
                MYMPD_LOG_INFO("New Websocket connection established (%lu)", nc->id);
                sds response = jsonrpc_event(sdsempty(), "welcome");
                mg_ws_send(nc, response, sdslen(response), WEBSOCKET_OP_TEXT);
                FREE_SDS(response);
            }
            else if (mg_http_match_uri(hm, "/api/script")) {
                if (sdslen(config->scriptacl) > 0 && mg_check_ip_acl(mg_str(config->scriptacl), nc->peer.ip) == false) {
                    nc->is_draining = 1;
                    webserver_send_error(nc, 403, "Request blocked by ACL");
                    break;
                }
                sds body = sdsnewlen(hm->body.ptr, hm->body.len);
                bool rc = handle_script_api((long long)nc->id, body);
                FREE_SDS(body);
                if (rc == false) {
                    MYMPD_LOG_ERROR("Invalid script API request");
                    sds response = jsonrpc_respond_message(sdsempty(), "", 0, true,
                        "script", "error", "Invalid script API request");
                    webserver_send_data(nc, response, sdslen(response), "Content-Type: application/json\r\n");
                    FREE_SDS(response);
                }
            }
            else if (mg_http_match_uri(hm, "/api/serverinfo")) {
                struct sockaddr_in localip;
                socklen_t len = sizeof(localip);
                if (getsockname((int)(long)nc->fd, (struct sockaddr *)&localip, &len) == 0) {
                    sds response = jsonrpc_result_start(sdsempty(), "", 0);
                    char addr[INET6_ADDRSTRLEN];
                    const char *str = inet_ntop(localip.sin_family, &localip.sin_addr, addr, INET6_ADDRSTRLEN);
                    if (str != NULL) {
                        response = tojson_char(response, "ip", str, false);
                    }
                    else {
                        MYMPD_LOG_ERROR("Can not get listening ip");
                        response = tojson_char(response, "ip", "", false);
                    }
                    response = jsonrpc_result_end(response);
                    webserver_send_data(nc, response, sdslen(response), "Content-Type: application/json\r\n");
                    FREE_SDS(response);
                }
            }
            else if (mg_http_match_uri(hm, "/api/")) {
                //api request
                sds body = sdsnewlen(hm->body.ptr, hm->body.len);
                struct mg_str *auth_header = mg_http_get_header(hm, "Authorization");
                bool rc = handle_api(nc, body, auth_header, mg_user_data);
                FREE_SDS(body);
                if (rc == false) {
                    MYMPD_LOG_ERROR("Invalid API request");
                    sds response = jsonrpc_respond_message(sdsempty(), "", 0, true,
                        "general", "error", "Invalid API request");
                    webserver_send_data(nc, response, sdslen(response), "Content-Type: application/json\r\n");
                    FREE_SDS(response);
                }
            }
            #ifdef ENABLE_SSL
            else if (mg_http_match_uri(hm, "/ca.crt")) {
                if (config->custom_cert == false) {
                    //deliver ca certificate
                    sds ca_file = sdscatfmt(sdsempty(), "%s/ssl/ca.pem", config->workdir);
                    static struct mg_http_serve_opts s_http_server_opts;
                    s_http_server_opts.root_dir = mg_user_data->browse_document_root;
                    s_http_server_opts.extra_headers = EXTRA_HEADERS_CACHE;
                    s_http_server_opts.mime_types = EXTRA_MIME_TYPES;
                    mg_http_serve_file(nc, hm, ca_file, &s_http_server_opts);
                    FREE_SDS(ca_file);
                }
                else {
                    webserver_send_error(nc, 404, "Custom cert enabled, don't deliver myMPD ca");
                }
            }
            #endif
            else if (mg_http_match_uri(hm, "/albumart/#")) {
                webserver_albumart_handler(nc, hm, mg_user_data, config, (long long)nc->id);
            }
            else if (mg_http_match_uri(hm, "/tagart/#")) {
                webserver_tagart_handler(nc, hm, mg_user_data);
            }
            else if (mg_http_match_uri(hm, "/pics/#")) {
                //serve directory
                MYMPD_LOG_DEBUG("Setting document root to \"%s\"", mg_user_data->pics_document_root);
                static struct mg_http_serve_opts s_http_server_opts;
                s_http_server_opts.root_dir = mg_user_data->pics_document_root;
                s_http_server_opts.extra_headers = EXTRA_HEADERS_CACHE;
                s_http_server_opts.mime_types = EXTRA_MIME_TYPES;
                hm->uri = mg_str_strip_parent(&hm->uri, 1);
                mg_http_serve_dir(nc, hm, &s_http_server_opts);
            }
            else if (mg_http_match_uri(hm, "/browse/#")) {
                static struct mg_http_serve_opts s_http_server_opts;
                s_http_server_opts.extra_headers = EXTRA_HEADERS_DIR;
                s_http_server_opts.mime_types = EXTRA_MIME_TYPES;
                if (mg_http_match_uri(hm, "/browse/")) {
                    sds dirs = sdsempty();
                    if (mg_user_data->feat_library == true) {
                        dirs = sdscat(dirs, "<tr><td><a href=\"music/\">music/</a></td><td>MPD music directory</td><td></td></tr>");
                    }
                    dirs = sdscat(dirs, "<tr><td><a href=\"pics/\">pics/</a></td><td>myMPD pics directory</td><td></td></tr>");
                    if (sdslen(mg_user_data->playlist_directory) > 0) {
                        dirs = sdscat(dirs, "<tr><td><a href=\"playlists/\">playlists/</a></td><td>MPD playlists directory</td><td></td></tr>");
                    }
                    dirs = sdscat(dirs, "<tr><td><a href=\"smartplaylists/\">smartplaylists/</a></td><td>myMPD smart playlists directory</td><td></td></tr>");
                    mg_http_reply(nc, 200, "Content-Type: text/html\r\n"EXTRA_HEADERS_DIR, "<!DOCTYPE html>"
                        "<html><head>"
                        "<meta charset=\"utf-8\">"
                        "<title>Index of /browse/</title>"
                        "<style>%s</style></style></head>"
                        "<body>"
                        "<h1>Index of /browse/</h1>"
                        "<table cellpadding=\"0\"><thead>"
                        "<tr><th>Name</th><th>Description</th><th></th></tr>"
                        "</thead>"
                        "<tbody id=\"tb\">%s</tbody>"
                        "</table>"
                        "<address>%s</address>"
                        "</body></html>",
                        nc->mgr->directory_listing_css,
                        dirs,
                        nc->mgr->product_name
                    );
                    sdsfree(dirs);
                }
                else if (mg_http_match_uri(hm, "/browse/pics/#")) {
                    s_http_server_opts.root_dir = mg_user_data->pics_document_root;
                    hm->uri = mg_str_strip_parent(&hm->uri, 2);
                    MYMPD_LOG_DEBUG("Serving directory %s%.*s", mg_user_data->music_directory, hm->uri.len, hm->uri.ptr);
                    mg_http_serve_dir(nc, hm, &s_http_server_opts);
                }
                else if (mg_http_match_uri(hm, "/browse/smartplaylists/#")) {
                    s_http_server_opts.root_dir = mg_user_data->smartpls_document_root;
                    hm->uri = mg_str_strip_parent(&hm->uri, 2);
                    MYMPD_LOG_DEBUG("Serving directory %s%.*s", mg_user_data->music_directory, hm->uri.len, hm->uri.ptr);
                    mg_http_serve_dir(nc, hm, &s_http_server_opts);
                }
                else if (sdslen(mg_user_data->playlist_directory) > 0 && mg_http_match_uri(hm, "/browse/playlists/#")) {
                    s_http_server_opts.root_dir = mg_user_data->playlist_directory;
                    hm->uri = mg_str_strip_parent(&hm->uri, 2);
                    MYMPD_LOG_DEBUG("Serving directory %s%.*s", mg_user_data->music_directory, hm->uri.len, hm->uri.ptr);
                    mg_http_serve_dir(nc, hm, &s_http_server_opts);
                }
                else if (mg_user_data->feat_library == true && mg_http_match_uri(hm, "/browse/music/#")) {
                    s_http_server_opts.root_dir = mg_user_data->music_directory;
                    hm->uri = mg_str_strip_parent(&hm->uri, 2);
                    MYMPD_LOG_DEBUG("Serving directory %s%.*s", mg_user_data->music_directory, hm->uri.len, hm->uri.ptr);
                    mg_http_serve_dir(nc, hm, &s_http_server_opts);
                }
                else {
                    webserver_send_error(nc, 403, "Access to this ressource is forbidden.");
                }
            }
            else if (mg_vcmp(&hm->uri, "/index.html") == 0) {
                webserver_send_header_redirect(nc, "/");
            }
            else if (mg_vcmp(&hm->uri, "/favicon.ico") == 0) {
                webserver_send_header_redirect(nc, "/assets/favicon.ico");
            }
            else {
                //all other uris
                #ifndef EMBEDDED_ASSETS
                    //serve all files from filesystem
                    static struct mg_http_serve_opts s_http_server_opts;
                    s_http_server_opts.root_dir = DOC_ROOT;
                    s_http_server_opts.extra_headers = EXTRA_HEADERS;
                    s_http_server_opts.mime_types = EXTRA_MIME_TYPES;
                    mg_http_serve_dir(nc, hm, &s_http_server_opts);
                #else
                    //serve embedded files
                    sds uri = sdsnewlen(hm->uri.ptr, hm->uri.len);
                    webserver_serve_embedded_files(nc, uri, hm);
                    FREE_SDS(uri);
                #endif
            }
            break;
        }
        case MG_EV_CLOSE: {
            MYMPD_LOG_INFO("HTTP connection %lu closed", nc->id);
            mg_user_data->connection_count--;
            if (backend_nc != NULL) {
                MYMPD_LOG_INFO("Closing backend connection \"%lu\"", backend_nc->id);
                //remove pointer to frontend connection
                backend_nc->fn_data = NULL;
                //close reverse proxy connection
                backend_nc->is_closing = 1;
            }
            break;
        }
    }
}

#ifdef ENABLE_SSL
static void ev_handler_redirect(struct mg_connection *nc, int ev, void *ev_data, void *fn_data) {
    (void)fn_data;
    struct t_mg_user_data *mg_user_data = (struct t_mg_user_data *) nc->mgr->userdata;
    struct t_config *config = mg_user_data->config;
    if (ev == MG_EV_ACCEPT) {
        //check connection count
        if (mg_user_data->connection_count > HTTP_CONNECTIONS_MAX) {
            nc->is_draining = 1;
            webserver_send_error(nc, 429, "Concurrent connections limit exceeded");
            return;
        }
        //check acl
        if (sdslen(config->acl) > 0 && mg_check_ip_acl(mg_str(config->acl), nc->peer.ip) == false) {
            nc->is_draining = 1;
            webserver_send_error(nc, 403, "Request blocked by ACL");
            return;
        }
        mg_user_data->connection_count++;
    }
    else if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        struct mg_str *host_hdr = mg_http_get_header(hm, "Host");
        if (host_hdr == NULL) {
            MYMPD_LOG_ERROR("No hoster header found, closing connection");
            nc->is_closing = 1;
            return;
        }

        sds host_header = sdscatlen(sdsempty(), host_hdr->ptr, (int)host_hdr->len);
        int count = 0;
        sds *tokens = sdssplitlen(host_header, (ssize_t)sdslen(host_header), ":", 1, &count);
        sds s_redirect = sdscatfmt(sdsempty(), "https://%s", tokens[0]);
        if (strcmp(config->ssl_port, "443") != 0) {
            s_redirect = sdscatfmt(s_redirect, ":%s", config->ssl_port);
        }
        MYMPD_LOG_INFO("Redirecting to %s", s_redirect);
        webserver_send_header_redirect(nc, s_redirect);
        nc->is_draining = 1;
        sdsfreesplitres(tokens, count);
        FREE_SDS(host_header);
        FREE_SDS(s_redirect);
    }
    else if (ev == MG_EV_CLOSE) {
        MYMPD_LOG_INFO("Connection %lu closed", nc->id);
        mg_user_data->connection_count--;
    }
}
#endif

static bool handle_api(struct mg_connection *nc, sds body, struct mg_str *auth_header, struct t_mg_user_data *mg_user_data) {
    MYMPD_LOG_DEBUG("API request (%lld): %s", (long long)nc->id, body);

    //first check if request is valid json string
    if (validate_json(body) == false) {
        return false;
    }

    sds cmd = NULL;
    sds jsonrpc = NULL;
    int id = 0;

    if (json_get_string_cmp(body, "$.jsonrpc", 3, 3, "2.0", &jsonrpc, NULL) == false ||
        json_get_string_max(body, "$.method", &cmd, vcb_isalnum, NULL) == false ||
        json_get_int(body, "$.id", 0, 0, &id, NULL) == false)
    {
        MYMPD_LOG_ERROR("Invalid jsonrpc2 request");
        FREE_SDS(cmd);
        FREE_SDS(jsonrpc);
        return false;
    }

    MYMPD_LOG_INFO("API request (%lld): %s", (long long)nc->id, cmd);

    enum mympd_cmd_ids cmd_id = get_cmd_id(cmd);
    if (cmd_id == GENERAL_API_UNKNOWN) {
        MYMPD_LOG_ERROR("Unknown API method");
        FREE_SDS(cmd);
        FREE_SDS(jsonrpc);
        return false;
    }

    if (is_public_api_method(cmd_id) == false) {
        MYMPD_LOG_ERROR("API method %s is for internal use only", cmd);
        FREE_SDS(cmd);
        FREE_SDS(jsonrpc);
        return false;
    }

    sds session = sdsempty();
    #ifdef ENABLE_SSL
    if (sdslen(mg_user_data->config->pin_hash) > 0 && is_protected_api_method(cmd_id) == true) {
        //format of authorization header must be: Bearer x
        //bearer token must be 20 characters long
        bool rc = false;
        if (auth_header != NULL && auth_header->len == 27 && strncmp(auth_header->ptr, "Bearer ", 7) == 0) {
            session = sdscatlen(session, auth_header->ptr, auth_header->len);
            sdsrange(session, 7, -1);
            rc = webserver_session_validate(&mg_user_data->session_list, session);
        }
        else {
            MYMPD_LOG_ERROR("No valid Authorization header found");
        }
        if (rc == false) {
            MYMPD_LOG_ERROR("API method %s is protected", cmd);
            sds response = jsonrpc_respond_message(sdsempty(), cmd, 0, true, "session", "error",
                (cmd_id == MYMPD_API_SESSION_VALIDATE ? "Invalid session" : "Authentication required"));
            mg_printf(nc, "HTTP/1.1 401 Unauthorized\r\n"
                "WWW-Authenticate: Bearer realm=\"myMPD\"\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n\r\n",
                (int)sdslen(response));
            mg_send(nc, response, sdslen(response));
            FREE_SDS(cmd);
            FREE_SDS(jsonrpc);
            FREE_SDS(session);
            FREE_SDS(response);
            return true;
        }
        MYMPD_LOG_INFO("API request is authorized");
    }
    #else
    (void) auth_header;
    #endif

    switch(cmd_id) {
        case MYMPD_API_SESSION_LOGIN: {
            sds pin = NULL;
            bool is_valid = false;
            if (json_get_string(body, "$.params.pin", 1, 20, &pin, vcb_isalnum, NULL) == true) {
                is_valid = pin_validate(pin, mg_user_data->config->pin_hash);
            }
            FREE_SDS(pin);
            sds response = sdsempty();
            if (is_valid == true) {
                sds ses = webserver_session_new(&mg_user_data->session_list);
                response = jsonrpc_result_start(response, "MYMPD_API_SESSION_LOGIN", 0);
                response = tojson_char(response, "session", ses, false);
                response = jsonrpc_result_end(response);
                FREE_SDS(ses);
            }
            else {
                response = jsonrpc_respond_message(response, "MYMPD_API_SESSION_LOGIN", 0, true, "session", "error", "Invalid pin");
            }
            webserver_send_data(nc, response, sdslen(response), "Content-Type: application/json\r\n");
            FREE_SDS(response);
            break;
        }
        case MYMPD_API_SESSION_LOGOUT: {
            bool rc = false;
            sds response = sdsempty();
            if (sdslen(session) == 20) {
                rc = webserver_session_remove(&mg_user_data->session_list, session);
                if (rc == true) {
                    response = jsonrpc_respond_message(response, "MYMPD_API_SESSION_LOGOUT", 0, false, "session", "info", "Session removed");
                }
            }
            if (rc == false) {
                response = jsonrpc_respond_message(response, "MYMPD_API_SESSION_LOGOUT", 0, true, "session", "error", "Invalid session");
            }

            webserver_send_data(nc, response, sdslen(response), "Content-Type: application/json\r\n");
            FREE_SDS(response);
            break;
        }
        case MYMPD_API_SESSION_VALIDATE: {
            //session is already validated
            sds response = jsonrpc_respond_ok(sdsempty(), "MYMPD_API_SESSION_VALIDATE", 0, "session");
            webserver_send_data(nc, response, sdslen(response), "Content-Type: application/json\r\n");
            FREE_SDS(response);
            break;
        }
        default: {
            //forward API request to mympd_api_handler
            struct t_work_request *request = create_request((long long)nc->id, id, cmd_id, body);
            mympd_queue_push(mympd_api_queue, request, 0);
        }
    }
    FREE_SDS(session);
    FREE_SDS(cmd);
    FREE_SDS(jsonrpc);
    return true;
}

static bool handle_script_api(long long conn_id, sds body) {
    MYMPD_LOG_DEBUG("Script API request (%lld): %s", conn_id, body);

    //first check if request is valid json string
    if (validate_json(body) == false) {
        return false;
    }

    sds cmd = NULL;
    sds jsonrpc = NULL;
    int id = 0;

    if (json_get_string_cmp(body, "$.jsonrpc", 3, 3, "2.0", &jsonrpc, NULL) == false ||
        json_get_string_max(body, "$.method", &cmd, vcb_isalnum, NULL) == false ||
        json_get_int(body, "$.id", 0, 0, &id, NULL) == false)
    {
        MYMPD_LOG_ERROR("Invalid jsonrpc2 request");
        FREE_SDS(cmd);
        FREE_SDS(jsonrpc);
        return false;
    }

    MYMPD_LOG_INFO("Script API request (%lld): %s", conn_id, cmd);

    enum mympd_cmd_ids cmd_id = get_cmd_id(cmd);
    if (cmd_id != INTERNAL_API_SCRIPT_POST_EXECUTE) {
        MYMPD_LOG_ERROR("API method %s is invalid for this uri", cmd);
        FREE_SDS(cmd);
        FREE_SDS(jsonrpc);
        return false;
    }

    struct t_work_request *request = create_request(conn_id, id, cmd_id, body);
    mympd_queue_push(mympd_api_queue, request, 0);

    FREE_SDS(cmd);
    FREE_SDS(jsonrpc);
    return true;
}
