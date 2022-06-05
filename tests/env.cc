#include <iostream>
#include <cstdlib>
#include <set>
#include "spell.hh"

int main () {
  std::cout << 1 << std::endl;
  spell::Spell ("programs/print_env.exe")
    .env ("foo", "bar")
    .arg ("foo")
    .cast ()
      .value ()
      .wait ();

  std::cout << 2 << std::endl;
  spell::Spell ("programs/print_env.exe")
    .env_clear ()
    .arg ("PATH")
    .cast ()
      .value ()
      .wait ();

  std::cout << 3 << std::endl;
#ifdef _WIN32
  _putenv ("foo=bar");
#else
  setenv ("foo", "bar", 1);
#endif
  auto env = spell::Spell ("").get_envs ();
  std::cout << env["foo"] << std::endl;

  std::cout << 4 << std::endl;
  {
    auto s = spell::Spell ("").env_clear ();
    auto &env = s.get_envs ();
    env.set ("one", "1");
    env.set ("two", "2");
    env.set ("three", "3");
    // Transfer to ordered set for predictable output
    std::set<std::string> vars {};
    for (auto i : env) {
      vars.insert (i.unwrap ());
    }
    for (auto i : vars) {
      std::cout << i << std::endl;
    }
  }

  std::cout << 5 << std::endl;
  {
    auto s = spell::Spell ("").env_clear ();
    auto &env = s.get_envs ();
    env.set ("a", "2");
    env.set ("a", "1");
    std::cout << env["a"] << std::endl;
  }
}
