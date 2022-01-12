/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2022 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"
#include "sds_extras.h"

#include "../../dist/utf8/utf8.h"
#include "log.h"

#include <ctype.h>
#include <string.h>

#define HEXTOI(x) (x >= '0' && x <= '9' ? x - '0' : x - 'W')

void sds_utf8_tolower(sds s) {
    utf8_int32_t cp;

    void *pn = utf8codepoint(s, &cp);
    while (cp != 0) {
        const size_t size = utf8codepointsize(cp);
        const utf8_int32_t lwr_cp = utf8lwrcodepoint(cp);
        const size_t lwr_size = utf8codepointsize(lwr_cp);

        if (lwr_cp != cp && lwr_size == size) {
            utf8catcodepoint(s, lwr_cp, lwr_size);
        }

        s = pn;
        pn = utf8codepoint(s, &cp);
    }
}

sds sds_catjson(sds s, const char *p, size_t len) {
    /* To avoid continuous reallocations, let's start with a buffer that
     * can hold at least stringlength + 10 chars. */
    s = sdsMakeRoomFor(s, len + 10);

    s = sdscatlen(s, "\"", 1);
    while (len--) {
        s = sds_catjsonchar(s, *p);
        p++;
    }
    return sdscatlen(s, "\"", 1);
}


sds sds_catjsonchar(sds s, const char p) {
    switch(p) {
        case '\\':
        case '"':
            s = sdscatfmt(s, "\\%c", p);
            break;
        case '\b': s = sdscatlen(s, "\\b", 2); break;
        case '\f': s = sdscatlen(s, "\\f", 2); break;
        case '\n': s = sdscatlen(s, "\\n", 2); break;
        case '\r': s = sdscatlen(s, "\\r", 2); break;
        case '\t': s = sdscatlen(s, "\\t", 2); break;
        //ignore vertical tabulator and alert
        case '\v':
        case '\a':
            //this escapes are not accepted in the unescape function
            break;
        default:
            s = sdscatfmt(s, "%c", p);
            break;
    }
    return s;
}

bool sds_json_unescape(const char *src, int slen, sds *dst) {
    char *send = (char *) src + slen;
    char *p;
    const char *esc1 = "\"\\/bfnrt";
    const char *esc2 = "\"\\/\b\f\n\r\t";

    while (src < send) {
        if (*src == '\\') {
            if (++src >= send) {
                return false;
            }
            if (*src == 'u') {
                if (send - src < 5) {
                    return false;
                }
                src += 4;
            }
            else if ((p = strchr(esc1, *src)) != NULL) {
                *dst = sdscatfmt(*dst, "%c", esc2[p - esc1]);
            }
            else {
                //other escapes are not accepted
                return false;
            }
        }
        else {
            *dst = sdscatfmt(*dst, "%c", *src);
        }
        src++;
    }

    return true;
}

static bool is_url_safe(char c) {
    if (isalnum(c) ||
        c == '/' || c == '-' || c == '.' ||
        c == '_')
    {
        return true;
    }
    return false;
}

sds sds_urlencode(sds s, const char *p, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (is_url_safe(p[i])) {
            s = sdscatfmt(s, "%c", p[i]);
        }
        else {
            s = sdscatprintf(s, "%%%hhX", p[i]);
        }
    }
    return s;
}

sds sds_urldecode(sds s, const char *p, size_t len, int is_form_url_encoded) {
    size_t i;
    int a;
    int b;

    for (i = 0; i < len; i++) {
        switch(*p) {
            case '%':
                if (i < len - 2 && isxdigit(*(const unsigned char *) (p + 1)) &&
                    isxdigit(*(const unsigned char *) (p + 2)))
                {
                    a = tolower(*(const unsigned char *) (p + 1));
                    b = tolower(*(const unsigned char *) (p + 2));
                    s = sdscatfmt(s, "%c", (char) ((HEXTOI(a) << 4) | HEXTOI(b)));
                    i += 2;
                    p += 2;
                }
                else {
                    sdsclear(s);
                    return s;
                }
                break;
            case '+':
                if (is_form_url_encoded == 1) {
                    s = sdscatlen(s, " ", 1);
                    break;
                }
                //fall through
            default:
                s = sdscatlen(s, p, 1);
        }
        p++;
    }
    return s;
}

