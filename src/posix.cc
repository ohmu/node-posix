#include <node.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h> // setrlimit, getrlimit
#include <limits.h> // PATH_MAX
#include <pwd.h> // getpwnam_r, passwd
#include <grp.h> // getgrnam_r, group

#define EXCEPTION(msg) ThrowException(Exception::Error(String::New(msg)))

using namespace v8;
using namespace node;

static Handle<Value> node_getppid(const Arguments& args) {
    HandleScope scope;

    if(args.Length() != 0) {
        return EXCEPTION("getppid: takes no arguments");
    }

    return scope.Close(Integer::New(getppid()));
}

static Handle<Value> node_getpgid(const Arguments& args) {
    HandleScope scope;

    if(args.Length() != 1) {
        return EXCEPTION("getpgid: takes exactly one argument");
    }

    if (!args[0]->IsNumber()) {
        return EXCEPTION("getpgid: first argument must be a integer");
    }

    return scope.Close(Integer::New(getpgid(args[0]->IntegerValue())));
}

static Handle<Value> node_geteuid(const Arguments& args) {
    HandleScope scope;

    if(args.Length() != 0) {
        return EXCEPTION("geteuid: takes no arguments");
    }

    return scope.Close(Integer::New(geteuid()));
}

static Handle<Value> node_getegid(const Arguments& args) {
    HandleScope scope;

    if(args.Length() != 0) {
        return EXCEPTION("getegid: takes no arguments");
    }

    return scope.Close(Integer::New(getegid()));
}

static Handle<Value> node_setsid(const Arguments& args) {
    HandleScope scope;

    if(args.Length() != 0) {
        return EXCEPTION("setsid: takes no arguments");
    }

    pid_t sid = setsid();
    if(sid == -1) {
        return ThrowException(ErrnoException(errno, "setsid"));
    }

    return scope.Close(Integer::New(sid));
}

static Handle<Value> node_chroot(const Arguments& args) {
    if(args.Length() != 1) {
        return EXCEPTION("chroot: takes exactly one argument");
    }

    if (!args[0]->IsString()) {
        return EXCEPTION("chroot: first argument must be a string");
    }

    String::Utf8Value dir_path(args[0]->ToString());

    // proper order is to first chdir() and then chroot()
    if(chdir(*dir_path)) {
        return ThrowException(ErrnoException(errno, "chroot: chdir: "));
    }

    if(chroot(*dir_path)) {
        return ThrowException(ErrnoException(errno, "chroot"));
    }

    return Undefined();
}

struct rlimit_name_to_res_t {
  const char* name;
  int resource;
};

static const rlimit_name_to_res_t rlimit_name_to_res[] = {
  { "core", RLIMIT_CORE },
  { "cpu", RLIMIT_CPU },
  { "data", RLIMIT_DATA },
  { "fsize", RLIMIT_FSIZE },
  { "nofile", RLIMIT_NOFILE },
  { "stack", RLIMIT_STACK },
  { "as", RLIMIT_AS },
  { 0, 0 }
};

static Handle<Value> node_getrlimit(const Arguments& args) {
    HandleScope scope;

    if(args.Length() != 1) {
        return EXCEPTION("getrlimit: requires exactly 1 argument");
    }

    struct rlimit limit;
    if (!args[0]->IsString()) {
        return EXCEPTION("getrlimit: argument must be a string");
    }

    String::Utf8Value rlimit_name(args[0]->ToString());
    int resource = -1;
    for(const rlimit_name_to_res_t* item = rlimit_name_to_res;
        item->name; ++item) {
        if(!strcmp(*rlimit_name, item->name)) {
            resource = item->resource;
            break;
        }
    }
    if(resource < 0)
    {
        return EXCEPTION("getrlimit: unknown resource name");
    }

    if(getrlimit(resource, &limit)) {
        return ThrowException(ErrnoException(errno, "getrlimit"));
    }

    Local<Object> info = Object::New();
    info->Set(String::New("soft"),
              Integer::NewFromUnsigned(limit.rlim_cur));
    info->Set(String::New("hard"),
              Integer::NewFromUnsigned(limit.rlim_max));

    return scope.Close(info);
}

