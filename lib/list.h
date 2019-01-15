/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2013, 2015 Nicira, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIST_H
#define LIST_H 1

/* Doubly linked list. */

#include <stdbool.h>
#include <stddef.h>
#include "util.h"
#include "openvswitch/list.h"

/* "struct ovs_list" with pointers that will (probably) cause segfaults if
 * dereferenced and, better yet, show up clearly in a debugger. */
#define OVS_LIST_POISON \
(struct ovs_list) { (struct ovs_list *) (uintptr_t) 0xccccccccccccccccULL, \
                    (struct ovs_list *) (uintptr_t) 0xccccccccccccccccULL }

static inline void list_init(struct ovs_list *);
static inline void list_poison(struct ovs_list *);

/* List insertion. */
static inline void list_insert(struct ovs_list *, struct ovs_list *);
static inline void list_splice(struct ovs_list *before, struct ovs_list *first,
                               struct ovs_list *last);
static inline void list_push_front(struct ovs_list *, struct ovs_list *);
static inline void list_push_back(struct ovs_list *, struct ovs_list *);
static inline void list_replace(struct ovs_list *, const struct ovs_list *);
static inline void list_moved(struct ovs_list *, const struct ovs_list *orig);
static inline void list_move(struct ovs_list *dst, struct ovs_list *src);

/* List removal. */
static inline struct ovs_list *list_remove(struct ovs_list *);
static inline struct ovs_list *list_pop_front(struct ovs_list *);
static inline struct ovs_list *list_pop_back(struct ovs_list *);

/* List elements. */
static inline struct ovs_list *list_front(const struct ovs_list *);
static inline struct ovs_list *list_back(const struct ovs_list *);

/* List properties. */
static inline size_t list_size(const struct ovs_list *);
static inline bool list_is_empty(const struct ovs_list *);
static inline bool list_is_singleton(const struct ovs_list *);
static inline bool list_is_short(const struct ovs_list *);

#define LIST_FOR_EACH(ITER, MEMBER, LIST)                               \
    for (INIT_CONTAINER(ITER, (LIST)->next, MEMBER);                    \
         &(ITER)->MEMBER != (LIST);                                     \
         ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.next, MEMBER))
#define LIST_FOR_EACH_CONTINUE(ITER, MEMBER, LIST)                      \
    for (ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.next, MEMBER);             \
         &(ITER)->MEMBER != (LIST);                                     \
         ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.next, MEMBER))
#define LIST_FOR_EACH_REVERSE(ITER, MEMBER, LIST)                       \
    for (INIT_CONTAINER(ITER, (LIST)->prev, MEMBER);                    \
         &(ITER)->MEMBER != (LIST);                                     \
         ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.prev, MEMBER))
#define LIST_FOR_EACH_REVERSE_CONTINUE(ITER, MEMBER, LIST)              \
    for (ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.prev, MEMBER);           \
         &(ITER)->MEMBER != (LIST);                                     \
         ASSIGN_CONTAINER(ITER, (ITER)->MEMBER.prev, MEMBER))
#define LIST_FOR_EACH_SAFE(ITER, NEXT, MEMBER, LIST)               \
    for (INIT_CONTAINER(ITER, (LIST)->next, MEMBER);               \
         (&(ITER)->MEMBER != (LIST)                                \
          ? INIT_CONTAINER(NEXT, (ITER)->MEMBER.next, MEMBER), 1   \
          : 0);                                                    \
         (ITER) = (NEXT))
#define LIST_FOR_EACH_POP(ITER, MEMBER, LIST)                      \
    while (!list_is_empty(LIST)                                    \
           && (INIT_CONTAINER(ITER, list_pop_front(LIST), MEMBER), 1))

/* Inline implementations. */

/* Initializes 'list' as an empty list. */
static inline void
list_init(struct ovs_list *list)
{
    list->next = list->prev = list;
}

/* Initializes 'list' with pointers that will (probably) cause segfaults if
 * dereferenced and, better yet, show up clearly in a debugger. */
static inline void
list_poison(struct ovs_list *list)
{
    *list = OVS_LIST_POISON;
}

/* Inserts 'elem' just before 'before'. */
static inline void
list_insert(struct ovs_list *before, struct ovs_list *elem)
{
    elem->prev = before->prev;
    elem->next = before;
    before->prev->next = elem;
    before->prev = elem;
}

/* Removes elements 'first' though 'last' (exclusive) from their current list,
   then inserts them just before 'before'. */
