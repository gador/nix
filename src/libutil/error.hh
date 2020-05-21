#pragma once


#include "ref.hh"
#include "types.hh"

#include <list>
#include <memory>
#include <map>
#include <optional>

#include "fmt.hh"

/* Before 4.7, gcc's std::exception uses empty throw() specifiers for
 * its (virtual) destructor and what() in c++11 mode, in violation of spec
 */
#ifdef __GNUC__
#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)
#define EXCEPTION_NEEDS_THROW_SPEC
#endif
#endif

namespace nix {

typedef enum {
    lvlError = 0,
    lvlWarn,
    lvlInfo,
    lvlTalkative,
    lvlChatty,
    lvlDebug,
    lvlVomit
} Verbosity;

typedef enum { 
    foFile,
    foStdin,
    foString
} FileOrigin;


struct ErrPos {
    int line = 0;
    int column = 0;
    string file;
    FileOrigin origin;

    operator bool() const
    {
        return line != 0;
    }

    template <class P>
    ErrPos& operator=(const P &pos)
    {
        origin = pos.origin;
        line = pos.line;
        column = pos.column;
        file = pos.file;
        return *this;
    }

    template <class P>
    ErrPos(const P &p)
    {
        *this = p;
    }
};

struct NixCode {
    ErrPos errPos;
    std::optional<string> prevLineOfCode;
    std::optional<string> errLineOfCode;
    std::optional<string> nextLineOfCode;
};

// -------------------------------------------------
// ErrorInfo.
struct ErrorInfo {
    Verbosity level;
    string name;
    string description;
    std::optional<hintformat> hint;
    std::optional<NixCode> nixCode;

    static std::optional<string> programName;
};

std::ostream& operator<<(std::ostream &out, const ErrorInfo &einfo);

/* BaseError should generally not be caught, as it has Interrupted as
   a subclass. Catch Error instead. */
class BaseError : public std::exception
{
protected:
    string prefix_; // used for location traces etc.
    mutable ErrorInfo err;

    mutable std::optional<string> what_;
    const string& calcWhat() const;
    
public:
    unsigned int status = 1; // exit status

    template<typename... Args>
    BaseError(unsigned int status, const Args & ... args)
        : err { .level = lvlError,
                .hint = hintfmt(args...)
              }
        , status(status)
    { }

    template<typename... Args>
    BaseError(const Args & ... args)
        : err { .level = lvlError,
                .hint = hintfmt(args...)
              }
    { }

    BaseError(hintformat hint)
        : err { .level = lvlError,
                .hint = hint
              }
    { }

    BaseError(ErrorInfo e)
        : err(e)
    { }

    virtual const char* sname() const { return "BaseError"; }

#ifdef EXCEPTION_NEEDS_THROW_SPEC
    ~BaseError() throw () { };
    const char * what() const throw () { return calcWhat().c_str(); }
#else
    const char * what() const noexcept override { return calcWhat().c_str(); }
#endif

    const string & msg() const { return calcWhat(); }
    const string & prefix() const { return prefix_; }
    BaseError & addPrefix(const FormatOrString & fs);

    const ErrorInfo & info() { calcWhat(); return err; }
};

#define MakeError(newClass, superClass) \
    class newClass : public superClass                  \
    {                                                   \
    public:                                             \
        using superClass::superClass;                   \
        virtual const char* sname() const override { return #newClass; } \
    }

MakeError(Error, BaseError);

class SysError : public Error
{
public:
    int errNo;

    template<typename... Args>
    SysError(const Args & ... args)
      :Error("")
    {
        errNo = errno;
        auto hf = hintfmt(args...);
        err.hint = hintfmt("%1% : %2%", normaltxt(hf.str()), strerror(errNo));
    }

    virtual const char* sname() const override { return "SysError"; }
};

}
