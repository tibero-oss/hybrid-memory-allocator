/**
 * @file    list.h
 * @brief   Original from Linux kernel
 */

/* get conatining struct of "type" from "ptr" which is "member" of "type" */
#define TB_CONTAINEROF(ptr, type, member) \
    ((type *)((char *)(ptr)-offsetof(type, member)))

#ifndef _LIST_H
#define _LIST_H

#ifdef _DIR_TBSVR
#ifndef CHECK_SHM_PTR
extern char *shared_mem_;
extern size_t shm_size_;
#define CHECK_SHM_PTR(ptr) \
    (((char *)ptr >= shared_mem_) && ((char *)ptr < shared_mem_ + shm_size_))
#endif /* CHECK_SHM_PTR */
#endif /* _DIR_TBSVR */

#define prefetch(link) /* real prefetch hint + ',' : like 0, */

/*
 * These are non-NULL pointers that will result in page faults
 * under normal circumstances, used to verify that nobody uses
 * non-initialized list entries.
 */
#define LIST_POISON1 ((void *)0x00100100)
#define LIST_POISON2 ((void *)0x00200200)

/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

/* ___TBSTAT_TYPE_PREFIX_START___ */
typedef struct list_link_s list_link_t;
typedef struct list_link_s list_t;
struct list_link_s
{
    list_link_t *next, *prev;
};
/* ___TBSTAT_TYPE_PREFIX_END___ */

#define LIST_HEAD_INIT(name) \
    {                        \
        &(name), &(name)     \
    }

#undef LIST_HEAD
#define LIST_HEAD(name) \
    list_link_t name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr)  \
    do                       \
    {                        \
        (ptr)->next = (ptr); \
        (ptr)->prev = (ptr); \
    } while (0)

#define INIT_LIST_LINK(ptr) \
    do                      \
    {                       \
        (ptr)->next = NULL; \
        (ptr)->prev = NULL; \
    } while (0)

/* head == link */
#define LIST_LINK_INIT LIST_HEAD_INIT
#define LIST_LINK LIST_HEAD

#define LIST_LINK_IS_INITED(link) \
    ((link)->prev == NULL && (link)->next == NULL)

/* TB_DEBUG mode에서만 동작하므로 TB_CHECK내에서만 사용 가능하고,
   TB_THR_ASSERT나, 일반 문장에서 사용할 수는 없다 */
#ifdef TB_DEBUG
#define LIST_LINK_IS_UNCHAINED(link)                                   \
    (((link)->prev == LIST_POISON2 && (link)->next == LIST_POISON1) || \
     LIST_LINK_IS_INITED(link))
#endif

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
#if !defined(_OS_AIX) && !defined(_SW_64)
static inline void __list_add(list_link_t *new_entry,
                              list_link_t *prev, list_link_t *next)
{
    *(volatile list_link_t **)&new_entry->next = next;
    new_entry->prev = prev;
    next->prev = new_entry;
    prev->next = new_entry;
}

#else
static inline void __list_add(list_link_t *new_entry,
                              list_link_t *prev, list_link_t *next)
{
    volatile list_link_t *local_prev;

    local_prev = prev;
    *(volatile list_link_t **)&new_entry->next = next;
    new_entry->prev = prev;
    next->prev = new_entry;
    local_prev->next = new_entry;
}
#endif

/**
 * list_add - add a new entry
 * @new_entry: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void list_add(list_link_t *new_entry, list_link_t *head)
{
    __list_add(new_entry, head, head->next);
}

/**
 * list_add_tail - add a new entry
 * @new_entry: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(list_link_t *new_entry, list_link_t *head)
{
    __list_add(new_entry, head->prev, head);
}

/**
 * list_add_before - add a new entry before given entry
 * @new_entry: new entry to be added
 * @next: next entry will be located after 'new_entry'
 *
 * Insert a new entry before the specified link.
 */
#define list_add_before(new_entry, next) list_add_tail(new_entry, next)

/**
 * list_add_after - add a new entry after given entry
 * @new_entry: new entry to be added
 * @prev: next entry will be located before 'new_entry'
 *
 * Insert a new entry after the specified link.
 */
