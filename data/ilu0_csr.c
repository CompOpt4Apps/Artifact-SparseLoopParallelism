
void ilu0_csr(int n,int *rowPtr, int *colIdx, int *diagPtr, double*values ) 
{ 

  int i,k, j1, j2,st;
  double tmp;
  for(i=0; i < n; i++) 
  { 
    for(k= rowPtr[i]; k < diagPtr[i]; k++)
    { 
      //st = diagPtr[colIdx[k]];
      values[k] = values[k]/values[diagPtr[colIdx[k]]]; // a_ik /= a_kk
      j1 = k + 1;
      j2 = diagPtr[colIdx[k]] + 1;
 
      while (j1 < rowPtr[i + 1] && j2 <rowPtr[colIdx[k] + 1])
      {
        if(colIdx[j1] == colIdx[j2])
        { 
          values[j1] -= values[k]*values[j2]; // a_ij -= a_ik*a_kj
          ++j1; ++j2; 
        }
	else if (colIdx[j1] < colIdx[j2]) ++j1;
	else                              ++j2;	
      } 
    } 
  } 
}



