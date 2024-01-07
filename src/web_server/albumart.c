/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"
#include "src/web_server/albumart.h"

#include "src/lib/api.h"
#include "src/lib/covercache.h"
#include "src/lib/filehandler.h"
#include "src/lib/jsonrpc.h"
#include "src/lib/log.h"
#include "src/lib/m3u.h"
#include "src/lib/mimetype.h"
#include "src/lib/msg_queue.h"
#include "src/lib/sds_extras.h"
#include "src/lib/utility.h"
#include "src/lib/validate.h"

#include <libgen.h>

//optional includes
#ifdef MYMPD_ENABLE_LIBID3TAG
    #include <id3tag.h>
#endif

#ifdef MYMPD_ENABLE_FLAC
    #include <FLAC/metadata.h>
#endif

/**
 * Privat definitions
 */
static bool handle_coverextract(struct mg_connection *nc, sds cachedir, const char *uri, const char *media_file, bool covercache, int offset);
static bool handle_coverextract_id3(sds cachedir, const char *uri, const char *media_file, sds *binary, bool covercache, int offset);
static bool handle_coverextract_flac(sds cachedir, const char *uri, const char *media_file, sds *binary, bool is_ogg, bool covercache, int offset);

/**
 * Public functions
 */

/**
 * Sends the albumart redirect response from the albumart_by_albumid handler
 * @param nc mongoose connection
 * @param data jsonrpc response
 */
void webserver_send_albumart_redirect(struct mg_connection *nc, sds data) {
    sds uri = NULL;
    unsigned size;
    if (json_get_string_max(data, "$.result.uri", &uri, vcb_isuri, NULL) == true &&
        json_get_uint(data, "$.result.size", 0, 1, &size, NULL) == true)
    {
        sds redirect_uri = size == ALBUMART_THUMBNAIL
            ? sdscatfmt(sdsempty(),"/albumart-thumb?offset=0&uri=")
            : sdscatfmt(sdsempty(),"/albumart?offset=0&uri=");
        redirect_uri = sds_urlencode(redirect_uri, uri, sdslen(uri));
        MYMPD_LOG_DEBUG(NULL, "Sending redirect to: %s", redirect_uri);
        webserver_send_header_found(nc, redirect_uri);
        FREE_SDS(redirect_uri);
    }
    else {
        webserver_serve_na_image(nc);
    }
    FREE_SDS(uri);
}

/**
 * Sends the albumart response from mpd to the client
 * @param nc mongoose connection
 * @param data jsonrpc response
 * @param binary the image
 */
void webserver_send_albumart(struct mg_connection *nc, sds data, sds binary) {
    size_t len = sdslen(binary);
    sds mime_type = NULL;
    if (len > 0 &&
        json_get_string(data, "$.result.mime_type", 1, 200, &mime_type, vcb_isname, NULL) == true &&
        strncmp(mime_type, "image/", 6) == 0)
    {
        MYMPD_LOG_DEBUG(NULL, "Serving albumart from memory (%s - %lu bytes) (%lu)", mime_type, (unsigned long)len, nc->id);
        sds headers = sdscatfmt(sdsempty(), "Content-Type: %S\r\n", mime_type);
        headers = sdscat(headers, EXTRA_HEADERS_IMAGE);
        webserver_send_data(nc, binary, len, headers);
        FREE_SDS(headers);
    }
    else {
        webserver_serve_na_image(nc);
    }
    FREE_SDS(mime_type);
}

/**
 * Request handler for /albumart/albumid and /albumart-thumb/albumid.
 * Sends the request to the mympd_api thread.
 * @param hm http message
 * @param conn_id connection id
 * @param size size of the albumart
 */
void request_handler_albumart_by_album_id(struct mg_http_message *hm, long long conn_id, enum albumart_sizes size) {
    sds albumid = sdsnewlen(hm->uri.ptr, hm->uri.len);
    basename_uri(albumid);
    MYMPD_LOG_DEBUG(NULL, "Sending getalbumart to mpd_client_queue");
    struct t_work_request *request = create_request(conn_id, 0, INTERNAL_API_ALBUMART_BY_ALBUMID, NULL, MPD_PARTITION_DEFAULT);
    request->data = tojson_sds(request->data, "albumid", albumid, true);
    request->data = tojson_uint(request->data, "size", size, false);
    request->data = jsonrpc_end(request->data);
    mympd_queue_push(mympd_api_queue, request, 0);
    FREE_SDS(albumid);
}