#define list_add_after(new_entry, prev) list_add(new_entry, prev)

/*
 * Delete a list entry by making the prev/next entries point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del(list_link_t *prev, list_link_t *next)
{
    next->prev = prev;
    *(volatile list_link_t **)&prev->next = next;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void list_del(list_link_t *entry)
{
    __list_del(entry->prev, entry->next);
#ifdef TB_DEBUG
    entry->next = (list_link_t *)LIST_POISON1;
    entry->prev = (list_link_t *)LIST_POISON2;
#endif
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static inline void list_del_init(list_link_t *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void list_move(list_link_t *list, list_link_t *head)
{
    __list_del(list->prev, list->next);
    list_add(list, head);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void list_move_tail(list_link_t *list, list_link_t *head)
{
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const list_link_t *head)
{
    return head->next == head;
}

/**
 * list_empty - tests whether a list have a single entry
 * @head: the list to test.
 */
static inline int list_single_entry(const list_link_t *head)
{
    return !list_empty(head) && head->next->next == head;
}

/**
 * list_empty_careful - tests whether a list is
 * empty _and_ checks that no other CPU might be
 * in the process of still modifying either member
 *
 * NOTE: using list_empty_careful() without synchronization
 * can only be safe if the only activity that can happen
 * to the list entry is list_del_init(). Eg. it cannot be used
 * if another CPU could re-list_add() it.
 *
 * @head: the list to test.
 */
static inline int list_empty_careful(const list_link_t *head)
{
    list_link_t *next = head->next;
    return (next == head) && (next == head->prev);
}

/**
 * list_is_last - tests whether a entry is last in list
 * @entry: the entry to check.
 * @head: the list to test.
 */
static inline int list_is_last(const list_link_t *entry, const list_link_t *head)
{
    return entry->next == head;
}

static inline void __list_splice(list_link_t *list, list_link_t *head)
{
    list_link_t *first = list->next;
    list_link_t *last = list->prev;
    list_link_t *at = head->next;

    first->prev = head;
    head->next = first;

    last->next = at;
    at->prev = last;
}

/**
 * list_splice - join two lists
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice(list_link_t *list, list_link_t *head)
{
    if (!list_empty(list))
        __list_splice(list, head);
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
static inline void list_splice_init(list_link_t *list, list_link_t *head)
{
    if (!list_empty(list))
    {
        __list_splice(list, head);
        INIT_LIST_HEAD(list);
    }
}

/**
 * list_append - append one list to another list(head)
 * @list: the new list to append.
 * @head: head of list to append.
 */
static inline void list_append(list_link_t *list, list_link_t *head)
{
    list_link_t *first = list->next;
    list_link_t *last = list->prev;
    list_link_t *at = head->prev;

    if (list_empty(list))
        return;

    at->next = first;
    first->prev = at;

    head->prev = last;
    last->next = head;

    INIT_LIST_HEAD(list);
}

/**
 * list_assign - assign list
 * @list: the new list to assign (source)
 * @head: the place to assign it in the first list. (destination)
 */
static inline void list_assign(list_link_t *list, list_link_t *head)
{
    if (!list_empty(list))
    {
        list->next->prev = head;
        list->prev->next = head;

        *head = *list;
    }
    else
    {
        INIT_LIST_HEAD(head);
    }
}

/**
 * list_entry - get the struct for this entry
 * @ptr:    the &list_link_t pointer.
 * @type:   the type of the struct this is embedded in.
 * @member: the name of the list_struct within the struct.
 */
#ifndef _WIN32_HYPER_LOADER
#define list_entry(ptr, type, member) \
    TB_CONTAINEROF(ptr, type, member)
#endif
#ifdef _DIR_TBSVR
#define list_entry_check(ptr, type, member)                                  \
    (CHECK_SHM_PTR(ptr) && CHECK_SHM_PTR(TB_CONTAINEROF(ptr, type, member))) \
        ? TB_CONTAINEROF(ptr, type, member)                                  \
        : NULL
#endif /* _DIR_TBSVR */

