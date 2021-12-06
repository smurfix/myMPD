/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "mympd_config_defs.h"

#include "../../dist/utest/utest.h"
#include "../../src/lib/jsonrpc.h"
#include "../../src/lib/list.h"
#include "../../src/mpd_shared/mpd_shared_tags.h"

UTEST(jsonrpc, test_json_get_bool) {
    bool result;
    //valid
    sds data = sdsnew("{\"key1\": true}");
    ASSERT_TRUE(json_get_bool(data, "$.key1", &result, NULL));
    sdsclear(data);
    //invalid
    data = sdscat(data, "{\"key1\": \"true\"}");
    ASSERT_FALSE(json_get_bool(data, "", &result, NULL));
    sdsfree(data);
}

UTEST(jsonrpc, test_json_get_int) {
    int result;
    //valid
    sds data = sdsnew("{\"key1\": 10}");
    ASSERT_TRUE(json_get_int(data, "$.key1", 0, 20, &result, NULL));
    sdsclear(data);
    data = sdscat(data, "{\"key1\": -30}");
    ASSERT_TRUE(json_get_int(data, "$.key1", -50, 20, &result, NULL));
    sdsclear(data);
    //invalid
    data = sdscat(data, "{\"key1\": 30}");
    ASSERT_FALSE(json_get_int(data, "$.key1", 0, 20, &result, NULL));
    sdsclear(data);
    data = sdscat(data, "{\"key2\": 10}");
    ASSERT_FALSE(json_get_int(data, "$.key1", 0, 20, &result, NULL));
    sdsfree(data);
}

UTEST(jsonrpc, test_json_get_int_max) {
    int result;
    //valid
    sds data = sdsnew("{\"key1\": 10}");
    ASSERT_TRUE(json_get_int_max(data, "$.key1", &result, NULL));
    sdsclear(data);
    //invalid
    data = sdscat(data, "{\"key2\": 10}");
    ASSERT_FALSE(json_get_int_max(data, "$.key1", &result, NULL));
    sdsfree(data);
}

UTEST(jsonrpc, test_json_get_uint) {
    unsigned result;
    //valid
    sds data = sdsnew("{\"key1\": 10}");
    ASSERT_TRUE(json_get_uint(data, "$.key1", 0, 20, &result, NULL));
    sdsclear(data);
    data = sdscat(data, "{\"key1\": -30}");
    ASSERT_FALSE(json_get_uint(data, "$.key1", 0, 20, &result, NULL));
    sdsclear(data);
    //invalid
    data = sdscat(data, "{\"key1\": 30}");
    ASSERT_FALSE(json_get_uint(data, "$.key1", 0, 20, &result, NULL));
    sdsclear(data);
    data = sdscat(data, "{\"key2\": 10}");
    ASSERT_FALSE(json_get_uint(data, "$.key1", 0, 20, &result, NULL));
    sdsfree(data);
}

UTEST(jsonrpc, test_json_get_uint_max) {
    unsigned result;
    //valid
    sds data = sdsnew("{\"key1\": 10}");
    ASSERT_TRUE(json_get_uint_max(data, "$.key1", &result, NULL));
    sdsclear(data);
    //invalid
    data = sdscat(data, "{\"key2\": 10}");
    ASSERT_FALSE(json_get_uint_max(data, "$.key1", &result, NULL));
    sdsclear(data);
    data = sdscat(data, "{\"key1\": -10}");
    ASSERT_FALSE(json_get_uint_max(data, "$.key1", &result, NULL));
    sdsfree(data);
}

UTEST(jsonrpc, test_json_get_string) {
    sds result = NULL;
    //valid
    sds data = sdsnew("{\"key1\": \"blafblasdf\"}");
    ASSERT_TRUE(json_get_string(data, "$.key1", 0, 20, &result, vcb_isname, NULL));
    sdsclear(data);
    sdsfree(result);
    result = NULL;
    //invalid
    data = sdscat(data, "{\"key1\": 30}");
    ASSERT_FALSE(json_get_string(data, "$.key1", 0, 20, &result, vcb_isname, NULL));
    sdsclear(data);
    sdsfree(result);
    result = NULL;
    data = sdscat(data, "{\"key1\": \"asdfawerwerwq3e3243sf\"}");
    ASSERT_FALSE(json_get_string(data, "$.key1", 0, 10, &result, vcb_isname, NULL));
    sdsfree(result);
    result = NULL;
    data = sdscat(data, "{\"key1\": \"\"}");
    ASSERT_FALSE(json_get_string(data, "$.key1", 1, 10, &result, vcb_isname, NULL));
    sdsfree(result);
    result = NULL;
    data = sdscat(data, "{\"key2\": \"asdfawerwerwq3e3243sf\"}");
    ASSERT_FALSE(json_get_string(data, "$.key1", 0, 10, &result, vcb_isname, NULL));
    sdsfree(result);
    sdsfree(data);
}

