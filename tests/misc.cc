#include <iostream>
#include "spell.hh"

int main () {
  std::cout << 1 << std::endl;
  {
    auto c = spell::Spell ("programs/echo_stdin_char.exe")
      .cast ()
        .value ();
    std::cout << (c.kill () ? "yes" : "no") << std::endl;
  }
  {
    auto c = spell::Spell ("programs/hello_world.exe")
      .set_stdout (spell::Stdio::Null)
      .cast ()
        .value ();
    c.wait ();
    std::cout << (c.kill () ? "yes" : "no") << std::endl;
  }
}
