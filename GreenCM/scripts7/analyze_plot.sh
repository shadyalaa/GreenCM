#!/bin/sh

scriptsfolder=$1
resultsfolder=$2/runs
summaryfolder=$2/summary
plotsfolder=$2/plots

backend[1]="adpt"
backend[2]="spin"
backend[3]="sleep"
backend[4]="spin-pause"
backend[5]="asym-adpt"
backend[6]="asym-spin"
backend[7]="ddasym-adpt"
backend[8]="dasym-adpt"
backend[9]="suicide"
backend[10]="delay"
backend[11]="asym-sleep"
backend[12]="dasym-sleep"
backend[13]="ddasym-sleep"


benchmarks[1]="r-tf-mf"
benchmarks[2]="rw-tf-mf"
benchmarks[3]="w-tf-mf"
benchmarks[4]="r-tf-mt"
benchmarks[5]="rw-tf-mt"
benchmarks[6]="w-tf-mt"
benchmarks[7]="r-tt-mf"
benchmarks[8]="rw-tt-mf"
benchmarks[9]="w-tt-mf"
benchmarks[10]="r-tt-mt"
benchmarks[11]="rw-tt-mt"
benchmarks[12]="w-tt-mt"

for c in 1 5 8
do
        for b in 1 2 3 #7 8 9 10 11 12 #1 2 3 4 5 6
        do
		#for x in 16 12 8 4 0
		#do
		echo "collecting results for ${benchmarks[$b]}-${backend[$c]}"
		python $scriptsfolder/results_analyzer.py ${benchmarks[$b]}-${backend[$c]} $resultsfolder > $summaryfolder/${benchmarks[$b]}-${backend[$c]}
		#done
	done
done

for b in 1 2 3 #7 8 9 10 11 12 #1 2 3 4 5 6
do 
	bash $scriptsfolder/plot.sh $plotsfolder ${benchmarks[$b]} $summaryfolder/${benchmarks[$b]}
	echo "collecting freq results for ${benchmarks[$b]}-${backend[$c]}"
	python $scriptsfolder/freqanalyzer.py $resultsfolder ${benchmarks[$b]} $summaryfolder
done


for t in 32 48 64
do
        for b in 1 2 3 #7 8 9 10 11 12 #1 2 3 4 5 6
        do
        	bash $scriptsfolder/plot_freq.sh $summaryfolder/${benchmarks[$b]}-$t $plotsfolder/${benchmarks[$b]}-${t}_freq.png
	done
done
