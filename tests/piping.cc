#include <iostream>
#include "spell.hh"

int main () {
  std::cout << 1 << std::endl;
  {
    auto c = spell::Spell ("programs/echo_stdin_char.exe")
      .set_stdin (spell::Stream::Piped)
      .cast ()
        .value ();
    spell::write (c.get_stdin (), "A", 1);
    c.wait ();
  }

  std::cout << 2 << std::endl;
  {
    auto c = spell::Spell ("programs/echo_stdin_char.exe")
      .set_stdin (spell::Stream::Null)
      .cast ()
        .value ();
    c.wait ();
  }

  std::cout << 3 << std::endl;
  {
    auto c = spell::Spell ("programs/echo_stdin_char.exe")
      .set_stdin (spell::Stream::Piped)
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
      c.stdout_.back () = 0;
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
