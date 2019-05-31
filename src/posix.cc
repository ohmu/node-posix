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

    const pid_t pid = Nan::To<v8::Integer>(info[0]).ToLocalChecked()->Value();
    // on some platforms pid_t is defined as long hence the static_cast
    info.GetReturnValue().Set(Nan::New<Integer>(static_cast<int32_t>(getpgid(pid))));
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

    if (setpgid(Nan::To<v8::Integer>(info[0]).ToLocalChecked()->Value(), Nan::To<v8::Integer>(info[1]).ToLocalChecked()->Value()) < 0) {
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

    Nan::Utf8String dir_path(info[0]);

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
static Local<Value> rlimit_value(rlim_t limit) {
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
    Nan::Utf8String rlimit_name(info[0]);
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
    Nan::Set(data, Nan::New<String>("soft").ToLocalChecked(), rlimit_value(limit.rlim_cur));
    Nan::Set(data, Nan::New<String>("hard").ToLocalChecked(), rlimit_value(limit.rlim_max));

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

    Nan::Utf8String rlimit_name(info[0]);
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

    Local<Object> limit_in = Nan::To<v8::Object>(info[1]).ToLocalChecked(); // Cast
    Local<String> soft_key = Nan::New<String>("soft").ToLocalChecked();
    Local<String> hard_key = Nan::New<String>("hard").ToLocalChecked();
    struct rlimit limit;
    bool get_soft = false, get_hard = false;
    if (Nan::Has(limit_in, soft_key).ToChecked()) {
        if (Nan::Get(limit_in, soft_key).ToLocalChecked()->IsNull()) {
            limit.rlim_cur = RLIM_INFINITY;
        } else {
            limit.rlim_cur = Nan::To<v8::Integer>(Nan::Get(limit_in, soft_key).ToLocalChecked()).ToLocalChecked()->Value();
        }
    } else {
        get_soft = true;
    }

    if (Nan::Has(limit_in, hard_key).ToChecked()) {
        if (Nan::Get(limit_in, hard_key).ToLocalChecked()->IsNull()) {
            limit.rlim_max = RLIM_INFINITY;
        } else {
            limit.rlim_max = Nan::To<v8::Integer>(Nan::Get(limit_in, hard_key).ToLocalChecked()).ToLocalChecked()->Value();
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
        pwd = getpwuid(Nan::To<v8::Int32>(info[0]).ToLocalChecked()->Value());
        if (errno) {
            return Nan::ThrowError(Nan::ErrnoException(errno, "getpwuid", ""));
        }
    } else if (info[0]->IsString()) {
        Nan::Utf8String pwnam(info[0]);
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
    Nan::Set(obj, Nan::New<String>("name").ToLocalChecked(), Nan::New<String>(pwd->pw_name).ToLocalChecked());
    Nan::Set(obj, Nan::New<String>("passwd").ToLocalChecked(), Nan::New<String>(pwd->pw_passwd).ToLocalChecked());
    Nan::Set(obj, Nan::New<String>("uid").ToLocalChecked(), Nan::New<Number>(pwd->pw_uid));
    Nan::Set(obj, Nan::New<String>("gid").ToLocalChecked(), Nan::New<Number>(pwd->pw_gid));
#ifdef __ANDROID__
    Nan::Set(obj, Nan::New<String>("gecos").ToLocalChecked(), Nan::Null());
#else
    Nan::Set(obj, Nan::New<String>("gecos").ToLocalChecked(), Nan::New<String>(pwd->pw_gecos).ToLocalChecked());
#endif
    Nan::Set(obj, Nan::New<String>("shell").ToLocalChecked(), Nan::New<String>(pwd->pw_shell).ToLocalChecked());
    Nan::Set(obj, Nan::New<String>("dir").ToLocalChecked(), Nan::New<String>(pwd->pw_dir).ToLocalChecked());

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
        grp = getgrgid(Nan::To<v8::Int32>(info[0]).ToLocalChecked()->Value());
        if (errno) {
            return Nan::ThrowError(Nan::ErrnoException(errno, "getgrgid", ""));
        }
    } else if (info[0]->IsString()) {
        Nan::Utf8String pwnam(info[0]);
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
    Nan::Set(obj, Nan::New<String>("name").ToLocalChecked(), Nan::New<String>(grp->gr_name).ToLocalChecked());
    Nan::Set(obj, Nan::New<String>("passwd").ToLocalChecked(), Nan::New<String>(grp->gr_passwd).ToLocalChecked());
    Nan::Set(obj, Nan::New<String>("gid").ToLocalChecked(), Nan::New<Number>(grp->gr_gid));

    Local<Array> members = Nan::New<Array>();
    char** cur = grp->gr_mem;
    for (size_t i=0; *cur; ++i, ++cur) {
        Nan::Set(members, i, Nan::New<String>(*cur).ToLocalChecked());
    }
    Nan::Set(obj, Nan::New<String>("members").ToLocalChecked(), members);

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

    Nan::Utf8String unam(info[0]);
    if (initgroups(*unam, Nan::To<v8::Int32>(info[1]).ToLocalChecked()->Value())) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "initgroups", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_seteuid) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("seteuid: requires exactly 1 argument");
    }

    if (seteuid(Nan::To<v8::Int32>(info[0]).ToLocalChecked()->Value())) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "seteuid", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_setegid) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("setegid: requires exactly 1 argument");
    }

    if (setegid(Nan::To<v8::Int32>(info[0]).ToLocalChecked()->Value())) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "setegid", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_setregid) {
    Nan::HandleScope scope;

    if (info.Length() != 2) {
        return Nan::ThrowError("setregid: requires exactly 2 arguments");
    }

    if (setregid(Nan::To<v8::Int32>(info[0]).ToLocalChecked()->Value(), Nan::To<v8::Int32>(info[1]).ToLocalChecked()->Value())) {
        return Nan::ThrowError(Nan::ErrnoException(errno, "setregid", ""));
    }

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_setreuid) {
    Nan::HandleScope scope;

    if (info.Length() != 2) {
        return Nan::ThrowError("setreuid: requires exactly 2 arguments");
    }

    if (setreuid(Nan::To<v8::Int32>(info[0]).ToLocalChecked()->Value(), Nan::To<v8::Int32>(info[1]).ToLocalChecked()->Value())) {
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

    Nan::Utf8String ident(info[0]);
    strncpy(syslog_ident, *ident, MAX_SYSLOG_IDENT);
    syslog_ident[MAX_SYSLOG_IDENT] = 0;
    if (!info[1]->IsNumber() || !info[2]->IsNumber()) {
        return Nan::ThrowError("openlog: invalid argument values");
    }
    // note: openlog does not ever fail, no return value
    openlog(syslog_ident, Nan::To<v8::Int32>(info[1]).ToLocalChecked()->Value(), Nan::To<v8::Int32>(info[2]).ToLocalChecked()->Value());

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

    Nan::Utf8String message(info[1]);
    // note: syslog does not ever fail, no return value
    syslog(Nan::To<v8::Int32>(info[0]).ToLocalChecked()->Value(), "%s", *message);

    info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(node_setlogmask) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
        return Nan::ThrowError("setlogmask: takes exactly 1 argument");
    }

    info.GetReturnValue().Set(Nan::New<Integer>(setlogmask(Nan::To<v8::Int32>(info[0]).ToLocalChecked()->Value())));
}

