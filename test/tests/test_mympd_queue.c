/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"

#include "dist/utest/utest.h"
#include "src/lib/api.h"
#include "src/lib/msg_queue.h"
#include "src/lib/sds_extras.h"

UTEST(mympd_queue, push_shift) {
    struct t_mympd_queue *test_queue = mympd_queue_create("test", QUEUE_TYPE_REQUEST);
    sds test_data_in0 = sdsnew("test0");
    sds test_data_in1 = sdsnew("test0");
    sds test_data_in2 = sdsnew("test0");

    sds test_data_out = NULL;

    mympd_queue_push(test_queue, test_data_in0, 0);
    test_data_out = mympd_queue_shift(test_queue, 50, 0);
    ASSERT_STREQ(test_data_in0, test_data_out);

    ASSERT_EQ(0, test_queue->length);

    mympd_queue_push(test_queue, test_data_in1, 0);
    mympd_queue_push(test_queue, test_data_in2, 0);
    test_data_out = NULL;
    test_data_out = mympd_queue_shift(test_queue, 50, 0);
    ASSERT_STREQ(test_data_in1, test_data_out);

    test_data_out = NULL;
    test_data_out = mympd_queue_shift(test_queue, 50, 0);
    ASSERT_STREQ(test_data_in2, test_data_out);

    ASSERT_EQ(0, test_queue->length);

    mympd_queue_free(test_queue);
    sdsfree(test_data_in0);
    sdsfree(test_data_in1);
    sdsfree(test_data_in2);
}

UTEST(mympd_queue, push_shift_id) {
    struct t_mympd_queue *test_queue = mympd_queue_create("test", QUEUE_TYPE_REQUEST);
    sds test_data_in0 = sdsnew("test0");
    sds test_data_in1 = sdsnew("test0");
    sds test_data_in2 = sdsnew("test0");

    sds test_data_out = NULL;

    mympd_queue_push(test_queue, test_data_in0, 10);
    mympd_queue_push(test_queue, test_data_in1, 20);
    mympd_queue_push(test_queue, test_data_in2, 10);

    ASSERT_EQ(3, test_queue->length);

    test_data_out = NULL;
    test_data_out = mympd_queue_shift(test_queue, 50, 20);
    ASSERT_STREQ(test_data_in1, test_data_out);

    test_data_out = NULL;
    test_data_out = mympd_queue_shift(test_queue, 50, 10);
    ASSERT_STREQ(test_data_in0, test_data_out);

    test_data_out = NULL;
    test_data_out = mympd_queue_shift(test_queue, 50, 10);
    ASSERT_STREQ(test_data_in2, test_data_out);

    ASSERT_EQ(0, test_queue->length);

    mympd_queue_free(test_queue);
    sdsfree(test_data_in0);
    sdsfree(test_data_in1);
    sdsfree(test_data_in2);
}

UTEST(mympd_queue, expire) {
    struct t_mympd_queue *test_queue = mympd_queue_create("test", QUEUE_TYPE_REQUEST);
    for (int i = 0; i < 50; i++) {
        struct t_work_request *request = create_request(0, 0, MYMPD_API_COLS_SAVE, "test", MPD_PARTITION_DEFAULT);
        request->extra = malloc(10);
        mympd_queue_push(test_queue, request, 10);
    }
    ASSERT_EQ(50, test_queue->length);

    //manually overwrite timestamp for first entry
    struct t_mympd_msg *current = test_queue->head;
    current->timestamp = time(NULL) - 100;

    int expired = mympd_queue_expire(test_queue, 50);
    ASSERT_EQ(1, expired);
    ASSERT_EQ(49, test_queue->length);

    //manually overwrite other entries
    current = test_queue->head;
    current = current->next;
    current->timestamp = time(NULL) - 100;
    current = current->next;
    current = current->next;
    current->timestamp = time(NULL) - 100;
    current = current->next;
    current->timestamp = time(NULL) - 100;

    expired = mympd_queue_expire(test_queue, 50);
    ASSERT_EQ(3, expired);
    ASSERT_EQ(46, test_queue->length);

    //manually overwrite timestamp for last entry
    current = test_queue->tail;
    current->timestamp = time(NULL) - 100;

    expired = mympd_queue_expire(test_queue, 50);
    ASSERT_EQ(1, expired);
    ASSERT_EQ(45, test_queue->length);

    mympd_queue_free(test_queue);
}
