#include <iostream>
#include "spell.hh"

int main () {
  std::cout << 1 << std::endl;
  {
    auto c = spell::Spell ("programs/echo_stdin_char.exe")
      .set_stdin (spell::Stdio::Piped)
      .cast ()
        .value ();
    c.get_stdin ().write ("A", 1);
    c.wait ();
  }

  std::cout << 2 << std::endl;
  {
    auto c = spell::Spell ("programs/echo_stdin_char.exe")
      .set_stdin (spell::Stdio::Null)
      .cast ()
        .value ();
    c.wait ();
  }

  std::cout << 3 << std::endl;
  {
    auto c = spell::Spell ("programs/echo_stdin_char.exe")
      .set_stdin (spell::Stdio::Piped)
      .cast ()
        .value ();
    c.wait ();
  }

  std::cout << 4 << std::endl;
  {
    auto c = spell::Spell ("programs/hello_world.exe")
      .cast_output ()
        .value ();
    if (c.stderr_.empty ()) {
      // Remove newline
    #ifdef _WIN32
      c.stdout_.pop_back ();
    #endif
      c.stdout_.pop_back ();
      std::cout << c.collect_stdout<std::string_view> () << std::endl;
    }
  }

  std::cout << 5 << std::endl;
  {
    auto c = spell::Spell ("programs/hello_world_stderr.exe")
      .cast_output ()
        .value ();
    if (c.stdout_.empty ()) {
      // Remove newline
    #ifdef _WIN32
      c.stderr_.pop_back ();
    #endif
      c.stderr_.pop_back ();
      std::cout << c.collect_stderr<std::string_view> () << std::endl;
    }
  }

  std::cout << 6 << std::endl;
  {
    auto c = spell::Spell ("programs/echo_stdin_char.exe")
      .set_stdin (spell::Stdio::Piped)
      .set_stdout (spell::Stdio::Piped)
      .cast ()
        .value ();
    if (auto result = c.get_stdout ().write ("abc", 3); result.has_value ()) {
      std::cout << result.value () << std::endl;
    }
    else {
      std::cout << "write fail" << std::endl;
    }
  }
}