UTEST(jsonrpc, test_json_get_string_max) {
    sds result = NULL;
    //valid
    sds data = sdsnew("{\"key1\": \"blafblasdf\"}");
    ASSERT_TRUE(json_get_string_max(data, "$.key1", &result, vcb_isname, NULL));
    sdsfree(result);
    sdsfree(data);
}

UTEST(jsonrpc, test_json_get_string_cmp) {
    sds result = NULL;
    //valid
    sds data = sdsnew("{\"key1\": \"tocompare\"}");
    ASSERT_TRUE(json_get_string_cmp(data, "$.key1", 0, 20, "tocompare", &result, NULL));
    sdsfree(result);
    result = NULL;
    //invalid
    sdsclear(data);
    data = sdscat(data, "{\"key1\": \"\"}");
    ASSERT_FALSE(json_get_string_cmp(data, "$.key1", 0, 20, "tocompare", &result, NULL));
    sdsfree(result);
    sdsfree(data);
}

UTEST(jsonrpc, test_json_get_array_string) {
    struct t_list l;
    list_init(&l);
    sds data = sdsnew("{\"key1\": [\"string1\", \"string2\"]}");
    //valid
    ASSERT_TRUE(json_get_array_string(data, "$.key1", &l, vcb_isname, 10, NULL));
    list_clear(&l);
    //invalid - validation error
    ASSERT_FALSE(json_get_array_string(data, "$.key1", &l, vcb_ishexcolor, 10, NULL));
    list_clear(&l);
    //invalid - too many array elements
    ASSERT_TRUE(json_get_array_string(data, "$.key1", &l, vcb_isname, 1, NULL));
    ASSERT_EQ(1, l.length);
    sdsfree(data);
    list_clear(&l);
}

UTEST(jsonrpc, test_json_get_object_string) {
    struct t_list l;
    list_init(&l);
    //valid
    sds data = sdsnew("{\"key1\": {\"k1\": \"string1\", \"k2\": \"string2\"}}");
    ASSERT_TRUE(json_get_object_string(data, "$.key1", &l, vcb_isname, 10, NULL));
    list_clear(&l);
    //invalid - validation error
    ASSERT_FALSE(json_get_object_string(data, "$.key1", &l, vcb_ishexcolor, 10, NULL));
    list_clear(&l);
    //invalid - too many array elements
    ASSERT_TRUE(json_get_object_string(data, "$.key1", &l, vcb_isname, 1, NULL));
    ASSERT_EQ(1, l.length);
    sdsfree(data);
    list_clear(&l);
}

UTEST(jsonrpc, test_json_get_tags) {
    struct t_tags tagcols;
    reset_t_tags(&tagcols);
    sds data = sdsnew("{\"params\": {\"cols\": [\"Artist\", \"Duration\"]}}");
    //valid
    ASSERT_TRUE(json_get_tags(data, "$.params.cols", &tagcols, MAX_COLS, NULL));
    sdsclear(data);
    reset_t_tags(&tagcols);
    data = sdscat(data, "{\"params\": {\"cols\": [\"Artist\", \"Invalid column name\"]}}");
    //invalid column names are ignored
    ASSERT_TRUE(json_get_tags(data, "$.params.cols", &tagcols, MAX_COLS, NULL));
    sdsfree(data);
}

UTEST(jsonrpc, test_list_to_json_array) {
    struct t_list l;
    list_init(&l);
    list_push(&l, "key1", 0, NULL, NULL);
    list_push(&l, "key2", 0, NULL, NULL);
    sds s = sdsempty();
    s = list_to_json_array(s, &l);
    ASSERT_STREQ("\"key1\",\"key2\"", s);
    list_clear(&l);
    sdsfree(s);
}

UTEST(jsonrpc, test_json_get_cols_as_string) {
    struct t_list l;
    list_init(&l);
    bool error;
    sds data = sdsnew("{\"params\": {\"cols\": [\"Artist\", \"Duration\"]}}");
    sds cols = sdsempty();
    cols = json_get_cols_as_string(data, cols, &error);
    ASSERT_STREQ("\"Artist\",\"Duration\"", cols);
    list_clear(&l);
    sdsfree(cols);
    sdsfree(data);
}
