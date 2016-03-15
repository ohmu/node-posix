var assert = require('assert'),
    posix = require("../../lib/posix");

assert.throws(function () {
  posix.setpgid()
}, /exactly two arguments/)

assert.throws(function () {
  posix.setpgid('a', 1)
}, /must be an integer/)

assert.throws(function () {
  posix.setpgid(1, 'a')
}, /must be an integer/)

var old = posix.getpgid(0);
assert.ok(old !== process.id);

posix.setpgid(0, process.pid);
assert.equal(posix.getpgid(0), process.pid);
