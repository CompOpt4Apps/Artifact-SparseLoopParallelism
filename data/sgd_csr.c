
void sgd_csr(int* rowPtr, int* colIdx, double** fv, double *ratings, int n, int SGD_FEATURE_SIZE, double SGD_LAMBDA)
{
  int i,j,k;
  double err, step_size;

  for(i=0; i < n; i++){
    for(j=rowPtr[i]; j < rowPtr[i+1]; j++){
      //Statement s0
      err = -ratings[j];//edge weight
      for(k=0; k < SGD_FEATURE_SIZE; k++){
        err += fv[i][k]*fv[colIdx[j]][k];
      }
      for(k=0; k < SGD_FEATURE_SIZE ; k++){
        //source feature vector update
        fv[i][k]-= step_size * (err * fv[colIdx[j]][k] + SGD_LAMBDA*fv[i][k]);
        // destination feature vector update
        fv[colIdx[j]][k] -= step_size * (err * fv[i][k] + SGD_LAMBDA*fv[colIdx[j]][k]);
      }//end for -k
    }//end for -j
  }//end for -i
}

