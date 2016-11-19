#include <nan.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h> // setrlimit, getrlimit
#include <limits.h> // PATH_MAX
#include <pwd.h> // getpwnam, passwd
#include <grp.h> // getgrnam, group
#include <syslog.h> // openlog, closelog, syslog, setlogmask

#ifdef __linux__
#  include <sys/swap.h>  // swapon, swapoff
#endif

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
    Nan::HandleScope scope;

    if (info.Length() != 0) {
        return Nan::ThrowError("getppid: takes no arguments");
    }

    // on some platforms pid_t is defined as long hence the static_cast
    info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int32_t>(getppid())));
}

NAN_METHOD(node_getpgid) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("getpgid: takes exactly one argument");
    }

    if (!info[0]->IsNumber()) {
       return Nan::ThrowTypeError("getpgid: first argument must be an integer");
    }

    // on some platforms pid_t is defined as long hence the static_cast
    info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int32_t>(getpgid(info[0]->IntegerValue()))));
}

NAN_METHOD(node_setpgid) {
    Nan::HandleScope scope;

    if (info.Length() != 2) {
        return Nan::ThrowError("setpgid: takes exactly two arguments");
    }

    if (!info[0]->IsNumber()) {
        return Nan::ThrowTypeError("setpgid: first argument must be an integer");
    }

    if (!info[1]->IsNumber()) {
        return Nan::ThrowTypeError("setpgid: first argument must be an integer");
    }

    if (setpgid(info[0]->IntegerValue(), info[1]->IntegerValue()) < 0) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "setpgid", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_geteuid) {
    Nan::HandleScope scope;

    if (info.Length() != 0) {
        return Nan::ThrowError("geteuid: takes no arguments");
    }

    info.GetReturnValue().Set(Nan::New<Integer>(geteuid()));
}

NAN_METHOD(node_getegid) {
    Nan::HandleScope scope;

    if (info.Length() != 0) {
        return Nan::ThrowError("getegid: takes no arguments");
    }

    info.GetReturnValue().Set(Nan::New<Integer>(getegid()));
}

NAN_METHOD(node_setsid) {
    Nan::HandleScope scope;

    if (info.Length() != 0) {
        return Nan::ThrowError("setsid: takes no arguments");
    }

    pid_t sid = setsid();

    if (sid == -1) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "setsid", ""));
    }

    // on some platforms pid_t is defined as long hence the static_cast
    info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int32_t>(sid)));
}

NAN_METHOD(node_chroot) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("chroot: takes exactly one argument");
    }

    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("chroot: first argument must be a string");
    }

    String::Utf8Value dir_path(info[0]->ToString());

    // proper order is to first chdir() and then chroot()
    if (chdir(*dir_path)) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "chroot: chdir: ", ""));
    }

    if(chroot(*dir_path)) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "chroot", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
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
  #ifdef RLIMIT_AS
  { "as", RLIMIT_AS },
  #endif
  { 0, 0 }
};

// return null if value is RLIM_INFINITY, otherwise the uint value
static Handle<Value> rlimit_value(rlim_t limit) {
    if (limit == RLIM_INFINITY) {
        return Nan::Null();
    } else {
        return Nan::New<Number>((double)limit);
    }
}

NAN_METHOD(node_getrlimit) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("getrlimit: requires exactly one argument");
    }

    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("getrlimit: argument must be a string");
    }

    struct rlimit limit;
    String::Utf8Value rlimit_name(info[0]->ToString());
    int resource = -1;

    for (const name_to_int_t* item = rlimit_name_to_res; item->name; ++item) {
        if (!strcmp(*rlimit_name, item->name)) {
            resource = item->resource;
            break;
        }
    }

    if (resource < 0) {
        return Nan::ThrowError("getrlimit: unknown resource name");
    }

    if (getrlimit(resource, &limit)) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "getrlimit", ""));
    }

    Local<Object> data = Nan::New<Object>();
    data->Set(Nan::New<String>("soft").ToLocalChecked(), rlimit_value(limit.rlim_cur));
    data->Set(Nan::New<String>("hard").ToLocalChecked(), rlimit_value(limit.rlim_max));

    info.GetReturnValue().Set(data);
}

