
// sparse matrix vector multiply (SpMV) for CSR format

void spMV_Mul_csr(int n, int* rowPtr, int* col, double* val, double *x, double *y)
{
  int i,j, tmp;

  for (i=0; i<n; i++) {
    for(j=rowPtr[i]; j<rowPtr[i+1]; j++){
      y[i] = y[i] + val[j]*x[col[j]];
    }
  }

}

