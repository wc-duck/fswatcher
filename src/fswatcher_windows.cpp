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
