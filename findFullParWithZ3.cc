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

#define NRL 8
#define NART 3
bool check_useRule[NRL][10]={0};
string checkRuleStr[NRL];
// The data structure that holds evaluation result for a dependence relation
typedef struct deprel{

  deprel(){
    fs = mono = coMono = tri = dr = combo = false;
    unsat = 0;
    origComplexity = simpComplexity = "";
    rel = NULL; simpRel = NULL;
  }

  Relation * rel;
  Relation * simpRel;
  int unsat;
  bool fs;    // Is it UnSat just based on linear inconsistency?
  bool mono;  // Is it MaySat after considering mopnotonicity domain information? 
  bool coMono;
  bool tri;
  bool dr;
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
string propName(int prop);
string b2s(bool cond){ if(cond){ return string("Yes");} return string("No");} 
void setCheck_useRule();
void genChillScript(json &analysisInfo);
std::vector<std::string> getUQR(int rlc, std::set<std::string> &UFSyms, 
                                std::set<std::string> &VarSyms, int &uqa_c);
// Decide a dependence relation with z3
bool decideWithZ3(json &z3Info, Relation *dep, int id,
                  std::set<std::string> glVarSyms,
                  std::set<std::string> UFSyms, std::vector<std::string> constrants, 
                  std::vector<std::string> properties, int uqa_c, 
                  int prop, ofstream &outRes);
void readLoop(string str, int &stNo, int &parLL, int &nRels);


//------------------------------------------ MAIN ----------------------------------------
int main(int argc, char **argv)
{
  setCheck_useRule();

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

  cout<<"\n\n\nProcessing "<<data[0][0]["Name"]<<":";
  z3_f_c = 0;

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

    // Read the dependences for a specific loop and store the unique ones for analysis
    i=0;
    for (ct=0; ct < nRels ; ct++){
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
    }
    nUniqueRels = i;

    unSatFound = 0;
    maySatFound = 0;
    json z3Info = data[0][0]["z3 info"];

    // Outfile for analysis result for a loop.
    string outFile;
    outFile = (data[0][0]["Result"].as<string>())+"_"+int2str(stNo)+"_"+int2str(parLL)+".txt";
    ofstream outRes(outFile.c_str(), std::ofstream::out);
    outRes<<"<<<<<<<<>>>>>>>> Loop: [StNo = "<<stNo<<", Level = "<<parLL<<", nRels = "<<nRels<<"]\n\n";

    // Use different combinations of domain information 
    for(int rlc = 0 ; rlc < NRL ; rlc++ ){

      outRes<<"\n------ Utilizing propertes: "<<checkRuleStr[rlc]<<"\n\n";

      bool sat = true;
      int unSatFound=0, maySatFound=0;
      if(nUniqueRels == 0) sat = false;

      int dc=0;
      for( i=0; i < nUniqueRels; i++){  // Loop over all unique relations

        // if the dependence is unsat just using functional consistency, do not use index array property
        if( dependences[i].unsat ) continue;   //.fs
        dc++;

        int uqa_c=1;
        std::set<std::string> UFSyms;
        std::set<std::string> VarSyms;
        std::vector<std::string> constrants = 
               dependences[i].rel->getZ3form(UFSyms, VarSyms);

        // Get z3 form of relevant universially quantified assertions
        std::vector<std::string> properties;
        if(rlc > 0) properties = getUQR(rlc, UFSyms, VarSyms, uqa_c);

        // Decide a dependence relation with z3
        sat = decideWithZ3(z3Info, dependences[i].rel, (z3_f_c++), 
                     VarSyms, UFSyms, constrants, properties, uqa_c, rlc, outRes);

        if( sat ){
//          setDependencesVal(dependences, i, rlc, false);
          maySatFound++;
        } else {
          setDependencesVal(dependences, i, (rlc+1), true);
          unSatFound++;
        }
      }

      if( unSatFound == dc ){
        outRes<<"\n\n>>>>>>>> Based on "<<checkRuleStr[rlc]<<" Loop: [StNo = "
              <<stNo<<", Level = "<<parLL<<"] is Fully parallel!\n";
      } else {
        outRes<<"\n\n>>>>>>>> Based on "<<checkRuleStr[rlc]<<" Loop: [StNo = "
              <<stNo<<", Level = "<<parLL<<"] is NOT Fully parallel!\n";
      }
    }
    outRes.close(); 
  }

  cout<<"\n\n\nProcessed "<<data[0][0]["Name"]<<":   Results were written to "
      <<(data[0][0]["Result"].as<string>()).c_str()<<"_StNo_LoopLevel.txt\n\n";
 } // End of input json file list loop

}
// ----------- End of driver function --------------------------------------




