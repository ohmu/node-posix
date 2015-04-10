var assert = require('assert');
var posix = require("../../lib/posix");

////////////////////////////////////////
// getppid()
var ppid = posix.getppid();
console.log("getppid: " + ppid);
assert.ok(ppid > 1);

////////////////////////////////////////
// getpid()
var pid = posix.getpid();
console.log("getpid: " + pid);
assert.equal(pid, process.pid);
