#pragma once
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <filesystem>
#include <span>
#include <mutex>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/ioctl.h>
#endif

#include <iostream>
#include <thread>
using namespace std::chrono_literals;

namespace spell {

#ifdef _WIN32
using Pipe_Handle = HANDLE;
using Pid = HANDLE;
#else
using Pipe_Handle = int;
using Pid = pid_t;
#endif

namespace detail {
class Env_Var {
public:
  Env_Var (std::string_view data)
  : data_ (data)
  {
    eq_ = data_.find ('=');
  }

  Env_Var (std::string_view key, std::string_view value)
  : data_ {}
  {
    const auto value_off = key.size () + 1;
    data_.resize (value_off + value.size ());
    std::memcpy (data_.data (), key.data (), key.size ());
    data_.data()[key.size ()] = '=';
    std::memcpy (data_.data () + value_off, value.data (), value.size ());
    eq_ = key.size ();
  }

  std::string_view key () const {
    return std::string_view (data_.c_str (), eq_);
  }

  void key (std::string_view str) {
    data_.replace (0, eq_, str);
    eq_ = str.size ();
  }

  std::string_view value () const {
    const auto pos = eq_ + 1;
    return std::string_view (data_.c_str () + pos, data_.size () - pos);
  }

  void value (std::string_view str) {
    data_.replace (data_.begin () + (eq_ + 1), data_.end (), str);
  }

  std::string &unwrap () {
    return data_;
  }

  const std::string &unwrap () const {
    return data_;
  }

private:
  std::string data_;
  std::size_t eq_;
};

bool operator== (const Env_Var &lhs, const Env_Var &rhs) {
  return lhs.key () == rhs.key ();
}

bool operator== (const Env_Var &var, std::string_view key) {
  return var.key () == key;
}

struct Env_Hash {
  using is_transparent = void;
  using transparent_key_equal = std::equal_to<>;
  using hash_type = std::hash<std::string_view>;

  size_t operator() (const Env_Var &v) const {
    return hash_type{} (v.key ());
  }

  size_t operator() (std::string_view key) const {
    return hash_type{} (key);
  }
};

using Env_Set = std::unordered_set<
  Env_Var,
  Env_Hash,
  Env_Hash::transparent_key_equal
>;

class Pipe {
  Pipe (Pipe_Handle r, Pipe_Handle w)
  : read_handle_ (r),
    write_handle_ (w)
  {}

public:
  Pipe () {
  #ifdef _WIN32
    SECURITY_ATTRIBUTES sa = {
      .nLength = sizeof (SECURITY_ATTRIBUTES),
      .lpSecurityDescriptor = nullptr,
      .bInheritHandle = true
    };
    CreatePipe (&read_handle_, &write_handle_, &sa, 0);
  #else
    int p[2];
    pipe (p);
    read_handle_ = p[0];
    write_handle_ = p[1];
  #endif
  }

  static Pipe null () {
    static std::once_flag once_flag;
  #ifdef _WIN32
    static HANDLE h = CreateFileA (
      "nul",
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
      NULL
    );
    std::call_once (once_flag, []() {
      std::atexit ([]() {
        CloseHandle (h);
      });
    });
    return Pipe (h, h);
  #else
    static FILE *s = fopen ("/dev/null", "r+");
    static int h = fileno (s);
    std::call_once (once_flag, []() {
      std::atexit ([]() {
        fclose (s);
      });
    });
    return Pipe (h, h);
  #endif
  }

#ifdef _WIN32
  static Pipe inherit (int stream) {
    HANDLE h = GetStdHandle (stream);
    return Pipe (h, h);
  }
#else
  static Pipe inherit (FILE *stream) {
    int h = fileno (stream);
    return Pipe (h, h);
  }
#endif

  Pipe_Handle read () {
    return read_handle_;
  }

  Pipe_Handle write () {
    return write_handle_;
  }

private:
  Pipe_Handle read_handle_;
  Pipe_Handle write_handle_;
};

} // namespace detail

using Args = std::vector<std::string>;

class Env {
public:
  using iterator = detail::Env_Set::iterator;

public:
  Env (bool load = true)
  : data_ {}
  {
    if (load) {
    #ifdef _WIN32
      LPCH envp = GetEnvironmentStrings ();
      std::size_t len;
      while ((len = std::strlen (envp)) != 0) {
        data_.emplace (envp);
        envp += len + 1;
      }
    #else
      for (char **envp = environ; *envp; ++envp) {
        data_.emplace (*envp);
      }
    #endif
    }
  }

  std::string_view get (std::string_view key) const {
    if (auto it = data_.find (key); it != data_.end ()) {
      return it->value ();
    }
    return {};
  }

  std::string_view operator[] (std::string_view key) const {
    return get (key);
  }

