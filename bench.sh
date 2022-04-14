#!/bin/sh

ITER=5

mkdir -p build/out

DoBenchmark () {
	echo ""
	echo "Starting $1 ($2)"
	rm -f build/out/$1-$2.txt
	# build/bin/plankton examples/$1.pl >> build/out/$1-$2.txt
	/usr/bin/time -f " >> %es" build/bin/plankton examples/$1.pl >> build/out/$1-$2.txt
}

for i in $(seq 1 $ITER)
do
	# DoBenchmark "CoarseSet" $i
	# DoBenchmark "OptimisticSet" $i

	# DoBenchmark "FineSet" $i
	# DoBenchmark "LazySet" $i
	# DoBenchmark "VechevYahavDCas" $i
	# DoBenchmark "VechevYahavCas" $i
	# DoBenchmark "ORVYY" $i
	# DoBenchmark "Michael" $i
	DoBenchmark "MichaelWaitFreeFind" $i
	# DoBenchmark "Harris" $i
	# DoBenchmark "HarrisNoK" $i
	# DoBenchmark "HarrisWaitFreeFindNoMark" $i

	# DoBenchmark "BinaryTreeNoMaintenance" $i
done
