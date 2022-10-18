#!/bin/bash

REPS=${1:-100}
FILE=${2:-'database.txt'}

./mk_footprints.sh $REPS $FILE
./mk_pdf.sh $FILE