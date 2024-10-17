#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <threads.h>
#include <stdint.h>

typedef struct {
  int val;
  char pad[60];
} int_padded;

typedef struct {
//  uint8_t ** attractors;
//  uint8_t ** convergences;
  int ib;
  int istep;
  int sz;
  int tx;
  mtx_t *mtx;
  cnd_t *cnd;
  int_padded *status;
} thrd_info_t;

typedef struct {
//  uint8_t ** attractors;
//  uint8_t ** convergences;
  int sz;
  int nthrds;
  mtx_t *mtx;
  cnd_t *cnd;
  int_padded *status;
} thrd_info_check_t;

// GLOBAL DECLERATION
int nthrds, sz, d;
uint8_t ** attractors;
uint8_t ** convergences;
char * colors[] = {"128 0 32\n", "34 85 50\n", "40 65 135\n", "204 153 0\n", "54 117 136\n", "75 0 130\n", "204 85 0\n"};
/*const char *colors[] = {
    "#800020",  // Deep Burgundy
    "#225532",  // Forest Green
    "#284187",  // Royal Blue
    "#CC9900",  // Mustard Yellow
    "#367588",  // Teal
    "#4B0082",  // Aubergine
    "#CC5500"   // Burnt Sienna
};
*/

int
main_thrd(
    void *args
    )
{
  const thrd_info_t *thrd_info = (thrd_info_t*) args;
//  uint8_t ** attractors = thrd_info->attractors;
//  uint8_t ** convergences = thrd_info->convergences;
  const int ib = thrd_info->ib;
  const int istep = thrd_info->istep;
  const int sz = thrd_info->sz;
  const int tx = thrd_info->tx;
  mtx_t *mtx = thrd_info->mtx;
  cnd_t *cnd = thrd_info->cnd;
  int_padded *status = thrd_info->status;


  for ( int ix = ib; ix < sz; ix += istep ) {
    uint8_t * convergence = (uint8_t*) malloc(sz*sizeof(uint8_t));
    uint8_t * attractor = (uint8_t*) malloc(sz*sizeof(uint8_t));

    for ( int jx = 0; jx < sz; ++jx ) {
      attractor[jx] = 0;
      convergence[jx] = 0;
    }
    mtx_lock(mtx);
    attractors[ix] = attractor;
    convergences[ix] = convergence;
    status[tx].val = ix + istep;
    mtx_unlock(mtx);
    cnd_signal(cnd);
  }

  return 0;
}


int
main_thrd_check(
    void *args
    )
{
  const thrd_info_check_t *thrd_info = (thrd_info_check_t*) args;
//  uint8_t ** attractors = thrd_info->attractors;
//  uint8_t **convergences = thrd_info->convergences;
  const int sz = thrd_info->sz;
  const int nthrds = thrd_info->nthrds;
  mtx_t *mtx = thrd_info->mtx;
  cnd_t *cnd = thrd_info->cnd;
  int_padded *status = thrd_info->status;

  char att_file[24];
  char conv_file[25];
  sprintf(att_file, "newton_attractors_x%d.ppm", d);
  sprintf(conv_file, "newton_convergence_x%d.ppm", d);
  FILE *fp_att = fopen(att_file, "w");
  FILE *fp_conv = fopen(conv_file, "w");
  if (fp_att == NULL) {
    printf("Error opening file\n");
    return -1; 
  }
  if (fp_conv == NULL) {
    printf("Error opening file\n");
    return -1; 
  }
  fprintf(fp_att, "P3\n%d %d\n255\n", sz, sz);
  fprintf(fp_conv, "P3\n%d %d\n50\n", sz, sz);
 
  for ( int ix = 0, ibnd; ix < sz; ) {
    for ( mtx_lock(mtx); ; ) {
      ibnd = sz;
      for ( int tx = 0; tx < nthrds; ++tx )
        if ( ibnd > status[tx].val )
          ibnd = status[tx].val;

      if ( ibnd <= ix )
        cnd_wait(cnd,mtx);
      else {
        mtx_unlock(mtx);
        break;
      }
    }

    fprintf(stderr, "checking until %i\n", ibnd);

    // Det är här vi vill skriva till vår fil sen
    for ( ; ix < ibnd; ++ix ) {
      for ( int jx = 0; jx < sz; ++jx ) {
        // printf(colors[attractors[ix][jx]]);
        fwrite(colors[attractors[ix][jx]], sizeof(char), strlen(colors[attractors[ix][jx]]), fp_att);
        char grey_color[50];
        sprintf(grey_color, "%d %d %d\n", convergences[ix][jx], convergences[ix][jx], convergences[ix][jx]);
        fwrite(grey_color, sizeof(char), strlen(grey_color), fp_conv);

      }
      free(attractors[ix]);
      free(convergences[ix]);
      printf("\n");
    }
  }
  fclose(fp_att);
  fclose(fp_conv);
  return 0;
}


  int 
main(
    int argc,
    char *argv[]
    )
{
  // default values
  nthrds = 1;
  sz = 1;
  d = 1;
  int opt;
  
  //const int sz = 500;

  while ((opt = getopt(argc,argv, "t:l:")) != -1) {
    switch (opt) {
      case 't':
        nthrds = atoi(optarg);
        break;
      case 'l':
        sz = atoi(optarg);
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


  attractors = (uint8_t**) malloc(sz*sizeof(uint8_t*));
  convergences = (uint8_t**) malloc(sz*sizeof(uint8_t*));
  /*
  float *ventries = (float*) malloc(sz*sz*sizeof(float));

  for ( int ix = 0, jx = 0; ix < sz; ++ix, jx += sz )
    v[ix] = ventries + jx;

  for ( int ix = 0; ix < sz*sz; ++ix )
    ventries[ix] = ix;
*/
  thrd_t thrds[nthrds];
  thrd_info_t thrds_info[nthrds];

  thrd_t thrd_check;
  thrd_info_check_t thrd_info_check;

  mtx_t mtx;
  mtx_init(&mtx, mtx_plain);

  cnd_t cnd;
  cnd_init(&cnd);

  int_padded status[nthrds];

   
  // Här gör vi threads
  for ( int tx = 0; tx < nthrds; ++tx ) {
//    thrds_info[tx].attractors = attractors;
//    thrds_info[tx].convergences = convergences;
    thrds_info[tx].ib = tx;
    thrds_info[tx].istep = nthrds;
    thrds_info[tx].sz = sz;
    thrds_info[tx].tx = tx;
    thrds_info[tx].mtx = &mtx;
    thrds_info[tx].cnd = &cnd;
    thrds_info[tx].status = status;
    status[tx].val = -1;

    int r = thrd_create(thrds+tx, main_thrd, (void*) (thrds_info+tx));
    if ( r != thrd_success ) {
      fprintf(stderr, "failer to create thread\n");
      exit(1);
    }
    thrd_detach(thrds[tx]);
  }

  {
//    thrd_info_check.attractors = attractors;
//    thrd_info_check.convergences = convergences;
    thrd_info_check.sz = sz;
    thrd_info_check.nthrds = nthrds;
    thrd_info_check.mtx = &mtx;
    thrd_info_check.cnd = &cnd;

    thrd_info_check.status = status;

    int r = thrd_create(&thrd_check, main_thrd_check, (void*) (&thrd_info_check));
    if ( r != thrd_success ) {
      fprintf(stderr, "failed to create thread\n");
      exit(1);
    }
  }

  {
    int r;
    thrd_join(thrd_check, &r);
  }

  //free(ventries);
  free(convergences);
  free(attractors);

  mtx_destroy(&mtx);
  cnd_destroy(&cnd);


  return 0;
}
