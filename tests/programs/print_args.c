#include <stdio.h>

int main (const int argc, const char *const *const argv) {
  int i;
  for (i = 1; i < argc; ++i) {
    fputs (argv[i], stdout);
    putchar ((i+1) == argc ? '\n' : ' ');
  }
}
