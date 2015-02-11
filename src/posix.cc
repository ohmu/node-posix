#include <nan.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h> // setrlimit, getrlimit
#include <limits.h> // PATH_MAX
#include <pwd.h> // getpwnam, passwd
#include <grp.h> // getgrnam, group
#include <syslog.h> // openlog, closelog, syslog, setlogmask

using node::ErrnoException;

using v8::Array;
using v8::FunctionTemplate;
using v8::Handle;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

NAN_METHOD(node_getppid) {
    NanScope();

    if(args.Length() != 0) {
        return NanThrowError("getppid: takes no arguments");
    }

    NanReturnValue(NanNew<Integer>(getppid()));
}

NAN_METHOD(node_getpgid) {
    NanScope();

    if(args.Length() != 1) {
        return NanThrowError("getpgid: takes exactly one argument");
    }

    if(!args[0]->IsNumber()) {
       return NanThrowTypeError("getpgid: first argument must be a integer");
    }

    NanReturnValue(NanNew<Integer>(getpgid(args[0]->IntegerValue())));
}

NAN_METHOD(node_geteuid) {
    NanScope();

    if(args.Length() != 0) {
        return NanThrowError("geteuid: takes no arguments");
    }

    NanReturnValue(NanNew<Integer>(geteuid()));
}

NAN_METHOD(node_getegid) {
    NanScope();

    if(args.Length() != 0) {
        return NanThrowError("getegid: takes no arguments");
    }

    NanReturnValue(NanNew<Integer>(getegid()));
}

NAN_METHOD(node_setsid) {
    NanScope();

    if(args.Length() != 0) {
        return NanThrowError("setsid: takes no arguments");
    }

    pid_t sid = setsid();

    if(sid == -1) {
        return NanThrowError(ErrnoException(errno, "setsid"));
    }

    NanReturnValue(NanNew<Integer>(sid));
}

NAN_METHOD(node_chroot) {
    NanScope();

    if(args.Length() != 1) {
        return NanThrowError("chroot: takes exactly one argument");
    }

    if(!args[0]->IsString()) {
        return NanThrowTypeError("chroot: first argument must be a string");
    }

    String::Utf8Value dir_path(args[0]->ToString());

    // proper order is to first chdir() and then chroot()
    if(chdir(*dir_path)) {
        return NanThrowError(ErrnoException(errno, "chroot: chdir: "));
    }

    if(chroot(*dir_path)) {
        return NanThrowError(ErrnoException(errno, "chroot"));
    }

    NanReturnValue(NanUndefined());
}

struct name_to_int_t {
  const char* name;
  int resource;
};

static const name_to_int_t rlimit_name_to_res[] = {
  { "core", RLIMIT_CORE },
  { "cpu", RLIMIT_CPU },
  { "data", RLIMIT_DATA },
  { "fsize", RLIMIT_FSIZE },
  { "nofile", RLIMIT_NOFILE },
  #ifdef RLIMIT_NPROC
  { "nproc", RLIMIT_NPROC },
    #endif
  { "stack", RLIMIT_STACK },
  { "as", RLIMIT_AS },
  { 0, 0 }
};

// return null if value is RLIM_INFINITY, otherwise the uint value
static Handle<Value> rlimit_value(rlim_t limit) {
    if(limit == RLIM_INFINITY) {
        return NanNull();
    } else {
        return NanNew<Number>((double)limit);
    }
}

NAN_METHOD(node_getrlimit) {
    NanScope();

    if(args.Length() != 1) {
        return NanThrowError("getrlimit: requires exactly one argument");
    }

    if (!args[0]->IsString()) {
        return NanThrowTypeError("getrlimit: argument must be a string");
    }

    struct rlimit limit;
    String::Utf8Value rlimit_name(args[0]->ToString());
    int resource = -1;

    for(const name_to_int_t* item = rlimit_name_to_res;
        item->name; ++item) {
        if(!strcmp(*rlimit_name, item->name)) {
            resource = item->resource;
            break;
        }
    }

    if(resource < 0) {
        return NanThrowError("getrlimit: unknown resource name");
    }

    if(getrlimit(resource, &limit)) {
        return NanThrowError(ErrnoException(errno, "getrlimit"));
    }

    Local<Object> info = NanNew<Object>();
    info->Set(NanNew<String>("soft"), rlimit_value(limit.rlim_cur));
    info->Set(NanNew<String>("hard"), rlimit_value(limit.rlim_max));

    NanReturnValue(info);
}

