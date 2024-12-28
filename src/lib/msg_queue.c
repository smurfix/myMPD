/*
 SPDX-License-Identifier: GPL-3.0-or-later
 myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
 https://github.com/jcorporation/mympd
*/

/*! \file
 * \brief Message queue implementation
 */

#include "compile_time.h"
#include "src/lib/msg_queue.h"

#include "dist/mongoose/mongoose.h"
#include "src/lib/api.h"
#include "src/lib/event.h"
#include "src/lib/log.h"
#include "src/lib/mem.h"

#ifdef MYMPD_ENABLE_LUA
    #include "src/mympd_api/lua_mympd_state.h"
#endif

#include <errno.h>

/*
 Message queue implementation to transfer messages between threads asynchronously
*/

//private definitions
static bool check_for_queue_id(struct t_mympd_queue *queue, unsigned id);
static void free_queue_node(struct t_mympd_msg *n, enum mympd_queue_types type);
static void free_queue_node_extra(void *extra, enum mympd_cmd_ids cmd_id);
static int unlock_mutex(pthread_mutex_t *mutex);
static void set_wait_time(int timeout_ms, struct timespec *max_wait);

//public functions

/**
 * Creates a thread safe message queue
 * @param name description of the queue
 * @param type type of the queue QUEUE_TYPE_REQUEST or QUEUE_TYPE_RESPONSE
 * @param event create an eventfd?
 * @return pointer to allocated and initialized queue struct
 */
struct t_mympd_queue *mympd_queue_create(const char *name, enum mympd_queue_types type,
        bool event) {
    struct t_mympd_queue *queue = malloc_assert(sizeof(struct t_mympd_queue));
    queue->head = NULL;
    queue->tail = NULL;
    queue->length = 0;
    queue->name = name;
    queue->type = type;
    queue->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    queue->wakeup = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    queue->event_fd = event == true
        ? event_eventfd_create()
        : -1;
    queue->mg_mgr = NULL;
    queue->mg_conn_id = 0;
    return queue;
}

/**
 * Frees all queue nodes and the queue itself
 * @param queue pointer to the queue
 * @return NULL
 */
void *mympd_queue_free(struct t_mympd_queue *queue) {
    mympd_queue_expire_age(queue, 0);
    event_fd_close(queue->event_fd);
    FREE_PTR(queue);
    return NULL;
}

/**
 * Appends data to the queue
 * @param queue pointer to the queue
 * @param data struct t_work_request or t_work_response
 * @param id id of the queue entry
 * @return true on success else false
 */
