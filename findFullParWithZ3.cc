/*!
 * \file simplification.cc
 *
 * This file is a driver for reproducing results for arXiv submission XXX 
 *

>> Please refer to README.md for build instructions

>> Building the driver after building all the dependencies: 

g++ -O3 -o findFullParWithZ3 findFullParWithZ3.cc -I IEGenLib/src IEGenLib/build/src/libiegenlib.a -lisl -std=c++11

>> Run the driver (in root directory):

./simplifyDriver list.txt

*/

#include <iostream>
#include "iegenlib.h"
#include "parser/jsoncons/json.hpp"
#include<cstdlib>
#include<fstream>
#include <map>

using jsoncons::json;
using namespace iegenlib;
using namespace std;

// The data structure that holds evaluation result for a dependence relation
typedef struct deprel{

  deprel(){
    bl = mono = coMono = tri = combo = false;
    origComplexity = simpComplexity = "";
    rel = NULL; simpRel = NULL;
  }

  Relation * rel;
  Relation * simpRel;
  bool bl;    // Is it part of baseline? (is it MaySat considering linear inconsistency?)
  bool mono;  // Is it MaySat after considering mopnotonicity domain information? 
  bool coMono;
  bool tri;
  bool combo;
  string origComplexity;
  string simpComplexity;
}depRel;


void driver(string list);

// Utility functions
void initComplexities(map<string,int> &complexities);
void restComplexities(map<string,int> &complexities);
void printCompString(string category, map<string,int> &complexities, ofstream &out);
void printComplexities(map<string,int> &complexities, string stage, ofstream &out);
string adMissingInductionConstraints(string str,json &missingConstraints);
void setDependencesVal(std::vector<depRel> &dependences, int relNo, int rule, bool val);
string getPrettyComplexity(string comp);
string giveCompWithOrd(int ord);
int compCompare(string comp1, string comp2);
string int2str(int i);
string trimO(string str);
string b2s(bool cond){ if(cond){ return string("Yes");} return string("No");} 
void genChillScript(json &analysisInfo);
// Decide a dependence relation with z3
bool decideWithZ3(json &z3Info, Relation *dep, int id, std::vector<std::string> constrantsAndDefs, 
                  std::vector<std::string> properties);
void readLoop(string str, int &stNo, int &parLL, int &nRels);


//------------------------------------------ MAIN ----------------------------------------
int main(int argc, char **argv)
{
  if (argc == 1)
  {
    cout<<"\n\nYou need to specify the input file. The input file should contain name of input JSON files:"
          "\n./simplifyDriver list.txt\n\n";
  } else if (argc >= 2){
    // Parsing command line arguments and reading given files.
    for(int arg = 1; arg < argc ; arg++){
      driver(string(argv[arg]));
    }
  }

  return 0;
}