NAN_METHOD(node_setrlimit) {
    NanScope();

    if(args.Length() != 2) {
        return NanThrowError("setrlimit: requires exactly two arguments");
    }

    if (!args[0]->IsString()) {
        return NanThrowTypeError("setrlimit: argument 0 must be a string");
    }

    if (!args[1]->IsObject()) {
        return NanThrowTypeError("setrlimit: argument 1 must be an object");
    }

    String::Utf8Value rlimit_name(args[0]->ToString());
    int resource = -1;
    for(const name_to_int_t* item = rlimit_name_to_res;
        item->name; ++item) {
        if(!strcmp(*rlimit_name, item->name)) {
            resource = item->resource;
            break;
        }
    }

    if(resource < 0) {
        return NanThrowError("setrlimit: unknown resource name");
    }

    Local<Object> limit_in = args[1]->ToObject(); // Cast
    Local<String> soft_key = NanNew<String>("soft");
    Local<String> hard_key = NanNew<String>("hard");
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
            return NanThrowError(ErrnoException(errno, "getrlimit"));
        }
        if(get_soft) { limit.rlim_cur = current.rlim_cur; }
        if(get_hard) { limit.rlim_max = current.rlim_max; }
    }

    if(setrlimit(resource, &limit)) {
        return NanThrowError(ErrnoException(errno, "setrlimit"));
    }

    NanReturnValue(NanUndefined());
}

NAN_METHOD(node_getpwnam) {
    NanScope();

    if(args.Length() != 1) {
        return NanThrowError("getpwnam: requires exactly 1 argument");
    }

    struct passwd* pwd;
    errno = 0; // reset errno before the call

    if(args[0]->IsNumber()) {
        pwd = getpwuid(args[0]->Int32Value());
        if(errno) {
            return NanThrowError(ErrnoException(errno, "getpwuid"));
        }
    } else if (args[0]->IsString()) {
        String::Utf8Value pwnam(args[0]->ToString());
        pwd = getpwnam(*pwnam);
        if(errno) {
            return NanThrowError(ErrnoException(errno, "getpwnam"));
        }
    } else {
        return NanThrowTypeError("argument must be a number or a string");
    }

    if(!pwd) {
        return NanThrowError("user id does not exist");
    }

    Local<Object> obj = NanNew<Object>();
    obj->Set(NanNew<String>("name"), NanNew<String>(pwd->pw_name));
    obj->Set(NanNew<String>("passwd"), NanNew<String>(pwd->pw_passwd));
    obj->Set(NanNew<String>("uid"), NanNew<Number>(pwd->pw_uid));
    obj->Set(NanNew<String>("gid"), NanNew<Number>(pwd->pw_gid));
    obj->Set(NanNew<String>("gecos"), NanNew<String>(pwd->pw_gecos));
    obj->Set(NanNew<String>("shell"), NanNew<String>(pwd->pw_shell));
    obj->Set(NanNew<String>("dir"), NanNew<String>(pwd->pw_dir));

    NanReturnValue(obj);
}

NAN_METHOD(node_getgrnam) {
    NanScope();

    if(args.Length() != 1) {
        return NanThrowError("getgrnam: requires exactly 1 argument");
    }

    struct group* grp;
    errno = 0; // reset errno before the call

    if(args[0]->IsNumber()) {
        grp = getgrgid(args[0]->Int32Value());
        if(errno) {
            return NanThrowError(ErrnoException(errno, "getgrgid"));
        }
    } else if (args[0]->IsString()) {
        String::Utf8Value pwnam(args[0]->ToString());
        grp = getgrnam(*pwnam);
        if(errno) {
            return NanThrowError(ErrnoException(errno, "getgrnam"));
        }
    } else {
        return NanThrowTypeError("argument must be a number or a string");
    }

    if(!grp) {
        return NanThrowError("group id does not exist");
    }

    Local<Object> obj = NanNew<Object>();
    obj->Set(NanNew<String>("name"), NanNew<String>(grp->gr_name));
    obj->Set(NanNew<String>("passwd"), NanNew<String>(grp->gr_passwd));
    obj->Set(NanNew<String>("gid"), NanNew<Number>(grp->gr_gid));

    Local<Array> members = NanNew<Array>();
    char** cur = grp->gr_mem;
    for(size_t i=0; *cur; ++i, ++cur) {
        (*members)->Set(i, NanNew<String>(*cur));
    }
    obj->Set(NanNew<String>("members"), members);

    NanReturnValue(obj);
}

