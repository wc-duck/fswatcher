[![Build Status](https://travis-ci.org/wc-duck/fswatcher.svg?branch=master)](https://travis-ci.org/wc-duck/fswatcher)
[![Build status](https://ci.appveyor.com/api/projects/status/yso5i3uaai2ibmi4?svg=true)](https://ci.appveyor.com/project/wc-duck/fswatcher)

What is it?
-----------
fswatcher is exactly what it sounds like, a platform independet wrapping of the different filesystem watcher systems on different platforms.

Note that it is currently under development and there are bugs and blemishes all over the place. Bugfixes/optimizations/implementations for other platforms is always appreciated!

Currently supported platforms:
* Linux via inotify
* Windows via ReadDirectoryChangesW
 
OSX port is worked on.

Why writing this?
-----------------

Simply put to scratch my own itch with me not finding a library to do this written in a way that I like and with not that much code.
The goal is not to make the most fully functional filesystem watcher ever, but rather a library that cover most peoples needs in a stable way.

Rules of engagement
-------------------

The user always knows better how his/her system works, we can't decide on how the user allocates memory in their app or how the work with threads. So all memory allocations that is made by the library should be well defined how they work and be done by a user-provided allocator if provided.
Also the library is poll-based ( blocking or non-blocking ) so the user can decide for them self if placing it in its own thread or not is the way to go.

Path and unicode
----------------

Different os:es = different encodings for paths =/
The way fswatcher deals with this is also decided by the user compile time.
Either fswatcher works in utf8 mode where all paths are utf8, both into the library and out of it, or the library works with "native" path encodings.

Controlling how the library works is done by defining 'FSWATCHER_PATH_ENCODING_NATIVE' or 'FSWATCHER_PATH_ENCODING_UTF8' where 'FSWATCHER_PATH_ENCODING_UTF8' is used if no encoding was defined.

Native encodings:
* Linux - utf8 as "char*"
* OSX - utf8 as "char*"
* Windows - utf16 as "WCHAR*"
