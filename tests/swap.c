/*
 * vim:ts=4:sw=4:expandtab
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "queue.h"

struct obj {
    int abc;
    TAILQ_ENTRY(obj) entry;
};

TAILQ_HEAD(objhead, obj) head;

void dump() {
    struct obj *e;
    printf("dump:\n");
    e = TAILQ_FIRST(&head);
    printf("first: %d\n", e->abc);
    e = TAILQ_LAST(&head, objhead);
    printf("last: %d\n", e->abc);
    TAILQ_FOREACH (e, &head, entry) {
        printf("  %d\n", e->abc);
    }
    printf("again, but reverse:\n");
    TAILQ_FOREACH_REVERSE (e, &head, objhead, entry) {
        printf("  %d\n", e->abc);
    }
    printf("done\n\n");
}

#define TAILQ_SWAP(first, second, head, field)                                     \
    do {                                                                           \
        *((first)->field.tqe_prev) = (second);                                     \
        (second)->field.tqe_prev = (first)->field.tqe_prev;                        \
        (first)->field.tqe_prev = &((second)->field.tqe_next);                     \
        (first)->field.tqe_next = (second)->field.tqe_next;                        \
        if ((second)->field.tqe_next)                                              \
            (second)->field.tqe_next->field.tqe_prev = &((first)->field.tqe_next); \
        (second)->field.tqe_next = first;                                          \
        if ((head)->tqh_last == &((second)->field.tqe_next))                       \
            (head)->tqh_last = &((first)->field.tqe_next);                         \
    } while (0)

void _TAILQ_SWAP(struct obj *first, struct obj *second, struct objhead *head) {
    struct obj **tqe_prev = first->entry.tqe_prev;
    *tqe_prev = second;

    second->entry.tqe_prev = first->entry.tqe_prev;

    first->entry.tqe_prev = &(second->entry.tqe_next);

    first->entry.tqe_next = second->entry.tqe_next;

    if (second->entry.tqe_next) {
        struct obj *tqe_next = second->entry.tqe_next;
        tqe_next->entry.tqe_prev = &(first->entry.tqe_next);
    }

    second->entry.tqe_next = first;

    if (head->tqh_last == &(second->entry.tqe_next))
        head->tqh_last = &(first->entry.tqe_next);
}

int main() {
    printf("hello\n");

    TAILQ_INIT(&head);

    struct obj first;
    first.abc = 123;

    struct obj second;
    second.abc = 456;

    struct obj third;
    third.abc = 789;

    struct obj fourth;
    fourth.abc = 999;

    struct obj fifth;
    fifth.abc = 5555;

    /*
     * ************************************************
     */
    printf("swapping first two elements:\n");

    TAILQ_INSERT_TAIL(&head, &first, entry);
    TAILQ_INSERT_TAIL(&head, &second, entry);
    TAILQ_INSERT_TAIL(&head, &third, entry);

    dump();

    TAILQ_SWAP(&first, &second, &head, entry);

    dump();

    /*
     * ************************************************
     */
    printf("swapping last two elements:\n");

    TAILQ_INIT(&head);

    TAILQ_INSERT_TAIL(&head, &first, entry);
    TAILQ_INSERT_TAIL(&head, &second, entry);
    TAILQ_INSERT_TAIL(&head, &third, entry);

    dump();

    TAILQ_SWAP(&second, &third, &head, entry);

    dump();

    /*
     * ************************************************
     */
    printf("longer list:\n");

    TAILQ_INIT(&head);

    TAILQ_INSERT_TAIL(&head, &first, entry);
    TAILQ_INSERT_TAIL(&head, &second, entry);
    TAILQ_INSERT_TAIL(&head, &third, entry);
    TAILQ_INSERT_TAIL(&head, &fourth, entry);

    dump();

    TAILQ_SWAP(&first, &second, &head, entry);

    dump();

    /*
     * ************************************************
     */
    printf("longer list 2:\n");

    TAILQ_INIT(&head);

    TAILQ_INSERT_TAIL(&head, &first, entry);
    TAILQ_INSERT_TAIL(&head, &second, entry);
    TAILQ_INSERT_TAIL(&head, &third, entry);
    TAILQ_INSERT_TAIL(&head, &fourth, entry);

    dump();

    TAILQ_SWAP(&second, &third, &head, entry);

    dump();

    /*
     * ************************************************
     */
    printf("longer list, swap, then insert:\n");

    TAILQ_INIT(&head);

    TAILQ_INSERT_TAIL(&head, &first, entry);
    TAILQ_INSERT_TAIL(&head, &second, entry);
    TAILQ_INSERT_TAIL(&head, &third, entry);
    TAILQ_INSERT_TAIL(&head, &fourth, entry);

    dump();

    TAILQ_SWAP(&second, &third, &head, entry);

    dump();

    TAILQ_INSERT_AFTER(&head, &third, &fifth, entry);

    dump();

    /*
     * ************************************************
     */
    printf("longer list, swap, then append:\n");

    TAILQ_INIT(&head);

    TAILQ_INSERT_TAIL(&head, &first, entry);
    TAILQ_INSERT_TAIL(&head, &second, entry);
    TAILQ_INSERT_TAIL(&head, &third, entry);
    TAILQ_INSERT_TAIL(&head, &fourth, entry);

    dump();

    TAILQ_SWAP(&second, &third, &head, entry);

    dump();

    TAILQ_INSERT_TAIL(&head, &fifth, entry);

    dump();

    /*
     * ************************************************
     */
    printf("longer list, swap, then remove:\n");

    TAILQ_INIT(&head);

    TAILQ_INSERT_TAIL(&head, &first, entry);
    TAILQ_INSERT_TAIL(&head, &second, entry);
    TAILQ_INSERT_TAIL(&head, &third, entry);
    TAILQ_INSERT_TAIL(&head, &fourth, entry);

    dump();

    TAILQ_SWAP(&second, &third, &head, entry);

    dump();

    TAILQ_REMOVE(&head, &second, entry);

    dump();
}
