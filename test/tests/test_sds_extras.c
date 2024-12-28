/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"
#include "utility.h"

#include "dist/utest/utest.h"
#include "src/lib/sds_extras.h"

#include <libgen.h>

const char *test_dirnames[] = {
    "/dir1/file1",
    "/dir1/file1/",
    "/dir1/dir2/file1",
    "/dir1/dir2//",
    "file1",
    "dir1/file1",
    "dir1/file1/",
    "/",
    "",
    NULL
};

UTEST(sds_extras, test_sds_dirname) {
    const char **p = test_dirnames;
    while (*p != NULL) {
        printf("Testing: \"%s\"\n", *p);

        sds dir = sdsnew(*p);
        dir = sds_dirname(dir);

        char *dir_check = strdup(*p);
        char *dir_check_org = dir_check;
        dir_check = dirname(dir_check);

        ASSERT_STREQ(dir_check, dir);
        sdsfree(dir);
        free(dir_check_org);
        p++;
    }
}

UTEST(sds_extras, test_sds_basename) {
    const char **p = test_dirnames;
    while (*p != NULL) {
        printf("Testing: \"%s\"\n", *p);

        sds dir = sdsnew(*p);
        dir = sds_basename(dir);

        char *dir_check = strdup(*p);
        char *dir_check_org = dir_check;
        dir_check = basename(dir_check);

        ASSERT_STREQ(dir_check, dir);
        sdsfree(dir);
        free(dir_check_org);
        p++;
    }
}

UTEST(sds_extras, test_sds_split_comma_trim) {
    sds names = sdsnew("cover, folder");
    int count;
    sds *array = sds_split_comma_trim(names, &count);
    ASSERT_EQ(count, 2);
    ASSERT_STREQ(array[0], "cover");
    ASSERT_STREQ(array[1], "folder");
    sdsfree(names);
    sdsfreesplitres(array, count);
}

UTEST(sds_extras, test_sds_hash_md5) {
    sds hash = sds_hash_md5("abc");
    ASSERT_STREQ("900150983cd24fb0d6963f7d28e17f72", hash);
    sdsfree(hash);
}

UTEST(sds_extras, test_sds_hash_sha1) {
    sds hash = sds_hash_sha1("abc");
    ASSERT_STREQ("a9993e364706816aba3e25717850c26c9cd0d89d", hash);
    sdsfree(hash);
}

UTEST(sds_extras, test_sds_hash_sha1_sds) {
    sds hash = sdsnew("abc");
    hash = sds_hash_sha1_sds(hash);
    ASSERT_STREQ("a9993e364706816aba3e25717850c26c9cd0d89d", hash);
    sdsfree(hash);
}

UTEST(sds_extras, test_sds_hash_sha256) {
    sds hash = sds_hash_sha256("abc");
    ASSERT_STREQ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", hash);
    sdsfree(hash);
}

UTEST(sds_extras, test_sds_hash_sha256_sds) {
    sds hash = sdsnew("abc");
    hash = sds_hash_sha256_sds(hash);
    ASSERT_STREQ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", hash);
    sdsfree(hash);
}

UTEST(sds_extras, test_sds_utf8_tolower) {
    sds test_input= sdsnew("EINSTÜRZENDE NEUBAUTEN");
    sds_utf8_tolower(test_input);
    ASSERT_STREQ("einstürzende neubauten", test_input);
    sdsclear(test_input);
    test_input = sdscat(test_input, "sdfßSdf");
    sds_utf8_tolower(test_input);
    ASSERT_STREQ("sdfßsdf", test_input);
    sdsfree(test_input);
}

UTEST(sds_extras, test_sds_catjson_plain) {
    sds s = sdsempty();
    const char *str = "test\"test";
    size_t len = strlen(str);
    s = sds_catjson_plain(s, str, len);
    ASSERT_STREQ("test\\\"test", s);
    ASSERT_EQ(strlen(s), sdslen(s));
    sdsfree(s);
}

