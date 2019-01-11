//
// Created by kazem on 4/5/18.
//

#ifndef SIMPLIFICATION_ICHOL_H
#define SIMPLIFICATION_ICHOL_H

#include <cstdio>
#include <vector>
#include <assert.h>

#undef  MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))

#undef  MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))

/*
 * Computes the DAG of dependency before simplification
 */
int computeDAG(int n, int *row, int *col, std::vector<std::vector<int>>& DAG){
 for(int i = 0 ; i < n ; i++ ){
  for(int  m = row[i]+1 ; m < row[i+1] ; m++ ){
   for(int l = m ; l < row[i+1] ; l++ ){
    int ip = col[m];
    for(int mp = row[ip]+1 ; mp < row[ip+1] ; mp++ ){
     int k = mp;
     if( row[col[m]] <= k && k < row[col[m]+1] ){
      for(int kp = row[col[mp]] ; kp < row[col[mp]+1] ; kp++ ){
       for(int lp = m ; lp < row[ip+1] ; lp++ ){
        if ( col[lp+1] <= col[kp] && col[l+1] <= col[k] && col[l] == col[k] && col[lp] == col[kp] ){
         if( i < ip ){
          // ip depends on i; So add an edge from ip to i in dependency graph.
          DAG[i].push_back(ip);
         }
         else if (ip < i){
          // i depends on ip So add an edge from i to ip in dependency graph.
          DAG[ip].push_back(i);
         }
        }
       }
      }
     }
    }
   }
  }
 }
#ifdef VERIFY
 // Make sure that all nodes exist in the computed DAG
 bool *toCheck = new bool[n];
 for (int j = 0; j < n; ++j) {
  toCheck[j]=false;
 }
 for (int j = 0; j < DAG.size(); ++j) {
  for (int i = 0; i < DAG[j].size(); ++i) {
   toCheck[DAG[j][i]]=true;
  }
 }
 for (int j = 0; j < n; ++j) {
  assert(toCheck[j]);
 }
#endif
 return 1;
}


/*
 * Computes the DAG of dependency after simplification
 */
int computeDAG_simplified(int n, int *row, int *col, std::vector<std::vector<int>>& DAG){
 for(int i = 0 ; i < n ; i++ ){
  for(int m = row[i]+1 ; m < row[i+1] ; m++ ){
   int ip = col[m];
   for(int mp = MAX(row[col[m]],row[ip]+1) ; mp < MIN(row[col[m]+1],row[ip+1]) ; mp++ ){
    if(row[col[mp]] < row[col[mp]+1]){
     if( i < ip ){
      // ip depends on i: So add an edge from ip to i in dependency graph.
      DAG[i].push_back(ip);
     }
     else if (ip < i){
      // i depends on ip: So add an edge from i to ip in dependency graph.
      DAG[ip].push_back(i);
     }
    }
   }
  }
 }
#ifdef VERIFY
 // Make sure that all nodes exist in the computed DAG
bool *toCheck = new bool[n];
 for (int j = 0; j < n; ++j) {
  toCheck[j]=false;
 }
 for (int j = 0; j < DAG.size(); ++j) {
  for (int i = 0; i < DAG[j].size(); ++i) {
   toCheck[DAG[j][i]]=true;
  }
 }
 for (int j = 0; j < n; ++j) {
  assert(toCheck[j]);
 }
#endif
 return 1;
}



#endif //SIMPLIFICATION_ICHOL_H