/**
 * list_for_each - iterate over a list
 * @pos:    the &list_link_t to use as a loop counter.
 * @head:   the head for your list.
 */
#define list_for_each(pos, head)                                  \
    for (pos = (head)->next; (prefetch(pos->next) pos != (head)); \
         pos = pos->next)
#ifdef _DIR_TBSVR
#define list_for_each_check(pos, head)                      \
    for (pos = (CHECK_SHM_PTR(head) ? (head)->next : NULL); \
         (CHECK_SHM_PTR(pos) && (pos != (head)));           \
         pos = pos->next)
#endif /* _DIR_TBSVR */
/**
 * __list_for_each - iterate over a list
 * @pos:    the &list_link_t to use as a loop counter.
 * @head:   the head for your list.
 *
 * This variant differs from list_for_each() in that it's the
 * simplest possible list iteration code, no prefetching is done.
 * Use this for code that knows the list to be very short (empty
 * or 1 entry) most of the time.
 */
#define __list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_prev - iterate over a list backwards
 * @pos:    the &list_link_t to use as a loop counter.
 * @head:   the head for your list.
 */
#define list_for_each_prev(pos, head)                             \
    for (pos = (head)->prev; (prefetch(pos->prev) pos != (head)); \
         pos = pos->prev)

/**
 * list_for_each_safe - iterate over a list safe against removal of list entry
 * @pos:    the &list_link_t to use as a loop counter.
 * @n:      another &list_link_t to use as temporary storage
 * @head:   the head for your list.
 */
#define list_for_each_safe(pos, n, head)                   \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

/**
 * list_for_each_continue_safe - iterate over a list safe of given type
 *          continuing after existing point against removal of list entry
 * @pos:    the &list_link_t to use as a loop counter.
 * @n:      another &list_link_t to use as temporary storage
 * @head:   the head for your list.
 */
#define list_for_each_continue_safe(pos, n, head) \
    for (n = pos->next; pos != (head);            \
         pos = n, n = pos->next)

/**
 * list_for_each_entry  -   iterate over list of given type
 * @pos:    the type * to use as a loop counter.
 * @head:   the head for your list.
 * @member: the name of the list_struct within the struct.
 * @type:   element_type
 */
#define list_for_each_entry(pos, head, member, type)           \
    for (pos = list_entry((head)->next, type, member);         \
         (prefetch(pos->member.next) & pos->member != (head)); \
         pos = list_entry(pos->member.next, type, member))
#ifdef _DIR_TBSVR
#define list_for_each_entry_check(pos, head, member, type)   \
    for (pos = list_entry_check((head)->next, type, member); \
         (pos != NULL) && (&pos->member != (head));          \
         pos = list_entry_check(pos->member.next, type, member))
#endif /* _DIR_TBSVR */

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:    the type * to use as a loop counter.
 * @head:   the head for your list.
 * @member: the name of the list_struct within the struct.
 * @type:   element_type
 */
#define list_for_each_entry_reverse(pos, head, member, type)   \
    for (pos = list_entry((head)->prev, type, member);         \
         (prefetch(pos->member.prev) & pos->member != (head)); \
         pos = list_entry(pos->member.prev, type, member))

#define list_for_each_entry_reverse_check(pos, head, member, type) \
    for (pos = list_entry_check((head)->prev, type, member);       \
         (pos != NULL) & (&pos->member != (head));                 \
         pos = list_entry_check(pos->member.prev, type, member))

/**
 * list_for_each_entry_safe -
 *  iterate backwards over list of given type safe against removal of list entry
 * @pos:    the type * to use as a loop counter.
 * @pos:    the type * to use as a loop counter.
 * @n:      another type * to use as temporary storage
 * @head:   the head for your list.
 * @member: the name of the list_struct within the struct.
 * @type:   element_type
 */
#define list_for_each_entry_reverse_safe(pos, n, head, member, type) \
    for (pos = list_entry((head)->prev, type, member),               \
        n = list_entry((pos)->member.prev, type, member);            \
         &pos->member != (head);                                     \
         pos = n, n = list_entry(n->member.prev, type, member))

