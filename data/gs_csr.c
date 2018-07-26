// Gauss-Seidel CSR

void gs_csr(int n, int *rowPtr, int *colIdx,int *idiag, double *values, double *y, const double *b)
{
  int i,j;
  double sum; 

  for (i = 0; i < n; i++) {
    sum = b[i];
    for (j = rowPtr[i]; j < rowPtr[i + 1]; j++) {
      sum -= values[j]*y[colIdx[j]];
    }
    y[i] = sum*idiag[i];
  } // for each row
}
