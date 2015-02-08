var assert = require('assert'),
    posix = require("../../lib/posix");

function test_initgroups() {
    assert.throws(function () {
        posix.initgroups("root", "dummyzzz1234");
    }, /group id does not exist|ENOENT/);

    assert.throws(function () {
        posix.setregid("dummyzzz1234", "root");
    }, /group id does not exist|ENOENT/);

    posix.initgroups("root", 0);
}

if('initgroups' in posix && posix.getuid() === 0) {
    test_initgroups()
}
else {
    console.log("warning: initgroups tests skipped - initgroups not supported or not a privileged user!");
}
