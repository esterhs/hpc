#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <threads.h>
#include <stdint.h>
#include <complex.h>

typedef struct {
  int val;
  char pad[60];
} int_padded;

typedef struct {
//  uint8_t ** attractors;
//  uint8_t ** convergences;
  int ib;
  int istep;
  int size;
  int tx;
  mtx_t *mtx;
  cnd_t *cnd;
  int_padded *status;
} thrd_info_t;

typedef struct {
//  uint8_t ** attractors;
//  uint8_t ** convergences;
  int size;
  int nthrds;
  mtx_t *mtx;
  cnd_t *cnd;
  int_padded *status;
} thrd_info_check_t;

// GLOBAL DECLERATION
int nthrds, size, d;
uint8_t ** attractors;
uint8_t ** convergences;
char * colors[] = {"128 000 032\n", "034 085 050\n", "040 065 135\n", "204 153 000\n", "054 117 136\n", "075 000 130\n", "204 085 000\n", "255 255 255\n"};
double * roots_re;
double * roots_im;

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
  const int size = thrd_info->size;
  const int tx = thrd_info->tx;
  mtx_t *mtx = thrd_info->mtx;
  cnd_t *cnd = thrd_info->cnd;
  int_padded *status = thrd_info->status;
  uint8_t conv, attr;

  for ( int ix = ib; ix < size; ix += istep ) {
    uint8_t * convergence = (uint8_t*) malloc(size*sizeof(uint8_t));
    uint8_t * attractor = (uint8_t*) malloc(size*sizeof(uint8_t));

    double im = -2 + (double) ix * 4 / (double) size;

    for ( int jx = 0; jx < size; ++jx ) {

      double re = -2 + (double) jx * 4 / (double) size;
      double complex x = re + im * I;
      for ( conv = 0, attr = 8; ; ++conv ) {
        double x_re = creal(x);
        double x_im = cimag(x);
        if ( conv == 50 ) {
          attr = 7;
          break;
        } 
        if ( x_re > 1e10 || x_re < -1e10 || x_im > 1e10 || x_im < -1e10 ) { //upper bound
          attr = 7;
          break;
        }
        if ( x_re * x_re + x_im * x_im < 1e-6) { //lower bound
          attr = 7;
          break;
        }
        for ( int kx = 0; kx < d; ++kx ) {
          double diff_re = ( x_re - roots_re[kx]) * ( x_re - roots_re[kx]);
          double diff_im = ( x_im - roots_im[kx]) * ( x_im - roots_im[kx]);
          double dist = diff_re + diff_im;
          if ( dist < 1e-6 ) {
            attr = kx;
            break;
          }
        }
        if ( attr != 8 )
          break;
        
        double complex x_old = x;
        //printf("x = %f + i%f\n", creal(x), cimag(x));
        switch ( d ) {
          case 1:
           x = 1;
           break;
          case 2:
           x = x_old - 0.5 * x_old + 0.5 / x_old;
           break;
          case 3:
           x = x_old - (1./3.) * x_old + (1./3.) / (x_old * x_old);
           break;
          case 4:
           x = x_old - (1./4.) * x_old + (1./4.) / (x_old * x_old * x_old);
           break;
          case 5:
           x = x_old - (1./5.) * x_old + (1./5.) / (x_old * x_old * x_old * x_old);
           break;
          case 6:
           x = x_old - (1./6.) * x_old + (1./6.) / (x_old * x_old * x_old * x_old * x_old);
           break;
          case 7:
           x = x_old - (1./7.) * x_old + (1./7.) / (x_old * x_old * x_old * x_old * x_old * x_old);
           break;
          default:
           fprintf(stderr, "unexpcted degree\n");
           exit(1);
        }
      }

    attractor[jx] = attr;
    convergence[jx] = conv;

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
  const int size = thrd_info->size;
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
  fprintf(fp_att, "P3\n%d %d\n255\n", size, size);
  fprintf(fp_conv, "P3\n%d %d\n51\n", size, size);

  char * grey_colors[51];
  for ( int i = 0; i < 51; ++i ) {
    grey_colors[i] = malloc(12 * sizeof(char));
    sprintf(grey_colors[i], "%d %d %d\n", i, i, i);
  }
 
  for ( int ix = 0, ibnd; ix < size; ) {
      for ( mtx_lock(mtx); ; ) {
      ibnd = size;
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

    //fprintf(stderr, "checking until %i\n", ibnd);

    // Det är här vi vill skriva till vår fil sen
    for ( ; ix < ibnd; ++ix ) {
      for ( int jx = 0; jx < size; ++jx ) {
       // printf("%u\n", convergences[ix][jx]);
        fwrite(colors[attractors[ix][jx]], sizeof(char), 12, fp_att);
        //sprintf(grey_color, "%d %d %d\n", convergences[ix][jx], convergences[ix][jx], convergences[ix][jx]);
        fwrite(grey_colors[convergences[ix][jx]], sizeof(char), strlen(grey_colors[convergences[ix][jx]]), fp_conv);
      }
      free(attractors[ix]);
      free(convergences[ix]);
    }
  }
  for (int i = 0; i < 51; ++i) {
   free(grey_colors[i]);
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
  nthrds = 5;
  size = 10;
  d = 1;
  int opt;
  
  //const int size = 500;

  while ((opt = getopt(argc,argv, "t:l:")) != -1) {
    switch (opt) {
      case 't':
        nthrds = atoi(optarg);
        break;
      case 'l':
        size = atoi(optarg);
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

  roots_re = (double*)malloc(d * sizeof(double));
  roots_im = (double*)malloc(d * sizeof(double));
  
  switch ( d ) {
    case 1:
      roots_re[0] = 1;
      roots_im[0] = 0;
      break;
    case 2: 
      roots_re[0] = 1;
      roots_im[0] = 0;
      
      roots_re[1] = -1;
      roots_im[1] = 0;
      break;
    case 3:
      roots_re[0] = 1;
      roots_im[0] = 0;
      
      roots_re[1] = -0.5;
      roots_im[1] = -0.86603;

      roots_re[2] = -0.5;
      roots_im[2] = 0.86603;      
      break;
    case 4:
      roots_re[0] = 1;
      roots_im[0] = 0;
      
      roots_re[1] = -1;
      roots_im[1] = 0;

      roots_re[2] = 0;
      roots_im[2] = 1; 

      roots_re[3] = 0;
      roots_im[3] = 1;
      break;
    case 5:
      roots_re[0] = 1;
      roots_im[0] = 0;
      
      roots_re[1] = -0.80902;
      roots_im[1] = -0.58779;

      roots_re[2] = -0.80902;
      roots_im[2] = 0.58779;

      roots_re[3] = 0.30902;
      roots_im[3] = -0.95106; 

      roots_re[4] = 0.30902;
      roots_im[4] = 0.95106;
      break;
    case 6:
      roots_re[0] = 1;
      roots_im[0] = 0;

      roots_re[1] = -1;
      roots_im[1] = 0;

      roots_re[2] = 0.5;
      roots_im[2] = 0.86603;

      roots_re[3] = -0.5;
      roots_im[3] = -0.86603;

      roots_re[4] = -0.5;
      roots_im[4] = 0.86603;

      roots_re[5] = 0.5;
      roots_im[5] = -0.86603;
      break;
    case 7:
      roots_re[0] = 1;
      roots_im[0] = 0;

      roots_re[1] = -0.90097;
      roots_im[1] = -0.43388;
      
      roots_re[2] = -0.90097;
      roots_im[2] = 0.43388;

      roots_re[3] = 0.62349;
      roots_im[3] = 0.78183;

      roots_re[4] = 0.62349;
      roots_im[4] = -0.78183;

      roots_re[5] = -0.22252;
      roots_im[5] = -0.97493;

      roots_re[6] = -0.22252;
      roots_im[6] = 0.97493;
      break;
    default:
      fprintf(stderr, "unexpcted degree\n");
      exit(1);
  }

  attractors = (uint8_t**) malloc(size*sizeof(uint8_t*));
  convergences = (uint8_t**) malloc(size*sizeof(uint8_t*));
  /*
  float *ventries = (float*) malloc(size*size*sizeof(float));

  for ( int ix = 0, jx = 0; ix < size; ++ix, jx += size )
    v[ix] = ventries + jx;

  for ( int ix = 0; ix < size*size; ++ix )
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
    thrds_info[tx].size = size;
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
    thrd_info_check.size = size;
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

  free(roots_re);
  free(roots_im);

  mtx_destroy(&mtx);
  cnd_destroy(&cnd);


  return 0;
}
