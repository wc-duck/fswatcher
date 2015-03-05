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

#include <stdio.h>

#if defined( _WIN32 )
#  include <windows.h>
#  define DIR_SEP "\\"
#else
#  define DIR_SEP "/"
#endif

static const char* get_test_dir()
{
	static char temp_path[4096] = {0};
	static bool fetched = false;
	if( !fetched )
	{
#if defined( _WIN32 )
		GetTempPathA( sizeof( temp_path ), temp_path );
		strcat( temp_path, "\\fswatcher_test\\" );
#else
		strcat( temp_path, P_tmpdir );
		strcat( temp_path, "/fswatcher_test/" );
#endif
		printf( "Running tests in \"%s\"\n", temp_path );
		fetched = true;
	}
	return temp_path;
}

static void create_dir( const char* dirname )
{
	char buffer[4096] = {0};
#if defined( _WIN32 )
	strcat( buffer, "mkdir " );
#else
	strcat( buffer, "mkdir -p " );
#endif
	strcat( buffer, dirname );
	system( buffer );
}

static void create_file( const char* filename )
{
	char buffer[4096] = {0};
#if defined( _WIN32 )
	strcat( buffer, "type nul>>  " );
#else
	strcat( buffer, "touch " );
#endif
	strcat( buffer, filename );
	system( buffer );
}

static void remove_dir( const char* dirname )
{
	char buffer[4096] = {0};
#if defined( _WIN32 )
	strcat( buffer, "rmdir /q /s " );
#else
	strcat( buffer, "rm -rf " );
#endif
	strcat( buffer, dirname );
	system( buffer );
}

static void remove_file( const char* filename )
{
	char buffer[4096] = {0};
#if defined( _WIN32 )
	strcat( buffer, "del " );
#else
	strcat( buffer, "rm " );
#endif
	strcat( buffer, filename );
	system( buffer );
}

static const char* test_dir_path( const char* path )
{
	static char buffer[4096];
	buffer[0] = '\0';
	strcat( buffer, get_test_dir() );
	strcat( buffer, path );
	return buffer;
}

static void setup_test_dir()
{
	// ... remove dir from prev test ...
	remove_dir( get_test_dir() );

	// ... and recreate it ...
	create_dir( get_test_dir() );
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

	fswatcher_t watcher = fswatcher_create( FSWATCHER_CREATE_DEFAULT, FSWATCHER_EVENT_ALL, get_test_dir(), 0x0 );
	ASSERT( 0x0 != watcher );
	test_handler handler = { { watch_event_handler }, FSWATCHER_EVENT_ALL, 0x0, 0x0 };

	const char* path = test_dir_path( "d1" );

	create_dir( path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_CREATE, path, 0x0, &handler ) ) return ret;
	HANDLER_RESET( handler );

	remove_dir( path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_REMOVE, path, 0x0, &handler ) ) return ret;
	HANDLER_RESET( handler );

	create_dir( path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_CREATE, path, 0x0, &handler ) ) return ret;
	HANDLER_RESET( handler );

	fswatcher_destroy( watcher );
	return 0;
}

TEST create_remove_file()
{
	setup_test_dir();

	fswatcher_t watcher = fswatcher_create( FSWATCHER_CREATE_DEFAULT, FSWATCHER_EVENT_ALL, get_test_dir(), 0x0 );
	test_handler handler = { { watch_event_handler }, FSWATCHER_EVENT_ALL, 0x0, 0x0 };

	const char* path = test_dir_path( "f1" );
	create_file( path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	ASSERT_EQ( 0, check_event_handler( FSWATCHER_EVENT_CREATE, path, 0x0, &handler ) );
	HANDLER_RESET( handler );

	remove_file( path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	ASSERT_EQ( 0, check_event_handler( FSWATCHER_EVENT_REMOVE, path, 0x0, &handler ) );
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
	get_test_dir();

    GREATEST_MAIN_BEGIN();
    RUN_SUITE( fswatcher );
    GREATEST_MAIN_END();
}
