The section "4.3 Performance Impact" in the paper, presents time results for fully parallelizing 2-loops in Incomplete cholesky. 
To reproduce results presented in Figugre 5 (a), you need to compile and run "test/IncompleteCholesky_m7.cc";
and to reproduce results presented in Figugre 5 (a), you need to compile and run "test/IncompleteCholesky_k.cc".
The instructions for compiling and running them are presented in their header. In general:

Inside test directory compile the code with:
```
g++ -o FILE FILE.cc -std=c++11 -fopenmp -O3
```
Run the code with (Inside same tesst directory):
```
./FILE matList.txt > result.txt
```

Please note: first line in matList.txt contains 3 numbers that are (in order): 
             "MaxNumber of Threads" "Number of Iterations" "Number of Runs (for getting average run time)"
              Subsequent lines in matList.txt should contain list of input matrices. 
              You need to download input matrices from:
              https://sparse.tamu.edu/ 
              And put them inside data directory.

After running the programs they print out detail time results that can be used to recreate figute 5. 
However, they do not create the figure.

