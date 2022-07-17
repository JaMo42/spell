/** @file spell.hh
 * @brief Single header C++ subprocess library.
 */
#pragma once
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace spell {

/** @var INVALID_PIPE
 * @brief Invalid pipe handle value.
 *
 * Represents a pipe handle that's closed, uninitialzed, or could not be created.
 */
#ifdef _WIN32
/**
 * @brief A pipe handle.
 *
 * Represent the pipes HANDLE.
 */
using Pipe_Handle = HANDLE;
/**
 * @brief A process ID.
 *
 * Represents the processes HANDLE, not the actual process id.
 */
using Pid = HANDLE;

// Cannot use constexpr as the definition of INVALID_HANDLE_VALUE does a
// reinterpret_cast: `((HANDLE)(LONG_PTR)-1)`
const Pipe_Handle INVALID_PIPE = INVALID_HANDLE_VALUE;
#else
/**
 * @brief A pipe handle.
 *
 * Represents the pipes file descriptor.
 */
using Pipe_Handle = int;
/**
 * @brief Pid a process ID.
 *
 * Represents the processes pid.
 */
using Pid = pid_t;

constexpr Pipe_Handle INVALID_PIPE = -1;
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

inline bool operator== (const Env_Var &lhs, const Env_Var &rhs) {
  return lhs.key () == rhs.key ();
}

inline bool operator== (const Env_Var &var, std::string_view key) {
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


inline Pipe_Handle duplicate_pipe (Pipe_Handle h) {
#ifdef _WIN32
  static const HANDLE proc = GetCurrentProcess ();
  HANDLE hh = INVALID_HANDLE_VALUE;
  DuplicateHandle (proc, h, proc, &hh, 0, TRUE, DUPLICATE_SAME_ACCESS);
  return hh;
#else
  return dup (h);
#endif
}


inline void close_pipe (Pipe_Handle h) {
#ifdef _WIN32
  CloseHandle (h);
#else
  ::close (h);
#endif
}

} // namespace detail

/**
 * @brief One end of an anonymous pipe.
 */
class Anonymous_Pipe {
  Anonymous_Pipe (Pipe_Handle h)
  : inner_ (h)
  {}

  template <class T>
  struct Pipes_Base {
    using Self = Pipes_Base<T>;

    T read;
    T write;

    Self& operator= (Self &&other) {
      read = std::move (other.read);
      write = std::move (other.write);
      return *this;
    }

    void drop () {
      read.drop ();
      write.drop ();
    }
  };

protected:
  using Pipes = Pipes_Base<Anonymous_Pipe>;
  friend class Spell;

  static Pipes create () {
  #ifdef _WIN32
    SECURITY_ATTRIBUTES sa = {
      .nLength = sizeof (SECURITY_ATTRIBUTES),
      .lpSecurityDescriptor = nullptr,
      .bInheritHandle = true
    };
    HANDLE r = INVALID_HANDLE_VALUE, w = INVALID_HANDLE_VALUE;
    CreatePipe (&r, &w, &sa, 0);
    return Pipes {r, w};
  #else
    int p[2] = { INVALID_PIPE, INVALID_PIPE };
    pipe2 (p, O_CLOEXEC);
    return Pipes {p[0], p[1]};
  #endif
  }

  #ifdef _WIN32
  static Pipes create_inherit (int device) {
    const HANDLE h = GetStdHandle (device);
    return Pipes {detail::duplicate_pipe (h), detail::duplicate_pipe (h)};
  }
  #else
  static Pipes create_inherit (FILE *stream) {
    const int h = fileno (stream);
    return Pipes {detail::duplicate_pipe (h), detail::duplicate_pipe (h)};
  }
  #endif

  static Pipes create_null () {
    static std::once_flag once_flag;
  #ifdef _WIN32
    static HANDLE h = CreateFileA (
      "nul",
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL,
      nullptr
    );
    std::call_once (once_flag, []() {
      std::atexit ([]() {
        CloseHandle (h);
      });
    });
  #else
    static FILE *s = fopen ("/dev/null", "r+");
    static int h = fileno (s);
    std::call_once (once_flag, []() {
      std::atexit ([]() {
        fclose (s);
      });
    });
  #endif
    return Pipes {detail::duplicate_pipe (h), detail::duplicate_pipe (h)};
  }

public:
  /**
   * @brief Creates a new pipe with an invalid inner handle.
   */
  Anonymous_Pipe ()
  : inner_ (INVALID_PIPE)
  {}

  /**
   * @brief Takes the handle from another pipe.
   *
   * The pipe moved from gets invalidated.
   */
  Anonymous_Pipe (Anonymous_Pipe &&from)
  : inner_ (from.handle ())
  {
    from.inner_ = INVALID_PIPE;
  }

  /**
   * @brief Drops the pipe.
   *
   * See @ref drop.
   */
  ~Anonymous_Pipe () {
    drop ();
  }

  /**
   * @brief Takes the handle from another pipe.
   *
   * If this pipe already has a valid value it gets dropped.
   *
   * The pipe moved from gets invalidated.
   */
  Anonymous_Pipe& operator= (Anonymous_Pipe &&from) {
    drop ();
    inner_ = from.take ();
    return *this;
  }

  /**
   * @brief Returns the inner handle.
   */
  Pipe_Handle handle () const {
    return inner_;
  }

  /**
   * @brief Invalidates the pipe and returns the handle it held.
   */
  [[nodiscard]] Pipe_Handle take () {
    const auto h = handle ();
    inner_ = INVALID_PIPE;
    return h;
  }

  /**
   * @brief Drops the pipe.
   *
   * If it holds a valid handle it gets closed and the pipe is invalidated.
   */
  void drop () {
    if (inner_ != INVALID_PIPE) {
      detail::close_pipe (inner_);
      inner_ = INVALID_PIPE;
    }
  }

  /**
   * @brief Reads from the pipe.
   * @param buf - The buffer to read to.
   * @param count - The number of bytes(!) to read.
   * @return The number of bytes read or `std::nullopt` if reading failed.
   */
  std::optional<std::size_t> read (auto *buf, std::size_t count) {
  #ifdef _WIN32
    DWORD nread;
    if (ReadFile (handle (), reinterpret_cast<void *> (buf), count, &nread, nullptr)) {
      return nread;
    } else {
      return std::nullopt;
    }
  #else
    if (auto n = ::read (handle (), reinterpret_cast<void *> (buf), count); n >= 0) {
      return n;
    }
    else {
      return std::nullopt;
    }
  #endif
  }

  /**
   * @brief Reads all data available in the pipe.
   *
   * The given vector gets cleared before reading.
   *
   * @param out - vector to write to.
   * @return The number of bytes read of `std::nullopt` if reading failed.
   */
  std::optional<std::size_t> read_all (std::vector<char> &out) {
  #ifdef _WIN32
    static char buf[64];
    DWORD nread;
    bool success;
    out.clear ();
    do {
      if ((success = ReadFile (handle (), buf, sizeof (buf), &nread, nullptr))) {
        out.insert (out.end (), buf, buf + nread);
      }
    } while (success && nread > 0);
    return success;
  #else
    int bytes;
    if (ioctl (handle (), FIONREAD, &bytes) < 0) {
      return std::nullopt;
    }
    if (bytes) {
      out.resize (bytes);
      if (auto r = read (out.data (), bytes); r.has_value ()) {
        out.resize (r.value ());
        return r;
      }
      out.clear ();
      return std::nullopt;
    }
    else {
      out.clear ();
      return 0;
    }
  #endif
  }

  /**
   * @brief Writes the end pipe.
   * @param buf - data to write.
   * @param count - number of bytes(!) to write.
   * @return number of bytes written or `std::nullopt` if writing failed.
   */
  std::optional<std::size_t> write (auto *buf, std::size_t count) {
  #ifdef _WIN32
    DWORD written;
    if (WriteFile (handle (), reinterpret_cast<const void *> (buf), count, &written, nullptr)) {
      return written;
    } else {
      return std::nullopt;
    }
  #else
    if (auto n = ::write (handle (), reinterpret_cast<const void *> (buf), count); n >= 0) {
      return n;
    }
    else {
      return std::nullopt;
    }
  #endif
  }

  /**
   * @brief Writes all data from the given vector.
   *
   * Unlike write(), which may not write the requested number of bytes,
   * this will keep writing until all data is written or a write fails.
   *
   * @param data - data to write.
   * @param count - number of bytes(!) to write.
   * @return whether writing was successful.
   */
  bool write_all (const auto *data, std::size_t count) {
    while (count) {
      if (auto r = write (data, count); r.has_value ()) {
        data += r.value ();
        count -= r.value ();
      }
      else {
        return false;
      }
    }
    return true;
  }

private:
  Pipe_Handle inner_;
};

/// @brief List of command arguments.
using Args = std::vector<std::string>;

/**
 * @brief An environment mapping.
 */
class Env {
public:
  using iterator = detail::Env_Set::iterator;

public:
  /**
   * @brief Creates an environment mapping thats either empty of copies the
   *        current processes environment.
   *
   * @param load - Whether to load the current processes environment.
   */
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

  /**
   * @brief Gets the value of a variable.
   *
   * @param key - Name of the variable.
   * @return value of the variable of an empty string if does not exist.
   */
  std::string_view get (std::string_view key) const {
    if (auto it = data_.find (key); it != data_.end ()) {
      return it->value ();
    }
    return {};
  }

  /**
   * @brief Alias for @ref get().
   *
   * Can not be used to insert/change a value.
   */
  std::string_view operator[] (std::string_view key) const {
    return get (key);
  }

  /**
   * @brief Insert of change a variable.
   * @param key - Name of the variable.
   * @param value - Value of the variable.
   */
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

  /**
   * @brief Removes a variable.
   * @param key - Name of the variable.
   */
  void remove (std::string_view key) {
    if (auto it = data_.find (key); it != data_.end ()) {
      data_.erase (it);
    }
  }

  /**
   * @brief Change the name of a variable.
   * @param key - Current name of the variable.
   * @param key_key - Name to give to the variable.
   */
  void rename (std::string_view key, std::string_view new_key) {
    if (auto it = data_.find (key); it != data_.end ()) {
      auto var = std::move (*it);
      var.key (new_key);
      data_.erase (it);
      data_.insert (var);
    }
  }

  /**
   * @brief Removes all variables.
   */
  void clear () {
    data_.clear ();
  }

  /**
   * @brief Returns an iterator pointing to the first element.
   */
  iterator begin () {
    return data_.begin ();
  }

  /**
   * @brief Returns an iterator pointing to the past-the-end element.
   */
  iterator end () {
    return data_.end ();
  }

protected:
  friend class Spell;

  // Used by the Spell class to return a constant reference to an empty
  // environment if its optional is `std::nullopt`.
  static const Env& empty_env () {
    const static Env instance {false};
    return instance;
  }

private:
  detail::Env_Set data_;
};

/**
 * @brief Describes what to do with a standard I/O stream for a child process.
 *
 * Used for the `set_stdin`, `set_stdout`, and `set_stderr` methods of @ref Spell.
 */
enum class Stdio {
  Default,
  Inherit,
  Piped,
  Null
};


/**
 * @brief Describes the result of a process.
 */
class Exit_Status {
public:
  explicit Exit_Status (int code)
  : code_ (code)
  {}

  /**
   * @brief Returns the exit code.
   */
  int code () const {
    return code_;
  }

  /**
   * @brief Was the exit status zero?
   */
  bool success () const {
    return code () == 0;
  }

private:
  int code_;
};

/**
 * @brief The output of a finished process.
 */
struct Output {
  Output (Exit_Status &&s)
  : status (s),
    stdout_ {},
    stderr_ {}
  {}

  /**
   * The status of the process.
   */
  Exit_Status status;

  /**
   * The data that the process wrote to stdout.
   */
  std::vector<char> stdout_;

  /**
   * The data that the process wrote to stderr.
   */
  std::vector<char> stderr_;

  /**
   * @brief Constructs a container out of the data that the process wrote to stdout.
   * @tparam Container Any type that can be constructed from a begin-end pair of iterators over `char`s.
   */
  template <class Container>
    Container collect_stdout () const {
      return Container { stdout_.begin (), stdout_.end () };
    }

  /**
   * @brief Constructs a container out of the data that the process wrote to stderr.
   * @tparam Container Any type that can be constructed from a begin-end pair of iterators over `char`s.
   */
  template <class Container>
    Container collect_stderr () const {
      return Container { stderr_.begin (), stderr_.end () };
    }
};

/**
 * @brief Representation of a running or exited child process.
 */
class Child {
protected:
  Child (Pid pid, Anonymous_Pipe &&i, Anonymous_Pipe &&o, Anonymous_Pipe &&e)
  : pid_ (pid),
    status_ (-1),
    stdin_ (std::move (i)),
    stdout_ (std::move (o)),
    stderr_ (std::move (e))
  {}
  friend class Spell;

public:
  /**
   * @brief Returns the process ID.
   *
   * Note: This is actually the processes handle on Windows.
   */
  Pid id () const {
    return pid_;
  }

  /**
   * @brief Gets the exit status of the child if it has already exited.
   *
   * @return If the child has exited, then the exit status of the child if returned.
   *         If the exit status is not available then `std::nullopt` is returned.
   */
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

  /**
   * @brief Waits for the child to exit, returning its exit status.
   *
   * This function will continue to have the same return value after it has
   * been called at least once.
   *
   * The stdin of the child gets closed before waiting to prevent a deadlock.
   */
  Exit_Status wait () {
  #ifdef _WIN32
    if (status_ != -1) {
      return Exit_Status (status_);
    }
    stdin_.drop ();
    DWORD status;
    WaitForSingleObject (id (), INFINITE);
    GetExitCodeProcess (id (), &status);
    CloseHandle (id ());
    return Exit_Status (status);
  #else
    if (status_ != -1) {
      return Exit_Status (WEXITSTATUS (status_));
    }
    stdin_.drop ();
    int status;
    pid_t wpid;
    while ((wpid = waitpid (id (), &status, 0)) > 0) {}
    status_ = status;
    return Exit_Status (WEXITSTATUS (status));
  #endif
  }

  /**
   * @brief Waits for the child to exit, collecting its remaining output and returning it.
   *
   * The returned @ref Output instance will continue to have the same exit status after it
   * has been called at least once but the stdout and stderr will only contain data after
   * the first call.
   *
   * The stdin of the child gets closed before waiting to prevent a deadlock.
   */
  Output wait_with_output () {
    Output o {wait ()};
    stdout_.read_all (o.stdout_);
    stderr_.read_all (o.stderr_);
    return o;
  }

  /**
   * @brief Forces the child process to exit.
   *
   * This is equivalent to sending a SIGKILL on Unix platforms and calling
   * TerminateProcess on Windows.
   *
   * @return Whether the child was killed (`true`) or had already exited (`false`).
   */
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

  /**
   * @brief Returns a reference to the childs standard input (stdin).
   */
  Anonymous_Pipe& get_stdin () {
    return stdin_;
  }

  /**
   * @brief Returns a reference to the childs standard output (stdout).
   */
  Anonymous_Pipe& get_stdout () {
    return stdout_;
  }

  /**
   * @brief Returns a reference to the childs standard error (stderr).
   */
  Anonymous_Pipe& get_stderr () {
    return stderr_;
  }

private:
  Pid pid_;
  int status_;
  Anonymous_Pipe stdin_;
  Anonymous_Pipe stdout_;
  Anonymous_Pipe stderr_;
};


/**
 * Command builder.
 *
 * `Spell(program)` generates a spell in the default configuration.
 * Additional builder methods allow the configuration to be changed prior to launch.
 * All of these builder methods return a reference to the spell, unless stated otherwise.
 */
class Spell {
public:
  /**
   * @brief Constructs a new `Spell` for launching the program at path `program`.
   *
   * It has the following configuration:
   *  - No arguments to the program
   *  - Inherit the environment of the current process
   *  - Inherit the current working directory
   *  - stdin/stdout/stderr are set to @ref Stdio::Default
   *
   *  If `program` is not an absolute path it is resolved by respective process
   *  execution function (CreateProcess for Windows, execvpe for other platforms).
   *
   *  @param program - path or name of the program
   */
  Spell (std::string_view program)
  : program_ (program),
    args_ {},
    env_ (std::nullopt),
    working_dir_ (std::filesystem::current_path ()),
    stdout_ (Stdio::Default),
    stderr_ (Stdio::Default),
    stdin_ (Stdio::Default)
  {}

  /**
   * @brief Constructs a spell from the given command line string.
   *
   * Takes a string containing a program and it's argument (for example 'echo Hello World')
   * and turns it into a spell for that program with the given arguments.
   *
   * @param command_line - Space-delimited string of the program and it's arguments.
   *                       Spaces can be escaped with a blackslash and are ignored
   *                       inside a string. Strings use either single or double quotes,
   *                       the quotes will not be included in the argument added to the
   *                       spell.
   * @return A @ref Spell object with arguments from the given command line string.
   */
  static Spell from_string (std::string_view command_line) {
    std::string chomped;
    auto chomp = [&] () -> const std::string& {
      auto it = command_line.begin ();
      const auto end = command_line.end ();
      auto in_string = '\0';
      chomped.clear ();
      for (; it != end; ++it) {
        switch (*it) {
          case '\\':
            ++it;
            if (it == end) {
              break;
            }
            chomped.push_back (*it);
            break;
          case '\'':
          case '"':
            if (in_string == '\0') {
              in_string = *it;
            }
            else if (*it == in_string) {
              in_string = '\0';
            }
            else {
              chomped.push_back (*it);
            }
            break;
          case ' ':
            if (in_string == '\0') {
              goto break_;
            }
            else {
              chomped.push_back (' ');
            }
            break;
          default:
            chomped.push_back (*it);
            break;
        }
      }
    break_:
      command_line.remove_prefix (std::distance (command_line.begin (), it));
      while (command_line.front () == ' ') {
        command_line.remove_prefix (1);
      }
      return chomped;
    };
    auto spell = Spell (chomp ());
    std::string_view arg;
    while (!(arg = chomp ()).empty ()) {
      spell.arg (arg);
    }
    return spell;
  }

  /**
   * @brief Returns the path to the program that was given to the constructor.
   */
  std::string_view get_program () const {
    return program_;
  }

  ////////////////////////////////////////////////////////////////////////
  // Arguments

  /**
   * @brief Adds an argument to pass to the program.
   *
   * These are quivalent to the individual `argv` elements so only one arguments can be passed per use.
   * Use @ref args to pass multiple arguments.
   *
   * @param arg - the argument to add.
   */
  Spell& arg (std::string_view arg) {
    args_.emplace_back (arg);
    return *this;
  }

  /**
   * @brief Adds multiple arguments to add to the program.
   *
   * Use @ref arg to pass a single argument.
   *
   * @param args - the arguments to add.
   */
  Spell& args (const std::vector<std::string_view> &args) {
    for (const auto &a : args) {
      args_.emplace_back (a);
    }
    return *this;
  }

  /// @copydoc args
  template <class... Args>
  Spell& args (const Args&... args) {
    (args_.emplace_back (args), ...);
    return *this;
  }

  /**
   * @brief Returns a mutable reference to the arguments.
   */
  Args& get_args () {
    return args_;
  }

  /**
   * @brief Returns a constant reference to the arguments.
   */
  const Args& get_args () const {
    return args_;
  }

  ////////////////////////////////////////////////////////////////////////
  // Environment

  /**
   * @brief Inserts or updates an environment variable.
   * @param key - name of the variable
   * @param val - value of the variable
   */
  Spell& env (std::string_view key, std::string_view val) {
    if (!env_.has_value ()) {
      env_.emplace ();
    }
    env_->set (key, val);
    return *this;
  }

  /**
   * @brief Adds of updates multiple environment variables.
   * @param vars - Span of {key, value} pairs
   */
  Spell& envs (std::span<std::pair<std::string_view, std::string_view>> vars) {
    if (!env_.has_value ()) {
      env_.emplace ();
    }
    for (const auto &[key, val] : vars) {
      env_->set (key, val);
    }
    return *this;
  }

  /**
   * @brief Clears the entire environment for the child process.
   */
  Spell& env_clear () {
    if (!env_.has_value ()) {
      env_.emplace (false);
    }
    else {
      env_->clear ();
    }
    return *this;
  }

  /**
   * @brief Removes an environment variable.
   * @param key - name of the variable.
   */
  Spell& env_remove (std::string_view key) {
    if (!env_.has_value ()) {
      env_.emplace ();
    }
    env_->remove (key);
    return *this;
  }

  /**
   * @brief Get a mutable reference to the environment for the child process.
   *
   * See @ref Env.
   */
  Env& get_envs () {
    if (!env_.has_value ()) {
      env_.emplace ();
    }
    return env_.value ();
  }

  /**
   * @brief Get a constant reference to the environment for the child process.
   *
   * See @ref Env.
   */
  const Env& get_envs () const {
    if (!env_.has_value ()) {
      return Env::empty_env ();
    }
    return env_.value ();
  }

  ////////////////////////////////////////////////////////////////////////
  // Directory

  /**
   * @brief Sets the working directory for the child process.
   *
   * The given path gets canonicalized.
   *
   * @param dir - absolute or relative path to working directory.
   */
  Spell& current_dir (const std::filesystem::path &dir) {
    if (dir.is_absolute ())
      working_dir_ = dir;
    else
      working_dir_ = std::filesystem::weakly_canonical (working_dir_ / dir);
    return *this;
  }

  /**
   * @brief Returns the working directory for the child process.
   */
  const std::filesystem::path &get_current_dir () const {
    return working_dir_;
  }

  ////////////////////////////////////////////////////////////////////////
  // Streams

  /**
   * @brief Configuration for the child process's standard output handle.
   *
   * Defaults to @ref Stdio::Inherit when used with @ref cast or @ref cast_status,
   * and defaults to @ref Stdio::Piped when used with @ref cast_output.
   *
   * See @ref Stdio.
   *
   * @param cfg - new configuration or @ref Stdio::Default.
   */
  Spell& set_stdout (Stdio cfg) {
    stdout_ = cfg;
    return *this;
  }

  /**
   * @brief Configuration for the child process's standard error handle.
   *
   * Defaults to @ref Stdio::Inherit when used with @ref cast or @ref cast_status,
   * and defaults to @ref Stdio::Piped when used with @ref cast_output.
   *
   * See @ref Stdio.
   *
   * @param cfg - new configuration or @ref Stdio::Default.
   */
  Spell& set_stderr (Stdio cfg) {
    stderr_ = cfg;
    return *this;
  }

  /**
   * @brief Configuration for the child process's standard input handle.
   *
   * Defaults to @ref Stdio::Inherit when used with @ref cast or @ref cast_status,
   * and defaults to @ref Stdio::Piped when used with @ref cast_output.
   *
   * See @ref Stdio.
   *
   * @param cfg - new configuration or @ref Stdio::Default.
   */
  Spell& set_stdin (Stdio cfg) {
    stdin_ = cfg;
    return *this;
  }

  ////////////////////////////////////////////////////////////////////////
  // Running

  /**
   * @brief Executes the command as a child process, returning a handle to it.
   *
   * By default, stdin, stdout and stderr are inherited from the parent.
   *
   * @return a @ref Child for the child process or std::nullopt if execution failed.
   */
  std::optional<Child> cast () {
    return do_cast (Stdio::Inherit);
  }

  /**
   * @brief Executes the command as a child process, waiting for it to finish and collecting its status.
   *
   * By default, stdin, stdout and stderr are inherited from the parent.
   *
   * @return the @ref Exit_Status of the child process or std::nullopt if execution failed.
   */
  std::optional<Exit_Status> cast_status () {
    auto child = do_cast (Stdio::Inherit);
    if (child.has_value ())
      return child->wait ();
    return std::nullopt;
  }

  /**
   * @brief Executes the command as a child process, waiting for it to finish and collecting all of its output.
   *
   * By default stdout and stderr are captured and used to provide the resulting output.
   * Stdin is captured and immediately closed.
   *
   * @return the @ref Output of the child process or std::nullopt if execution failed.
   */
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
    Anonymous_Pipe::Pipes out, err, in;

    auto set_pipe = [default_cfg](Anonymous_Pipe::Pipes &p, Stdio &cfg, auto s) {
      if (cfg == Stdio::Default) {
        cfg = default_cfg;
      }
      switch (cfg) {
      break; case Stdio::Inherit: {
        p = Anonymous_Pipe::create_inherit (s);
      }
      break; case Stdio::Piped: {
        p = Anonymous_Pipe::create ();
      }
      break; case Stdio::Null: {
        p = Anonymous_Pipe::create_null ();
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
    startup_info.hStdOutput = out.write.handle ();
    startup_info.hStdError = err.write.handle ();
    startup_info.hStdInput = in.read.handle ();
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
    SetHandleInformation (in.write.handle (), HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation (out.read.handle (), HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation (err.read.handle (), HANDLE_FLAG_INHERIT, 0);

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

    in.read.drop ();
    out.write.drop ();
    err.write.drop ();

    return Child (
      process_info.hProcess,
      std::move (in.write),
      std::move (out.read),
      std::move (err.read)
    );

  #else

    set_pipe (out, stdout_, stdout);
    set_pipe (err, stderr_, stderr);
    set_pipe (in, stdin_, stdin);

    auto [input, output] = Anonymous_Pipe::create ();

    const pid_t pid = fork ();
    if (pid == 0) {
      input.drop ();
      // Duplicate and close pipes
      dup2 (out.write.handle (), STDOUT_FILENO);
      out.drop ();
      dup2 (err.write.handle (), STDERR_FILENO);
      err.drop ();
      dup2 (in.read.handle (), STDIN_FILENO);
      in.drop ();
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
      const std::int32_t error = errno;
      assert (output.write (&error, 4));
      _exit (127);
    }

    output.drop ();

    in.read.drop ();
    out.write.drop ();
    err.write.drop ();

    if (std::int32_t error = 0; input.read (&error, 4).value () == 4) {
      input.drop ();
    #if 0
      std::fprintf (stderr, "%s: %s\n", program_.c_str (), std::strerror (error));
    #endif
      in.write.drop ();
      out.read.drop ();
      err.read.drop ();
      return std::nullopt;
    }
    input.drop ();

    return Child (
      pid,
      std::move (in.write),
      std::move (out.read),
      std::move (err.read)
    );
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

/**
 * @brief Sets the SIGCHLD handler to SIG_IGN on unix platforms.
 *
 * When SIGCHLD is ignored you can just launch a child process and never await
 * it without getting a zombie child.
 */
inline void ignore_sigchld () {
#ifndef _WIN32
  signal (SIGCHLD, SIG_IGN);
#endif
}

} // namespace spell

/**
 * @brief Prints the given environment variable in form "<key>=<value>".
 */
inline std::ostream& operator<< (std::ostream &os, const spell::detail::Env_Var &var) {
  return os << var.unwrap ();
}

/**
 * @brief Prints the given child process handle in form "Child(<pid>)"
 */
inline std::ostream& operator<< (std::ostream &os, const spell::Child &child) {
  return os << "Child(" << child.id () << ')';
}

/**
 * @brief Prints the given exit status in form "Exit_Status(<code>)".
 */
inline std::ostream& operator<< (std::ostream &os, const spell::Exit_Status &exit_status) {
  os << "Exit_Status(" << exit_status.code () << ')';
  return os;
}