/**
 * Request handler for /albumart and /albumart-thumb
 * @param nc mongoose connection
 * @param hm http message
 * @param mg_user_data pointer to mongoose configuration
 * @param conn_id connection id
 * @param size albumart size
 * @return true: an image was served,
 *         false: request was sent to the mympd_api thread to get the image by MPD
 */
bool request_handler_albumart_by_uri(struct mg_connection *nc, struct mg_http_message *hm,
        struct t_mg_user_data *mg_user_data, long long conn_id, enum albumart_sizes size)
{
    struct t_config *config = mg_user_data->config;
    sds query = sdsnewlen(hm->query.ptr, hm->query.len);
    sds uri_decoded = sdsempty();
    int offset = 0;
    if (sdslen(query) > 13 &&
        strncmp(query, "offset=", 7) == 0)
    {
        //remove offset=
        sdsrange(query, 7, -1);
        offset = sds_toimax(query);
        if (sdslen(query) > 5 &&
            strncmp(query, "&uri=", 5) == 0)
        {
            //remove &uri=
            sdsrange(query, 5, -1);
            //decode uri
            uri_decoded = sds_urldecode(uri_decoded, query, sdslen(query), false);
        }
    }
    FREE_SDS(query);
    if (sdslen(uri_decoded) == 0) {
        MYMPD_LOG_ERROR(NULL, "Failed to decode query");
        webserver_serve_na_image(nc);
        FREE_SDS(uri_decoded);
        return true;
    }

    if (is_streamuri(uri_decoded) == false &&
        vcb_isfilepath(uri_decoded) == false)
    {
        MYMPD_LOG_ERROR(NULL, "Invalid URI: %s", uri_decoded);
        webserver_serve_na_image(nc);
        FREE_SDS(uri_decoded);
        return true;
    }

    MYMPD_LOG_DEBUG(NULL, "Handle albumart for uri \"%s\", offset %d", uri_decoded, offset);

    //check for cover in /pics/thumbs/ and webradio m3u
    if (is_streamuri(uri_decoded) == true) {
        if (sdslen(uri_decoded) == 0) {
            MYMPD_LOG_ERROR(NULL, "Uri to short");
            webserver_serve_na_image(nc);
            FREE_SDS(uri_decoded);
            return true;
        }
        sanitize_filename(uri_decoded);

        sds coverfile = sdscatfmt(sdsempty(), "%S/%s/%S", config->workdir, DIR_WORK_PICS_THUMBS, uri_decoded);
        MYMPD_LOG_DEBUG(NULL, "Check for stream cover \"%s\"", coverfile);
        coverfile = webserver_find_image_file(coverfile);

        if (sdslen(coverfile) == 0) {
            //no coverfile found, next try to find a webradio m3u
            sds webradio_file = sdscatfmt(sdsempty(), "%S/%s/%S.m3u", config->workdir, DIR_WORK_WEBRADIOS, uri_decoded);
            MYMPD_LOG_DEBUG(NULL, "Check for webradio playlist \"%s\"", webradio_file);
            if (testfile_read(webradio_file) == true) {
                sds extimg = m3u_get_field(sdsempty(), "#EXTIMG", webradio_file);
                if (is_streamuri(extimg) == true) {
                    //full uri, send redirect to covercache proxy
                    //use relative path to support hosting myMPD behind a reverse proxy in a subdir
                    sds redirect_uri = sdsnew("proxy-covercache?uri=");
                    redirect_uri = sds_urlencode(redirect_uri, extimg, sdslen(extimg));
                    webserver_send_header_found(nc, redirect_uri);
                    FREE_SDS(redirect_uri);
                    FREE_SDS(uri_decoded);
                    FREE_SDS(coverfile);
                    FREE_SDS(extimg);
                    FREE_SDS(webradio_file);
                    return true;
                }
                if (sdslen(extimg) > 0) {
                    //local coverfile
                    coverfile = sdscatfmt(sdsempty(), "%S/%s/%S", config->workdir, DIR_WORK_PICS_THUMBS, extimg);
                }
                FREE_SDS(extimg);
            }
            FREE_SDS(webradio_file);
        }
        if (sdslen(coverfile) > 0) {
            //found a local coverfile
            const char *mime_type = get_mime_type_by_ext(coverfile);
            MYMPD_LOG_DEBUG(NULL, "Serving file \"%s\" (%s)", coverfile, mime_type);
            static struct mg_http_serve_opts s_http_server_opts;
            s_http_server_opts.root_dir = mg_user_data->browse_directory;
            s_http_server_opts.extra_headers = EXTRA_HEADERS_CACHE;
            s_http_server_opts.mime_types = EXTRA_MIME_TYPES;
            mg_http_serve_file(nc, hm, coverfile, &s_http_server_opts);
            webserver_handle_connection_close(nc);
        }
        else {
            //serve fallback image
            webserver_serve_stream_image(nc);
        }
        FREE_SDS(uri_decoded);
        FREE_SDS(coverfile);
        return true;
    }

