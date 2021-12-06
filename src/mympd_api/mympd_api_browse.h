/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#ifndef MYMPD_API_BROWSE_H
#define MYMPD_API_BROWSE_H

#include "../lib/mympd_state.h"

sds mympd_api_browse_fingerprint(struct t_mympd_state *mympd_state, sds buffer, sds method,
                               long request_id, const char *uri);
sds mympd_api_browse_songdetails(struct t_mympd_state *mympd_state, sds buffer, sds method,
                               long request_id, const char *uri);
sds mympd_api_browse_filesystem(struct t_mympd_state *mympd_state, sds buffer,
                              sds method, long request_id, sds path, const unsigned offset,
                              const unsigned limit, sds searchstr,
                              const struct t_tags *tagcols);
sds mympd_api_browse_album_songs(struct t_mympd_state *mympd_state, sds buffer, sds method,
                                  long request_id, sds album, struct t_list *albumartists,
                                  const struct t_tags *tagcols);
sds mympd_api_browse_album_list(struct t_mympd_state *mympd_state, sds buffer, sds method,
                                       long request_id, sds expression, sds sort, bool sortdesc,
                                       const unsigned offset, unsigned limit);
sds mympd_api_browse_tag_list(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id,
                          sds searchstr, sds tag, const unsigned offset, const unsigned limit);
sds mympd_api_browse_read_comments(struct t_mympd_state *mympd_state, sds buffer, sds method, long request_id,
                             const char *uri);
#endif