void readLoop(string str, int &stNo, int &parLL, int &nRels){
  sscanf (str.c_str(),"[First stmt = %d, Loop level = %d, No. of Rels = %d]",&stNo,&parLL,&nRels);
}

// Decide a dependence relation with z3
bool decideWithZ3(json &z3Info, Relation *dep, int id,
                  std::set<std::string> glVarSyms,
                  std::set<std::string> UFSyms, std::vector<std::string> constrants, 
                  std::vector<std::string> properties, int uqa_c, 
                  int prop, ofstream &outRes){
  // Generate UFSymbol definitions, as well as their domain and range definition
  std::vector<std::string> UFSymDef;
  for (std::set<std::string>::iterator it=UFSyms.begin(); it!=UFSyms.end(); it++){
    int nd = queryDomainArityCurrEnv(*it), nr = queryRangeArityCurrEnv(*it);
    string z3Str = "(declare-fun " + (*it) + " ( ";
    for(int i=0; i < nd; i++) z3Str += "Int ";
    z3Str += ")";
    for(int i=0; i < nr; i++) z3Str += " Int";
    z3Str += ") ";
    UFSymDef.push_back(z3Str);
    // Creating assertion for Domain and Range of the UF Symbol
    // We are gonna make a universially quantified rule out of 
    // Domain and range, and get its z3 form 
//    if(prop == DomainRange || prop == FuncConsistency){
      UniQuantRule *uqRule = getUQRForFuncDomainRange(*it);
      std::set<std::string> relUFSs; relUFSs.insert(*it);
      string drZ3Str = uqRule->getZ3Form(relUFSs, glVarSyms, uqa_c++);
      UFSymDef.push_back(drZ3Str);
      delete uqRule;
//    }
  }
  // Add any user defined extra symbolic constant definition
  json jExtraSyms = z3Info[0]["Extra Symbols"];
  for(int i=0; i < jExtraSyms.size() ; i++ ){
     glVarSyms.insert(jExtraSyms[i].as<string>());
  }
  // Generate the z3 input file for a dependence relation
  bool sat = true;
  string n_outF = z3Info[0]["Path"].as<string>() + "_" +int2str(id) + ".smt2";
  ofstream outf;
  outf.open( n_outF.c_str(), std::ofstream::out);
  // Setting up some options
  outf<<"(set-option :timeout "<<z3Info[0]["timeout"].as<string>()<<")\n";
  outf<<"(set-option :produce-unsat-cores true)\n";

  // Defining Global variables (symbolic constants)
  outf<<"\n\n; Defining Global variables:\n\n";
  for (std::set<std::string>::iterator it=glVarSyms.begin(); it!=glVarSyms.end(); it++)
    outf<<("(declare-const "+ *it + " Int)")<<"\n";
  // Defining UFSymbols, and their domain and range 
  outf<<"\n\n;  Defining UFSymbols, and their domain and range :\n\n";
  for(int i = 0 ; i < UFSymDef.size(); i++)
    outf<<UFSymDef[i]<<"\n";
  // Defining the constraints in the dependence relation (and tuple variables)
  outf<<"\n\n; Defining the constraints in the dependence relation (and tuple variables):\n\n";
  outf<<"; Dependence (IEGenLib Relation) = "<<dep->getString()<<"\n\n";
  for(int i = 0 ; i < constrants.size(); i++)
    outf<<constrants[i]<<"\n";
  // Defining universially quantified assertions 
  // relevant to UFCalls found in the dependence
  outf<<"\n\n; Defining universially quantified assertions "
               "relevant to UFCalls found in the dependence:\n\n";
  for(int i = 0 ; i < properties.size(); i++)
    outf<<properties[i]<<"\n";

  outf<<"\n\n(check-sat)\n";
  outf<<"\n\n(get-unsat-core)\n";
  outf.close();

  // Running z3 
  string ans,unsatcore;
  string z3Command = "./z3/build/z3 " + n_outF + "> data/tempData/z3ansF.txt" + " 2> /dev/null"; 
  int z3Err = system (z3Command.c_str());
  ifstream z3ansF("data/tempData/z3ansF.txt", std::ofstream::in);
  getline(z3ansF, ans);
//cout<<"ans = "<<ans<<"\n";
  getline(z3ansF, unsatcore);
//cout<<"UnSat core = "<<unsatcore<<"\n";
  z3ansF.close();

  if(ans == "unsat") sat = false;

  
  outRes<<"\n  Relation = "<<dep->getString()<<"\n     z3 file got written to "<<n_outF<<"\n";
  if(sat) outRes<<"   "<<ans<<"\n";
  else    outRes<<"   "<<ans<<"  <>  UnSat core = "<<unsatcore<<"\n";

  return sat;
}




