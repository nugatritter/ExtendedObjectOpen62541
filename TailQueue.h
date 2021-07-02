/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)queue.h	8.5 (Berkeley) 8/20/94
 *
 * 
 * extracted from open62541_queue.h
 * used by ExtendedObjectOpen62541.cpp to provide
 * UA_printEnum, UA_printStructure and UA_printUnion
*/

#pragma once

#if defined(QUEUE_MACRO_DEBUG) || (defined(_KERNEL) && defined(DIAGNOSTIC))
#define _Q_INVALIDATE(a) (a) = ((void *)-1)
#else
#define _Q_INVALIDATE(a)
#endif

/*
 * Tail queue definitions.
 */
#define TAILQ_HEAD(name, type)						\
struct name {								\
    struct type *tqh_first;	/* first element */			\
    struct type **tqh_last;	/* addr of last next element */		\
}

#define TAILQ_HEAD_INITIALIZER(head)					\
    { NULL, &(head).tqh_first }

#define TAILQ_ENTRY(type)						\
struct {								\
    struct type *tqe_next;	/* next element */			\
    struct type **tqe_prev;	/* address of previous next element */	\
}

 /*
  * tail queue access methods
  */
#define	TAILQ_FIRST(head)		((head)->tqh_first)
#define	TAILQ_END(head)			NULL
#define	TAILQ_NEXT(elm, field)		((elm)->field.tqe_next)
#define TAILQ_LAST(head, headname)					\
    (*(((struct headname *)((head)->tqh_last))->tqh_last))
  /* XXX */
#define TAILQ_PREV(elm, headname, field)				\
    (*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))
#define	TAILQ_EMPTY(head)						\
    (TAILQ_FIRST(head) == TAILQ_END(head))

#define TAILQ_FOREACH(var, head, field)					\
    for((var) = TAILQ_FIRST(head);					\
        (var) != TAILQ_END(head);					\
        (var) = TAILQ_NEXT(var, field))

#define	TAILQ_FOREACH_SAFE(var, head, field, tvar)			\
    for ((var) = TAILQ_FIRST(head);					\
        (var) != TAILQ_END(head) &&					\
        ((tvar) = TAILQ_NEXT(var, field), 1);			\
        (var) = (tvar))


#define TAILQ_FOREACH_REVERSE(var, head, headname, field)		\
    for((var) = TAILQ_LAST(head, headname);				\
        (var) != TAILQ_END(head);					\
        (var) = TAILQ_PREV(var, headname, field))

#define	TAILQ_FOREACH_REVERSE_SAFE(var, head, headname, field, tvar)	\
    for ((var) = TAILQ_LAST(head, headname);			\
        (var) != TAILQ_END(head) &&					\
        ((tvar) = TAILQ_PREV(var, headname, field), 1);		\
        (var) = (tvar))

/*
 * Tail queue functions.
 */
#define	TAILQ_INIT(head) do {						\
    (head)->tqh_first = NULL;					\
    (head)->tqh_last = &(head)->tqh_first;				\
} while (0)

#define TAILQ_INSERT_HEAD(head, elm, field) do {			\
    if (((elm)->field.tqe_next = (head)->tqh_first) != NULL)	\
        (head)->tqh_first->field.tqe_prev =			\
            &(elm)->field.tqe_next;				\
    else								\
        (head)->tqh_last = &(elm)->field.tqe_next;		\
    (head)->tqh_first = (elm);					\
    (elm)->field.tqe_prev = &(head)->tqh_first;			\
} while (0)

#define TAILQ_INSERT_TAIL(head, elm, field) do {			\
    (elm)->field.tqe_next = NULL;					\
    (elm)->field.tqe_prev = (head)->tqh_last;			\
    *(head)->tqh_last = (elm);					\
    (head)->tqh_last = &(elm)->field.tqe_next;			\
} while (0)

#define TAILQ_INSERT_AFTER(head, listelm, elm, field) do {		\
    if (((elm)->field.tqe_next = (listelm)->field.tqe_next) != NULL)\
        (elm)->field.tqe_next->field.tqe_prev =			\
            &(elm)->field.tqe_next;				\
    else								\
        (head)->tqh_last = &(elm)->field.tqe_next;		\
    (listelm)->field.tqe_next = (elm);				\
    (elm)->field.tqe_prev = &(listelm)->field.tqe_next;		\
} while (0)

#define	TAILQ_INSERT_BEFORE(listelm, elm, field) do {			\
    (elm)->field.tqe_prev = (listelm)->field.tqe_prev;		\
    (elm)->field.tqe_next = (listelm);				\
    *(listelm)->field.tqe_prev = (elm);				\
    (listelm)->field.tqe_prev = &(elm)->field.tqe_next;		\
} while (0)

#define TAILQ_REMOVE(head, elm, field) do {				\
    if (((elm)->field.tqe_next) != NULL)				\
        (elm)->field.tqe_next->field.tqe_prev =			\
            (elm)->field.tqe_prev;				\
    else								\
        (head)->tqh_last = (elm)->field.tqe_prev;		\
    *(elm)->field.tqe_prev = (elm)->field.tqe_next;			\
    _Q_INVALIDATE((elm)->field.tqe_prev);				\
    _Q_INVALIDATE((elm)->field.tqe_next);				\
} while (0)

#define TAILQ_REPLACE(head, elm, elm2, field) do {			\
    if (((elm2)->field.tqe_next = (elm)->field.tqe_next) != NULL)	\
        (elm2)->field.tqe_next->field.tqe_prev =		\
            &(elm2)->field.tqe_next;				\
    else								\
        (head)->tqh_last = &(elm2)->field.tqe_next;		\
    (elm2)->field.tqe_prev = (elm)->field.tqe_prev;			\
    *(elm2)->field.tqe_prev = (elm2);				\
    _Q_INVALIDATE((elm)->field.tqe_prev);				\
    _Q_INVALIDATE((elm)->field.tqe_next);				\
} while (0)

#pragma warning(disable : 4200)
typedef struct UA_PrintElement {
    TAILQ_ENTRY(UA_PrintElement) next;
    size_t length;
    UA_Byte data[];
} UA_PrintOutput;
#pragma warning(default : 4200)

typedef struct {
    size_t depth;
    TAILQ_HEAD(, UA_PrintElement) outputs;
} UA_PrintContext;