NAN_METHOD(node_setrlimit) {
    Nan::HandleScope scope;

    if (info.Length() != 2) {
        return Nan::ThrowError("setrlimit: requires exactly two arguments");
    }

    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("setrlimit: argument 0 must be a string");
    }

    if (!info[1]->IsObject()) {
        return Nan::ThrowTypeError("setrlimit: argument 1 must be an object");
    }

    String::Utf8Value rlimit_name(info[0]->ToString());
    int resource = -1;
    for (const name_to_int_t* item = rlimit_name_to_res; item->name; ++item) {
        if (!strcmp(*rlimit_name, item->name)) {
            resource = item->resource;
            break;
        }
    }

    if (resource < 0) {
        return Nan::ThrowError("setrlimit: unknown resource name");
    }

    Local<Object> limit_in = info[1]->ToObject(); // Cast
    Local<String> soft_key = Nan::New<String>("soft").ToLocalChecked();
    Local<String> hard_key = Nan::New<String>("hard").ToLocalChecked();
    struct rlimit limit;
    bool get_soft = false, get_hard = false;
    if (limit_in->Has(soft_key)) {
        if (limit_in->Get(soft_key)->IsNull()) {
            limit.rlim_cur = RLIM_INFINITY;
        } else {
            limit.rlim_cur = limit_in->Get(soft_key)->IntegerValue();
        }
    } else {
        get_soft = true;
    }

    if (limit_in->Has(hard_key)) {
        if (limit_in->Get(hard_key)->IsNull()) {
            limit.rlim_max = RLIM_INFINITY;
        } else {
            limit.rlim_max = limit_in->Get(hard_key)->IntegerValue();
        }
    } else {
        get_hard = true;
    }

    if (get_soft || get_hard) {
        // current values for the limits are needed
        struct rlimit current;
        if (getrlimit(resource, &current)) {
            return Nan::ThrowError(Nan::ErrnoException(errno, "getrlimit", ""));
        }
        if (get_soft) { limit.rlim_cur = current.rlim_cur; }
        if (get_hard) { limit.rlim_max = current.rlim_max; }
    }

    if (setrlimit(resource, &limit)) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "setrlimit", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_getpwnam) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("getpwnam: requires exactly 1 argument");
    }

    struct passwd* pwd;
    errno = 0; // reset errno before the call

    if (info[0]->IsNumber()) {
        pwd = getpwuid(info[0]->Int32Value());
        if (errno) {
            return Nan::ThrowError(Nan::ErrnoException(errno, "getpwuid", ""));
        }
    } else if (info[0]->IsString()) {
        String::Utf8Value pwnam(info[0]->ToString());
        pwd = getpwnam(*pwnam);
        if(errno) {
            return Nan::ThrowError(Nan::ErrnoException(errno, "getpwnam", ""));
        }
    } else {
        return Nan::ThrowTypeError("argument must be a number or a string");
    }

    if (!pwd) {
        return Nan::ThrowError("user id does not exist");
    }

    Local<Object> obj = Nan::New<Object>();
    obj->Set(Nan::New<String>("name").ToLocalChecked(), Nan::New<String>(pwd->pw_name).ToLocalChecked());
    obj->Set(Nan::New<String>("passwd").ToLocalChecked(), Nan::New<String>(pwd->pw_passwd).ToLocalChecked());
    obj->Set(Nan::New<String>("uid").ToLocalChecked(), Nan::New<Number>(pwd->pw_uid));
    obj->Set(Nan::New<String>("gid").ToLocalChecked(), Nan::New<Number>(pwd->pw_gid));
#ifdef __ANDROID__
    obj->Set(Nan::New<String>("gecos").ToLocalChecked(), Nan::Null());
#else
    obj->Set(Nan::New<String>("gecos").ToLocalChecked(), Nan::New<String>(pwd->pw_gecos).ToLocalChecked());
#endif
    obj->Set(Nan::New<String>("shell").ToLocalChecked(), Nan::New<String>(pwd->pw_shell).ToLocalChecked());
    obj->Set(Nan::New<String>("dir").ToLocalChecked(), Nan::New<String>(pwd->pw_dir).ToLocalChecked());

    info.GetReturnValue().Set(obj);
}

