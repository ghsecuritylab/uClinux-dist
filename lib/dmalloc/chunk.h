/*
 * Defines for low level memory management routines
 *
 * Copyright 2000 by Gray Watson
 *
 * This file is part of the dmalloc package.
 *
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies, and that the name of Gray Watson not be used in advertising
 * or publicity pertaining to distribution of the document or software
 * without specific, written prior permission.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted via http://dmalloc.com/
 *
 * $Id$
 */

#ifndef __CHUNK_H__
#define __CHUNK_H__

/*<<<<<<<<<<  The below prototypes are auto-generated by fillproto */

/* limit in how much memory we are allowed to allocate */
extern
unsigned long		_dmalloc_memory_limit;

/* total number of bytes that the heap has allocated */
extern
unsigned long		_dmalloc_alloc_total;

/*
 * int _dmalloc_chunk_startup
 * 
 * DESCRIPTION:
 *
 * Startup the low level malloc routines.
 *
 * RETURNS:
 *
 * Success - 1
 *
 * Failure - 0
 *
 * ARGUMENTS:
 *
 * None.
 */
extern
int	_dmalloc_chunk_startup(void);

/*
 * char *_dmalloc_chunk_desc_pnt
 *
 * DESCRIPTION:
 *
 * Write into a buffer a pointer description with file and
 * line-number.
 *
 * RETURNS:
 *
 * Pointer to buffer 1st argument.
 *
 * ARGUMENTS:
 *
 * buf <-> Passed in buffer which will be filled with a description of
 * the pointer.
 *
 * buf_size -> Size of the buffer in bytes.
 *
 * file -> File name, return address, or NULL.
 *
 * line -> Line number or 0.
 */
extern
char	*_dmalloc_chunk_desc_pnt(char *buf, const int buf_size,
				 const char *file, const unsigned int line);

/*
 * int _dmalloc_chunk_read_info
 *
 * DESCRIPTION:
 *
 * Return some information associated with a pointer.
 *
 * RETURNS:
 *
 * Success - 1 pointer is okay
 *
 * Failure - 0 problem with pointer
 *
 * ARGUMENTS:
 *
 * user_pnt -> Pointer we are checking.
 *
 * where <- Where the check is being made from.
 *
 * user_size_p <- Pointer to an unsigned int which, if not NULL, will
 * be set to the size of bytes that the user requested.
 *
 * alloc_size_p <- Pointer to an unsigned int which, if not NULL, will
 * be set to the total given size of bytes including block overhead.
 *
 * file_p <- Pointer to a character pointer which, if not NULL, will
 * be set to the file where the pointer was allocated.
 *
 * line_p <- Pointer to a character pointer which, if not NULL, will
 * be set to the line-number where the pointer was allocated.
 *
 * ret_attr_p <- Pointer to a void pointer, if not NULL, will be set
 * to the return-address where the pointer was allocated.
 *
 * seen_cp <- Pointer to an unsigned long which, if not NULL, will be
 * set to the number of times the pointer has been "seen".
 *
 * used_p <- Pointer to an unsigned long which, if not NULL, will be
 * set to the last time the pointer was "used".
 *
 * valloc_bp <- Pointer to an integer which, if not NULL, will be set
 * to 1 if the pointer was allocated with valloc otherwise 0.
 *
 * fence_bp <- Pointer to an integer which, if not NULL, will be set
 * to 1 if the pointer has the fence bit set otherwise 0.
 */
extern
int	_dmalloc_chunk_read_info(const void *user_pnt, const char *where,
				 unsigned int *user_size_p,
				 unsigned int *alloc_size_p, char **file_p,
				 unsigned int *line_p, void **ret_attr_p,
				 unsigned long **seen_cp,
				 unsigned long *used_p, int *valloc_bp,
				 int *fence_bp);

/*
 * int _dmalloc_chunk_heap_check
 *
 * DESCRIPTION:
 *
 * Run extensive tests on the entire heap.
 *
 * RETURNS:
 *
 * Success - 1 if the heap is okay
 *
 * Failure - 0 if a problem was detected
 *
 * ARGUMENTS:
 *
 * None.
 */
extern
int	_dmalloc_chunk_heap_check(void);

