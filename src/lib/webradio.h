/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

/*! \file
 * \brief Webradio functions
 */

#ifndef MYMPD_LIB_WEBRADIO_H
#define MYMPD_LIB_WEBRADIO_H

#include "dist/rax/rax.h"
#include "dist/sds/sds.h"
#include "src/lib/config_def.h"
#include "src/lib/list.h"

/**
 * Webradio Types
 */
enum webradio_type {
    WEBRADIO_WEBRADIODB,
    WEBRADIO_FAVORITE,
    WEBRADIO_ALL
};

/**
 * Wraps the indexes of webradios
 */
struct t_webradios {
    rax *db;                  //!< Index by name
    rax *idx_uris;            //!< Index by uri
    pthread_rwlock_t rwlock;  //!< pthreads read-write lock object
};

/**
 * Webradio tag types
 */
enum webradio_tag_type {
    WEBRADIO_TAG_UNKNOWN = -1,
    WEBRADIO_TAG_NAME,
    WEBRADIO_TAG_IMAGE,
    WEBRADIO_TAG_HOMEPAGE,
    WEBRADIO_TAG_COUNTRY,
    WEBRADIO_TAG_REGION,
    WEBRADIO_TAG_DESCRIPTION,
    WEBRADIO_TAG_URIS,
    WEBRADIO_TAG_BITRATE,
    WEBRADIO_TAG_CODEC,
    WEBRADIO_TAG_GENRES,
    WEBRADIO_TAG_LANGUAGES,
    WEBRADIO_TAG_ADDED,
    WEBRADIO_TAG_LASTMODIFIED,
    WEBRADIO_TAG_COUNT
};

/**
 * Struct holding webradio tag types
 */
struct t_webradio_tags {
    size_t len;                                       //!< number of tags in the array
    enum webradio_tag_type tags[WEBRADIO_TAG_COUNT];  //!< tags array
};

/**
 * Holds the webradio data
 */
struct t_webradio_data {
    sds name;                   //!< Station name
    sds image;                  //!< Station image
    sds homepage;               //!< Homepage
    sds country;                //!< Country
    sds region;                 //!< State or region
    sds description;            //!< Short description
    struct t_list uris;         //!< List of Uris (uri, bitrate and codec)
    struct t_list genres;       //!< List of genres
    struct t_list languages;    //!< List of languages
    enum webradio_type type;    //!< Type of the webradio
    time_t added;               //!< Added timestamp
    time_t last_modified;       //!< Last modified timestamp
};

struct t_webradio_data *webradio_by_uri(struct t_webradios *webradio_favorites, struct t_webradios *webradiodb,
        const char *uri);
sds webradio_get_extm3u(struct t_webradios *webradio_favorites, struct t_webradios *webradiodb, sds buffer, sds uri);
struct t_webradio_data *webradio_data_new(enum webradio_type type);
void webradio_data_free(struct t_webradio_data *data);
sds webradio_get_cover_uri(struct t_webradio_data *webradio, sds buffer);
enum webradio_tag_type webradio_tag_name_parse(const char *name);
void webradio_tags_search(struct t_webradio_tags *tags);
const char *webradio_type_name(enum webradio_type type);
const char *webradio_get_tag(const struct t_webradio_data *webradio, enum webradio_tag_type tag_type, unsigned int idx);
sds webradio_to_extm3u(const struct t_webradio_data *webradio, sds buffer, const char *uri);

struct t_webradios *webradios_new(void);
void webradios_clear(struct t_webradios *webradios, bool init_rax);
void webradios_free(struct t_webradios *webradios);
bool webradios_get_read_lock(struct t_webradios *webradios);
bool webradios_get_write_lock(struct t_webradios *webradios);
bool webradios_release_lock(struct t_webradios *webradios);
bool webradios_save_to_disk(struct t_config *config, struct t_webradios *webradios, const char *filename);
bool webradios_read_from_disk(struct t_config *config, struct t_webradios *webradios, const char *filename, enum webradio_type type);

#endif
