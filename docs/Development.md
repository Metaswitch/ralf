# Development

This document describes how to build and test Ralf.

Ralf development is ongoing on Ubuntu 14.04, so the processes described
below are targetted for (and tested on) this platform.  The code has been
written to be portable, though, and should compile on other platforms once the
required dependencies are installed.

## Dependencies

Ralf depends on a number of tools and libraries.  Some of these are
included as git submodules, but the rest must be installed separately.

On Ubuntu 14.04,

1.  update the package list

        sudo apt-get update

2.  install the required packages
  
        sudo apt-get install git cmake make gcc g++ scons bison flex libsctp-dev libgnutls-dev debhelper fakeroot libevent-dev libtool autoconf libboost-all-dev automake pkg-config libzmq3-dev libcurl4-openssl-dev debhelper devscripts valgrind libxml2-utils libsnmp-dev

## Getting the Code

The ralf code is all in the `ralf` repository, and its submodules, which
are in the `modules` subdirectory.

To get all the code, clone the ralf repository with the `--recursive` flag to
indicate that submodules should be cloned too.

    git clone --recursive git@github.com:Metaswitch/ralf.git

This accesses the repository over SSH on Github, and will not work unless you have a Github account and registered SSH key. If you do not have both of these, you will need to configure Git to read over HTTPS instead:

    git config --global url."https://github.com/".insteadOf git@github.com:
    git clone --recursive git@github.com:Metaswitch/ralf.git
	
## Building Binaries

To build ralf and all its dependencies, change to the top-level `ralf`
directory and issue `make`.

On completion,

* the ralf binary is in `build/bin`
* libraries on which it depends are in `usr/lib`.

Subsequent builds should be quicker, but still check all of the
dependencies.  For fast builds when you've only changed ralf code, change to
the `src` subdirectory below the top-level `ralf` directory and then run
`make`.

## Building Debian Packages

To build Debian packages, run `make deb`.  On completion, Debian packages
are in the parent of the top-level `ralf` directory.

`make deb` does a full build before building the Debian packages and, even if
the code is already built, it can take a minute or two to check all the
dependencies.  If you are sure the code has already been built, you can use
`make deb-only` to just build the Debian packages without checking the
binaries.

`make deb` and `make deb-only` can push the resulting binaries to a Debian
repository server.  To push to a repository server on the build machine, set
the `REPO_DIR` environment variable to the appropriate path.  To push (via
scp) to a repository server on a remote machine, also set the `REPO_SERVER`
environment variable to the user and server name.

## Running Unit Tests

To run the ralf unit test suite, change to the `src` subdirectory below
the top-level `ralf` directory and issue `make test`.

Ralf unit tests use the [Google Test](https://code.google.com/p/googletest/)
framework, so the output from the test run looks something like this.

    [==========] Running 92 tests from 20 test cases.
    [----------] Global test environment set-up.
	...
	[----------] 2 tests from BasicSessionStoreTest/0, where TypeParam = SessionStore::BinarySerializerDeserializer
	[ RUN      ] BasicSessionStoreTest/0.SimpleTest
	[       OK ] BasicSessionStoreTest/0.SimpleTest (0 ms)
	[ RUN      ] BasicSessionStoreTest/0.DeletionTest
	[       OK ] BasicSessionStoreTest/0.DeletionTest (0 ms)
	[----------] 2 tests from BasicSessionStoreTest/0 (0 ms total)
	...
    [----------] Global test environment tear-down
    [==========] 92 tests from 20 test cases ran. (27347 ms total)
    [  PASSED  ] 92 tests.

`make test` also automatically runs code coverage (using
[gcov](http://gcc.gnu.org/onlinedocs/gcc/Gcov.html)) and memory leak checks
(using [Valgrind](http://valgrind.org/)).  If code coverage decreases or
memory is leaked during the tests, an error is displayed. To see the detailed
code coverage results, run `make coverage_raw`.

The ralf makefile offers the following additional options and targets.

*   `make run_test` just runs the tests without doing code coverage or memory
    leak checks.
*   Passing `JUSTTEST=testname` just runs the specified test case.
*   Passing `NOISY=T` enables verbose logging during the tests; you can add
    a logging level (e.g., `NOISY=T:99`) to control which logs you see.
*   `make debug` runs the tests under gdb.
*   `make vg_raw` just runs the memory leak checks.