    //check covercache
    if (check_covercache(nc, hm, mg_user_data, uri_decoded, offset) == true) {
        FREE_SDS(uri_decoded);
        return true;
    }

    if (sdslen(mg_user_data->music_directory) > 0) {
        //create absolute file
        sds mediafile = sdscatfmt(sdsempty(), "%S/%S", mg_user_data->music_directory, uri_decoded);
        MYMPD_LOG_DEBUG(NULL, "Absolut media_file: %s", mediafile);
        //try image in folder under music_directory
        if (mg_user_data->coverimage_names_len > 0 &&
            offset == 0)
        {
            sds path = sdsdup(uri_decoded);
            path = sds_dirname(path);
            if (is_virtual_cuedir(mg_user_data->music_directory, path) == true) {
                //fix virtual cue sheet directories
                path = sds_dirname(path);
            }
            bool found = false;
            sds coverfile = sdsempty();
            if (size == ALBUMART_THUMBNAIL) {
                //thumbnail images
                for (int j = 0; j < mg_user_data->thumbnail_names_len; j++) {
                    coverfile = sdscatfmt(coverfile, "%S/%S/%S", mg_user_data->music_directory, path, mg_user_data->thumbnail_names[j]);
                    if (strchr(mg_user_data->thumbnail_names[j], '.') == NULL) {
                        //basename, try extensions
                        coverfile = webserver_find_image_file(coverfile);
                    }
                    if (sdslen(coverfile) > 0 &&
                        testfile_read(coverfile) == true)
                    {
                        found = true;
                        break;
                    }
                    sdsclear(coverfile);
                }
            }
            if (found == false) {
                for (int j = 0; j < mg_user_data->coverimage_names_len; j++) {
                    coverfile = sdscatfmt(coverfile, "%S/%S/%S", mg_user_data->music_directory, path, mg_user_data->coverimage_names[j]);
                    if (strchr(mg_user_data->coverimage_names[j], '.') == NULL) {
                        //basename, try extensions
                        coverfile = webserver_find_image_file(coverfile);
                    }
                    if (sdslen(coverfile) > 0 &&
                        testfile_read(coverfile) == true)
                    {
                        found = true;
                        break;
                    }
                    sdsclear(coverfile);
                }
            }
            if (found == true) {
                const char *mime_type = get_mime_type_by_ext(coverfile);
                MYMPD_LOG_DEBUG(NULL, "Serving file %s (%s)", coverfile, mime_type);
                static struct mg_http_serve_opts s_http_server_opts;
                s_http_server_opts.root_dir = mg_user_data->browse_directory;
                s_http_server_opts.extra_headers = EXTRA_HEADERS_IMAGE;
                s_http_server_opts.mime_types = EXTRA_MIME_TYPES;
                mg_http_serve_file(nc, hm, coverfile, &s_http_server_opts);
                webserver_handle_connection_close(nc);
                FREE_SDS(uri_decoded);
                FREE_SDS(coverfile);
                FREE_SDS(mediafile);
                FREE_SDS(path);
                return true;
            }

            FREE_SDS(coverfile);
            MYMPD_LOG_DEBUG(NULL, "No cover file found in music directory");
            FREE_SDS(path);
        }

        if (testfile_read(mediafile) == true)
        {
            //try to extract albumart from media file
            bool covercache = mg_user_data->config->covercache_keep_days > 0 ? true : false;
            bool rc = handle_coverextract(nc, config->cachedir, uri_decoded, mediafile, covercache, offset);
            if (rc == true) {
                FREE_SDS(uri_decoded);
                FREE_SDS(mediafile);
                return true;
            }
        }
        FREE_SDS(mediafile);
    }

