# node-posix

The missing POSIX system calls for Node.

## FAQ

* Q: Why?
* A: Because the Node core has a limited set of sometimes useful POSIX system
  calls.
* Q: How mature/stable is this?
* A: Each version released in NPM has decent automated test coverage. The
  module is still new and not battle-hardened.
* Q: I have a feature request/bug report...
* A: Please submit a pull request or an issue ticket at
  https://github.com/melor/node-posix

## Roadmap

### User and Group ID Management
* `getgid()` in Node core as `process.getgid`
* `getuid()` in Node core as `process.getuid`
* `setgid()` in Node core as `process.setgid`
* `setregid()` TODO
* `setreuid()` TODO
* `setuid()` in Node core as `process.setuid`, NOTE: should not be used
  because of inconsistent behavior under different operating systems, see
  http://www.cs.ucdavis.edu/~hchen/paper/usenix02.html
* `ulimit()` obsolete, use `posix.setrlimit()` instead

## General usage

* Installation: `npm install posix`
* In your code: `var posix = require('posix');`

## High-Level Utility Interface

### Drop process privileges temporarily
TODO

### Restore process privileges
TODO

### Drop process privileges permanently
TODO

## POSIX System Calls

### posix.chroot(path, options)

Changes the root directory of the calling process to that specified in `path`.
This directory will be used for pathnames beginning with `/`. The root
directory is inherited by all children of the calling process.

The working directory is also automatically set to the new root directory.

NOTE: Please be aware of the limitations of `chroot` jails:

* "Best Practices for UNIX `chroot()` Operations":
  http://www.unixwiz.net/techtips/chroot-practices.html
* "How to break out of a `chroot()` jail":
  http://www.bpfh.net/simes/computing/chroot-break.html

    posix.chroot('/somewhere/safe');

### posix.getegid()

Returns the current process's effective group ID.

    console.log("Effective GID: " + posix.getegid());

### posix.geteuid()

Returns the current process's effective user ID.

    console.log("Effective UID: " + posix.geteuid());

### posix.getpgid(pid)

Return the process group ID of the current process (`posix.getpgid(0)`) or of
a process of a given PID (`posix.getpgid(PID)`).

    console.log("My PGID: " + posix.getpgid(0));
    console.log("init's PGID: " + posix.getpgid(1));

### posix.getppid()

Returns the parent process's PID.

    console.log("Parent PID: " + posix.getppid());

### posix.getrlimit(resource)

Get resource limits. (See getrlimit(2).)

The `soft` limit is the value that the kernel enforces for the
corresponding resource. The `hard` limit acts as a ceiling for the soft
limit: an unprivileged process may only set its soft limit to a value in the
range from 0 up to the hard limit, and (irreversibly) lower its hard limit.

Supported resources:

`'core'` (RLIMIT_CORE) Maximum size of core file.  When 0 no core dump files
are created.

`'cpu'` (RLIMIT_CPU) CPU time limit in seconds.  When the process reaches the
soft limit, it is sent a SIGXCPU signal. The default action for this signal is
to terminate the process.

`'data'` (RLIMIT_DATA) The maximum size of the process's data segment
(initialized data, uninitialized data, and heap).

`'fsize'` (RLIMIT_FSIZE) The maximum size of files that the process may create.
Attempts to extend a file beyond this limit result in delivery of a SIGXFSZ
signal.

`'nofile'` (RLIMIT_NOFILE) Specifies a value one greater than the maximum file
descriptor number that can be opened by this process.

`'stack'` (RLIMIT_STACK) The maximum size of the process stack, in bytes. Upon
reaching this limit, a SIGSEGV signal is generated.

`'as'` (RLIMIT_AS) The maximum size of the process's virtual memory (address
space) in bytes.

    var limits = posix.getrlimit("nofile");
    console.log('Current limits: soft=' + limits.soft + ", max=" + limits.hard);

### posix.setegid(gid)

Sets the effective group ID of the current process. `gid` can be either a
numeric GID or a group name (string).

    posix.setegid(0); // set effective UID to "wheel"
    posix.seteuid("nobody");

### posix.seteuid(uid)

Sets the effective user ID of the current process. `uid` can be either a
numeric UID or a username (string).

    posix.seteuid(0); // set effective UID to root
    posix.seteuid("nobody");

### posix.setrlimit(resource, limits)

Set resource limits. (See setrlimit(2).) Supported resource types are listed
under `posix.getrlimit`.

The `limits` argument is an object in the form
`{ soft: SOFT_LIMIT, hard: HARD_LIMIT }`. Current limit values are used if
either `soft` or `hard` key is not specifing in the `limits` object. A limit
value of `null` indicates "unlimited" (RLIM_INFINITY).

    // raise maximum number of open file descriptors to 10k, hard limit is left unchanged
    posix.setrlimit("nofile", { soft: 10000 });

    // enable core dumps of unlimited size
    posix.setrlimit("core", { soft: null, hard: null });

### posix.setsid()

Creates a session and sets the process group ID. Returns the process group ID.

    console.log("Session ID: " + posix.setsid());

## Credits

* Some of the documentation strings stolen from Linux man pages.
* `posix.seteuid` etc. implementation is based on Node core project `SetUid`

## LICENSE

Copyright (c) 2011 Mika Eloranta

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
