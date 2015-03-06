/*
   A small drop-in library for watching the filesystem for changes.

   version 0.1, february, 2015

   Copyright (C) 2015- Fredrik Kihlander

   This software is provided 'as-is', without any express or implied
   warranty.  In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.
   3. This notice may not be removed or altered from any source distribution.

   Fredrik Kihlander
*/

#ifndef FSWATCHER_H_INCLUDED
#define FSWATCHER_H_INCLUDED

#include <stddef.h> // for size_t

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 *
 */
#if !defined(FSWATCHER_PATH_FORMAT_NATIVE) || !defined(FSWATCHER_PATH_FORMAT_UTF8)
#    define FSWATCHER_PATH_FORMAT_UTF8
#endif

/**
 *
 */
enum fswatcher_create_flags
{
	FSWATCHER_CREATE_BLOCKING  = (1 << 1), ///< calls to fswatcher_poll should block until 1 or more events arrive.
	FSWATCHER_CREATE_RECURSIVE = (1 << 2), ///< the directory watch should recursively add all sub-directories to watch.
	FSWATCHER_CREATE_DEFAULT   = FSWATCHER_CREATE_RECURSIVE
};

/**
 *
 */
enum fswatcher_event_type
{
	FSWATCHER_EVENT_CREATE = (1 << 1), ///< file in "src" was just created.
	FSWATCHER_EVENT_REMOVE = (1 << 2), ///< file in "src" was just removed.
	FSWATCHER_EVENT_MODIFY = (1 << 3), ///< file in "src" was just modified.
	FSWATCHER_EVENT_MOVE   = (1 << 4), ///< file was moved from "src" to "dst", if "src" or "dst" is 0x0 it indicates that the path was outside the current watch.

	FSWATCHER_EVENT_ALL = FSWATCHER_EVENT_CREATE |
						  FSWATCHER_EVENT_REMOVE |
						  FSWATCHER_EVENT_MODIFY |
						  FSWATCHER_EVENT_MOVE   ,

	FSWATCHER_EVENT_BUFFER_OVERFLOW ///< doc me
};

/**
 *
 */
typedef struct fswatcher* fswatcher_t;

/**
 * Allocator interface used by fswatcher, read function-documentation for allocation pattern descriptions.
 *
 * @example:
 *
 * struct my_stack_allocator
 * {
 *     fswatcher_allocator alloc;
 *
 *     size_t allocated;
 *     char buffer[ 4096 ];
 * };
 *
 * void* my_stack_realloc( fswatcher_allocator* allocator, void* ptr, size_t old_size, size_t new_size )
 * {
 *     my_stack_allocator* salloc = (my_stack_allocator*)allocator;
 *     if( salloc->allocated + new_size > 4096 )
 *         return 0x0;
 *     return salloc->buffer[ salloc->allocated += new_size ];
 * }
 *
 * void run_poll( fswatcher_t w, fswatcher_event_handler* h )
 * {
 *     my_stack_allocator salloc = { { my_stack_realloc, 0x0 }, 0 },
 *     fswatcher_poll( w, h, &salloc.alloc );
 * }
 */
struct fswatcher_allocator
{
	/**
	 * realloc function, should work in the same way as realloc().
	 *
	 * @param allocator struct holding the function pointer.
	 * @param ptr to memory to realloc or 0x0 if not allocated.
	 * @param old_size size of allocation if ptr != 0x0.
	 * @param new_size requested allocation size.
	 *
	 * @return pointer to newly allocated memory or 0x0 on failure.
	 */
	void* ( *realloc )( fswatcher_allocator* allocator, void* ptr, size_t old_size, size_t new_size );

	/**
	 * free function, should be used to free memory allocated with realloc().
	 * This function can be set to 0x0 if no free call is needed.
	 *
	 * @param allocator struct holding the function pointer.
	 * @param ptr pointer to memory to free.
	 */
	void  ( *free )( fswatcher_allocator* allocator, void* ptr );
};

/**
 * Struct used together with fswatcher_poll() to fetch events from fswatcher.
 *
 * @example
 *
 * struct my_fsevent_handler
 * {
 *     fswatcher_event_handler eh;
 *
 *     // ... some userdata here ...
 * };
 *
 * static bool my_fsevent_func( fswatcher_event_handler* handler, fswatcher_event_type evtype, const char* src, const char* dst )
 * {
 *     my_fsevent_handler* h = (my_fsevent_handler*)handler;
 *
 *     // ... do stuff in handler ...
 * }
 *
 * void poll_it( fswatcher_t w )
 * {
 *     my_fsevent_handler h = { my_fsevent_func };
 *     fswatcher_poll( w, &h.eh, 0x0 );
 * }
 */
struct fswatcher_event_handler
{
	/**
	 * Callback used per event that is queued on the fswatcher.
	 *
	 * @param callback called per event.
	 * @param evtype type of event received.
	 * @param src path to source file, can be 0x0 if event has no source. ( this can happen for example on a move if file was moved from non-watched folder to watched folder. )
	 * @param dst path to destination file, can be 0x0 if event has no destination. ( this can happen for example on a move if file was moved from watched folder to non-watched folder. )
	 *
	 * @return false if poll should end.
	 */
	bool ( *callback )( fswatcher_event_handler* handler, fswatcher_event_type evtype, const char* src, const char* dst );
};

/**
 * Create a new fswatcher watching a specific directory and potential sub-directories.
 *
 * @note The allocator passed to this function will all need to persist during the life-time of the fswatcher and might need reallocation.
 *
 * @param flags ...
 * @param types events to watch and get events for.
 * @param watch_dir directory to watch.
 * @param allocator to use for this fswatcher or 0x0 to use malloc/free
 */
fswatcher_t fswatcher_create( fswatcher_create_flags flags, fswatcher_event_type types, const char* watch_dir, fswatcher_allocator* allocator );

/**
 * Destroy fswatcher_t and free all its used resources.
 *
 * @param watcher to destroy.
 */
void fswatcher_destroy( fswatcher_t watcher );

/**
 * Poll an fswatcher for new events, this call is blocking if FSWATCHER_CREATE_BLOCKING was passed to fswatcher_create().
 *
 * @note This function might allocate data in 2 instances, first if an event was detected that a directory was created,
 *       in that case the allocator passed to fswatcher_poll() will be used.
 *       The more usual case is the src/dst parameters to the callback function. These allocations will only be alive during
 *       the callback and can thusly be allocated with a stack-allocator or other temporary allocator.
 *
 * @param watcher to poll.
 * @param handler to poll events with.
 * @param allocator used to allocate temporary data during poll or 0x0 to use malloc/free.
 */
void fswatcher_poll( fswatcher_t watcher, fswatcher_event_handler* handler, fswatcher_allocator* allocator );

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif // FSWATCHER_H_INCLUDED