    //ask mpd - mpd can read only first image
    if (mg_user_data->feat_albumart == true &&
        offset == 0)
    {
        MYMPD_LOG_DEBUG(NULL, "Sending getalbumart to mpd_client_queue");
        struct t_work_request *request = create_request(conn_id, 0, INTERNAL_API_ALBUMART_BY_URI, NULL, MPD_PARTITION_DEFAULT);
        request->data = tojson_sds(request->data, "uri", uri_decoded, false);
        request->data = jsonrpc_end(request->data);
        mympd_queue_push(mympd_api_queue, request, 0);
        FREE_SDS(uri_decoded);
        return false;
    }

    MYMPD_LOG_INFO(NULL, "No coverimage found for \"%s\"", uri_decoded);
    FREE_SDS(uri_decoded);
    webserver_serve_na_image(nc);
    return true;
}

/**
 * Extracts albumart from media files
 * @param nc mongoose connection
 * @param cachedir covercache directory
 * @param uri song uri
 * @param media_file full path to the song
 * @param covercache true = covercache is enabled
 * @param offset number of embedded image to extract
 * @return true on success, else false
 */
static bool handle_coverextract(struct mg_connection *nc, sds cachedir,
        const char *uri, const char *media_file, bool covercache, int offset)
{
    bool rc = false;
    const char *mime_type_media_file = get_mime_type_by_ext(media_file);
    MYMPD_LOG_DEBUG(NULL, "Handle coverextract for uri \"%s\"", uri);
    MYMPD_LOG_DEBUG(NULL, "Mimetype of %s is %s", media_file, mime_type_media_file);
    sds binary = sdsempty();
    if (strcmp(mime_type_media_file, "audio/mpeg") == 0) {
        rc = handle_coverextract_id3(cachedir, uri, media_file, &binary, covercache, offset);
    }
    else if (strcmp(mime_type_media_file, "audio/ogg") == 0) {
        rc = handle_coverextract_flac(cachedir, uri, media_file, &binary, true, covercache, offset);
    }
    else if (strcmp(mime_type_media_file, "audio/flac") == 0) {
        rc = handle_coverextract_flac(cachedir, uri, media_file, &binary, false, covercache, offset);
    }
    if (rc == true) {
        const char *mime_type = get_mime_type_by_magic_stream(binary);
        MYMPD_LOG_DEBUG(NULL, "Serving coverimage for \"%s\" (%s)", media_file, mime_type);
        sds headers = sdscatfmt(sdsempty(), "Content-Type: %s\r\n", mime_type);
        headers = sdscat(headers, EXTRA_HEADERS_IMAGE);
        webserver_send_data(nc, binary,  sdslen(binary), headers);
        FREE_SDS(headers);
    }
    FREE_SDS(binary);
    return rc;
}

/**
 * Extracts albumart from id3v2 tagged files
 * @param cachedir covercache directory
 * @param uri song uri
 * @param media_file full path to the song
 * @param binary pointer to already allocates sds string to hold the image
 * @param covercache true = covercache is enabled
 * @param offset number of embedded image to extract
 * @return true on success, else false
 */
