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

#include "greatest.h"
#include <fswatcher/fswatcher.h>

#include <string.h>
#include <stdlib.h> // system

#define TEST_DIR "local/testdir/"

#define CREATE_DIR( dir ) system( "mkdir -p " dir )
#define REMOVE_DIR( dir ) system( "rm -rf " dir )
#define CREATE_FILE( file ) system( "touch " file )
#define REMOVE_FILE( file ) system( "rm " file )

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

int check_event_handler( fswatcher_event_type evtype, const char* src, const char* dst, test_handler* handler )
{
	ASSERT_EQ( handler->type, evtype );
	if( src )
		ASSERT_STR_EQ( src, handler->src );
	else
		ASSERT_EQ( src, handler->src );
	if( dst )
		ASSERT_STR_EQ( dst, handler->dst );
	else
		ASSERT_EQ( dst, handler->dst );
	return 0;
}

TEST create_remove_dir()
{
	setup_test_dir();

	fswatcher_t watcher = fswatcher_create( FSWATCHER_CREATE_DEFAULT, FSWATCHER_EVENT_ALL, TEST_DIR, 0x0 );
	test_handler handler = { { watch_event_handler }, FSWATCHER_EVENT_ALL, 0x0, 0x0 };

	CREATE_DIR( TEST_DIR "d1" );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_DIR_CREATE, TEST_DIR "d1", 0x0, &handler ) ) return ret;
	HANDLER_RESET( handler );

	REMOVE_DIR( TEST_DIR "d1" );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_DIR_REMOVE, TEST_DIR "d1", 0x0, &handler ) ) return ret;
	HANDLER_RESET( handler );

	CREATE_DIR( TEST_DIR "d1" );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_DIR_CREATE, TEST_DIR "d1", 0x0, &handler ) ) return ret;
	HANDLER_RESET( handler );

	fswatcher_destroy( watcher );
	return 0;
}

TEST create_remove_file()
{
	setup_test_dir();

	fswatcher_t watcher = fswatcher_create( FSWATCHER_CREATE_DEFAULT, FSWATCHER_EVENT_ALL, TEST_DIR, 0x0 );
	test_handler handler = { { watch_event_handler }, FSWATCHER_EVENT_ALL, 0x0, 0x0 };

	CREATE_FILE( TEST_DIR "f1" );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	ASSERT_EQ( 0, check_event_handler( FSWATCHER_EVENT_FILE_CREATE, TEST_DIR "f1", 0x0, &handler ) );
	HANDLER_RESET( handler );

	REMOVE_FILE( TEST_DIR "f1" );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	ASSERT_EQ( 0, check_event_handler( FSWATCHER_EVENT_FILE_REMOVE, TEST_DIR "f1", 0x0, &handler ) );
	HANDLER_RESET( handler );

	fswatcher_destroy( watcher );
	return 0;
}

GREATEST_SUITE( fswatcher )
{
	RUN_TEST( create_remove_dir );
	RUN_TEST( create_remove_file );
}

GREATEST_MAIN_DEFS();

int main( int argc, char **argv )
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE( fswatcher );
    GREATEST_MAIN_END();
}
