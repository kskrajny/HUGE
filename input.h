#include "err.h"

/* indeksy poszczególnych danych w tablicy argv */

struct input {
  int host;
  int resource;
  int port;
  int meta;
  int timeout;
  int client_port;
  int multi;
  int client_timeout;
};

void get_input(struct input* input, int* argc_ptr, char ***argv_ptr);