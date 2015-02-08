#include "greatest.h"
#include <fswatcher/fswatcher.h>

#include <string.h>
#include <stdlib.h> // system

#define TEST_DIR "local/testdir/"

#define CREATE_DIR( dir ) system( "mkdir -p " dir )
#define REMOVE_DIR( dir ) system( "rm -rf " dir )

static void setup_test_dir()
{
	// ... remove dir from prev test ...
	REMOVE_DIR( TEST_DIR );

	// ... and recreate it ...
	CREATE_DIR( TEST_DIR );
}

struct test_handler
{
	fswatcher_event_handler handler;
	fswatcher_event_type type;
	const char* src;
	const char* dst;
};

#define HANDLER_RESET( handler ) \
	free( (void*)handler.src ); \
	free( (void*)handler.dst ); \
	handler.type = FSWATCHER_EVENT_ALL

static bool watch_event_handler( fswatcher_event_handler* handler, fswatcher_event_type evtype, const char* src, const char* dst )
{
	test_handler* h = (test_handler*)handler;
	h->type = evtype;
	h->src  = src ? strdup( src ) : 0;
	h->dst  = dst ? strdup( dst ) : 0;
	return true;
}

TEST create_remove_dir()
{
	setup_test_dir();

	fswatcher_t watcher = fswatcher_create( FSWATCHER_CREATE_DEFAULT, FSWATCHER_EVENT_ALL, TEST_DIR, 0x0 );
	test_handler handler = { watch_event_handler, FSWATCHER_EVENT_ALL, 0x0, 0x0 };

	CREATE_DIR( TEST_DIR "d1" );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	ASSERT_EQ( handler.type, FSWATCHER_EVENT_DIR_CREATE );
	ASSERT_STR_EQ( TEST_DIR "d1", handler.src );
	ASSERT_EQ( 0, handler.dst );
	HANDLER_RESET( handler );

	REMOVE_DIR( TEST_DIR "d1" );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	ASSERT_EQ( handler.type, FSWATCHER_EVENT_DIR_REMOVE );
	ASSERT_STR_EQ( TEST_DIR "d1", handler.src );
	ASSERT_EQ( 0, handler.dst );
	HANDLER_RESET( handler );

	CREATE_DIR( TEST_DIR "d1" );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	ASSERT_EQ( handler.type, FSWATCHER_EVENT_DIR_CREATE );
	ASSERT_STR_EQ( TEST_DIR "d1", handler.src );
	ASSERT_EQ( 0, handler.dst );
	HANDLER_RESET( handler );

	fswatcher_destroy( watcher );
	return 0;
}

GREATEST_SUITE( fswatcher )
{
	RUN_TEST( create_remove_dir );
}

GREATEST_MAIN_DEFS();

int main( int argc, char **argv )
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE( fswatcher );
    GREATEST_MAIN_END();
}