/*
 * int _dmalloc_chunk_pnt_check
 *
 * DESCRIPTION:
 *
 * Run extensive tests on a pointer.
 *
 * RETURNS:
 *
 * Success - 1 if the pointer is okay
 *
 * Failure - 0 if not
 *
 * ARGUMENTS:
 *
 * func -> Function string which is checking the pointer.
 *
 * user_pnt -> Pointer we are checking.
 *
 * exact_b -> Set to 1 to find the pointer specifically.  Otherwise we
 * can find the pointer inside of an allocation.
 *
 * min_size -> Make sure that pnt can hold at least that many bytes.
 * If -1 then do a strlen + 1 for the \0.  If 0 then ignore.
 */
extern
int	_dmalloc_chunk_pnt_check(const char *func, const void *user_pnt,
				 const int exact_b, const int min_size);

/*
 * void *_dmalloc_chunk_malloc
 *
 * DESCRIPTION:
 *
 * Allocate a chunk of memory.
 *
 * RETURNS:
 *
 * Success - Valid pointer.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * file -> File-name or return-address location of the allocation.
 *
 * line -> Line-number location of the allocation.
 *
 * size -> Number of bytes to allocate.
 *
 * func_id -> Calling function-id as defined in dmalloc.h.
 *
 * alignment -> If greater than 0 then try to align the returned
 * block.
 */
extern
void	*_dmalloc_chunk_malloc(const char *file, const unsigned int line,
			       const unsigned long size, const int func_id,
			       const unsigned int alignment);

/*
 * int _dmalloc_chunk_free
 *
 * DESCRIPTION:
 *
 * Free a user pointer from the heap.
 *
 * RETURNS:
 *
 * Success - FREE_NOERROR
 *
 * Failure - FREE_ERROR
 *
 * ARGUMENTS:
 *
 * file -> File-name or return-address location of the allocation.
 *
 * line -> Line-number location of the allocation.
 *
 * user_pnt -> Pointer we are freeing.
 *
 * func_id -> Function ID
 */
extern
int	_dmalloc_chunk_free(const char *file, const unsigned int line,
			    void *user_pnt, const int func_id);

/*
 * void *_dmalloc_chunk_realloc
 *
 * DESCRIPTION:
 *
 * Re-allocate a chunk of memory either shrinking or expanding it.
 *
 * RETURNS:
 *
 * Success - Valid pointer.
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * file -> File-name or return-address location of the allocation.
 *
 * line -> Line-number location of the allocation.
 *
 * old_user_pnt -> Old user pointer that we are reallocating.
 *
 * new_size -> New-size to change the pointer.
 *
 * func_id -> Calling function-id as defined in dmalloc.h.
 */
extern
void	*_dmalloc_chunk_realloc(const char *file, const unsigned int line,
				void *old_user_pnt,
				const unsigned long new_size,
				const int func_id);

/*
 * void _dmalloc_chunk_log_stats
 *
 * DESCRIPTION:
 *
 * Log general statistics from the heap to the logfile.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * None.
 */
extern
void	_dmalloc_chunk_log_stats(void);

/*
 * void _dmalloc_chunk_log_changed
 *
 * DESCRIPTION:
 *
 * Log the pointers that has changed since a pointer in time.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * mark -> Dmalloc counter used to mark a specific time so that
 * servers can check on the changed pointers.
 *
 * log_non_free_b -> If set to 1 then log the new not-freed
 * (i.e. used) pointers.
 *
 * log_free_b -> If set to 1 then log the new freed pointers.
 *
 * details_b -> If set to 1 then dump the individual pointer entries
 * instead of just the summary.
 */
extern
void	_dmalloc_chunk_log_changed(const unsigned long mark,
				   const int log_not_freed_b,
				   const int log_freed_b, const int details_b);

/*
 * unsigned long _dmalloc_chunk_count_changed
 *
 * DESCRIPTION:
 *
 * Return the pointers that has changed since a pointer in time.
 *
 * RETURNS:
 *
 * Number of bytes changed since mark.
 *
 * ARGUMENTS:
 *
 * mark -> Dmalloc counter used to mark a specific time so that
 * servers can check on the changed pointers.
 *
 * count_non_free_b -> If set to 1 then count the new not-freed
 * (i.e. used) pointers.
 *
 * count_free_b -> If set to 1 then count the new freed pointers.
 */
extern
unsigned long	_dmalloc_chunk_count_changed(const unsigned long mark,
					     const int count_not_freed_b,
					     const int count_freed_b);

/*<<<<<<<<<<   This is end of the auto-generated output from fillproto. */

#endif /* ! __CHUNK_H__ */