// The driver that gathers the results for arXiv submission
void driver(string list)
{
  map<string,int> currentCodeOrigComplexities, origComplexities, blComplexities,
                  monoComplexities, coMonoComplexities, triComplexities, comboComplexities;
  initComplexities(origComplexities); initComplexities(blComplexities);
  initComplexities(monoComplexities); initComplexities(coMonoComplexities);
  initComplexities(triComplexities);  initComplexities(comboComplexities);

  int z3_f_c=0;
  string inputFileName="", line="";

  // Read name of json files from input list
  ifstream inL(list);

 // Looping over examples listed in the input file (JSON files)
 for(; getline( inL, inputFileName );){ 

  // Read the data from inputFileName
  ifstream in(inputFileName);
  json data;
  in >> data;

  cout<<"\n"<<data[0][0]["Name"]<<":\n\n";

  string kernelComplexity = data[0][0]["Kernel Complexity"].as<string>();

  iegenlib::setCurrEnv();
  // Introduce the uninterpreted function symbols to environment, and 
  // indicate their domain, range, whether they are bijective, or monotonic.
  json ufcs = data[0][0];
  // Read UFCs' data for code No. p, from ith relation
  addUFCs(ufcs);
  // Add defined domain information to environment
  json uqCons = data[0][0]["User Defined"];
  addUniQuantRules(uqCons);


  // Generating the CHILL script
  json analysisInfo = data[0][0]["CHILL analysis info"];
  genChillScript(analysisInfo);

  // Extracting dependence relations with CHILL
  string chillCommand = "./chill/build/chill " + analysisInfo[0]["Script file"].as<string>() + " 2> /dev/null"; 
  int chillErr = system (chillCommand.c_str());

  ifstream depf((analysisInfo[0]["Output file"].as<string>()).c_str());

  // Loop over distinct loops for an input loops nest
  while(getline( depf, line) ){

    int ct=0, i=0, stNo=0, parLL=0, nRels=0, unSatFound=0, maySatFound=0, nUniqueRels=0;

    std::pair<std::set<Relation>::iterator,bool> uniqRel;
    std::set<Relation> uniqueRelations;
    std::vector<Relation*> simpRels;
    std::vector<depRel> dependences (100);
    std::set<int> parallelTvs;
    Relation *rel;
    
    // Extract the loop ID, and number of extracted relations for it from CHILL output file
    readLoop(line, stNo, parLL, nRels);


    // Specify loops that are going to be parallelized, 
    // so we are not going to project them out.
//cout<<"\nDebug: Loop: [StNo = "<<stNo<<", Level = "<<parLL<<", nRels = "<<nRels<<"]\n";

    i=0;
    for (ct=0; ct < nRels ; ct++){
//cout<<"\nReading r#"<<ct;
      // Read a dependence 
      getline( depf, line);

      // Adding missing constraints related to induction variables (only applies to ILU0)
      json missingConstraints = data[0][0]["Missing induction iterator constraints"];
      if( missingConstraints.size() > 0 ){
        line = adMissingInductionConstraints(line, missingConstraints);
      }
    
      // If the relation is not unique, ignore it
      rel = new Relation(line); 
      uniqRel = uniqueRelations.insert(*rel);
      if( !(uniqRel.second) ){
        delete rel;
        continue;
      }
      // Otherwise store it for analysis
      dependences[i].rel = rel;
      i++;
//cout<<"\nParsed r#"<<ct;
    }
    nUniqueRels = i;
    unSatFound = 0;
    maySatFound = 0;

    std::vector<std::string> properties;
    json z3Info = data[0][0]["z3 info"];

    std::vector<std::string> extraSyms; extraSyms.clear();
    for( i=0; i < nUniqueRels; i++){    //Loop over all unique relations
//cout<<"\nCalling z3form for uqRel no: "<<i<<"\n";

      std::vector<std::string> UFSyms;

      std::vector<std::string> constrantsAndDefs = dependences[i].rel->getZ3form(UFSyms);
      // Decide a dependence relation with z3
      int noAvalRules = queryNoUniQuantRules();
      cout<<"\nNo. of UQRs = "<<noAvalRules<<"\n";
      

      decideWithZ3(z3Info, dependences[i].rel, (z3_f_c++), constrantsAndDefs, properties);
    }
/* 
    if(unSatFound == nUniqueRels){
     // cout<<"\n\n-------- Loop: [StNo = "<<stNo<<", Level = "<<parLL<<"] is Fully parallel!\n";
    } else {
     // cout<<"\n\n-------- Loop: [StNo = "<<stNo<<", Level = "<<parLL<<"] is NOT Fully parallel!\n";
    }
*/
  }

 } // End of input json file list loop

}
// ----------- End of driver function --------------------------------------







void readLoop(string str, int &stNo, int &parLL, int &nRels){
  sscanf (str.c_str(),"[First stmt = %d, Loop level = %d, No. of Rels = %d]",&stNo,&parLL,&nRels);
}


// Generate CHILL scripts using analysis info from json file 
void genChillScript(json &analysisInfo){

  ofstream outf;
  outf.open((analysisInfo[0]["Script file"].as<string>()).c_str(), std::ofstream::out);

  outf<<"from chill import *\n";
  outf<<"source(\'"<<analysisInfo[0]["Source"].as<string>()<<"\')\n";
  outf<<"destination(\'"<<analysisInfo[0]["Destination"].as<string>()<<"\')\n";
  outf<<"procedure(\'"<<analysisInfo[0]["Procedure"].as<string>()<<"\')\n";
  outf<<"loop("<<analysisInfo[0]["Loop"].as<string>()<<")\n";
  outf<<"print_dep_ufs(\'"<<analysisInfo[0]["Output file"].as<string>()
                   <<"\',\'"<<analysisInfo[0]["Private Arrays"].as<string>()
                   <<"\',\'"<<analysisInfo[0]["Reduction Statements"].as<string>()
                          <<"\')\n";
}


