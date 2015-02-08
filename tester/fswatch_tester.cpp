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

#include <stdio.h>

static bool watch_event_handler( fswatcher_event_handler* handler, fswatcher_event_type evtype, const char* src, const char* dst )
{
	(void)handler; (void)dst;

	switch( evtype )
	{
		case FSWATCHER_EVENT_FILE_CREATE:
			printf("file create %s!\n", src);
			break;
		case FSWATCHER_EVENT_FILE_REMOVE:
			printf("file remove %s!\n", src);
			break;
		case FSWATCHER_EVENT_FILE_MODIFY:
			printf("file modify %s!\n", src);
			break;
		case FSWATCHER_EVENT_FILE_MOVED:
			printf("file moved %s -> %s!\n", src, dst);
			break;
		case FSWATCHER_EVENT_DIR_CREATE:
			printf("dir create %s!\n", src);
			break;
		case FSWATCHER_EVENT_DIR_REMOVE:
			printf("dir remove %s!\n", src);
			break;
		case FSWATCHER_EVENT_DIR_MOVED:
			printf("dir moved %s -> %s!\n", src, dst);
			break;
		default:
			printf("unhandled event!\n");
	}
	return true;
}

int main( int argc, const char** argv )
{
	(void)argc; (void)argv;
	fswatcher_t watcher = fswatcher_create( FSWATCHER_CREATE_DEFAULT, FSWATCHER_EVENT_ALL, argv[1], 0x0 );

	while( true )
	{
		fswatcher_event_handler handler = { watch_event_handler };
		fswatcher_poll( watcher, &handler, 0x0 );
	};

	fswatcher_destroy( watcher );
	return 0;
}