/**
 * list_for_each_entry_safe2 -
 *  iterate backwards over list of given type safe against removal of list entry
 * @pos:    the type * to use as a loop counter.
 * @pos:    the type * to use as a loop counter.
 * @n:      another type * to use as temporary storage
 * @head:   the head for your list.
 * @member: the name of the list_struct within the struct.
 * @type:   element_type
 */
#define list_for_each_entry_reverse_safe2(pos, n, head, member, type) \
    for (pos = list_entry((head)->prev, type, member),                \
        n = list_entry((pos)->member.next, type, member);             \
         &pos->member != (head);                                      \
         pos = (pos == list_entry((n)->member.prev, type, member))    \
                   ? (list_entry((pos)->member.prev, type, member))   \
                   : (list_entry((n)->member.prev, type, member)),    \
        n = list_entry((pos)->member.next, type, member))

/**
 * list_prepare_entry - prepare a pos entry for use as a start point in
 *          list_for_each_entry_continue
 * @pos:    the type * to use as a start point
 * @head:   the head of the list
 * @member: the name of the list_struct within the struct.
 * @type:   element_type
 */
#define list_prepare_entry(pos, head, member, type) \
    ((pos) ? (pos) : list_entry(head, type, member))

/**
 * list_for_each_entry_continue -   iterate over list of given type
 *          continuing after existing point
 * @pos:    the type * to use as a loop counter.
 * @head:   the head for your list.
 * @member: the name of the list_struct within the struct.
 * @type:   element_type
 */
#define list_for_each_entry_continue(pos, head, member, type)    \
    for (; (prefetch(pos->member.next) & pos->member != (head)); \
         pos = list_entry(pos->member.next, type, member))

/**
 * list_for_each_entry_reverse_continue - iterate backwards
 *                                        over list of given type.
 * @pos:    the type * to use as a loop counter.
 * @head:   the head for your list.
 * @member: the name of the list_struct within the struct.
 * @type:   element_type
 */
#define list_for_each_entry_reverse_continue(pos, head, member, type) \
    for (pos = list_entry(pos->member.prev, type, member);            \
         (prefetch(pos->member.prev) & pos->member != (head));        \
         pos = list_entry(pos->member.prev, type, member))

/**
 * list_for_each_entry_reverse_continue_safe - iterate backwards
 *                                             over list of given type.
 * @pos:    the type * to use as a loop counter.
 * @n:      another type * to use as temporary storage
 * @head:   the head for your list.
 * @member: the name of the list_struct within the struct.
 * @type:   element_type
 */
