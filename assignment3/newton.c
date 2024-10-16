#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <threads.h>

typedef struct {
  int val;
  char pad[60];
} int_padded;

typedef struct {
  const float **v;
  float **w;
  int ib;
  int istep;
  int sz;
  int tx;
  mtx_t *mtx;
  cnd_t *cnd;
  int_padded *status;
} thrd_info_t;

typedef struct {
  const float **v;
  float **w;
  int sz;
  int nthrds;
  mtx_t *mtx;
  cnd_t *cnd;
  int_padded *status;
} thrd_info_check_t;

// GLOBAL DECLERATION
int nthrds, sz, d;

int
main_thrd(
    void *args
    )
{
  const thrd_info_t *thrd_info = (thrd_info_t*) args;
  const float **v = thrd_info->v;
  float **w = thrd_info->w;
  const int ib = thrd_info->ib;
  const int istep = thrd_info->istep;
  const int sz = thrd_info->sz;
  const int tx = thrd_info->tx;
  mtx_t *mtx = thrd_info->mtx;
  cnd_t *cnd = thrd_info->cnd;
  int_padded *status = thrd_info->status;


  for ( int ix = ib; ix < sz; ix += istep ) {
    const float *vix = v[ix];
    float *wix = (float*) malloc(sz*sizeof(float));

    for ( int jx = 0; jx < sz; ++jx )
      wix[jx] = ix;

    mtx_lock(mtx);
    w[ix] = wix;
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
  const float **v = thrd_info->v;
  float **w = thrd_info->w;
  const int sz = thrd_info->sz;
  const int nthrds = thrd_info->nthrds;
  mtx_t *mtx = thrd_info->mtx;
  cnd_t *cnd = thrd_info->cnd;
  int_padded *status = thrd_info->status;

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
      //printf("\n");
      for ( int jx = 0; jx < sz; ++jx )
        printf("%f ", w[ix][jx]);
      free(w[ix]);
      printf("\n");
    }
  }

  return 0;
}
      





  int 
main(
    int argc,
    char *argv[]
    )
{
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


  float **v = (float**) malloc(sz*sizeof(float*));
  float **w = (float**) malloc(sz*sizeof(float*));
  float *ventries = (float*) malloc(sz*sz*sizeof(float));

  for ( int ix = 0, jx = 0; ix < sz; ++ix, jx += sz )
    v[ix] = ventries + jx;

  for ( int ix = 0; ix < sz*sz; ++ix )
    ventries[ix] = ix;

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
    thrds_info[tx].v = (const float**) v;
    thrds_info[tx].w = w;
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
    thrd_info_check.v = (const float**) v;
    thrd_info_check.w = w;
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

  free(ventries);
  free(v);
  free(w);

  mtx_destroy(&mtx);
  cnd_destroy(&cnd);


    //  printf("t: %i\n", n_thrds);
//  printf("l: %i\n", n_rows);
//  printf("d: %i\n", d);
  return 0;
}