std::vector<std::string> getUQR(int rlc, std::set<std::string> &UFSyms, 
                                std::set<std::string> &VarSyms, int &uqa_c){

  bool useRule[10]={0};
  for(int i=0;i<=NART;i++) useRule[i]=check_useRule[rlc][i];
//  if( r_it == FuncConsistency){// FuncConsistency signals we want to use all the rules
//    for(int j = 0 ; j < TheOthers ; j++ ) useRule[j] = true;
//  } else {
//    useRule[r_it] = true; 
//  }

  std::vector<std::string> uqrs;
  UniQuantRule* uqRule;
  int noAvalRules = queryNoUniQuantRules();

//cout<<"\nNo. of UQRs = "<<noAvalRules<<"\n";

  for(int i = 0 ; i < noAvalRules ; i++ ){
    // Query rule No. i from environment
    uqRule = queryUniQuantRuleEnv(i);
    // If we do not want to instantiate this rule move on to next one
    if( !(useRule[uqRule->getType()]) ) continue;

    string z3Str = uqRule->getZ3Form(UFSyms, VarSyms, uqa_c++);
    if( z3Str != "" ) uqrs.push_back(z3Str);
  }

  return uqrs;
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

void setDependencesVal(std::vector<depRel> &dependences, int relNo, int ruleCombo, bool val){

  dependences[relNo].unsat = ruleCombo; 
//  if(ruleCombo == 0)  dependences[relNo].fs = val;
/*
  if(rule == Monotonicity)          dependences[relNo].mono = val;
  else if(rule == CoMonotonicity)   dependences[relNo].coMono = val;
  else if(rule == Triangularity)    dependences[relNo].tri = val;
  else if(rule == DomainRange)    dependences[relNo].dr = val;
  else if(rule == FuncConsistency)  dependences[relNo].combo = val;
*/
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


string propName(int prop){
  string str;

  if(prop == Monotonicity)          str = "Monotonicity";
  else if(prop == CoMonotonicity)   str = "Periodic Monotonicity";
  else if(prop == Triangularity)    str = "Triangularity";
  else if(prop == DomainRange)      str = "Domain and Range";
  else if(prop == -1)               str = "Functional Consistency";
  else if(prop == FuncConsistency)  str = "All properties";

  return str;
}



void setCheck_useRule(){

  //check_useRule[0]={0};   //{0,0,0,0,0,0,0,0,0,0,0};
  check_useRule[1][0]=true; //{1,0,0,0,0,0,0,0,0,0,0};
  check_useRule[2][1]=true; //{0,1,0,0,0,0,0,0,0,0,0};
  check_useRule[3][2]=true; //{0,0,1,0,0,0,0,0,0,0,0};
  check_useRule[4][0]=check_useRule[4][1]=true; //{1,1,0,0,0,0,0,0,0,0,0};
  check_useRule[5][0]=check_useRule[5][2]=true; //{1,0,1,0,0,0,0,0,0,0,0};
  check_useRule[6][1]=check_useRule[6][2]=true; //{0,1,1,0,0,0,0,0,0,0,0};
  check_useRule[7][0]=check_useRule[7][1]=check_useRule[7][2]=true; //{1,1,1,0,0,0,0,0,0,0,0};
  checkRuleStr[0]="[Only Functional Consistency]";
  checkRuleStr[1]="[Monotonicity]";
  checkRuleStr[2]="[Periodic Monotonicity]";
  checkRuleStr[3]="[Triangularity]";
  checkRuleStr[4]="[Monotonicity,Periodic Monotonicity]";
  checkRuleStr[5]="[Monotonicity,Triangularity]";
  checkRuleStr[6]="[Periodic Monotonicity,Triangularity]";
  checkRuleStr[7]="[Monotonicity,Periodic Monotonicity,Triangularity]";

}
