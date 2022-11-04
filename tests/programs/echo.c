#include <stdio.h>

int main (int argc, const char **argv) {
  if (argc == 1) {
    puts ("");
  } else {
    for (int i = 1; i < argc; ++i) {
      if (i != 1) {
        fputc (' ', stdout);
      }
      fputs (argv[i], stdout);
    }
    fputc ('\n', stdout);
  }
}
