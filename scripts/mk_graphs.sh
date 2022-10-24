#!/bin/bash

touch graphs/Fine.txt
touch graphs/Lazy.txt
touch graphs/VY-DCAS.txt
touch graphs/VY-CAS.txt
touch graphs/ORVYY.txt
touch graphs/Michael.txt
touch graphs/MichaelWF.txt
touch graphs/Harris.txt
touch graphs/HarrisWF.txt
touch graphs/FEMRS.txt

./plankton programs/Fine.txt -f graphs/Fine.txt
./plankton programs/Lazy.txt -f graphs/Lazy.txt
./plankton programs/VY-DCAS.txt -f graphs/VY-DCAS.txt
./plankton programs/VY-CAS.txt -f graphs/VY-CAS.txt
./plankton programs/ORVYY.txt -f graphs/ORVYY.txt
./plankton programs/Michael.txt -f graphs/Michael.txt
./plankton programs/MichaelWF.txt -f graphs/MichaelWF.txt
./plankton programs/Harris.txt -f graphs/Harris.txt
./plankton programs/HarrisWF.txt -f graphs/HarrisWF.txt
./plankton programs/FEMRS.txt --loopNoPostJoin -f graphs/FEMRS.txt