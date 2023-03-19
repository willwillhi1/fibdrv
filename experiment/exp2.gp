reset
set ylabel 'time(ns)'
set title 'runtime'
set key left top
set term png enhanced font 'Verdana,10'
set output 'exp2.png'
plot [1:1000][:] \
'experiment/exp2_plot_input' using 1:2 with linespoints linewidth 2 title "fib v1",\
'' using 1:3 with linespoints linewidth 2 title "fib v0",\