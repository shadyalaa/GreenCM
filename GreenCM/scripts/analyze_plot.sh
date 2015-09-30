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
backend[14]="karma"
backend[15]="timestamp"
backend[16]="aggressive"
backend[17]="modsuicide"

benchmarks[1]="redblacktree"
benchmarks[2]="genomepp"
benchmarks[3]="intruderpp"
benchmarks[4]="kmeans"
benchmarks[5]="labyrinth"
benchmarks[6]="ssca2pp"
benchmarks[7]="vacationhpp"
benchmarks[8]="yadapp"
benchmarks[9]="array"
benchmarks[10]="bayes"
benchmarks[11]="genome"
benchmarks[12]="intruder"
benchmarks[13]="yada"
benchmarks[14]="intruderp"



for c in 1 2 5 8 14 15 16 17
do
        for b in 2 3 4 8
        do
		#for x in 16 12 8 4 0
		#do
		echo "collecting results for ${benchmarks[$b]}-${backend[$c]}"
		python $scriptsfolder/results_analyzer.py ${benchmarks[$b]}-${backend[$c]} $resultsfolder > $summaryfolder/${benchmarks[$b]}-${backend[$c]}
		#done
	done
done

for b in 2 3 4 8
do
	bash $scriptsfolder/plot.sh $plotsfolder ${benchmarks[$b]} $summaryfolder/${benchmarks[$b]}
	echo "collecting freq results for ${benchmarks[$b]}-${backend[$c]}"
	python $scriptsfolder/freqanalyzer.py $resultsfolder ${benchmarks[$b]} $summaryfolder
done


for t in 4 8 16 32 48 64
do
        for b in 2 3 4 8
        do
        	bash $scriptsfolder/plot_freq.sh $summaryfolder/${benchmarks[$b]}-$t $plotsfolder/${benchmarks[$b]}-${t}_freq.png
	done
done
