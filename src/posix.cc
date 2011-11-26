#include <node.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h> /* setrlimit, getrlimit */

#define EXCEPTION(msg) ThrowException(Exception::Error(String::New(msg)));

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

extern "C" void init(Handle<Object> target)
{
    HandleScope scope;
    NODE_SET_METHOD(target, "getppid", node_getppid);
    NODE_SET_METHOD(target, "getpgid", node_getpgid);
    NODE_SET_METHOD(target, "setsid", node_setsid);
    NODE_SET_METHOD(target, "chroot", node_chroot);
    NODE_SET_METHOD(target, "getrlimit", node_getrlimit);
    NODE_SET_METHOD(target, "setrlimit", node_setrlimit);
}
