## UniTables

UniTables is a small C library that generates tables for Unicode properties. It does not attempt to be a full Unicode processing library.
The generator is primarily maintained with the help of AI, which seems to understand the Unicode data files better than I do. Forgive me.

### Building

With Bazel: `bazel build //:unitables`

With CMake: `cmake -B build && cmake --build build`

Either way the build downloads the pinned Unicode data files and runs the generator before compiling the library.