NAN_METHOD(node_getgrnam) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("getgrnam: requires exactly 1 argument");
    }

    struct group* grp;
    errno = 0; // reset errno before the call

    if (info[0]->IsNumber()) {
        grp = getgrgid(info[0]->Int32Value());
        if (errno) {
            return Nan::ThrowError(Nan::ErrnoException(errno, "getgrgid", ""));
        }
    } else if (info[0]->IsString()) {
        String::Utf8Value pwnam(info[0]->ToString());
        grp = getgrnam(*pwnam);
        if (errno) {
            return Nan::ThrowError(Nan::ErrnoException(errno, "getgrnam", ""));
        }
    } else {
        return Nan::ThrowTypeError("argument must be a number or a string");
    }

    if (!grp) {
        return Nan::ThrowError("group id does not exist");
    }

    Local<Object> obj = Nan::New<Object>();
    obj->Set(Nan::New<String>("name").ToLocalChecked(), Nan::New<String>(grp->gr_name).ToLocalChecked());
    obj->Set(Nan::New<String>("passwd").ToLocalChecked(), Nan::New<String>(grp->gr_passwd).ToLocalChecked());
    obj->Set(Nan::New<String>("gid").ToLocalChecked(), Nan::New<Number>(grp->gr_gid));

    Local<Array> members = Nan::New<Array>();
    char** cur = grp->gr_mem;
    for (size_t i=0; *cur; ++i, ++cur) {
        (*members)->Set(i, Nan::New<String>(*cur).ToLocalChecked());
    }
    obj->Set(Nan::New<String>("members").ToLocalChecked(), members);

    info.GetReturnValue().Set(obj);
}

NAN_METHOD(node_initgroups) {
    Nan::HandleScope scope;

    if (info.Length() != 2) {
        return Nan::ThrowError("initgroups: requires exactly 2 arguments");
    }

    if (!info[0]->IsString() || !info[1]->IsNumber()) {
        return Nan::ThrowTypeError("initgroups: first argument must be a string "
                         " and the second an integer");
    }

    String::Utf8Value unam(info[0]->ToString());
    if (initgroups(*unam, info[1]->Int32Value())) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "initgroups", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_seteuid) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("seteuid: requires exactly 1 argument");
    }

    if (seteuid(info[0]->Int32Value())) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "seteuid", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_setegid) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("setegid: requires exactly 1 argument");
    }

    if (setegid(info[0]->Int32Value())) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "setegid", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_setregid) {
    Nan::HandleScope scope;

    if (info.Length() != 2) {
        return Nan::ThrowError("setregid: requires exactly 2 arguments");
    }

    if (setregid(info[0]->Int32Value(), info[1]->Int32Value())) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "setregid", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_setreuid) {
    Nan::HandleScope scope;

    if (info.Length() != 2) {
        return Nan::ThrowError("setreuid: requires exactly 2 arguments");
    }

    if (setreuid(info[0]->Int32Value(), info[1]->Int32Value())) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "setreuid", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

// openlog() first argument (const char* ident) is not guaranteed to be
// copied within the openlog() call so we need to keep it in a safe location
static const size_t MAX_SYSLOG_IDENT=100;
static char syslog_ident[MAX_SYSLOG_IDENT+1] = {0};

NAN_METHOD(node_openlog) {
    Nan::HandleScope scope;

    if (info.Length() != 3) {
        return Nan::ThrowError("openlog: requires exactly 3 arguments");
    }

    String::Utf8Value ident(info[0]->ToString());
    strncpy(syslog_ident, *ident, MAX_SYSLOG_IDENT);
    syslog_ident[MAX_SYSLOG_IDENT] = 0;
    if (!info[1]->IsNumber() || !info[2]->IsNumber()) {
        return Nan::ThrowError("openlog: invalid argument values");
    }
    // note: openlog does not ever fail, no return value
    openlog(syslog_ident, info[1]->Int32Value(), info[2]->Int32Value());

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_closelog) {
    Nan::HandleScope scope;

    if (info.Length() != 0) {
        return Nan::ThrowError("closelog: does not take any arguments");
    }

    // note: closelog does not ever fail, no return value
    closelog();

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_syslog) {
    Nan::HandleScope scope;

    if (info.Length() != 2) {
        return Nan::ThrowError("syslog: requires exactly 2 arguments");
    }

    String::Utf8Value message(info[1]->ToString());
    // note: syslog does not ever fail, no return value
    syslog(info[0]->Int32Value(), "%s", *message);

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_setlogmask) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("setlogmask: takes exactly 1 argument");
    }

    info.GetReturnValue().Set(Nan::New<Integer>(setlogmask(info[0]->Int32Value())));
}