NAN_METHOD(node_initgroups) {
    NanScope();

    if (args.Length() != 2) {
        return NanThrowError("initgroups: requires exactly 2 arguments");
    }

    if (!args[0]->IsString() || !args[1]->IsNumber()) {
        return NanThrowTypeError("initgroups: first argument must be a string "
                         " and the second an integer");
    }

    String::Utf8Value unam(args[0]->ToString());
    if (initgroups(*unam, args[1]->Int32Value())) {
        return NanThrowError(ErrnoException(errno, "initgroups"));
    }

    NanReturnValue(NanUndefined());
}

NAN_METHOD(node_seteuid) {
    NanScope();

    if(args.Length() != 1) {
        return NanThrowError("seteuid: requires exactly 1 argument");
    }

    if(seteuid(args[0]->Int32Value())) {
        return NanThrowError(ErrnoException(errno, "seteuid"));
    }

    NanReturnValue(NanUndefined());
}

NAN_METHOD(node_setegid) {
    NanScope();

    if(args.Length() != 1) {
        return NanThrowError("setegid: requires exactly 1 argument");
    }

    if(setegid(args[0]->Int32Value())) {
        return NanThrowError(ErrnoException(errno, "setegid"));
    }

    NanReturnValue(NanUndefined());
}

NAN_METHOD(node_setregid) {
    NanScope();

    if(args.Length() != 2) {
        return NanThrowError("setregid: requires exactly 2 arguments");
    }

    if(setregid(args[0]->Int32Value(), args[1]->Int32Value())) {
        return NanThrowError(ErrnoException(errno, "setregid"));
    }

    NanReturnValue(NanUndefined());
}

NAN_METHOD(node_setreuid) {
    NanScope();

    if(args.Length() != 2) {
        return NanThrowError("setreuid: requires exactly 2 arguments");
    }

    if(setreuid(args[0]->Int32Value(), args[1]->Int32Value())) {
        return NanThrowError(ErrnoException(errno, "setreuid"));
    }

    NanReturnValue(NanUndefined());
}

// openlog() first argument (const char* ident) is not guaranteed to be
// copied within the openlog() call so we need to keep it in a safe location
static const size_t MAX_SYSLOG_IDENT=100;
static char syslog_ident[MAX_SYSLOG_IDENT+1] = {0};

NAN_METHOD(node_openlog) {
    NanScope();

    if(args.Length() != 3) {
        return NanThrowError("openlog: requires exactly 3 arguments");
    }

    String::Utf8Value ident(args[0]->ToString());
    strncpy(syslog_ident, *ident, MAX_SYSLOG_IDENT);
    syslog_ident[MAX_SYSLOG_IDENT] = 0;
    if(!args[1]->IsNumber() || !args[2]->IsNumber()) {
        return NanThrowError("openlog: invalid argument values");
    }
    // note: openlog does not ever fail, no return value
    openlog(syslog_ident, args[1]->Int32Value(), args[2]->Int32Value());

    NanReturnValue(NanUndefined());
}

NAN_METHOD(node_closelog) {
    NanScope();

    if(args.Length() != 0) {
        return NanThrowError("closelog: does not take any arguments");
    }

    // note: closelog does not ever fail, no return value
    closelog();

    NanReturnValue(NanUndefined());
}

NAN_METHOD(node_syslog) {
    NanScope();

    if(args.Length() != 2) {
        return NanThrowError("syslog: requires exactly 2 arguments");
    }

    String::Utf8Value message(args[1]->ToString());
    // note: syslog does not ever fail, no return value
    syslog(args[0]->Int32Value(), "%s", *message);

    NanReturnValue(NanUndefined());
}