static bool handle_coverextract_id3(sds cachedir, const char *uri, const char *media_file,
        sds *binary, bool covercache, int offset)
{
    bool rc = false;
    #ifdef MYMPD_ENABLE_LIBID3TAG
    MYMPD_LOG_DEBUG(NULL, "Exctracting coverimage from %s", media_file);
    struct id3_file *file_struct = id3_file_open(media_file, ID3_FILE_MODE_READONLY);
    if (file_struct == NULL) {
        MYMPD_LOG_ERROR(NULL, "Can't parse id3_file: %s", media_file);
        return false;
    }
    struct id3_tag *tags = id3_file_tag(file_struct);
    if (tags == NULL) {
        MYMPD_LOG_ERROR(NULL, "Can't read id3 tags from file: %s", media_file);
        return false;
    }
    struct id3_frame *frame = id3_tag_findframe(tags, "APIC", (unsigned)offset);
    if (frame != NULL) {
        id3_length_t length = 0;
        const id3_byte_t *pic = id3_field_getbinarydata(id3_frame_field(frame, 4), &length);
        if (length > 0) {
            *binary = sdscatlen(*binary, pic, length);
            const char *mime_type = get_mime_type_by_magic_stream(*binary);
            if (mime_type != NULL) {
                if (covercache == true) {
                    covercache_write_file(cachedir, uri, mime_type, *binary, offset);
                }
                else {
                    MYMPD_LOG_DEBUG(NULL, "Covercache is disabled");
                }
                MYMPD_LOG_DEBUG(NULL, "Coverimage successfully extracted (%lu bytes)", (unsigned long)sdslen(*binary));
                rc = true;
            }
            else {
                MYMPD_LOG_WARN(NULL, "Could not determine mimetype, discarding image");
                sdsclear(*binary);
            }
        }
        else {
            MYMPD_LOG_WARN(NULL, "Embedded picture size is zero");
        }
    }
    else {
        MYMPD_LOG_DEBUG(NULL, "No embedded picture detected");
    }
    id3_file_close(file_struct);
    #else
    (void) cachedir;
    (void) uri;
    (void) media_file;
    (void) binary;
    (void) covercache;
    (void) offset;
    #endif
    return rc;
}

/**
 * Extracts albumart from vorbis tagged files
 * @param cachedir covercache directory
 * @param uri song uri
 * @param media_file full path to the song
 * @param binary pointer to already allocates sds string to hold the image
 * @param is_ogg true if it is a ogg file, false if it is a flac file
 * @param covercache true = covercache is enabled
 * @param offset number of embedded image to extract
 * @return true on success, else false
 */
static bool handle_coverextract_flac(sds cachedir, const char *uri, const char *media_file,
        sds *binary, bool is_ogg, bool covercache, int offset)
{
    bool rc = false;
    #ifdef MYMPD_ENABLE_FLAC
    MYMPD_LOG_DEBUG(NULL, "Exctracting coverimage from %s", media_file);
    FLAC__StreamMetadata *metadata = NULL;

    FLAC__Metadata_Chain *chain = FLAC__metadata_chain_new();

    if(! (is_ogg? FLAC__metadata_chain_read_ogg(chain, media_file) : FLAC__metadata_chain_read(chain, media_file)) ) {
        MYMPD_LOG_ERROR(NULL, "Error reading metadata from \"%s\"", media_file);
        FLAC__metadata_chain_delete(chain);
        return false;
    }

    FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();
    FLAC__metadata_iterator_init(iterator, chain);
    if (iterator == NULL) {
        MYMPD_LOG_ERROR(NULL, "Error initializing iterator for \"%s\"", media_file);
        FLAC__metadata_chain_delete(chain);
        return false;
    }
    int i = 0;
    do {
        FLAC__StreamMetadata *block = FLAC__metadata_iterator_get_block(iterator);
        if (block->type == FLAC__METADATA_TYPE_PICTURE) {
            if (i == offset) {
                metadata = block;
                break;
            }
            i++;
        }
    } while (FLAC__metadata_iterator_next(iterator) && metadata == NULL);

    if (metadata == NULL) {
        MYMPD_LOG_DEBUG(NULL, "No embedded picture detected");
    }
    else if (metadata->data.picture.data_length > 0) {
        *binary = sdscatlen(*binary, metadata->data.picture.data, metadata->data.picture.data_length);
        const char *mime_type = get_mime_type_by_magic_stream(*binary);
        if (mime_type != NULL) {
            if (covercache == true) {
                covercache_write_file(cachedir, uri, mime_type, *binary, offset);
            }
            else {
                MYMPD_LOG_DEBUG(NULL, "Covercache is disabled");
            }
            MYMPD_LOG_DEBUG(NULL, "Coverimage successfully extracted (%lu bytes)", (unsigned long)sdslen(*binary));
            rc = true;
        }
        else {
            MYMPD_LOG_WARN(NULL, "Could not determine mimetype, discarding image");
            sdsclear(*binary);
        }
    }
    else {
        MYMPD_LOG_WARN(NULL, "Embedded picture size is zero");
    }
    FLAC__metadata_iterator_delete(iterator);
    FLAC__metadata_chain_delete(chain);
    #else
    (void) cachedir;
    (void) uri;
    (void) media_file;
    (void) binary;
    (void) is_ogg;
    (void) covercache;
    (void) offset;
    #endif
    return rc;
}