  void set (std::string_view key, std::string_view value) {
    if (auto it = data_.find (key); it != data_.end ()) {
      // The hashset gives us a constant iterator as we shouldn't mutate
      // elements of a set. However both hashing and comparison of `Env_Var`s
      // only depends on the key part so we can safely change the value here.
      auto &x = const_cast<detail::Env_Var &> (*it);
      x.value (value);
    }
    else {
      data_.emplace (key, value);
    }
  }

  void remove (std::string_view key) {
    if (auto it = data_.find (key); it != data_.end ()) {
      data_.erase (it);
    }
  }

  void rename (std::string_view key, std::string_view new_key) {
    if (auto it = data_.find (key); it != data_.end ()) {
      auto var = std::move (*it);
      var.key (new_key);
      data_.erase (it);
      data_.insert (var);
    }
  }

  void clear () {
    data_.clear ();
  }

  iterator begin () {
    return data_.begin ();
  }

  iterator end () {
    return data_.end ();
  }

protected:
  friend class Spell;

  static const Env& empty_env () {
    const static Env instance {false};
    return instance;
  }

private:
  detail::Env_Set data_;
};

enum class Stdio {
  Default,
  Inherit,
  Piped,
  Null
};


class Exit_Status {
public:
  explicit Exit_Status (int code)
  : code_ (code)
  {}

  int code () const {
    return code_;
  }

  bool success () const {
    return code () == 0;
  }

private:
  int code_;
};


struct Output {
  Output (Exit_Status &&s)
  : status (s),
    stdout_ {},
    stderr_ {}
  {}

  Exit_Status status;
  std::vector<char8_t> stdout_;
  std::vector<char8_t> stderr_;
};


bool read_stream (Pipe_Handle handle, std::vector<char8_t> &out) {
#ifdef _WIN32
  static char buf[16];
  DWORD read;
  bool success;
  // Can't get number of bytes in anonymous pipe ahead of time
  do {
    success = ReadFile (handle, buf, sizeof (buf), &read, nullptr);
    out.insert (out.end (), buf, buf + read);
  } while (success && read > 0);
  return !out.empty ();
#else
  int bytes;
  ioctl (handle, FIONREAD, &bytes);
  if (bytes) {
    out.resize (bytes);
    ::read (handle, out.data (), bytes);
    return true;
  }
  return false;
#endif
}


class Child {
protected:
  explicit Child (Pid pid, bool p, Pipe_Handle i, Pipe_Handle o, Pipe_Handle e)
  : pid_ (pid),
    status_ (-1),
    stdin_is_piped_ (p),
    stdin_ (i),
    stdout_ (o),
    stderr_ (e)
  {}
  friend class Spell;

public:
  Pid id () const {
    return pid_;
  }

  std::optional<Exit_Status> try_wait () {
  #ifdef _WIN32
    if (WaitForSingleObject (id (), 0) == WAIT_OBJECT_0) {
      DWORD status;
      GetExitCodeProcess (id (), &status);
      status_ = status;
      CloseHandle (id ());
      return Exit_Status (status_);
    }
  #else
    if (int status = 0; waitpid (id (), &status, WNOHANG) > 0) {
      status_ = status;
      return Exit_Status (status);
    }
  #endif
    return std::nullopt;
  }

  Exit_Status wait () {
  #ifdef _WIN32
    if (status_ != -1)
      return Exit_Status (status_);
    DWORD status;
    if (stdin_is_piped_) {
      CloseHandle (stdin_);
    }
    WaitForSingleObject (id (), INFINITE);
    GetExitCodeProcess (id (), &status);
    CloseHandle (id ());
    return Exit_Status (status);
  #else
    if (status_ != -1)
      return Exit_Status (WEXITSTATUS (status_));
    int status;
    pid_t wpid;
    if (stdin_is_piped_) {
      close (stdin_);
    }
    while ((wpid = waitpid (id (), &status, 0)) > 0) {}
    status_ = status;
    return Exit_Status (WEXITSTATUS (status));
  #endif
  }

  Output wait_with_output () {
    Output o {wait ()};
    read_stream (stdout_, o.stdout_);
    read_stream (stderr_, o.stderr_);
    return o;
  }

  bool kill () {
  #ifdef _WIN32
    if (TerminateProcess (id (), 0)) {
      CloseHandle (id ());
      return true;
    }
    return false;
  #else
    return ::kill (id (), SIGKILL) != -1;
  #endif
  }

  Pipe_Handle get_stdin () {
    return stdin_;
  }

  Pipe_Handle get_stdout () {
    return stdout_;
  }