NAN_METHOD(node_setlogmask) {
    NanScope();

    if(args.Length() != 1) {
        return NanThrowError("setlogmask: takes exactly 1 argument");
    }

    NanReturnValue(NanNew<Integer>(setlogmask(args[0]->Int32Value())));
}

#define ADD_MASK_FLAG(name, flag) \
    obj->Set(NanNew<String>(name), NanNew<Integer>(flag)); \
    obj->Set(NanNew<String>("mask_" name), NanNew<Integer>(LOG_MASK(flag)));

NAN_METHOD(node_update_syslog_constants) {
    NanScope();

    if(args.Length() != 1) {
      return NanThrowError("update_syslog_constants: takes exactly 1 argument");
    }

    if(!args[0]->IsObject()) {
        return NanThrowTypeError("update_syslog_constants: argument must be an object");
    }

    Local<Object> obj = args[0]->ToObject();
    ADD_MASK_FLAG("emerg", LOG_EMERG);
    ADD_MASK_FLAG("alert", LOG_ALERT);
    ADD_MASK_FLAG("crit", LOG_CRIT);
    ADD_MASK_FLAG("err", LOG_ERR);
    ADD_MASK_FLAG("warning", LOG_WARNING);
    ADD_MASK_FLAG("notice", LOG_NOTICE);
    ADD_MASK_FLAG("info", LOG_INFO);
    ADD_MASK_FLAG("debug", LOG_DEBUG);

    // facility constants
    obj->Set(NanNew<String>("auth"), NanNew<Integer>(LOG_AUTH));
#ifdef LOG_AUTHPRIV
    obj->Set(NanNew<String>("authpriv"), NanNew<Integer>(LOG_AUTHPRIV));
#endif
    obj->Set(NanNew<String>("cron"), NanNew<Integer>(LOG_CRON));
    obj->Set(NanNew<String>("daemon"), NanNew<Integer>(LOG_DAEMON));
#ifdef LOG_FTP
    obj->Set(NanNew<String>("ftp"), NanNew<Integer>(LOG_FTP));
#endif
    obj->Set(NanNew<String>("kern"), NanNew<Integer>(LOG_KERN));
    obj->Set(NanNew<String>("lpr"), NanNew<Integer>(LOG_LPR));
    obj->Set(NanNew<String>("mail"), NanNew<Integer>(LOG_MAIL));
    obj->Set(NanNew<String>("news"), NanNew<Integer>(LOG_NEWS));
    obj->Set(NanNew<String>("syslog"), NanNew<Integer>(LOG_SYSLOG));
    obj->Set(NanNew<String>("user"), NanNew<Integer>(LOG_USER));
    obj->Set(NanNew<String>("uucp"), NanNew<Integer>(LOG_UUCP));
    obj->Set(NanNew<String>("local0"), NanNew<Integer>(LOG_LOCAL0));
    obj->Set(NanNew<String>("local1"), NanNew<Integer>(LOG_LOCAL1));
    obj->Set(NanNew<String>("local2"), NanNew<Integer>(LOG_LOCAL2));
    obj->Set(NanNew<String>("local3"), NanNew<Integer>(LOG_LOCAL3));
    obj->Set(NanNew<String>("local4"), NanNew<Integer>(LOG_LOCAL4));
    obj->Set(NanNew<String>("local5"), NanNew<Integer>(LOG_LOCAL5));
    obj->Set(NanNew<String>("local6"), NanNew<Integer>(LOG_LOCAL6));
    obj->Set(NanNew<String>("local7"), NanNew<Integer>(LOG_LOCAL7));

    // option constants
    obj->Set(NanNew<String>("pid"), NanNew<Integer>(LOG_PID));
    obj->Set(NanNew<String>("cons"), NanNew<Integer>(LOG_CONS));
    obj->Set(NanNew<String>("ndelay"), NanNew<Integer>(LOG_NDELAY));
    obj->Set(NanNew<String>("odelay"), NanNew<Integer>(LOG_ODELAY));
    obj->Set(NanNew<String>("nowait"), NanNew<Integer>(LOG_NOWAIT));

    NanReturnValue(NanUndefined());
}