UTEST(sds_extras, test_sds_catjson_plain_long) {
    sds s = sdsempty();
    const char *str = "\"\"\"\"\"test\"\"\"\"\"\"test\"\"\"\"";
    size_t len = strlen(str);
    s = sds_catjson_plain(s, str, len);
    ASSERT_STREQ("\\\"\\\"\\\"\\\"\\\"test\\\"\\\"\\\"\\\"\\\"\\\"test\\\"\\\"\\\"\\\"", s);
    ASSERT_EQ(strlen(s), sdslen(s));
    sdsfree(s);
}

UTEST(sds_extras, test_sds_catchar) {
    sds s = sdsempty();
    s = sds_catchar(s, 'a');
    ASSERT_STREQ("a", s);
    ASSERT_EQ(strlen(s), sdslen(s));

    s = sdscat(s, "bcdefghijklmnopqrstuvwxy");
    s = sds_catchar(s, 'z');
    ASSERT_STREQ("abcdefghijklmnopqrstuvwxyz", s);
    ASSERT_EQ(strlen(s), sdslen(s));

    sdsfree(s);
}

UTEST(sds_extras, test_sds_catjsonchar) {
    sds s = sdsempty();
    s = sds_catjsonchar(s, '\n');
    ASSERT_STREQ("\\n", s);
    ASSERT_EQ(strlen(s), sdslen(s));
    sdsfree(s);
}

UTEST(sds_extras, test_sds_catjson) {
    sds s = sdsempty();
    const char *str = "test\"test";
    size_t len = strlen(str);
    s = sds_catjson(s, str, len);
    ASSERT_STREQ("\"test\\\"test\"", s);
    ASSERT_EQ(strlen(s), sdslen(s));
    sdsfree(s);
}

UTEST(sds_extras, test_sds_json_unescape) {
    sds s = sdsempty();
    const char *str = "test\"test";
    size_t len = strlen(str);
    s = sds_catjson_plain(s, str, len);
    ASSERT_STREQ("test\\\"test", s);
    sds dst = sdsempty();
    bool rc = sds_json_unescape(s, sdslen(s), &dst);
    ASSERT_TRUE(rc);
    ASSERT_STREQ(str, dst);
    ASSERT_EQ(strlen(dst), sdslen(dst));
    sdsfree(s);
    sdsfree(dst);
}

UTEST(sds_extras, test_sds_urldecode) {
    sds test_input = sdsnew("/Musict/Led%20Zeppelin/1975%20-%20Physical%20Graffiti%20%5B1994%2C%20Atlantic%2C%207567-92442-2%5D/CD%201/folder.jpg");
    sds s = sds_urldecode(sdsempty(), test_input, sdslen(test_input), false);
    ASSERT_STREQ("/Musict/Led Zeppelin/1975 - Physical Graffiti [1994, Atlantic, 7567-92442-2]/CD 1/folder.jpg", s);
    ASSERT_EQ(strlen(s), sdslen(s));
    sdsfree(test_input);
    sdsfree(s);
}

UTEST(sds_extras, test_sds_urlencode) {
    sds test_input = sdsnew("/Musict/Led Zeppelin/1975 - Physical Graffiti [1994, Atlantic, 7567-92442-2]/CD 1/folder.jpg");
    sds s = sds_urlencode(sdsempty(), test_input, sdslen(test_input));
    ASSERT_STREQ("/Musict/Led%20Zeppelin/1975%20-%20Physical%20Graffiti%20%5B1994%2C%20Atlantic%2C%207567-92442-2%5D/CD%201/folder.jpg", s);
    ASSERT_EQ(strlen(s), sdslen(s));
    sdsfree(test_input);
    sdsfree(s);
}

UTEST(sds_extras, test_sds_replace) {
    sds s = sdsnew("old");
    s = sds_replace(s, "new");
    ASSERT_STREQ("new", s);
    sdsfree(s);
    //should also work with NULL ptr
    s = NULL;
    s = sds_replace(s, "new");
    ASSERT_STREQ("new", s);
    sdsfree(s);
}

UTEST(sds_extras, test_sds_catbool) {
    sds s = sdsempty();
    s = sds_catbool(s, true);
    ASSERT_STREQ("true", s);
    sdsfree(s);
}
