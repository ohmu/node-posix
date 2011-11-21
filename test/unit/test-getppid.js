var assert = require('assert');
var posix = require("posix");

var ppid = posix.getppid()
console.log("getppid: " + ppid);
assert.ok(ppid > 1);
