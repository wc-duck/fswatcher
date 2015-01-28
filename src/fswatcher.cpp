#if defined( __linux__ )
#   include "fswatcher_linux.cpp"
#elif defined( _WIN32 )
#   include "fswatcher_windows.cpp"
#elif defined( __OSX__ )
#   include "fswatcher_osx.cpp"
#else
#   error "Unsupported platform"
#endif
