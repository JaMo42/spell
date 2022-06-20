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

  std::cout << 2 << std::endl;
  {
    auto s = spell::Spell ("i_do_not_exist");
    std::cout << (s.cast ().has_value () ? "yes" : "no") << std::endl;
  }

  std::cout << 3 << std::endl;
  {
    spell::Spell::from_string ("echo Hello World").cast ()->wait ();
    spell::Spell::from_string ("echo 'Hello World'").cast ()->wait ();
    spell::Spell::from_string ("echo '\\'Hello World\\''").cast ()->wait ();
    spell::Spell::from_string ("echo '\"Hello World\"'").cast ()->wait ();
    spell::Spell::from_string ("echo H'ell'o World").cast ()->wait ();
    spell::Spell::from_string ("echo 안녕'하세'요").cast ()->wait ();
  }
}

