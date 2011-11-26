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

posix.getgid = process.getgid;
posix.getuid = process.getuid;
posix.setgid = process.setgid;
posix.setuid = process.setuid;

posix.seteuid = function(euid) {
    euid = (typeof(euid) == 'string') ? posix.getpwnam(euid).uid : euid;
    return posix._seteuid(euid);
}

posix.setreuid = function(ruid, euid) {
    ruid = (typeof(ruid) == 'string') ? posix.getpwnam(ruid).uid : ruid;
    euid = (typeof(euid) == 'string') ? posix.getpwnam(euid).uid : euid;
    return posix._setreuid(ruid, euid);
}

posix.setegid = function(egid) {
    egid = (typeof(egid) == 'string') ? posix.getgrnam(egid).gid : egid;
    return posix._setegid(egid);
}

posix.setregid = function(rgid, egid) {
    rgid = (typeof(rgid) == 'string') ? posix.getgrnam(rgid).gid : rgid;
    egid = (typeof(egid) == 'string') ? posix.getgrnam(egid).gid : egid;
    return posix._setregid(rgid, egid);
}

module.exports = posix;
