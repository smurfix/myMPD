/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_API_SCRIPTS_H
#define MYMPD_API_SCRIPTS_H

#include "../../dist/sds/sds.h"
#include "../lib/list.h"
#include "../lib/mympd_configuration.h"

#include <stdbool.h>

#ifdef ENABLE_LUA
    bool mympd_api_script_save(struct t_config *config, sds script, sds oldscript, int order, sds content, struct t_list *arguments);
    bool mympd_api_script_delete(struct t_config *config, const char *script);
    sds mympd_api_script_get(struct t_config *config, sds buffer, sds method, long request_id, const char *script);
    sds mympd_api_script_list(struct t_config *config, sds buffer, sds method, long request_id, bool all);
    bool mympd_api_script_start(struct t_config *config, const char *script, struct t_list *arguments, bool localscript);
#endif
#endif
