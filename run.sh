export PARLAY_NUM_THREADS=4
gcc main.cpp -lstdc++ -o main -fopenmp
./main