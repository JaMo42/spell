#include <stdio.h>
#include <stdlib.h>

int main (const int argc, const char *const *const argv) {
  int i;
  char *v;
  for (i = 1; i < argc; ++i) {
    v = getenv (argv[i]);
    if (v) {
      printf ("%s=%s\n", argv[i], v);
    }
    else {
      printf ("%s not found\n", argv[i]);
    }
  }
}