#define ADD_MASK_FLAG(name, flag) \
    Nan::Set(obj, Nan::New<String>(name).ToLocalChecked(), Nan::New<Integer>(flag)); \
    Nan::Set(obj, Nan::New<String>("mask_" name).ToLocalChecked(), Nan::New<Integer>(LOG_MASK(flag)));

NAN_METHOD(node_update_syslog_constants) {
    Nan::HandleScope scope;

    if (info.Length() != 1) {
      return Nan::ThrowError("update_syslog_constants: takes exactly 1 argument");
    }

    if (!info[0]->IsObject()) {
        return Nan::ThrowTypeError("update_syslog_constants: argument must be an object");
    }

    Local<Object> obj = Nan::To<v8::Object>(info[0]).ToLocalChecked();
    ADD_MASK_FLAG("emerg", LOG_EMERG);
    ADD_MASK_FLAG("alert", LOG_ALERT);
    ADD_MASK_FLAG("crit", LOG_CRIT);
    ADD_MASK_FLAG("err", LOG_ERR);
    ADD_MASK_FLAG("warning", LOG_WARNING);
    ADD_MASK_FLAG("notice", LOG_NOTICE);
    ADD_MASK_FLAG("info", LOG_INFO);
    ADD_MASK_FLAG("debug", LOG_DEBUG);

    // facility constants
    Nan::Set(obj, Nan::New<String>("auth").ToLocalChecked(), Nan::New<Integer>(LOG_AUTH));
#ifdef LOG_AUTHPRIV
    Nan::Set(obj, Nan::New<String>("authpriv").ToLocalChecked(), Nan::New<Integer>(LOG_AUTHPRIV));
#endif
    Nan::Set(obj, Nan::New<String>("cron").ToLocalChecked(), Nan::New<Integer>(LOG_CRON));
    Nan::Set(obj, Nan::New<String>("daemon").ToLocalChecked(), Nan::New<Integer>(LOG_DAEMON));
#ifdef LOG_FTP
    Nan::Set(obj, Nan::New<String>("ftp").ToLocalChecked(), Nan::New<Integer>(LOG_FTP));
#endif
    Nan::Set(obj, Nan::New<String>("kern").ToLocalChecked(), Nan::New<Integer>(LOG_KERN));
    Nan::Set(obj, Nan::New<String>("lpr").ToLocalChecked(), Nan::New<Integer>(LOG_LPR));
    Nan::Set(obj, Nan::New<String>("mail").ToLocalChecked(), Nan::New<Integer>(LOG_MAIL));
    Nan::Set(obj, Nan::New<String>("news").ToLocalChecked(), Nan::New<Integer>(LOG_NEWS));
    Nan::Set(obj, Nan::New<String>("syslog").ToLocalChecked(), Nan::New<Integer>(LOG_SYSLOG));
    Nan::Set(obj, Nan::New<String>("user").ToLocalChecked(), Nan::New<Integer>(LOG_USER));
    Nan::Set(obj, Nan::New<String>("uucp").ToLocalChecked(), Nan::New<Integer>(LOG_UUCP));
    Nan::Set(obj, Nan::New<String>("local0").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL0));
    Nan::Set(obj, Nan::New<String>("local1").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL1));
    Nan::Set(obj, Nan::New<String>("local2").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL2));
    Nan::Set(obj, Nan::New<String>("local3").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL3));
    Nan::Set(obj, Nan::New<String>("local4").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL4));
    Nan::Set(obj, Nan::New<String>("local5").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL5));
    Nan::Set(obj, Nan::New<String>("local6").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL6));
    Nan::Set(obj, Nan::New<String>("local7").ToLocalChecked(), Nan::New<Integer>(LOG_LOCAL7));

    // option constants
    Nan::Set(obj, Nan::New<String>("pid").ToLocalChecked(), Nan::New<Integer>(LOG_PID));
    Nan::Set(obj, Nan::New<String>("cons").ToLocalChecked(), Nan::New<Integer>(LOG_CONS));
    Nan::Set(obj, Nan::New<String>("ndelay").ToLocalChecked(), Nan::New<Integer>(LOG_NDELAY));
    Nan::Set(obj, Nan::New<String>("odelay").ToLocalChecked(), Nan::New<Integer>(LOG_ODELAY));
    Nan::Set(obj, Nan::New<String>("nowait").ToLocalChecked(), Nan::New<Integer>(LOG_NOWAIT));

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

    Nan::Utf8String str(info[0]);

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

    Nan::Utf8String str(info[0]);

    int rc = swapon(*str, Nan::To<v8::Integer>(info[1]).ToLocalChecked()->Value());
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

    Nan::Utf8String str(info[0]);

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

    Local<Object> obj = Nan::To<v8::Object>(info[0]).ToLocalChecked();
    Nan::Set(obj, Nan::New<String>("prefer").ToLocalChecked(), Nan::New<Integer>(SWAP_FLAG_PREFER));
#ifdef SWAP_FLAG_DISCARD
    Nan::Set(obj, Nan::New<String>("discard").ToLocalChecked(), Nan::New<Integer>(SWAP_FLAG_DISCARD));
#endif // SWAP_FLAG_DISCARD

    info.GetReturnValue().Set(Nan::Undefined());
}
#endif // __linux__

#define EXPORT(name, symbol) Nan::Set(exports, \
  Nan::New<String>(name).ToLocalChecked(), \
  Nan::GetFunction(Nan::New<FunctionTemplate>(symbol)).ToLocalChecked()    \
)

void init(Local<Object> exports) {
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
