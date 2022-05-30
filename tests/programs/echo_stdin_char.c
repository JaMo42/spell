#include <stdio.h>

int main () {
  char c = fgetc (stdin);
  if (c == EOF) {
    puts ("EOF");
  }
  else {
    printf ("%c\n", c);
  }
}

