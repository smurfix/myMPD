/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_WEB_SERVER_H
#define MYMPD_WEB_SERVER_H

#include "../dist/sds/sds.h"
#include "lib/mympd_configuration.h"
#include "web_server/web_server_utility.h"

void *web_server_loop(void *arg_mgr);
bool web_server_init(void *arg_mgr, struct t_config *config, struct t_mg_user_data *mg_user_data);
void web_server_free(void *arg_mgr);
#endif