#define list_for_each_entry_reverse_continue_safe(pos, n, head, member, type) \
    for (pos = list_entry((pos)->member.prev, type, member),                  \
        n = list_entry((pos)->member.prev, type, member);                     \
         &pos->member != (head);                                              \
         pos = n, n = list_entry(n->member.prev, type, member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe
 *                            against removal of list entry
 * @pos:    the type * to use as a loop counter.
 * @n:      another type * to use as temporary storage
 * @head:   the head for your list.
 * @member: the name of the list_struct within the struct.
 * @type:   element_type
 */
#define list_for_each_entry_safe(pos, n, head, member, type) \
    for (pos = list_entry((head)->next, type, member),       \
        n = list_entry(pos->member.next, type, member);      \
         &pos->member != (head);                             \
         pos = n, n = list_entry(n->member.next, type, member))

/**
 * list_for_each_pair_entry - iterate over a pair of list of given type
 * @pos1:    the type * to use as a loop counter.
 * @head1:   the head1 for your list.
 * @member1: the name1 of the list_struct within the struct.
 * @type1:   element_type1
 * @pos2:    the type * to use as a loop counter.
 * @head2:   the head2 for your list.
 * @member2: the name2 of the list_struct within the struct.
 * @type2:   element_type2
 */
#define list_for_each_pair_entry(pos1, head1, member1, type1,         \
                                 pos2, head2, member2, type2)         \
    for (pos1 = list_entry((head1)->next, type1, member1),            \
        pos2 = list_entry((head2)->next, type2, member2);             \
         (prefetch(pos1->member1.next) & pos1->member1 != (head1)) && \
         (prefetch(pos2->member2.next) & pos2->member2 != (head2));   \
         pos1 = list_entry(pos1->member1.next, type1, member1),       \
        pos2 = list_entry(pos2->member2.next, type2, member2))

#define list_find(pos, head, member, type, cond)         \
    do                                                   \
    {                                                    \
        if (list_empty(head))                            \
        {                                                \
            pos = NULL;                                  \
        }                                                \
        else                                             \
        {                                                \
            list_for_each_entry(pos, head, member, type) \
            {                                            \
                if (cond)                                \
                    break;                               \
            }                                            \
            if (&pos->member == head)                    \
                pos = NULL;                              \
        }                                                \
    } while (0)

#define list_find_reverse(pos, head, member, type, cond)         \
    do                                                           \
    {                                                            \
        if (list_empty(head))                                    \
        {                                                        \
            pos = NULL;                                          \
        }                                                        \
        else                                                     \
        {                                                        \
            list_for_each_entry_reverse(pos, head, member, type) \
            {                                                    \
                if (cond)                                        \
                    break;                                       \
            }                                                    \
            if (&pos->member == head)                            \
                pos = NULL;                                      \
        }                                                        \
    } while (0)

/**
 * list_count - counts list elements
 * @head: the list to count
 */
static inline int list_count(const list_t *head)
{
    list_link_t *link;
    int volatile count = 0;

    list_for_each(link, head)
        count++;

    return count;
}

#define list_first_entry(head, type, member) \
    ((list_empty(head))                      \
         ? NULL                              \
         : list_entry((head)->next, type, member))

#define list_last_entry(head, type, member) \
    ((list_empty(head))                     \
         ? NULL                             \
         : list_entry((head)->prev, type, member))

#define list_next_entry(pos, head, type, member) \
    (((pos)->next == (head))                     \
         ? NULL                                  \
         : list_entry((pos)->next, type, member))

#define list_prev_entry(pos, head, type, member) \
    (((pos)->prev == (head))                     \
         ? NULL                                  \
         : list_entry((pos)->prev, type, member))

/*
 * Double linked lists with a single pointer list head.
 * Mostly useful for hash tables where the two pointer list head is
 * too wasteful.
 * You lose the ability to access the tail in O(1).
 */

typedef struct hlist_s hlist_t;

struct hlist_s
{
    list_link_t *first;
};

#define HLIST_HEAD_INIT \
    {                   \
        NULL            \
    }
#define HLIST_HEAD(name) hlist_t name = {NULL}
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
#define INIT_HLIST_LINK INIT_LIST_LINK

static inline int hlist_unhashed(const list_link_t *h)
{
    return !h->prev;
}

static inline int hlist_empty(const hlist_t *h)
{
    return !h->first;
}

static inline void __hlist_del(list_link_t *n)
{
    list_link_t *next = n->next;
    list_link_t *prev = n->prev;

    *(volatile list_link_t **)&prev->next = next;
    if (next)
        next->prev = prev;
}

static inline void hlist_del(list_link_t *n)
{
    __hlist_del(n);
#ifdef TB_DEBUG
    n->next = (list_link_t *)LIST_POISON1;
    n->prev = (list_link_t *)LIST_POISON2;
#endif
}

static inline void hlist_del_init(list_link_t *n)
{
    if (n->prev)
    {
        __hlist_del(n);
        INIT_HLIST_LINK(n);
    }
}

static inline void hlist_add_head(list_link_t *n, hlist_t *h)
{
    list_link_t *first = h->first;

    *(volatile list_link_t **)&n->next = first;
    if (first)
        first->prev = n;
    h->first = n;
    n->prev = (list_link_t *)h;
}

/* next must be != NULL */
static inline void hlist_add_before(list_link_t *new_entry, list_link_t *next)
{
    new_entry->prev = next->prev;
    *(volatile list_link_t **)&new_entry->next = next;
    next->prev = new_entry;
    new_entry->prev->next = new_entry;
}

static inline void hlist_add_after(list_link_t *new_entry, list_link_t *prev)
{
    *(volatile list_link_t **)&new_entry->next = prev->next;
    prev->next = new_entry;
    new_entry->prev = prev;

    if (new_entry->next)
        new_entry->next->prev = new_entry;
}

#define hlist_entry(ptr, type, member) TB_CONTAINEROF(ptr, type, member)

#define hlist_for_each(pos, head)                             \
    for (pos = (head)->first; pos && (prefetch(pos->next) 1); \
         pos = pos->next)

#define hlist_for_each_safe(pos, n, head)                \
    for (pos = (head)->first; pos && (n = pos->next, 1); \
         pos = n)

/**
 * hlist_for_each_entry - iterate over list of given type
 * @tpos:   the type * to use as a loop counter.
 * @pos:    the &list_link_t to use as a loop counter.
 * @head:   the head for your list.
 * @member: the name of the hlist_node within the struct.
 * @type:   element_type
 */
#define hlist_for_each_entry(tpos, pos, head, member, type)              \
    for (pos = (head)->first;                                            \
         pos &&                                                          \
         (prefetch(pos->next) tpos = hlist_entry(pos, type, member), 1); \
         pos = pos->next)
#ifdef _DIR_TBSVR
#define hlist_for_each_entry_check(tpos, pos, head, member, type) \
    for (pos = (CHECK_SHM_PTR((head)) ? (head)->first : NULL);    \
         pos && (tpos = list_entry_check(pos, type, member), 1);  \
         pos = pos->next)
#endif /* _DIR_TBSVR */
/*
#define list_entry_check(ptr, type, member)                                    \
    (CHECK_SHM_PTR(ptr) && CHECK_SHM_PTR(TB_CONTAINEROF(ptr, type, member)))   \
    ? TB_CONTAINEROF(ptr, type, member)                                        \
    : NULL
           //list_entry_check((head), hlist_t, first);                           \
*/

/**
 * hlist_for_each_entry_continue -
 *              iterate over a hlist continuing after existing point
 * @tpos:   the type * to use as a loop counter.
 * @pos:    the &list_link_t to use as a loop counter.
 * @member: the name of the hlist_node within the struct.
 * @type:   element_type
 */
#define hlist_for_each_entry_continue(tpos, pos, member, type)           \
    for (pos = (pos)->next;                                              \
         pos &&                                                          \
         (prefetch(pos->next) tpos = hlist_entry(pos, type, member), 1); \
         pos = pos->next)

/**
 * hlist_for_each_entry_from - iterate over a hlist continuing from existing point
 * @tpos:   the type * to use as a loop counter.
 * @pos:    the &list_link_t to use as a loop counter.
 * @member: the name of the hlist_node within the struct.
 * @type:   element_type
 */
#define hlist_for_each_entry_from(tpos, pos, member, type)                 \
    for (; pos &&                                                          \
           (prefetch(pos->next) tpos = hlist_entry(pos, type, member), 1); \
         pos = pos->next)

/**
 * hlist_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @tpos:   the type * to use as a loop counter.
 * @pos:    the &list_link_t to use as a loop counter.
 * @n:      another &list_link_t to use as temporary storage
 * @head:   the head for your list.
 * @member: the name of the hlist_node within the struct.
 * @type:   element_type
 */
#define hlist_for_each_entry_safe(tpos, pos, n, head, member, type) \
    for (pos = (head)->first;                                       \
         pos && (n = pos->next, 1) &&                               \
         (tpos = hlist_entry(pos, type, member), 1);                \
         pos = n)

#define hlist_find(pos, head, member, type, cond)               \
    do                                                          \
    {                                                           \
        list_link_t *__link__;                                  \
        hlist_for_each_entry(pos, __link__, head, member, type) \
        {                                                       \
            if (cond)                                           \
                break;                                          \
        }                                                       \
        if (__link__ == NULL)                                   \
            pos = NULL;                                         \
    } while (0)

static inline int hlist_count(const hlist_t *head)
{

    list_link_t *link;
    int volatile count = 0;
    hlist_for_each(link, head)
        count++;

    return count;
}
#endif /* no _LIST_H */
