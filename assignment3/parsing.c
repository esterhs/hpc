#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

  int 
main(
    int argc,
    char *argv[]
    )
{
  int n_thrds = 1;
  int n_rows = 1;
  int d = 1;
  int opt;

  while ((opt = getopt(argc,argv, "t:l:")) != -1) {
    switch (opt) {
      case 't':
        n_thrds = atoi(optarg);
        break;
      case 'l':
        n_rows = atoi(optarg);
        break;
      default:
        printf("Usage: -t -l d");
        return -1;
    }
  }

  if (optind < argc) {
    d = atoi(argv[optind]);
  } else {
    printf("Missing d.");
  }

  printf("t: %i\n", n_thrds);
  printf("l: %i\n", n_rows);
  printf("d: %i\n", d);
  return 0;
}
