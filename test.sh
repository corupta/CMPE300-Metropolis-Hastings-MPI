#!/bin/bash

if [ "$#" -ne 6 ]; then
  if [ "$#" -ne 7 ]; then
    echo "Error: Illegal number of parameters, $#! Please run the program as:"
    echo "./test.sh <nof_processes (int)> <program_name without.c> <base_directory> <file name (without extension or _noisy)> <output_file> <beta (float)> <pi (float)> [grid/row (optional)]"
    exit 1
  fi
fi

# $1 is number of processes
# $2 is name of program (c file without .c)
# $3 is base_directory
# $4 is file name
# $5 is beta (float)
# $6 is pi (float)
# $7 is grid 

original_path="$3/$4.txt"
noisy_path="$3/$4_noisy.txt"
output_path="$3/$4_output.txt"
image_path="$3/$4_output.png"

mpicc -g $2.c -o $2 &&
mpiexec -n $1 $2 $noisy_path $output_path $5 $6 $7 &&
node input-output/test.js $original_path $noisy_path $output_path &&
python scripts/text_to_image.py $output_path $image_path
