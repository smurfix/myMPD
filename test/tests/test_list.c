/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

#include "compile_time.h"
#include "utility.h"

#include "dist/utest/utest.h"
#include "src/lib/list.h"

static long populate_list(struct t_list *l) {
    list_init(l);
    list_push(l, "key1", 1, "value1", NULL);
    list_push(l, "key2", 2, "value2", NULL);
    list_push(l, "key3", 3, "value3", NULL);
    list_push(l, "key4", 4, "value4", NULL);
    list_push(l, "key5", 5, "value5", NULL);
    list_insert(l, "key0", 0, "value0", NULL);
    return l->length;
}

static void print_list(struct t_list *l) {
    struct t_list_node *current = l->head;
    printf("Head: %s\n", l->head->key);
    printf("List: ");
    while (current != NULL) {
        printf("%s, ", current->key);
        current = current->next;
    }
    printf("\nTail: %s\n", l->tail->key);
}

UTEST(list, test_list_push) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;

    list_push(&test_list, "last", 6, "value6", NULL);
    ASSERT_STREQ("last", test_list.tail->key);
    ASSERT_EQ(7U, test_list.length);
    current = list_node_at(&test_list, test_list.length - 1);
    ASSERT_STREQ(current->key, test_list.tail->key);

    list_clear(&test_list);
}

UTEST(list, test_list_insert) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;

    list_insert(&test_list, "first", -1, "value-1", NULL);
    ASSERT_STREQ("first", test_list.head->key);
    ASSERT_EQ(7U, test_list.length);
    current = list_node_at(&test_list, 0);
    ASSERT_STREQ(current->key, test_list.head->key);

    list_clear(&test_list);
}

UTEST(list, test_list_insert_empty) {
    struct t_list test_list;
    list_init(&test_list);
    struct t_list_node *current;

    list_insert(&test_list, "first", -1, "value-1", NULL);
    ASSERT_STREQ("first", test_list.head->key);
    ASSERT_EQ(1U, test_list.length);
    current = list_node_at(&test_list, 0);
    ASSERT_STREQ(current->key, test_list.head->key);
    ASSERT_STREQ(current->key, test_list.tail->key);

    list_clear(&test_list);
}

UTEST(list, test_remove_node) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;
    //remove middle item
    list_remove_node(&test_list, 3);
    current = list_node_at(&test_list, 4);
    ASSERT_STREQ("key5", current->key);
    ASSERT_EQ(5U, test_list.length);

    //remove last item
    list_remove_node(&test_list, test_list.length - 1);
    ASSERT_STREQ("key4", test_list.tail->key);
    ASSERT_EQ(4U, test_list.length);

    //check tail
    current = list_node_at(&test_list, test_list.length - 1);
    ASSERT_STREQ(current->key, test_list.tail->key);

    //remove first item
    list_remove_node(&test_list, 0);
    ASSERT_STREQ("key1", test_list.head->key);
    ASSERT_EQ(3U, test_list.length);

    current = list_node_at(&test_list, 0);
    ASSERT_STREQ(current->key, test_list.head->key);

    list_clear(&test_list);
}

UTEST(list, test_list_remove_nody_by_key) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;
    //remove middle item
    list_remove_node_by_key(&test_list, "key3");
    current = list_node_at(&test_list, 3);
    ASSERT_STREQ("key4", current->key);
    ASSERT_EQ(5U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_replace) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;
    //replace middle item
    list_replace(&test_list, 4, "replace0", 0, NULL, NULL);
    current = list_node_at(&test_list, 4);
    ASSERT_STREQ("replace0", current->key);
    ASSERT_EQ(6U, test_list.length);

    //replace last item
    list_replace(&test_list, test_list.length - 1, "replace1", 0, NULL, NULL);
    ASSERT_STREQ("replace1", test_list.tail->key);
    current = list_node_at(&test_list, 5);
    ASSERT_STREQ(current->key, test_list.tail->key);
    ASSERT_EQ(6U, test_list.length);

    //replace first item
    list_replace(&test_list, 0, "replace2", 0, NULL, NULL);
    ASSERT_STREQ("replace2", test_list.head->key);
    current = list_node_at(&test_list, 0);
    ASSERT_STREQ(current->key, test_list.head->key);
    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_shuffle) {
    struct t_list test_list;
    populate_list(&test_list);
    struct t_list_node *current;

    list_shuffle(&test_list);
    bool shuffled = false;
    for (unsigned i = 0; i < test_list.length; i++) {
        current = list_node_at(&test_list, i);
        if (current->value_i != (int)i) {
            shuffled = true;
        }
    }
    ASSERT_TRUE(shuffled);

    ASSERT_EQ(6U, test_list.length);
    list_clear(&test_list);
}