NAN_METHOD(node_gethostname) {
    NanScope();

    if(args.Length() != 0) {
        return NanThrowError("gethostname: takes no arguments");
    }
#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX 255
#endif

    char hostname[HOST_NAME_MAX];

    int rc = gethostname(hostname, HOST_NAME_MAX);
    if (rc != 0) {
        return NanThrowError(ErrnoException(errno, "gethostname"));
    }

    NanReturnValue(NanNew<String>(hostname));
}

NAN_METHOD(node_sethostname) {
    NanScope();

    if (args.Length() != 1) {
        return NanThrowError("sethostname: takes exactly 1 argument");
    }

    if (!args[0]->IsString()) {
        return NanThrowTypeError("sethostname: first argument must be a string");
    }

    String::Utf8Value str(args[0]);

    int rc = sethostname(*str, str.length());
    if (rc != 0) {
        return NanThrowError(ErrnoException(errno, "sethostname"));
    }

    NanReturnValue(NanUndefined());
}

void init(Handle<Object> exports) {
    exports->Set(NanNew<String>("getppid"),
        NanNew<FunctionTemplate>(node_getppid)->GetFunction());

    exports->Set(NanNew<String>("getpgid"),
        NanNew<FunctionTemplate>(node_getpgid)->GetFunction());

    exports->Set(NanNew<String>("geteuid"),
        NanNew<FunctionTemplate>(node_geteuid)->GetFunction());

    exports->Set(NanNew<String>("getegid"),
        NanNew<FunctionTemplate>(node_getegid)->GetFunction());

    exports->Set(NanNew<String>("setsid"),
        NanNew<FunctionTemplate>(node_setsid)->GetFunction());

    exports->Set(NanNew<String>("chroot"),
        NanNew<FunctionTemplate>(node_chroot)->GetFunction());

    exports->Set(NanNew<String>("getrlimit"),
        NanNew<FunctionTemplate>(node_getrlimit)->GetFunction());

    exports->Set(NanNew<String>("setrlimit"),
        NanNew<FunctionTemplate>(node_setrlimit)->GetFunction());

    exports->Set(NanNew<String>("getpwnam"),
        NanNew<FunctionTemplate>(node_getpwnam)->GetFunction());

    exports->Set(NanNew<String>("getgrnam"),
        NanNew<FunctionTemplate>(node_getgrnam)->GetFunction());

    exports->Set(NanNew<String>("initgroups"),
        NanNew<FunctionTemplate>(node_initgroups)->GetFunction());

    exports->Set(NanNew<String>("seteuid"),
        NanNew<FunctionTemplate>(node_seteuid)->GetFunction());

    exports->Set(NanNew<String>("setegid"),
        NanNew<FunctionTemplate>(node_setegid)->GetFunction());

    exports->Set(NanNew<String>("setregid"),
        NanNew<FunctionTemplate>(node_setregid)->GetFunction());

    exports->Set(NanNew<String>("setreuid"),
        NanNew<FunctionTemplate>(node_setreuid)->GetFunction());

    exports->Set(NanNew<String>("openlog"),
        NanNew<FunctionTemplate>(node_openlog)->GetFunction());

    exports->Set(NanNew<String>("closelog"),
        NanNew<FunctionTemplate>(node_closelog)->GetFunction());

    exports->Set(NanNew<String>("syslog"),
        NanNew<FunctionTemplate>(node_syslog)->GetFunction());

    exports->Set(NanNew<String>("setlogmask"),
        NanNew<FunctionTemplate>(node_setlogmask)->GetFunction());

    exports->Set(NanNew<String>("update_syslog_constants"),
        NanNew<FunctionTemplate>(node_update_syslog_constants)->GetFunction());

    exports->Set(NanNew<String>("gethostname"),
        NanNew<FunctionTemplate>(node_gethostname)->GetFunction());

    exports->Set(NanNew<String>("sethostname"),
        NanNew<FunctionTemplate>(node_sethostname)->GetFunction());
}

NODE_MODULE(posix, init);
