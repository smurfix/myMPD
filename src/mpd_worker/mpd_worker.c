/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"
#include "mpd_worker.h"

#include "../../dist/sds/sds.h"
#include "../lib/log.h"
#include "../lib/mem.h"
#include "../lib/sds_extras.h"
#include "../mpd_client/mpd_client_connection.h"
#include "../mpd_client/mpd_client_errorhandler.h"
#include "../mpd_client/mpd_client_tags.h"
#include "mpd_worker_api.h"
#include "mpd_worker_cache.h"
#include "mpd_worker_smartpls.h"

#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

//private definitions
static void *mpd_worker_run(void *arg);

//public functions

/**
 * Starts the worker thread
 */
bool mpd_worker_start(struct t_mympd_state *mympd_state, struct t_work_request *request) {
    MYMPD_LOG_NOTICE("Starting mpd_worker thread for %s", request->method);
    pthread_t mpd_worker_thread;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        MYMPD_LOG_ERROR("Can not init mpd_worker thread attribute");
        return false;
    }
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        MYMPD_LOG_ERROR("Can not set mpd_worker thread to detached");
        return false;
    }
    //create mpd worker state from mympd_state
    struct t_mpd_worker_state *mpd_worker_state = malloc_assert(sizeof(struct t_mpd_worker_state));
    mpd_worker_state->request = request;
    mpd_worker_state->smartpls = mympd_state->smartpls == true ? mympd_state->mpd_state->feat_mpd_smartpls == true ? true : false : false;
    mpd_worker_state->smartpls_sort = sdsdup(mympd_state->smartpls_sort);
    mpd_worker_state->smartpls_prefix = sdsdup(mympd_state->smartpls_prefix);
    mpd_worker_state->config = mympd_state->config;
    copy_tag_types(&mympd_state->smartpls_generate_tag_types, &mpd_worker_state->smartpls_generate_tag_types);

    //mpd state
    mpd_worker_state->mpd_state = malloc_assert(sizeof(struct t_mpd_state));
    mympd_state_default_mpd_state(mpd_worker_state->mpd_state);
    mpd_worker_state->mpd_state->mpd_host = sds_replace(mpd_worker_state->mpd_state->mpd_host, mympd_state->mpd_state->mpd_host);
    mpd_worker_state->mpd_state->mpd_port = mympd_state->mpd_state->mpd_port;
    mpd_worker_state->mpd_state->mpd_pass = sds_replace(mpd_worker_state->mpd_state->mpd_pass, mympd_state->mpd_state->mpd_pass);
    mpd_worker_state->mpd_state->feat_mpd_tags = mympd_state->mpd_state->feat_mpd_tags;
    mpd_worker_state->mpd_state->feat_mpd_stickers = mympd_state->mpd_state->feat_mpd_stickers;
    mpd_worker_state->mpd_state->feat_mpd_playlists = mympd_state->mpd_state->feat_mpd_playlists;
    mpd_worker_state->mpd_state->feat_mpd_advsearch = mympd_state->mpd_state->feat_mpd_advsearch;
    mpd_worker_state->mpd_state->feat_mpd_whence = mympd_state->mpd_state->feat_mpd_whence;
    mpd_worker_state->mpd_state->tag_albumartist = mympd_state->mpd_state->tag_albumartist;
    copy_tag_types(&mympd_state->mpd_state->tag_types_mympd, &mpd_worker_state->mpd_state->tag_types_mympd);

    if (pthread_create(&mpd_worker_thread, &attr, mpd_worker_run, mpd_worker_state) != 0) {
        MYMPD_LOG_ERROR("Can not create mpd_worker thread");
        mpd_worker_state_free(mpd_worker_state);
        return false;
    }
    worker_threads++;
    return true;
}

/**
 * Worker thread
 */
static void *mpd_worker_run(void *arg) {
    thread_logname = sds_replace(thread_logname, "mpdworker");
    prctl(PR_SET_NAME, thread_logname, 0, 0, 0);
    struct t_mpd_worker_state *mpd_worker_state = (struct t_mpd_worker_state *) arg;

    if (mpd_client_connect(mpd_worker_state->mpd_state) == true) {
        //set interesting tags
        enable_mpd_tags(mpd_worker_state->mpd_state, &mpd_worker_state->mpd_state->tag_types_mympd);
        //call api handler
        mpd_worker_api(mpd_worker_state);
        //disconnect
        mpd_client_disconnect(mpd_worker_state->mpd_state);
    }
    MYMPD_LOG_NOTICE("Stopping mpd_worker thread");
    FREE_SDS(thread_logname);
    mpd_worker_state_free(mpd_worker_state);
    worker_threads--;
    return NULL;
}
