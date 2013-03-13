search-library
==============

A generic C++ searching library, proposed for inclusion in the C++ standard library.

The original proposal was [n3411](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3411.pdf), but the interface has been changed based on feedback from the Library Working Group. An updated paper describing the new interface should be available soon.


There are three test programs, unimaginatively named `basic_tests.cpp`, `timing_tests.cpp` and `random_test.cpp`

* `basic_tests.cpp` is basic sanity checking. It makes sure that all the algorithms work.

* `timing_tests.cpp` takes does a bunch of tests on a canned set of data, and reports how long they took.

* `random_test.cpp` is timing on random data. It generates a 3MB corpus, some pattens to search for, and then reports on the results and the timings. It takes one command-line parameter, the number of iterations (default == 3)

