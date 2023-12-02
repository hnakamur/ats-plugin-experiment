#!/bin/bash -x
set -e # exit on error

DIR=${@:-.}

target_files=$(find $DIR -iname \*.[ch] -o -iname \*.cc -o -iname \*.h.in)
for file in ${target_files}; do
  ${FORMAT} -i $file
done