  Pipe_Handle get_stderr () {
    return stderr_;
  }

private:
  Pid pid_;
  int status_;
  bool stdin_is_piped_;
  Pipe_Handle stdin_;
  Pipe_Handle stdout_;
  Pipe_Handle stderr_;
};


class Spell {
  using Self = Spell;
public:
  Spell (std::string_view program)
  : program_ (program),
    args_ {},
    env_ (std::nullopt),
    working_dir_ (std::filesystem::current_path ()),
    stdout_ (Stdio::Default),
    stderr_ (Stdio::Default),
    stdin_ (Stdio::Default)
  {}

  std::string_view get_program () const {
    return program_;
  }

  ////////////////////////////////////////////////////////////////////////
  // Arguments

  Self& arg (std::string_view arg) {
    args_.emplace_back (arg);
    return *this;
  }

  Self& args (const std::vector<std::string_view> &args) {
    for (const auto &a : args) {
      args_.emplace_back (a);
    }
    return *this;
  }

  template <class... Args>
  Self& args (const Args&... args) {
    (args_.emplace_back (args), ...);
    return *this;
  }

  Args& get_args () {
    return args_;
  }

  const Args& get_args () const {
    return args_;
  }

  ////////////////////////////////////////////////////////////////////////
  // Environment

  Self& env (std::string_view key, std::string_view val) {
    if (!env_.has_value ()) {
      env_.emplace ();
    }
    env_->set (key, val);
    return *this;
  }

  Self& envs (std::span<std::pair<std::string_view, std::string_view>> vars) {
    if (!env_.has_value ()) {
      env_.emplace ();
    }
    for (const auto &[key, val] : vars) {
      env_->set (key, val);
    }
    return *this;
  }

  Self& env_clear () {
    if (!env_.has_value ()) {
      env_.emplace (false);
    }
    else {
      env_->clear ();
    }
    return *this;
  }

  Self& env_remove (std::string_view key) {
    if (!env_.has_value ()) {
      env_.emplace ();
    }
    env_->remove (key);
    return *this;
  }

  Env& get_envs () {
    if (!env_.has_value ()) {
      env_.emplace ();
    }
    return env_.value ();
  }

  const Env& get_envs () const {
    if (!env_.has_value ()) {
      return Env::empty_env ();
    }
    return env_.value ();
  }

  ////////////////////////////////////////////////////////////////////////
  // Directory

  Self& current_dir (const std::filesystem::path &dir) {
    if (dir.is_absolute ())
      working_dir_ = dir;
    else
      working_dir_ = std::filesystem::weakly_canonical (working_dir_ / dir);
    return *this;
  }

  const std::filesystem::path &get_current_dir () const {
    return working_dir_;
  }
  ////////////////////////////////////////////////////////////////////////
  // Streams

  Self& set_stdout (Stdio cfg) {
    stdout_ = cfg;
    return *this;
  }

  Self& set_stderr (Stdio cfg) {
    stderr_ = cfg;
    return *this;
  }

  Self& set_stdin (Stdio cfg) {
    stdin_ = cfg;
    return *this;
  }

  ////////////////////////////////////////////////////////////////////////
  // Running

  std::optional<Child> cast () {
    return do_cast (Stdio::Inherit);
  }

  std::optional<Exit_Status> cast_status () {
    auto child = do_cast (Stdio::Inherit);
    if (child.has_value ())
      return child->wait ();
    return std::nullopt;
  }