static Handle<Value> node_setrlimit(const Arguments& args) {
    HandleScope scope;

    if(args.Length() != 2) {
        return EXCEPTION("setrlimit: requires exactly 2 arguments");
    }

    String::Utf8Value rlimit_name(args[0]->ToString());
    int resource = -1;
    for(const rlimit_name_to_res_t* item = rlimit_name_to_res;
        item->name; ++item) {
        if(!strcmp(*rlimit_name, item->name)) {
            resource = item->resource;
            break;
        }
    }

    if(resource < 0)
    {
        return EXCEPTION("setrlimit: unknown resource name");
    }

    if (!args[1]->IsObject()) {
        return EXCEPTION("getrlimit: second argument must be an object");
    }

    Local<Object> limit_in = args[1]->ToObject(); // Cast
    Local<String> soft_key = String::New("soft");
    Local<String> hard_key = String::New("hard");
    struct rlimit limit;
    bool get_soft = false, get_hard = false;
    if (limit_in->Has(soft_key)) {
        if(limit_in->Get(soft_key)->IsNull()) {
            limit.rlim_cur = RLIM_INFINITY;
        }
        else {
            limit.rlim_cur = limit_in->Get(soft_key)->IntegerValue();
        }
    }
    else {
        get_soft = true;
    }

    if (limit_in->Has(hard_key)) {
        if(limit_in->Get(hard_key)->IsNull()) {
            limit.rlim_max = RLIM_INFINITY;
        }
        else {
            limit.rlim_max = limit_in->Get(hard_key)->IntegerValue();
        }
    }
    else {
        get_hard = true;
    }

    if(get_soft || get_hard) {
        // current values for the limits are needed
        struct rlimit current;
        if(getrlimit(resource, &current)) {
            return ThrowException(ErrnoException(errno, "getrlimit"));
        }
        if(get_soft) { limit.rlim_cur = current.rlim_cur; }
        if(get_hard) { limit.rlim_max = current.rlim_max; }
    }

    if(setrlimit(resource, &limit)) {
        return ThrowException(ErrnoException(errno, "setrlimit"));
    }

    return Undefined();
}

Handle<Value> arg_to_uid(const Local<Value> arg, uid_t* uid) {
    char getbuf[PATH_MAX + 1];
    if(arg->IsNumber()) {
        *uid = arg->Int32Value();
    }
    else if (arg->IsString()) {
        String::Utf8Value pwnam(arg->ToString());
        struct passwd pwd, *pwdp = NULL;
        int err = getpwnam_r(*pwnam, &pwd, getbuf, sizeof(getbuf), &pwdp);
        if(err || (pwdp == NULL)) {
            if(errno == 0)
                return EXCEPTION("user id does not exist");
            else
                return ThrowException(ErrnoException(errno, "getpwnam_r"));
        }

        *uid = pwdp->pw_uid;
    }
    else {
        return EXCEPTION("argument must be a number or a string");
    }

    return Null();
}

Handle<Value> arg_to_gid(const Local<Value> arg, gid_t* gid) {
    char getbuf[PATH_MAX + 1];
    if(arg->IsNumber()) {
        *gid = arg->Int32Value();
    }
    else if(arg->IsString()) {
        String::Utf8Value grpnam(arg->ToString());
        struct group grp, *grpp = NULL;
        int err = getgrnam_r(*grpnam, &grp, getbuf, sizeof(getbuf), &grpp);
        if(err || (grpp == NULL)) {
            if(errno == 0)
                return EXCEPTION("group id does not exist");
            else
                return ThrowException(ErrnoException(errno, "getpwnam_r"));
        }

        *gid = grpp->gr_gid;
    }
    else {
        return EXCEPTION("argument must be a number or a string");
    }

    return Null();
}

static Handle<Value> node_seteuid(const Arguments& args) {
    HandleScope scope;

    if(args.Length() != 1) {
        return EXCEPTION("seteuid: requires exactly 1 argument");
    }

    uid_t uid = 0;
    Handle<Value> error = arg_to_uid(args[0], &uid);
    if(!error->IsNull()) {
        return error;
    }

    if(seteuid(uid)) {
        return ThrowException(ErrnoException(errno, "seteuid"));
    }

    return Undefined();
}

static Handle<Value> node_setegid(const Arguments& args) {
    HandleScope scope;

    if(args.Length() != 1) {
        return EXCEPTION("setegid: requires exactly 1 argument");
    }

    uid_t gid = 0;
    Handle<Value> error = arg_to_gid(args[0], &gid);
    if(!error->IsNull()) {
        return error;
    }

    if(setegid(gid)) {
        return ThrowException(ErrnoException(errno, "setegid"));
    }

    return Undefined();
}

static Handle<Value> node_setreuid(const Arguments& args) {
    HandleScope scope;

    if(args.Length() != 2) {
        return EXCEPTION("setreuid: requires exactly 2 arguments");
    }

    uid_t ruid = 0;
    Handle<Value> error = arg_to_uid(args[0], &ruid);
    if(!error->IsNull()) {
        return error;
    }

    uid_t euid = 0;
    error = arg_to_uid(args[1], &euid);
    if(!error->IsNull()) {
        return error;
    }

    if(setreuid(ruid, euid)) {
        return ThrowException(ErrnoException(errno, "setreuid"));
    }

    return Undefined();
}


extern "C" void init(Handle<Object> target)
{
    HandleScope scope;
    NODE_SET_METHOD(target, "chroot", node_chroot);
    NODE_SET_METHOD(target, "getegid", node_getegid);
    NODE_SET_METHOD(target, "geteuid", node_geteuid);
    NODE_SET_METHOD(target, "getpgid", node_getpgid);
    NODE_SET_METHOD(target, "getppid", node_getppid);
    NODE_SET_METHOD(target, "getrlimit", node_getrlimit);
    NODE_SET_METHOD(target, "setegid", node_setegid);
    NODE_SET_METHOD(target, "seteuid", node_seteuid);
    NODE_SET_METHOD(target, "setreuid", node_setreuid);
    NODE_SET_METHOD(target, "setrlimit", node_setrlimit);
    NODE_SET_METHOD(target, "setsid", node_setsid);
}
