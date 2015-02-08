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

#include <fswatcher/fswatcher.h>

fswatcher_t fswatcher_create( fswatcher_create_flags flags, fswatcher_event_type types, const char* watch_dir, fswatcher_allocator* allocator )
{
	(void)flags; (void)types; (void)watch_dir; (void)allocator;
	return 0x0;
}

void fswatcher_destroy( fswatcher_t watcher )
{
	(void)watcher;
}

void fswatcher_poll( fswatcher_t watcher, fswatcher_event_handler* handler, fswatcher_allocator* allocator )
{
	(void)watcher; (void)handler; (void)allocator;
}
