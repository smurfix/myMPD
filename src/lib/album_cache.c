/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"
#include "album_cache.h"

#include "../../dist/libmpdclient/src/isong.h"
#include "../lib/sds_extras.h"
#include "../mpd_client/tags.h"
#include "log.h"

#include <inttypes.h>

/**
 * myMPD saves album information in the album cache as a mpd_song struct.
 * Used fields:
 *   tags: tags from all songs of the album
 *   last_modified: last_modified from newest song
 *   duration: the album total time in seconds
 *   duration_ms: the album total time in milliseconds
 *   pos: number of discs
 *   prio: number of songs
 */

/**
 * Contructs the albumkey from song info
 * @param song mpd song struct
 * @param albumkey sds string replaced by the key
 * @return pointer to albumkey
 */
sds album_cache_get_key(struct mpd_song *song, sds albumkey) {
    sdsclear(albumkey);
    albumkey = mpd_client_get_tag_value_string(song, MPD_TAG_ALBUM, albumkey);
    if (sdslen(albumkey) == 0) {
        //album tag is empty
        MYMPD_LOG_WARN("Can not create albumkey for uri \"%s\", tag Album is empty", mpd_song_get_uri(song));
        return albumkey;
    }
    albumkey = sdscatlen(albumkey, "::", 2);
    size_t old_len = sdslen(albumkey);
    //first try AlbumArtist tag
    albumkey = mpd_client_get_tag_value_string(song, MPD_TAG_ALBUM_ARTIST, albumkey);
    if (old_len == sdslen(albumkey)) {
        //AlbumArtist tag is empty, fallback to Artist tag
        MYMPD_LOG_DEBUG("AlbumArtist for uri \"%s\" is empty, falling back to Artist", mpd_song_get_uri(song));
        albumkey = mpd_client_get_tag_value_string(song, MPD_TAG_ARTIST, albumkey);
    }
    if (old_len == sdslen(albumkey)) {
        MYMPD_LOG_WARN("Can not create albumkey for uri \"%s\", tags AlbumArtist and Artist are empty", mpd_song_get_uri(song));
        sdsclear(albumkey);
    }
    sds_utf8_tolower(albumkey);
    return albumkey;
}

/**
 * Gets the album from the album cache
 * @param album_cache pointer to t_cache struct
 * @param key the album
 * @return mpd_song struct representing the album
 */
struct mpd_song *album_cache_get_album(struct t_cache *album_cache, sds key) {
    if (album_cache->cache == NULL) {
        return NULL;
    }
    //try to get album
    void *data = raxFind(album_cache->cache, (unsigned char*)key, sdslen(key));
    if (data == raxNotFound) {
        MYMPD_LOG_ERROR("Album for key \"%s\" not found in cache", key);
        return NULL;
    }
    return (struct mpd_song *) data;
}

/**
 * Frees the album cache
 * @param album_cache pointer to t_cache struct
 */
void album_cache_free(struct t_cache *album_cache) {
    if (album_cache->cache == NULL) {
        MYMPD_LOG_DEBUG("Album cache is NULL not freeing anything");
        return;
    }
    MYMPD_LOG_DEBUG("Freeing album cache");
    raxIterator iter;
    raxStart(&iter, album_cache->cache);
    raxSeek(&iter, "^", NULL, 0);
    while (raxNext(&iter)) {
        mpd_song_free((struct mpd_song *)iter.data);
    }
    raxStop(&iter);
    raxFree(album_cache->cache);
    album_cache->cache = NULL;
}

/**
 * Gets the number of songs
 * @param album mpd_song struct representing the album
 * @return number of songs
 */
unsigned album_get_song_count(struct mpd_song *album) {
    return mpd_song_get_prio(album);
}

/**
 * Gets the number of discs
 * @param album mpd_song struct representing the album
 * @return number of discs
 */
unsigned album_get_discs(struct mpd_song *album) {
    return mpd_song_get_pos(album);
}

/**
 * Gets the total play time
 * @param album mpd_song struct representing the album
 * @return total play time
 */
unsigned album_get_total_time(struct mpd_song *album) {
    return mpd_song_get_duration(album);
}

/**
 * Sets the albums disc number
 * @param album mpd_song struct representing the album
 * @param song mpd song to set discs from
 */
void album_cache_set_discs(struct mpd_song *album, struct mpd_song *song) {
    const char *disc;
    if ((disc = mpd_song_get_tag(song, MPD_TAG_DISC, 0)) != NULL) {
        unsigned d = (unsigned)strtoumax(disc, NULL, 10);
        if (d > album->pos) {
            album->pos = d;
        }
    }
}

/**
 * Sets the albums last modified date
 * @param album mpd_song struct representing the album
 * @param song mpd song to set last_modified from
 */
void album_cache_set_last_modified(struct mpd_song *album, struct mpd_song *song) {
    time_t last_modified_old = mpd_song_get_last_modified(album);
    time_t last_modified_new = mpd_song_get_last_modified(song);
    if (last_modified_old < last_modified_new) {
        album->last_modified = last_modified_new;
    }
}

/**
 * Increments the albums duration
 * @param album mpd_song struct representing the album
 * @param song pointer to a mpd_song struct
 */
void album_cache_inc_total_time(struct mpd_song *album, struct mpd_song *song) {
    album->duration += mpd_song_get_duration(song);
    album->duration_ms += mpd_song_get_duration_ms(song);
}

/**
 * Set the song count
 * @param album mpd_song struct representing the album
 * @param count song count
 */
void album_cache_set_song_count(struct mpd_song *album, unsigned count) {
    album->prio = count;
}

/**
 * Increments the song count
 * @param album pointer to a mpd_song struct
 */
void album_cache_inc_song_count(struct mpd_song *album) {
    album->prio++;
}

/**
 * Appends tag values to the album
 * @param album pointer to a mpd_song struct representing the album
 * @param song song to add tag values from
 * @param tags tags to append
 * @return true on success else false
 */
bool album_cache_append_tags(struct mpd_song *album,
		struct mpd_song *song, struct t_tags *tags)
{
    for (unsigned tagnr = 0; tagnr < tags->len; ++tagnr) {
        const char *value;
        enum mpd_tag_type tag = tags->tags[tagnr];
        //append only multivalue tags
        if (is_multivalue_tag(tag) == true) {
            unsigned value_nr = 0;
            while ((value = mpd_song_get_tag(song, tag, value_nr)) != NULL) {
                if (mympd_mpd_song_add_tag_dedup(album, tag, value) == false) {
                    return false;
                }
                value_nr++;
            }
        }
    }
    return true;
}

/**
 * Copies all values from a tag to another tag
 * @param song pointer to a mpd_song struct
 * @param src source tag
 * @param dst destination tag
 * @return true on success, else false
 */
bool album_cache_copy_tags(struct mpd_song *song, enum mpd_tag_type src, enum mpd_tag_type dst) {
    const char *value;
    unsigned value_nr = 0;
    while ((value = mpd_song_get_tag(song, src, value_nr)) != NULL) {
        if (mympd_mpd_song_add_tag_dedup(song, dst, value) == false) {
            return false;
        }
        value_nr++;
    }
    return true;
}
