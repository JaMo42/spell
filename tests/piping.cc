#include <iostream>
#include "spell.hh"

int main () {
  std::cout << 1 << std::endl;
  {
    auto c = spell::Spell ("programs/echo_stdin_char.exe")
      .set_stdin (spell::Stdio::Piped)
      .cast ()
        .value ();
    spell::write (c.get_stdin ().handle (), "A", 1);
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
      // Need to remove \r\n
      c.stdout_[c.stdout_.size () - 2] = 0;
    #else
      c.stdout_.back () = 0;
    #endif
      std::cout << reinterpret_cast<char *> (c.stdout_.data ()) << std::endl;
    }
  }

  std::cout << 5 << std::endl;
  {
    auto c = spell::Spell ("programs/hello_world_stderr.exe")
      .cast_output ()
        .value ();
    if (c.stdout_.empty ()) {
      c.stderr_.back () = 0;
      std::cout << reinterpret_cast<char *> (c.stderr_.data ()) << std::endl;
    }
  }
}
