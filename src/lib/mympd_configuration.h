/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_CONFIGURATION_H
#define MYMPD_CONFIGURATION_H

#include "mympd_config_defs.h"
#include "../dist/sds/sds.h"

#include <stdbool.h>

struct t_config {
    sds user;
    sds workdir;
    sds cachedir;
    sds http_host;
    sds http_port;
#ifdef ENABLE_SSL
    bool ssl;
    sds ssl_port;
    sds ssl_cert;
    sds ssl_key;
    bool custom_cert;
    sds ssl_san;
#endif
    sds acl;
    sds scriptacl;
#ifdef ENABLE_LUA
    sds lualibs;
#endif
    bool log_to_syslog;
    int loglevel;
    time_t startup_time;
    bool first_startup;
    bool bootstrap;
    sds pin_hash;
};

#endif
