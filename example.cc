#include <iostream>
#include "spell.hh"

int main () {
  auto output = spell::Spell ("echo")
                      .arg ("Hello world")
                      .cast_output ();
  if (output.has_value ()) {
    std::cout << output->collect_stdout<std::string_view> ();
  }
  else {
    std::cerr << "Failed to execute command\n";
  }
}

