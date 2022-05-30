#include <iostream>
#include <cctype>
#include "spell.hh"

int main () {
  int number = 1;

  auto run = [&number]<class... Args> (Args... args) {
    auto s = spell::Spell ("programs/print_args.exe")
      .args (args...);
    std::cout << number++ << std::endl;
    if (auto c = s.cast (); c.has_value ()) {
      c->wait ();
    }
    fflush (stdout);
  };

  run ("Hello", "World");

  run ();

  std::cout << number++ << std::endl;
  std::vector<std::string_view> args = { "foo", "bar" };
  if (auto c = spell::Spell ("programs/print_args.exe")
        .args (args)
        .cast ();
      c.has_value ()) {
    c->wait ();
  }

  std::cout << number++ << std::endl;
  {
    auto s = spell::Spell ("programs/print_args.exe")
      .arg ("one")
      .arg ("two");
    auto &args = s.get_args ();
    for (auto &arg : args) {
      arg[0] = std::toupper (arg[0]);
    }
    s.cast ()
      .value ()
      .wait ();
  }
}
