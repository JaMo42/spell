# Spell

Single header C++ subprocess library.

## Requirements

The library requires C++20.
The implementation is split into Windows and non-Windows platforms.

## Usage

### Example

```cpp
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
```

This can be build using `make example`.

## Documentation

Run `make doc` to generate doxygen HTML documentation in `doc/html`.

The library is currently not thread safe.

## Tests

`make build-tests` builds the testing programs, `./test` or `python test` runs them.

Synopsis: `test [-vg] [TEST...]`

If `-vg` is passed tests are run with valgrind.
All other arguments are treated as test names, if none are provided all tests are run.

