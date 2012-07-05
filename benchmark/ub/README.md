## how to build

Ideally, the below is all you need:

    gyp -Dlibrary=static_library --depth=. build.gyp

However, until [gyp bug #279](http://code.google.com/p/gyp/issues/detail?id=279)
is resolved, you will need to build from the top-level directory.

    gyp -Dlibrary=static_library --depth=$PWD benchmark/ub/build.gyp

Note that it clobbers the existing Makefile but not existing builds. node.js
builds in `out/Release` and `out/Debug`, this project builds in `out/Default`.
