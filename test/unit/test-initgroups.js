var assert = require('assert'),
    posix = require("../../lib/posix");

function test_initgroups() {
    posix.initgroups("root", 0);
}

if(posix.getuid() == 0) {
    test_initgroups()
}
else {
    console.log("warning: initgroups tests skipped - not a privileged user!");
}
