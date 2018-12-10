
//#include <math.h>

extern double sqrt(double in);
//  return in/10;
//}

void ic0_csc(int n, double *val, int * colPtr, int *rowIdx)
{
  double temp;
  for (int i = 0; i < n - 1; i++){
    temp = val[colPtr[i]];
    val[colPtr[i]] = val[colPtr[i]]/sqrt(temp);//S1

    for (int m1 = colPtr[i] + 1; m1 < colPtr[i+1]; m1++){
      val[m1] = val[m1] / val[colPtr[i]];//S2
    }

    for (int m2 = colPtr[i] + 1; m2 < colPtr[i+1]; m2++) {
      for (int k = colPtr[rowIdx[m2]] ; k < colPtr[rowIdx[m2]+1]; k++){
        for (int l = m2; l < colPtr[i+1] ; l++){
          if (rowIdx[l] == rowIdx[k] ){
            if(rowIdx[l+1] <= rowIdx[k]){
              val[k] -= val[m2]* val[l]; //S3
            }
          }
        }
      }
    }
  }
}

