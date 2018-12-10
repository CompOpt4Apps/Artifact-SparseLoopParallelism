// Gauss-Seidel CSR

void gs_csr(int n, int *rowPtr, int *colIdx,int *idiag, double *values, double *y, const double *b)
{
  int i,j;
  double sum[100]; 

  for (i = 0; i < n; i++) {
    sum[i] = b[i];
    for (j = rowPtr[i]; j < rowPtr[i + 1]; j++) {
      sum[i] -= values[j]*y[colIdx[j]];
    }
    y[i] = sum[i]*idiag[i];
  } // for each row
}