#define ADD_MASK_FLAG(name, flag) \
    obj->Set(Nan::New<String>(name).ToLocalChecked(), Nan::New<Integer>(flag)); \
    obj->Set(Nan::New<String>("mask_" name).ToLocalChecked(), Nan::New<Integer>(LOG_MASK(flag)));

NAN_METHOD(node_update_syslog_constants) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
      return Nan::ThrowError("update_syslog_constants: takes exactly 1 argument");
    }

    if (!info[0]->IsObject()) {
        return Nan::ThrowTypeError("update_syslog_constants: argument must be an object");
    }

    Local<Object> obj = info[0]->ToObject();
    ADD_MASK_FLAG("emerg", LOG_EMERG);
    ADD_MASK_FLAG("alert", LOG_ALERT);
    ADD_MASK_FLAG("crit", LOG_CRIT);
    ADD_MASK_FLAG("err", LOG_ERR);
    ADD_MASK_FLAG("warning", LOG_WARNING);
    ADD_MASK_FLAG("notice", LOG_NOTICE);
    ADD_MASK_FLAG("info", LOG_INFO);
    ADD_MASK_FLAG("debug", LOG_DEBUG);

    // facility constants
    obj->Set(Nan::New<String>("auth").ToLocalChecked(), Nan::New<Integer>(LOG_AUTH));
#ifdef LOG_AUTHPRIV
    obj->Set(Nan::New<String>("authpriv").ToLocalChecked(), Nan::New<Integer>(LOG_AUTHPRIV));
#endif
    obj->Set(Nan::New<String>("cron").ToLocalChecked(), Nan::New<Integer>(LOG_CRON));
    obj->Set(Nan::New<String>("daemon").ToLocalChecked(), Nan::New<Integer>(LOG_DAEMON));
#ifdef LOG_FTP
    obj->Set(Nan::New<String>("ftp").ToLocalChecked(), Nan::New<Integer>(LOG_FTP));
#endif
    obj->Set(Nan::New<String>("kern").ToLocalChecked(), Nan::New<Integer>(LOG_KERN));
    obj->Set(Nan::New<String>("lpr").ToLocalChecked(), Nan::New<Integer>(LOG_LPR));
    obj->Set(Nan::New<String>("mail").ToLocalChecked(), Nan::New<Integer>(LOG_MAIL));
    obj->Set(Nan::New<String>("news").ToLocalChecked(), Nan::New<Integer>(LOG_NEWS));
    obj->Set(Nan::New<String>("syslog").ToLocalChecked(), Nan::New<Integer>(LOG_SYSLOG));
    obj->Set(Nan::New<String>("user").ToLocalChecked(), Nan::New<Integer>(LOG_USER));
    obj->Set(Nan::New<String>("uucp").ToLocalChecked(), Nan::New<Integer>(LOG_UUCP));
    obj->Set(Nan::New<String>("local0").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL0));
    obj->Set(Nan::New<String>("local1").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL1));
    obj->Set(Nan::New<String>("local2").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL2));
    obj->Set(Nan::New<String>("local3").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL3));
    obj->Set(Nan::New<String>("local4").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL4));
    obj->Set(Nan::New<String>("local5").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL5));
    obj->Set(Nan::New<String>("local6").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL6));
    obj->Set(Nan::New<String>("local7").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL7));

    // option constants
    obj->Set(Nan::New<String>("pid").ToLocalChecked(), Nan::New<Integer>(LOG_PID));
    obj->Set(Nan::New<String>("cons").ToLocalChecked(), Nan::New<Integer>(LOG_CONS));
    obj->Set(Nan::New<String>("ndelay").ToLocalChecked(), Nan::New<Integer>(LOG_NDELAY));
    obj->Set(Nan::New<String>("odelay").ToLocalChecked(), Nan::New<Integer>(LOG_ODELAY));
    obj->Set(Nan::New<String>("nowait").ToLocalChecked(), Nan::New<Integer>(LOG_NOWAIT));

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_gethostname) {
    Nan::HandleScope scope;

    if (info.Length() != 0) {
        return Nan::ThrowError("gethostname: takes no arguments");
    }
#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX 255
#endif

    char hostname[HOST_NAME_MAX];

    int rc = gethostname(hostname, HOST_NAME_MAX);
    if (rc != 0) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "gethostname", ""));
    }

    info.GetReturnValue().Set(Nan::New<String>(hostname).ToLocalChecked());
}

