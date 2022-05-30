#include <iostream>
#include "spell.hh"

int main () {
  {
    auto s = spell::Spell ("programs/return_number_of_args.exe")
      .cast_status ()
        .value ();
    std::cout << (s.success () ? "yes" : "no") << ' ' << s.code () << std::endl;
  }

  {
    auto s = spell::Spell ("programs/return_number_of_args.exe")
      .arg ("1")
      .cast_status ()
        .value ();
    std::cout << (s.success () ? "yes" : "no") << ' ' << s.code () << std::endl;
  }

  {
    auto s = spell::Spell ("programs/return_number_of_args.exe")
      .args ("1", "2", "3", "4", "5", "6", "7")
      .cast_status ()
        .value ();
    std::cout << (s.success () ? "yes" : "no") << ' ' << s.code () << std::endl;
  }
}
