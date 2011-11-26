var posix;
try{
    // Node >=0.5
    posix = require(__dirname + '/../../build/Release/posix.node');
}
catch(e) {
    // Node <0.5
    posix = require(__dirname + '/../../build/default/posix.node');
}

// http://pubs.opengroup.org/onlinepubs/007904875/functions/getpgrp.html
posix.getpgrp = function() {
    return posix.getpgid(0);
}

posix.getuid = process.getuid;
posix.getgid = process.getgid;

module.exports = posix;