UTEST(list, test_list_move_item_pos_4_2) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;

    list_move_item_pos(&test_list, 4, 2);
    current = list_node_at(&test_list, 2);
    ASSERT_STREQ("key4", current->key);
    current = list_node_at(&test_list, 4);
    ASSERT_STREQ("key3", current->key);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_move_item_pos_4_3) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;
    list_move_item_pos(&test_list, 4, 3);
    current = list_node_at(&test_list, 3);
    ASSERT_STREQ("key4", current->key);
    current = list_node_at(&test_list, 4);
    ASSERT_STREQ("key3", current->key);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_move_item_pos_2_4) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;
    list_move_item_pos(&test_list, 2, 4);
    current = list_node_at(&test_list, 4);
    ASSERT_STREQ("key2", current->key);
    current = list_node_at(&test_list, 2);
    ASSERT_STREQ("key3", current->key);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_move_item_pos_2_3) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;
    list_move_item_pos(&test_list, 2, 3);
    current = list_node_at(&test_list, 3);
    ASSERT_STREQ("key2", current->key);
    current = list_node_at(&test_list, 2);
    ASSERT_STREQ("key3", current->key);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_move_item_pos_to_start) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;

    list_move_item_pos(&test_list, 1, 0);
    current = list_node_at(&test_list, 0);
    ASSERT_STREQ("key1", current->key);
    ASSERT_STREQ("key1", test_list.head->key);

    current = list_node_at(&test_list, 1);
    ASSERT_STREQ("key0", current->key);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_move_item_pos_to_end) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;

    const long last_idx = test_list.length - 1;
    list_move_item_pos(&test_list, 1, last_idx);
    current = list_node_at(&test_list, last_idx);
    ASSERT_STREQ("key1", current->key);
    ASSERT_STREQ("key1", test_list.tail->key);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_move_item_pos_from_start) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;

    list_move_item_pos(&test_list, 0, 2);
    current = list_node_at(&test_list, 0);
    ASSERT_STREQ("key1", current->key);
    ASSERT_STREQ("key1", test_list.head->key);

    current = list_node_at(&test_list, 2);
    ASSERT_STREQ("key0", current->key);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_move_item_pos_from_end) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;

    const long last_idx = test_list.length - 1;
    list_move_item_pos(&test_list, last_idx, 1);
    current = list_node_at(&test_list, last_idx);
    ASSERT_STREQ("key4", current->key);
    ASSERT_STREQ("key4", test_list.tail->key);

    current = list_node_at(&test_list, 1);
    ASSERT_STREQ("key5", current->key);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_move_item_pos_from_start_to_end) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;

    const long last_idx = test_list.length - 1;
    list_move_item_pos(&test_list, 0, last_idx);
    current = list_node_at(&test_list, last_idx);
    ASSERT_STREQ("key0", current->key);
    ASSERT_STREQ("key0", test_list.tail->key);

    current = list_node_at(&test_list, 0);
    ASSERT_STREQ("key1", current->key);
    ASSERT_STREQ("key1", test_list.head->key);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_move_item_pos_from_end_to_start) {
    struct t_list test_list;
    populate_list(&test_list);

    struct t_list_node *current;

    const long last_idx = test_list.length - 1;
    list_move_item_pos(&test_list, last_idx, 0);
    current = list_node_at(&test_list, last_idx);
    ASSERT_STREQ("key4", current->key);
    ASSERT_STREQ("key4", test_list.tail->key);

    current = list_node_at(&test_list, 0);
    ASSERT_STREQ("key5", current->key);
    ASSERT_STREQ("key5", test_list.head->key);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

sds write_disk_cb(sds buffer, struct t_list_node *current, bool newline) {
    buffer = sdscatsds(buffer, current->key);
    if (newline == true) {
        buffer = sdscatlen(buffer, "\n", 1);
    }
    return buffer;
}

UTEST(list, test_list_write_to_disk) {
    init_testenv();

    struct t_list test_list;
    populate_list(&test_list);
    sds filepath = sdsnew("/tmp/mympd-test/state/test_list");
    bool rc = list_write_to_disk(filepath, &test_list, write_disk_cb);
    ASSERT_TRUE(rc);
    unlink(filepath);
    sdsfree(filepath);
    list_clear(&test_list);

    clean_testenv();
}

UTEST(list, list_sort_by_value_i) {
    struct t_list test_list;
    populate_list(&test_list);

    list_sort_by_value_i(&test_list, LIST_SORT_DESC);
    ASSERT_EQ(0, test_list.tail->value_i);
    ASSERT_EQ(5, test_list.head->value_i);

    list_sort_by_key(&test_list, LIST_SORT_ASC);
    ASSERT_EQ(0, test_list.head->value_i);
    ASSERT_EQ(5, test_list.tail->value_i);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_sort_by_value_p) {
    struct t_list test_list;
    populate_list(&test_list);

    list_sort_by_key(&test_list, LIST_SORT_DESC);
    ASSERT_STREQ("value0", test_list.tail->value_p);
    ASSERT_STREQ("value5", test_list.head->value_p);

    list_sort_by_key(&test_list, LIST_SORT_ASC);
    ASSERT_STREQ("value0", test_list.head->value_p);
    ASSERT_STREQ("value5", test_list.tail->value_p);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

UTEST(list, test_list_sort_by_key) {
    struct t_list test_list;
    populate_list(&test_list);

    list_sort_by_key(&test_list, LIST_SORT_DESC);
    ASSERT_STREQ("key0", test_list.tail->key);
    ASSERT_STREQ("key5", test_list.head->key);

    list_sort_by_key(&test_list, LIST_SORT_ASC);
    ASSERT_STREQ("key0", test_list.head->key);
    ASSERT_STREQ("key5", test_list.tail->key);

    ASSERT_EQ(6U, test_list.length);

    list_clear(&test_list);
}

static unsigned count_list(struct t_list *l) {
    long i = 0;
    struct t_list_node *current = l->head;
    while (current != NULL) {
        i++;
        current = current->next;
    }
    return i;
}

UTEST(list, test_list_crop) {
    struct t_list test_list;

    // crop to 0
    populate_list(&test_list);
    list_crop(&test_list, 0, NULL);
    ASSERT_EQ(0U, test_list.length);
    unsigned count = count_list(&test_list);
    ASSERT_EQ(0U, count);
    list_clear(&test_list);
    ASSERT_TRUE(test_list.tail == NULL);

    // crop to 2
    populate_list(&test_list);
    list_crop(&test_list, 2, NULL);
    ASSERT_EQ(2U, test_list.length);
    count = count_list(&test_list);
    ASSERT_EQ(2U, count);
    list_clear(&test_list);
    ASSERT_FALSE(test_list.head != NULL);
    ASSERT_FALSE(test_list.tail != NULL);

    // crop greater than list length
    unsigned org_len = populate_list(&test_list);
    list_crop(&test_list, 10, NULL);
    ASSERT_EQ(org_len, test_list.length);
    count = count_list(&test_list);
    ASSERT_EQ(org_len, count);
    list_clear(&test_list);
    ASSERT_FALSE(test_list.head != NULL);
    ASSERT_FALSE(test_list.tail != NULL);

    // crop to exact list length
    org_len = populate_list(&test_list);
    list_crop(&test_list, org_len, NULL);
    ASSERT_EQ(org_len, test_list.length);
    count = count_list(&test_list);
    ASSERT_EQ(org_len, count);
    list_clear(&test_list);
    ASSERT_FALSE(test_list.head != NULL);
    ASSERT_FALSE(test_list.tail != NULL);
}

UTEST(list, test_list_append) {
    struct t_list src;
    list_init(&src);
    list_push(&src, "key0", 0, NULL, NULL);
    list_push(&src, "key1", 0, NULL, NULL);
    list_push(&src, "key2", 0, NULL, NULL);

    struct t_list dst;
    list_init(&dst);
    list_append(&dst, &src);
    ASSERT_EQ(src.length, dst.length);
    list_clear(&src);
    list_clear(&dst);
}

UTEST(list, test_list_dup) {
    struct t_list src;
    list_init(&src);
    list_push(&src, "key0", 0, NULL, NULL);
    list_push(&src, "key1", 0, NULL, NULL);
    list_push(&src, "key2", 0, NULL, NULL);

    struct t_list *new = list_dup(&src);
    ASSERT_EQ(src.length, new->length);
    list_clear(&src);
    list_free(new);
}
