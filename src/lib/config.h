/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

/*! \file
 * \brief Configuration handling
 */

#ifndef MYMPD_CONFIG_H
#define MYMPD_CONFIG_H

#include "src/lib/config_def.h"

#include <stdbool.h>

void mympd_config_defaults_initial(struct t_config *config);
void mympd_config_defaults(struct t_config *config);
void mympd_config_free(struct t_config *config);
bool mympd_config_rm(struct t_config *config);
bool mympd_config_rw(struct t_config *config, bool write);
bool mympd_version_set(sds workdir);
bool mympd_version_check(sds workdir);

#endif