sds sds_replacelen(sds s, const char *value, size_t len) {
    if (s != NULL) {
        sdsclear(s);
    }
    else {
        s = sdsempty();
    }
    if (value != NULL) {
        s = sdscatlen(s, value, len);
    }
    return s;
}

sds sds_replace(sds s, const char *value) {
    return sds_replacelen(s, value, strlen(value));
}

//custom getline function
//trims line for \n \r, \t and spaces
//returns 0 on success
//-1 for EOF
//-2 for too long line

int sds_getline(sds *s, FILE *fp, size_t max) {
    sdsclear(*s);
    size_t i = 0;
    for (;;) {
        int c = fgetc(fp);
        if (c == EOF) {
            sdstrim(*s, "\r \t");
            if (sdslen(*s) > 0) {
                return 0;
            }
            return -1;
        }
        if (c == '\n') {
            sdstrim(*s, "\r \t");
            return 0;
        }
        if (i < max) {
            *s = sdscatfmt(*s, "%c", c);
            i++;
        }
        else {
            MYMPD_LOG_ERROR("Line is too long, max length is %u", max);
            return -2;
        }
    }
}

int sds_getline_n(sds *s, FILE *fp, size_t max) {
    int rc = sds_getline(s, fp, max);
    *s = sdscat(*s, "\n");
    return rc;
}

int sds_getfile(sds *s, FILE *fp, size_t max) {
    sdsclear(*s);
    size_t i = 0;
    for (;;) {
        int c = fgetc(fp);
        if (c == EOF) {
            sdstrim(*s, "\r \t\n");
            MYMPD_LOG_DEBUG("Read %u bytes from file", sdslen(*s));
            if (sdslen(*s) > 0) {
                return 0;
            }
            return -1;
        }
        if (i < max) {
            *s = sdscatfmt(*s, "%c", c);
            i++;
        }
        else {
            MYMPD_LOG_ERROR("File is too long, max length is %u", max);
            return -2;
        }
    }
}

void sds_basename_uri(sds uri) {
    const int uri_len = (int)sdslen(uri);
    int i;

    if (uri_len == 0) {
        return;
    }

    if (strstr(uri, "://") == NULL) {
        //filename, remove path
        for (i = uri_len - 1; i >= 0; i--) {
            if (uri[i] == '/') {
                break;
            }
        }
        sdsrange(uri, i + 1, -1);
        return;
    }

    //uri, remove query and hash
    for (i = 0; i < uri_len; i++) {
        if (uri[i] == '#' || uri[i] == '?') {
            break;
        }
    }
    if (i < uri_len - 1) {
        sdsrange(uri, 0, i - 1);
    }
}

void sds_strip_slash(sds s) {
    char *sp = s;
    char *ep = s + sdslen(s) - 1;
    while(ep >= sp && *ep == '/') {
        ep--;
    }
    size_t len = (sp > ep) ? 0 : ((ep - sp) + 1);
    s[len] = '\0';
    sdssetlen(s, len);
}

sds sds_get_extension_from_filename(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (ext == NULL || ext[0] == '\0') {
        return sdsempty();
    }
    sds extension = sdsnew(ext);
    //trim starting dot
    sdsrange(extension, 1, -1);
    sdstolower(extension);
    return extension;
}

void sds_strip_file_extension(sds s) {
    char *sp = s;
    char *ep = s + sdslen(s) - 1;
    while (ep >= sp) {
        if (*ep == '.') {
            size_t len = (sp > ep) ? 0 : (ep - sp);
            s[len] = '\0';
            sdssetlen(s, len);
            break;
        }
        ep --;
    }
}

sds sds_catbool(sds s, bool v) {
    return v == true ? sdscatlen(s, "true", 4) : sdscatlen(s, "false", 5);
}

static const char *invalid_filename_chars = "<>/.:?&$!#\a\b\f\n\r\t\v\\|";
void sds_sanitize_filename(sds s) {
    const size_t len = strlen(invalid_filename_chars);
    for (size_t i = 0; i < len; i++) {
        for (size_t j = 0; j < sdslen(s); j++) {
            if (s[j] == invalid_filename_chars[i]) {
                s[j] = '_';
            }
        }
    }
}