bool mympd_queue_push(struct t_mympd_queue *queue, void *data, unsigned id) {
    int rc = pthread_mutex_lock(&queue->mutex);
    if (rc != 0) {
        MYMPD_LOG_ERROR(NULL, "Error in pthread_mutex_lock: %d", rc);
        return false;
    }
    struct t_mympd_msg* new_node = malloc_assert(sizeof(struct t_mympd_msg));
    new_node->data = data;
    new_node->id = id;
    new_node->timestamp = time(NULL);
    new_node->next = NULL;
    queue->length++;
    if (queue->head == NULL &&
        queue->tail == NULL)
    {
        queue->head = queue->tail = new_node;
    }
    else {
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    if (unlock_mutex(&queue->mutex) != 0) {
        return false;
    }
    rc = pthread_cond_signal(&queue->wakeup);
    if (rc != 0) {
        MYMPD_LOG_ERROR(NULL, "Error in pthread_cond_signal: %d", rc);
        return 0;
    }
    if (queue->event_fd > -1) {
        event_eventfd_write(queue->event_fd);
    }
    else if (queue->mg_mgr != NULL) {
        mg_wakeup(queue->mg_mgr, queue->mg_conn_id, "Q", 1);
    }
    return true;
}

/**
 * Gets the first entry or the entry with specific id
 * @param queue pointer to the queue
 * @param timeout_ms timeout in ms to wait for a queue entry,
 *                   0 to wait infinite
 *                   -1 for no wait
 * @param id 0 for first entry or specific id
 * @return t_work_request or t_work_response
 */
void *mympd_queue_shift(struct t_mympd_queue *queue, int timeout_ms, unsigned id) {
    //lock the queue
    int rc = pthread_mutex_lock(&queue->mutex);
    if (rc != 0) {
        MYMPD_LOG_ERROR(NULL, "Error in pthread_mutex_lock: %d", rc);
        assert(NULL);
    }
    if (timeout_ms > -1) {
        if (check_for_queue_id(queue, id) == false) {
            //check and wait for entries
            if (timeout_ms > 0) {
                struct timespec max_wait = {0, 0};
                set_wait_time(timeout_ms, &max_wait);
                errno = 0;
                rc = pthread_cond_timedwait(&queue->wakeup, &queue->mutex, &max_wait);
            }
            else {
                //wait infinite for a queue entry
                errno = 0;
                rc = pthread_cond_wait(&queue->wakeup, &queue->mutex);
            }
            if (rc != 0) {
                if (rc != ETIMEDOUT) {
                    MYMPD_LOG_ERROR(NULL, "Error in pthread_cond_timedwait: %d", rc);
                    MYMPD_LOG_ERRNO(NULL, errno);
                }
                unlock_mutex(&queue->mutex);
                return NULL;
            }
        }
    }
    if (queue->head != NULL) {
        //queue has entry
        struct t_mympd_msg *current = NULL;
        struct t_mympd_msg *previous = NULL;

        for (current = queue->head; current != NULL; previous = current, current = current->next) {
            if (id == 0 ||
                id == current->id)
            {
                void *data = current->data;
                if (previous == NULL) {
                    //Fix beginning pointer
                    queue->head = current->next;
                }
                else {
                    //Fix previous nodes next to skip over the removed node.
                    previous->next = current->next;
                }
                //Fix tail
                if (queue->tail == current) {
                    queue->tail = previous;
                }
                FREE_PTR(current);
                queue->length--;
                MYMPD_LOG_DEBUG(NULL, "Queue \"%s\": %u entries", queue->name, queue->length);
                unlock_mutex(&queue->mutex);
                return data;
            }
            MYMPD_LOG_DEBUG(NULL, "Skipping queue entry with id %u", current->id);
        }
    }
    unlock_mutex(&queue->mutex);
    return NULL;
}

/**
 * Expire entries from the queue by age
 * @param queue pointer to the queue
 * @param max_age_s max age of nodes in seconds
 * @return number of expired nodes
 */
int mympd_queue_expire_age(struct t_mympd_queue *queue, time_t max_age_s) {
    int rc = pthread_mutex_lock(&queue->mutex);
    if (rc != 0) {
        MYMPD_LOG_ERROR(NULL, "Error in pthread_mutex_lock: %d", rc);
        return 0;
    }
    int expired_count = 0;
    if (queue->head != NULL) {
        //queue has entry
        struct t_mympd_msg *current = NULL;
        struct t_mympd_msg *previous = NULL;

        time_t expire_time = time(NULL) - max_age_s;

        for (current = queue->head; current != NULL;) {
            if (max_age_s == 0 ||
                current->timestamp < expire_time)
            {
                struct t_mympd_msg *to_remove = current;
                if (queue->tail == current) {
                    //Fix tail
                    queue->tail = previous;
                }
                if (previous == NULL) {
                    //Fix beginning pointer
                    queue->head = current->next;
                    //Set current to queue head
                    current = queue->head;
                }
                else {
                    //Fix previous nodes next to skip over the removed node.
                    previous->next = current->next;
                    //Set current to previous
                    current = previous;
                }
                free_queue_node(to_remove, queue->type);
                queue->length--;
                expired_count++;
            }
            else {
                //skip this node
                previous = current;
                current = current->next;
            }
        }
    }

    unlock_mutex(&queue->mutex);
    return expired_count;
}

//privat functions

/**
 * Checks if queue has a entry with requested id
 * @param queue Pointer to the queue
 * @param id 0 for first entry or specific id
 * @return true if there is an entry, else false
 */
static bool check_for_queue_id(struct t_mympd_queue *queue, unsigned id) {
    if (queue->length == 0) {
        MYMPD_LOG_DEBUG(NULL, "Queue %s is empty", queue->name);
        return false;
    }
    if (id == 0) {
        return true;
    }
    struct t_mympd_msg *current = queue->head;
    while (current != NULL) {
        if (current->id == id) {
            return true;
        }
        current = current->next;
    }
    MYMPD_LOG_DEBUG(NULL, "No entry with id %u found in queue %s", id, queue->name);
    return false;
}

/**
 * Frees a queue node, node must be detached from queue
 * @param node to free
 * @param type type of the queue QUEUE_TYPE_REQUEST or QUEUE_TYPE_RESPONSE
 */
static void free_queue_node(struct t_mympd_msg *node, enum mympd_queue_types type) {
    //free data
    if (type == QUEUE_TYPE_REQUEST) {
        struct t_work_request *request = node->data;
        free_queue_node_extra(request->extra, request->cmd_id);
        free_request(request);
    }
    else {
        //QUEUE_TYPE_RESPONSE
        struct t_work_response *response = node->data;
        free_queue_node_extra(response->extra, response->cmd_id);
        free_response(response);
    }
    //free the node itself
    FREE_PTR(node);
}

/**
 * Frees the extra data from a t_work_request or t_work_response struct.
 * It respects the cmd_id
 * @param extra extra data from request or response
 * @param cmd_id method of request or response
 */
static void free_queue_node_extra(void *extra, enum mympd_cmd_ids cmd_id) {
    if (extra == NULL) {
        return;
    }
    if (cmd_id == INTERNAL_API_SCRIPT_INIT) {
        #ifdef MYMPD_ENABLE_LUA
            lua_mympd_state_free(extra);
        #endif
    }
    else {
        FREE_PTR(extra);
    }
}

/**
 * Unlocks the queue mutex
 * @param mutex the mutex to unlock
 * @return 0 on success, else 1
 */
static int unlock_mutex(pthread_mutex_t *mutex) {
    int rc = pthread_mutex_unlock(mutex);
    if (rc != 0) {
        MYMPD_LOG_ERROR(NULL, "Error in pthread_mutex_unlock: %d", rc);
    }
    return rc;
}

/**
 * Time conversion numbers
 */
enum {
    MSEC_NSEC = 1000000,
    SEC_NSEC = 1000000000,
    NSEC_MAX = 999999999
};

/**
 * Populates the timespec struct with time now + timeout
 * @param timeout_ms timeout in ms
 * @param max_wait timespec struct to populate
 */
static void set_wait_time(int timeout_ms, struct timespec *max_wait) {
    errno = 0;
    if (clock_gettime(CLOCK_REALTIME, max_wait) == -1) {
        MYMPD_LOG_ERROR(NULL, "Error getting realtime");
        MYMPD_LOG_ERRNO(NULL, errno);
        assert(NULL);
    }
    int64_t timeout_ns = (int64_t)max_wait->tv_nsec + ((int64_t)timeout_ms * MSEC_NSEC);
    if (timeout_ns > NSEC_MAX) {
        max_wait->tv_sec += (time_t)(timeout_ns / SEC_NSEC);
        max_wait->tv_nsec = (long)(timeout_ns % SEC_NSEC);
    }
    else {
        max_wait->tv_nsec += (long)timeout_ns;
    }
}