  std::optional<Output> cast_output () {
    auto child = do_cast (Stdio::Piped);
    if (child.has_value ()) {
      return child->wait_with_output ();
    }
    return std::nullopt;
  }

private:
  std::optional<Child> do_cast (Stdio default_cfg) {
    using namespace detail;
    Pipe out, err, in;

    auto set_pipe = [default_cfg](Pipe &p, Stdio &cfg, auto s) {
      if (cfg == Stdio::Default) {
        cfg = default_cfg;
      }
      switch (cfg) {
      break; case Stdio::Inherit: {
        p = Pipe::inherit (s);
      }
      break; case Stdio::Piped: {
        p = Pipe ();
      }
      break; case Stdio::Null: {
        p = Pipe::null ();
      }
      break; case Stdio::Default:;
      }
    };

  #ifdef _WIN32
    set_pipe (out, stdout_, STD_OUTPUT_HANDLE);
    set_pipe (err, stderr_, STD_ERROR_HANDLE);
    set_pipe (in, stdin_, STD_INPUT_HANDLE);

    PROCESS_INFORMATION process_info {};
    ZeroMemory (&process_info, sizeof (PROCESS_INFORMATION));

    STARTUPINFO startup_info = {};
    ZeroMemory (&startup_info, sizeof (STARTUPINFO));
    startup_info.cb = sizeof (STARTUPINFO);
    startup_info.hStdOutput = out.write ();
    startup_info.hStdError = err.write ();
    startup_info.hStdInput = in.read ();
    startup_info.dwFlags |= STARTF_USESTDHANDLES;

    // Arguments
    std::string command_line {program_};
    for (auto &arg : args_) {
      command_line.push_back (' ');
      command_line.append (arg);
    }

    // Environment
    std::string environment {};
    if (env_.has_value ()) {
      for (const auto &env : *env_) {
        environment.append (env.unwrap ());
        environment.push_back ('\0');
      }
    }

    // Don't inherit parent ends of pipes
    if (stdin_ == Stdio::Piped) {
      SetHandleInformation (in.write (), HANDLE_FLAG_INHERIT, 0);
    }
    if (stdout_ == Stdio::Piped) {
      SetHandleInformation (out.read (), HANDLE_FLAG_INHERIT, 0);
    }
    if (stderr_ == Stdio::Piped) {
      SetHandleInformation (err.read (), HANDLE_FLAG_INHERIT, 0);
    }

    if (!CreateProcessA (
      nullptr,
      command_line.data (),
      nullptr,
      nullptr,
      true,
      0,
      env_.has_value () ? reinterpret_cast<void *> (environment.data ()) : nullptr,
      nullptr,
      &startup_info,
      &process_info
    )) {
      return std::nullopt;
    }

    CloseHandle (process_info.hThread);

    if (stdin_ == Stdio::Piped)
      CloseHandle (in.read ());
    if (stdout_ == Stdio::Piped)
      CloseHandle (out.write ());
    if (stderr_ == Stdio::Piped)
      CloseHandle (err.write ());

    return Child (process_info.hProcess, stdin_ == Stdio::Piped, in.write (), out.read (), err.read ());

  #else

    set_pipe (out, stdout_, stdout);
    set_pipe (err, stderr_, stderr);
    set_pipe (in, stdin_, stdin);

    const pid_t pid = fork ();
    if (pid == 0) {
      // Set streams
      if (stdout_ != Stdio::Inherit) {
        dup2 (out.write (), STDOUT_FILENO);
        if (stdout_ == Stdio::Piped) {
          close (out.read ());
          close (out.write ());
        }
      }
      if (stderr_ != Stdio::Inherit) {
        dup2 (err.write (), STDERR_FILENO);
        if (stderr_ == Stdio::Piped) {
          close (err.read ());
          close (err.write ());
        }
      }
      if (stdin_ != Stdio::Inherit) {
        dup2 (in.read (), STDIN_FILENO);
        if (stdin_ == Stdio::Piped) {
          close (in.write ());
          close (in.read ());
        }
      }
      // Arguments
      std::vector<char *> p_args;
      p_args.push_back (program_.data ());
      std::transform (args_.begin (), args_.end (), std::back_inserter (p_args),
        [] (std::string &a) -> char * {
          return a.data ();
        }
      );
      p_args.push_back (NULL);
      // Environment
      if (env_.has_value ()) {
        std::vector<char *> p_envs;
        std::transform (env_->begin (), env_->end (), std::back_inserter (p_envs),
          [] (const Env_Var &v) -> char * {
            return const_cast<char *> (v.unwrap ().data ());
          }
        );
        p_envs.push_back (NULL);
        execvpe (program_.c_str (), p_args.data (), p_envs.data ());
      }
      else {
        execvp (program_.c_str (), p_args.data ());
      }
      // TODO: return nullopt from parent process
      exit (1);
    }

    if (stdin_ == Stdio::Piped)
      close (in.read ());
    if (stdout_ == Stdio::Piped)
      close (out.write ());
    if (stderr_ == Stdio::Piped)
      close (err.write ());

    return Child (pid, stdin_ == Stdio::Piped, in.write (), out.read (), err.read ());
  #endif
  }

private:
  std::string program_;
  Args args_;
  std::optional<Env> env_;
  std::filesystem::path working_dir_;
  Stdio stdout_;
  Stdio stderr_;
  Stdio stdin_;
};

void ignore_sigchld () {
#ifndef _WIN32
  signal (SIGCHLD, SIG_IGN);
#endif
}

std::size_t write (Pipe_Handle handle, const char *data, std::size_t count) {
#ifdef _WIN32
  DWORD written;
  WriteFile (handle, reinterpret_cast<const void *> (data), count, &written, nullptr);
  return written;
#else
  return ::write (handle, reinterpret_cast<const void *> (data), count);
#endif
}

std::size_t read (Pipe_Handle handle, char *out, std::size_t max_count) {
#ifdef _WIN32
  DWORD nread;
  ReadFile (handle, reinterpret_cast<void *> (out), max_count, &nread, nullptr);
  return nread;
#else
  return ::read (handle, reinterpret_cast<void *> (out), max_count);
#endif
}

} // namespace spell

std::ostream& operator<< (std::ostream &os, const spell::Exit_Status &exit_status) {
  os << "Exit_Status(" << exit_status.code () << ')';
  return os;
}
