//
// Created by kazem on 30/07/18.
//

#ifndef TRIANGULAR_TRIANGULARSOLVE_H
#define TRIANGULAR_TRIANGULARSOLVE_H

/*
 ****** Serial implementation
 */
int lsolve (int n, int* Lp, int* Li, double* Lx, double *x){
 int j;
 if (!Lp || !Li || !x) return (0) ;                     /* check inputs */
 for (j = 0 ; j < n ; j++)
 {
  x [j] /= Lx [Lp [j]] ;
  for ( int p = Lp [j]+1 ; p < Lp [j+1] ; p++)
  {
   x [Li [p]] -= Lx [p] * x [j] ;
  }
 }
 return (1) ;
}


/*
 ****** Parallel inner loop implementation
 */
int lsolve_parInner (int n, int* Lp, int* Li, double * __restrict__ Lx, double * __restrict__ x){
 int  j;
 if (!Lp || !Li || !x) return (0) ;                     /* check inputs */



 double *inp = (double*)(__builtin_assume_aligned(Lx, 16));
 double *res = (double*)(__builtin_assume_aligned(x, 16));


 for (j = 0 ; j < n ; j++)
 {
  x [j] /= Lx [Lp [j]] ;
  //res [j] /= inp [Lp [j]] ;
  int lb = Lp [j]+1, ub = Lp [j+1] ;
  //#pragma GCC ivdep
  #pragma omp parallel for schedule(auto)
  for (int p = lb ; p < ub ; p++)
  //for (int p = Lp [j]+1 ; p < Lp [j+1] ; p++)
  {
   int idx = Li [p];
   x [Li[p]] -= Lx [p] * x [j] ;
   //res [Li[p]] -= inp [p] * res [j] ;
  }
 }
 return (1) ;
}


/*
 *
 */

void rhsInit(int n, int *Ap, int *Ai, double *Ax, double *b){
 /*generating a rhs that produces a result of all 1 vector*/
 for (int j = 0; j < n; ++j) {
  b[j]=0;
 }
 for (int c = 0; c < n ; ++c) {
  for (int cc = Ap[c]; cc < Ap[c + 1]; ++cc) {
   b[Ai[cc]]+=Ax[cc];
  }
 }
}

/*
 * Testing lower triangular solve to make sure all x elements are ONE.
 */

int testTriangular(size_t n, const double *x) {//Testing
 int test=0;
 for (int i = 0; i < n; ++i) {
  if(1-x[i]<0.001)
   test++;
  //else
  // cout<<i<<";";
 }
 if(n-test>0){
  return false;
 }
 return true;
}
#endif //TRIANGULAR_TRIANGULARSOLVE_H
