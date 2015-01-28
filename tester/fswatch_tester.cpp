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
