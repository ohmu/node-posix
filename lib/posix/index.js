try{
    // Node >=0.5
    module.exports = require(__dirname + '/../../build/Release/posix.node');
}
catch(e) {
    // Node <0.5
    module.exports = require(__dirname + '/../../build/default/posix.node');
}
