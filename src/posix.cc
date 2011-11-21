#include <node.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
//#include <assert.h>
//#include <stdlib.h>
#include <sys/resource.h> /* setrlimit, getrlimit */

#define THROW(msg) ThrowException(Exception::Error(String::New(msg)));

using namespace v8;
using namespace node;

static Handle<Value> node_getppid(const Arguments& args) {
    HandleScope scope;

    if(args.Length() != 0) {
        THROW("getppid takes no arguments");
    }

    return scope.Close(Integer::New(getppid()));
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
        return THROW("getrlimit requires exactly 1 argument");
    }

    struct rlimit limit;
    if (!args[0]->IsString()) {
        return THROW("getrlimit argument must be a string");
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
        return THROW("getrlimit: unknown resource name");
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
        return THROW("setrlimit requires exactly 2 arguments");
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
        return THROW("setrlimit: unknown resource name");
    }

    if (!args[1]->IsObject()) {
        return THROW("getrlimit second argument must be an object");
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
    NODE_SET_METHOD(target, "getrlimit", node_getrlimit);
    NODE_SET_METHOD(target, "setrlimit", node_setrlimit);
}
