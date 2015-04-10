var assert = require('assert');
var posix = require("../../lib/posix");

assert.throws(function () {
    posix.fork(123);
}, /takes no arguments/);

var parentPid = process.pid;
var pid = posix.fork();

if(pid === 0) {
  // If this fails it won't reached its parent
  // but will still print to the terminal
  assert.notEqual(process.pid, parentPid);
} else {
  assert.notEqual(process.pid, pid);
}

