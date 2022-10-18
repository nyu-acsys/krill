#!/bin/bash

REPS=${1:-100}
FILE=${2:-database.txt}

rm -f $FILE
touch $FILE

./krill -o $FILE -r $REPS graphs/Fine.txt
./krill -o $FILE -r $REPS graphs/Lazy.txt
./krill -o $FILE -r $REPS graphs/VY-DCAS.txt
./krill -o $FILE -r $REPS graphs/VY-CAS.txt
./krill -o $FILE -r $REPS graphs/ORVYY.txt
./krill -o $FILE -r $REPS graphs/Michael.txt
./krill -o $FILE -r $REPS graphs/MichaelWF.txt
./krill -o $FILE -r $REPS graphs/Harris.txt
./krill -o $FILE -r $REPS graphs/HarrisWF.txt
./krill -o $FILE -r $REPS graphs/FEMRS.txt