static inline void
list_splice(struct ovs_list *before, struct ovs_list *first, struct ovs_list *last)
{
    if (first == last) {
        return;
    }
    last = last->prev;

    /* Cleanly remove 'first'...'last' from its current list. */
    first->prev->next = last->next;
    last->next->prev = first->prev;

    /* Splice 'first'...'last' into new list. */
    first->prev = before->prev;
    last->next = before;
    before->prev->next = first;
    before->prev = last;
}

/* Inserts 'elem' at the beginning of 'list', so that it becomes the front in
   'list'. */
static inline void
list_push_front(struct ovs_list *list, struct ovs_list *elem)
{
    list_insert(list->next, elem);
}

/* Inserts 'elem' at the end of 'list', so that it becomes the back in
 * 'list'. */
static inline void
list_push_back(struct ovs_list *list, struct ovs_list *elem)
{
    list_insert(list, elem);
}

/* Puts 'elem' in the position currently occupied by 'position'.
 * Afterward, 'position' is not part of a list. */
static inline void
list_replace(struct ovs_list *element, const struct ovs_list *position)
{
    element->next = position->next;
    element->next->prev = element;
    element->prev = position->prev;
    element->prev->next = element;
}

/* Adjusts pointers around 'list' to compensate for 'list' having been moved
 * around in memory (e.g. as a consequence of realloc()), with original
 * location 'orig'.
 *
 * ('orig' likely points to freed memory, but this function does not
 * dereference 'orig', it only compares it to 'list'.  In a very pedantic
 * language lawyer sense, this still yields undefined behavior, but it works
 * with actual compilers.) */
static inline void
list_moved(struct ovs_list *list, const struct ovs_list *orig)
{
    if (list->next == orig) {
        list_init(list);
    } else {
        list->prev->next = list->next->prev = list;
    }
}

/* Initializes 'dst' with the contents of 'src', compensating for moving it
 * around in memory.  The effect is that, if 'src' was the head of a list, now
 * 'dst' is the head of a list containing the same elements. */
static inline void
list_move(struct ovs_list *dst, struct ovs_list *src)
{
    *dst = *src;
    list_moved(dst, src);
}

/* Removes 'elem' from its list and returns the element that followed it.
   Undefined behavior if 'elem' is not in a list. */
static inline struct ovs_list *
list_remove(struct ovs_list *elem)
{
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    return elem->next;
}

/* Removes the front element from 'list' and returns it.  Undefined behavior if
   'list' is empty before removal. */
static inline struct ovs_list *
list_pop_front(struct ovs_list *list)
{
    struct ovs_list *front = list->next;

    list_remove(front);
    return front;
}

/* Removes the back element from 'list' and returns it.
   Undefined behavior if 'list' is empty before removal. */
static inline struct ovs_list *
list_pop_back(struct ovs_list *list)
{
    struct ovs_list *back = list->prev;

    list_remove(back);
    return back;
}

/* Returns the front element in 'list_'.
   Undefined behavior if 'list_' is empty. */
static inline struct ovs_list *
list_front(const struct ovs_list *list_)
{
    struct ovs_list *list = CONST_CAST(struct ovs_list *, list_);

    ovs_assert(!list_is_empty(list));

    return list->next;
}

/* Returns the back element in 'list_'.
   Undefined behavior if 'list_' is empty. */
static inline struct ovs_list *
list_back(const struct ovs_list *list_)
{
    struct ovs_list *list = CONST_CAST(struct ovs_list *, list_);

    ovs_assert(!list_is_empty(list));

    return list->prev;
}

/* Returns the number of elements in 'list'.
   Runs in O(n) in the number of elements. */
static inline size_t
list_size(const struct ovs_list *list)
{
    const struct ovs_list *e;
    size_t cnt = 0;

    for (e = list->next; e != list; e = e->next) {
        cnt++;
    }
    return cnt;
}

/* Returns true if 'list' is empty, false otherwise. */
static inline bool
list_is_empty(const struct ovs_list *list)
{
    return list->next == list;
}

/* Returns true if 'list' has exactly 1 element, false otherwise. */
static inline bool
list_is_singleton(const struct ovs_list *list)
{
    return list_is_short(list) && !list_is_empty(list);
}

/* Returns true if 'list' has 0 or 1 elements, false otherwise. */
static inline bool
list_is_short(const struct ovs_list *list)
{
    return list->next == list->prev;
}

#endif /* list.h */