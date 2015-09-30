#!/bin/sh



gnuplot << EOF


reset
set terminal png
set title "Average core frequency"
set key invert reverse Left outside
set yrange [0:100]
set ylabel "% of total"
unset ytics
set grid ytics
set border 3
set style data histograms
set style histogram rowstacked
set style fill solid border -1
set boxwidth 0.75
set output '$2'
#
plot '$1' using (100.*\$3/\$2):xticlabels(1) t column(3), for [i=4:9] '' using (100.*column(i)/column(2)) title column(i) 
#
#plot 'freq.dat' every::1 using 2:xtic(1), for [i=3:8] '' using i
EOF