#ifndef __ANDROID__
NAN_METHOD(node_sethostname) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("sethostname: takes exactly 1 argument");
    }

    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("sethostname: first argument must be a string");
    }

    String::Utf8Value str(info[0]);

    int rc = sethostname(*str, str.length());
    if (rc != 0) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "sethostname", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}
#endif // __ANDROID__

#ifdef __linux__
NAN_METHOD(node_swapon) {
    Nan::HandleScope scope;

    if (info.Length() != 2) {
        return Nan::ThrowError("swapon: takes exactly 2 argument");
    }

    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("swapon: first argument must be a string");
    }

    if (!info[1]->IsNumber()) {
        return Nan::ThrowTypeError("swapon: second argument must be an integer");
    }

    String::Utf8Value str(info[0]);

    int rc = swapon(*str, info[1]->IntegerValue());
    if (rc != 0) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "swapon", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_swapoff) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("swapoff: takes exactly 1 argument");
    }

    if (!info[0]->IsString()) {
        return Nan::ThrowTypeError("swapoff: first argument must be a string");
    }

    String::Utf8Value str(info[0]);

    int rc = swapoff(*str);
    if (rc != 0) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "swapoff", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_update_swap_constants) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
      return Nan::ThrowError("update_syslog_constants: takes exactly 1 argument");
    }

    if (!info[0]->IsObject()) {
        return Nan::ThrowTypeError("update_syslog_constants: argument must be an object");
    }

    Local<Object> obj = info[0]->ToObject();
    obj->Set(Nan::New<String>("prefer").ToLocalChecked(), Nan::New<Integer>(SWAP_FLAG_PREFER));
#ifdef SWAP_FLAG_DISCARD
    obj->Set(Nan::New<String>("discard").ToLocalChecked(), Nan::New<Integer>(SWAP_FLAG_DISCARD));
#endif // SWAP_FLAG_DISCARD

    info.GetReturnValue().Set(Nan::Undefined());
}
#endif // __linux__

#define EXPORT(name, symbol) exports->Set( \
  Nan::New<String>(name).ToLocalChecked(), \
  Nan::New<FunctionTemplate>(symbol)->GetFunction() \
)

void init(Handle<Object> exports) {
    EXPORT("getppid", node_getppid);
    EXPORT("getpgid", node_getpgid);
    EXPORT("setpgid", node_setpgid);
    EXPORT("geteuid", node_geteuid);
    EXPORT("getegid", node_getegid);
    EXPORT("setsid", node_setsid);
    EXPORT("chroot", node_chroot);
    EXPORT("getrlimit", node_getrlimit);
    EXPORT("setrlimit", node_setrlimit);
    EXPORT("getpwnam", node_getpwnam);
    EXPORT("getgrnam", node_getgrnam);
    EXPORT("initgroups", node_initgroups);
    EXPORT("seteuid", node_seteuid);
    EXPORT("setegid", node_setegid);
    EXPORT("setregid", node_setregid);
    EXPORT("setreuid", node_setreuid);
    EXPORT("openlog", node_openlog);
    EXPORT("closelog", node_closelog);
    EXPORT("syslog", node_syslog);
    EXPORT("setlogmask", node_setlogmask);
    EXPORT("update_syslog_constants", node_update_syslog_constants);
    EXPORT("gethostname", node_gethostname);
#ifndef __ANDROID__
    EXPORT("sethostname", node_sethostname);
#endif // __ANDROID__

    #ifdef __linux__
      EXPORT("swapon", node_swapon);
      EXPORT("swapoff", node_swapoff);
      EXPORT("update_swap_constants", node_update_swap_constants);
    #endif
}

NODE_MODULE(posix, init);
