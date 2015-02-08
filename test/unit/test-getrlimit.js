var assert = require('assert');
var posix = require('../../lib/posix');

var limits = [
    "core",
    "cpu",
    "data",
    "fsize",
    "nofile",
    "stack",
    "as"
];

var unsupportedLimits = [];

if(['linux', 'darwin', 'freebsd'].indexOf(process.platform) !== -1) {
    limits.push('nproc');
} else {
    unsupportedLimits.push('nproc');
}

// getrlimit: invalid input args
try {
    posix.getrlimit("foobar");
    assert.ok(false);
}
catch(e) { }

try {
    posix.getrlimit();
    assert.ok(false);
}
catch(e) { }

try {
    posix.getrlimit(0);
    assert.ok(false);
}
catch(e) { }

// getrlimit: check all supported resources
for(var i in limits) {
    var limit = posix.getrlimit(limits[i]);
    console.log("getrlimit " + limits[i] + ": " + JSON.stringify(limit));
    assert.equal(true, typeof limit.soft == 'number' || limit.soft === null);
    assert.equal(true, typeof limit.hard == 'number' || limit.hard === null);
}

for(i in unsupportedLimits) {
    try {
        posix.getrlimit(unsupportedLimits[i]);
        assert.ok(false);
    }
    catch(e) { }
}