// Decide a dependence relation with z3
bool decideWithZ3(json &z3Info, Relation *dep, int id, std::vector<std::string> constrantsAndDefs, 
                  std::vector<std::string> properties){
  bool sat = true;
  string n_outF = z3Info[0]["Path"].as<string>() + int2str(id) + ".smt2";
  ofstream outf;
  outf.open( n_outF.c_str(), std::ofstream::out);

  outf<<"(set-option :timeout "<<z3Info[0]["timeout"].as<string>()<<")\n";
  json jExtraSyms = z3Info[0]["Extra Symbols"];
  for(int i=0; i < jExtraSyms.size() ; i++ ){
     outf<<string("(declare-const "+ jExtraSyms[i].as<string>() + " Int)")<<"\n";
  }
  for(int i = 0 ; i < constrantsAndDefs.size(); i++)
    outf<<constrantsAndDefs[i]<<"\n";

  for(int i = 0 ; i < properties.size(); i++)
    outf<<properties[i]<<"\n";

  outf<<"\n(check-sat)\n";

  outf.close();

  //
  string ans;
  string z3Command = "./z3/build/z3 " + n_outF + "> data/tempData/z3ansF.txt" + " 2> /dev/null"; 
  int z3Err = system (z3Command.c_str());
  ifstream z3ansF("data/tempData/z3ansF.txt", std::ofstream::in);
  getline(z3ansF, ans);
  cout<<"\n"<<ans;
  z3ansF.close();

  if(ans == "unsat") sat = false;

  return sat;
}



string adMissingInductionConstraints(string str,json &missingConstraints){
  bool notChanged=true;
  int inArrity = 5, outArity = 5;
  string newStr, inTupleVars[20], outTupleVars[20];
  srParts parts = getPartsFromStr(str);
  std::queue<std::string> tupVars = tupVarsExtract(parts.tupDecl, inArrity, outArity);
  for(int i=0; i < inArrity; i++){
    inTupleVars[i] = tupVars.front(); 
    tupVars.pop();
  }
  for(int i=0; i < outArity; i++){
    outTupleVars[i] = tupVars.front(); 
    tupVars.pop();
  }
  for(int i=0; i < missingConstraints.size(); i++){
    string it = missingConstraints[i]["Indunction iterator"].as<string>();
    if( str.find(it) != std::string::npos ){
       notChanged = false;
       parts.constraints += " && " + 
                            missingConstraints[i]["Constraints to add"].as<string>();
       for(int j=0;j<missingConstraints[i]["Iterators to add"].size();j++){
         string itToAdd = missingConstraints[i]["Iterators to add"][j].as<string>();
         if( itToAdd[(itToAdd.size()-1)] == 'p' ) {
           outTupleVars[outArity] = itToAdd;
           outArity++;
         } else {
           inTupleVars[inArrity] = itToAdd;
           inArrity++;
         }
       }
    }
  }

  if(notChanged) return str;

  string newTupleDecl = "["+inTupleVars[0];
  for(int i=1; i < inArrity; i++) newTupleDecl += "," + inTupleVars[i];
  newTupleDecl += "] -> ["+outTupleVars[0];
  for(int i=1; i < outArity; i++) newTupleDecl += "," + outTupleVars[i];
  parts.tupDecl = newTupleDecl + "]";
  
  newStr = getFullStrFromParts(parts);

  return newStr;
}

void setDependencesVal(std::vector<depRel> &dependences, int relNo, int rule, bool val){

  if(rule == Monotonicity)          dependences[relNo].mono = val;
  else if(rule == CoMonotonicity)   dependences[relNo].coMono = val;
  else if(rule == Triangularity)    dependences[relNo].tri = val;
  else if(rule == FuncConsistency)  dependences[relNo].combo = val;
}

