//#define BS 2

void gs_bcsr(double ***values, double **y, const double **b, double ***idiag, int *rowptr, int *colidx, int BS, double *sum)
{
//  double sum[2]={0};
  int n, i, ii1,ii2,ii3,ii4, j, jj1,jj2;

  for (i = 0; i < n; ++i) {
    for (ii1 = 0; ii1 < BS; ++ii1) {
      sum[ii1] = b[i][ii1];//S0
    }
    for (j = rowptr[i]; j < rowptr[i+1]; ++j) {
      for (jj1 = 0; jj1 < BS; ++jj1) {
        for (ii2 = 0; ii2 < BS; ++ii2) {
          sum[ii2] -= values[j][jj1][ii2] * y[colidx[j]][jj1];//S1
        }
      }
    }

    for (ii3 = 0; ii3 < BS; ++ii3) {
      y[i][ii3] = 0;  //S2
    }
    for (jj2 = 0; jj2 < BS; ++jj2) {
      for ( ii4 = 0; ii4 < BS; ++ii4) {
        y[i][ii4] += idiag[i][jj2][ii4] * sum[jj2];//S3
      }
    }
  } // for each row
}

