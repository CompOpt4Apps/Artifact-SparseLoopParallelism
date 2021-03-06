
#ifndef SIMPLIFICATION_ICHOL0_H
#define SIMPLIFICATION_ICHOL0_H
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cmath>
using namespace std;

extern double sqrt(int);
void ic0_csr(int n, double *val, int * colPtr, int *rowIdx)
{
 int i, k,l, m;
 for (i = 0; i < n - 1; i++){
  val[colPtr[i]] = sqrt(val[colPtr[i]]);//S1

  for (m = colPtr[i] + 1; m < colPtr[i+1]; m++){
   val[m] = val[m] / val[colPtr[i]];//S2
  }

  for (m = colPtr[i] + 1; m < colPtr[i+1]; m++) {
   for (k = colPtr[rowIdx[m]] ; k < colPtr[rowIdx[m]+1]; k++){
    for ( l = m; l < colPtr[i+1] ; l++){
     if (rowIdx[l] == rowIdx[k] && rowIdx[l+1] <= rowIdx[k]){
      val[k] -= val[m]* val[l]; //S3
     }
    }
   }
  }
 }
}

void ic0_csr_parInner(int n, double *val, int * colPtr, int *rowIdx)
{
 int i, m;
 for (i = 0; i < n - 1; i++){
  val[colPtr[i]] = sqrt(val[colPtr[i]]);//S1

  //#pragma omp parallel for schedule(auto)
  for (m = colPtr[i] + 1; m < colPtr[i+1]; m++){
   val[m] = val[m] / val[colPtr[i]];//S2
  }
  //#pragma omp parallel for schedule(auto)
  for (m = colPtr[i] + 1; m < colPtr[i+1]; m++) {
   #pragma omp parallel for schedule(auto)
   for (int k = colPtr[rowIdx[m]] ; k < colPtr[rowIdx[m]+1]; k++){
    //#pragma omp parallel for schedule(auto)
    for (int l = m; l < colPtr[i+1] ; l++){
     if (rowIdx[l] == rowIdx[k] && rowIdx[l+1] <= rowIdx[k]){
      val[k] -= val[m]* val[l]; //S3
     }
    }
   }
  }
 }
}


#endif
