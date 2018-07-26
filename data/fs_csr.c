


void fs_csr(int n, int* rowPtr, int* colIdx, double* val, double *b, double *x)
{
  int i,j,tmp[100];

  for(i=0;i<n;i++) {
    tmp[i] = b[i];
    for (j=rowPtr[i]; j<rowPtr[i+1]-1;j++) {
      tmp[i] -= val[j]*x[colIdx[j]];
    }
    x[i] = tmp[i] / val[rowPtr[i+1]-1];
  }
}

