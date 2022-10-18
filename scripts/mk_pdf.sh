#!/bin/bash

FILE=${1:-database.txt}

rm -f eval.pdf
python3 mk_latex.py $FILE | pdflatex -jobname=eval
rm -f eval.aux
rm -f eval.log