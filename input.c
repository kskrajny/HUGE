#include "input.h"

void get_input(struct input* input, int* argc_ptr, char ***argv_ptr) {

  if(*argc_ptr % 2 == 0)
    fatal("Bad parameters!");

  input->host = -1;
  input->resource = -1;
  input->port = -1;
  input->meta = -1;
  input->timeout = -1;
  input->client_port = -1;
  input->multi = -1;
  input->client_timeout = -1;

  int i = 1;
  while(i < *argc_ptr - 1) {
    switch((int)(*argv_ptr)[i][1]) {
      case (int)'h':
        input->host = i+1;
      break;
      case (int)'r':
        input->resource = i+1;
      break;
      case (int)'p':
        input->port = i+1;
      break;
      case (int)'m':
        input->meta = i+1;
      break;
      case (int)'t':
        input->timeout = i+1;
      break;
      case (int)'P':
        input->client_port = i+1;
      break;
      case (int)'B':
        input->multi = i+1;
      break;
      case (int)'T':
        input->client_timeout = i+1;
      break;
    }
    i += 2;
  }
  if(input->host == -1 ||
     input->resource == -1 ||
     input->port == -1 || 
     input->client_port == -1)
    fatal("Bad parameters!");
}
