#!/bin/bash

#You can run this file if you want many runs of the code
#You may prefer reducing the usleep time in master.c in this case

for ((i=1; i<=10; i++)); do
    echo "Running Makefile iteration $i"
    cat input.txt | make all
done