string getPrettyComplexity(string comp){
  string pComp;
  if(comp == "O(0)")              pComp = "O(0)";
  else if(comp == "O(1)")         pComp = "O(1)";
  else if(comp == "O(n^1)")       pComp = "O(n)";
  else if(comp == "O(nnz^1)")     pComp = "O(nnz)";
  else if(comp == "O(nnz^2/n^1)") pComp = "O(nnz*(nnz/n))";
  else if(comp == "O(nnz^3/n^2)") pComp = "O(nnz*(nnz/n)^2)";
  else if(comp == "O(nnz^4/n^3)") pComp = "O(nnz*(nnz/n)^3)";
  else if(comp == "O(nnz^5/n^4)") pComp = "O(nnz*(nnz/n)^4)";
  else if(comp == "O(n^2)")       pComp = "O(n^2)";
  else if(comp == "O(n^1*nnz^1)") pComp = "O(n*nnz)";
  else if(comp == "O(nnz^2)")     pComp = "O(nnz^2)";
  else if(comp == "O(nnz^3/n^1)") pComp = "O((nnz^2)*(nnz/n))";
  else if(comp == "O(nnz^4/n^2)") pComp = "O((nnz^2)*(nnz/n)^2)";
  else if(comp == "O(nnz^5/n^3)") pComp = "O((nnz^2)*(nnz/n)^3)";
  else                            pComp = "NaN";
  return pComp;
}

void initComplexities(map<string,int> &complexities){
  complexities[string("O(0)")] = 0;
  complexities[string("O(1)")] = 0;
  complexities[string("O(n^1)")] = 0;
  complexities[string("O(nnz^1)")] = 0;
  complexities[string("O(nnz^2/n^1)")] = 0;
  complexities[string("O(nnz^3/n^2)")] = 0;
  complexities[string("O(nnz^4/n^3)")] = 0;
  complexities[string("O(nnz^5/n^4)")] = 0;
  complexities[string("O(n^2)")] = 0;
  complexities[string("O(n^1*nnz^1)")] = 0;
  complexities[string("O(nnz^2)")] = 0;
  complexities[string("O(nnz^3/n^1)")] = 0;
  complexities[string("O(nnz^2/n^2)")] = 0;
  complexities[string("O(nnz^5/n^3)")] = 0;
}


int compCompare(string comp1, string comp2){
  map<string,int> complexities;
  complexities[string("O(0)")] = 0;
  complexities[string("O(1)")] = 1;
  complexities[string("O(n^1)")] = 2;
  complexities[string("O(nnz^1)")] = 3;
  complexities[string("O(nnz^2/n^1)")] = 4;
  complexities[string("O(nnz^3/n^2)")] = 5;
  complexities[string("O(nnz^4/n^3)")] = 6;
  complexities[string("O(nnz^5/n^4)")] = 7;
  complexities[string("O(n^2)")] = 8;
  complexities[string("O(n^1*nnz^1)")] = 9;
  complexities[string("O(nnz^2)")] = 10;
  complexities[string("O(nnz^3/n^1)")] = 11;
  complexities[string("O(nnz^4/n^2)")] = 12;
  complexities[string("O(nnz^5/n^3)")] = 13;
  if( complexities[comp1] < complexities[comp2] )       return -1;
  else if( complexities[comp1] == complexities[comp2] ) return 0;
  else if( complexities[comp1] > complexities[comp2] )  return 1;

  return -1;
}

string giveCompWithOrd(int ord){
  string comp="";
  if(ord == 0) comp = "O(0)";
  else if(ord == 1) comp = "O(1)";
  else if(ord == 2) comp = "O(n^1)";
  else if(ord == 3) comp = "O(nnz^1)";
  else if(ord == 4) comp = "O(nnz^2/n^1)";
  else if(ord == 5) comp = "O(nnz^3/n^2)";
  else if(ord == 6) comp = "O(nnz^4/n^3)";
  else if(ord == 7) comp = "O(nnz^5/n^4)";
  else if(ord == 8) comp = "O(n^2)";
  else if(ord == 9) comp = "O(n^1*nnz^1)";
  else if(ord == 10) comp = "O(nnz^2)";
  else if(ord == 11) comp = "O(nnz^3/n^1)";
  else if(ord == 12) comp = "O(nnz^4/n^2)";
  else if(ord == 13) comp = "O(nnz^5/n^3)";
  else              comp = "NaN";
  return comp;  
}

void restComplexities(map<string,int> &complexities){
  for (map<string,int>::iterator it = complexities.begin(); it!=complexities.end(); it++)
    it->second=0;
}

void printComplexities(map<string,int> &complexities, string stage, ofstream &out){
  out<<"\n"<<stage;
  for (map<string,int>::iterator it = complexities.begin(); it!=complexities.end(); it++)
    out<<"  "<<it->second<<"  ";
}

string trimO(string str){
  return str.erase(0,1);
}

string int2str(int i){
  char buf[50];
  sprintf (buf, "%d",i);
  return string(buf);
}

