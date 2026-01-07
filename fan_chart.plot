set title "Distribution of scores over generations"
set xlabel "# of generations"
set ylabel "Score"
plot ARG1 using 1:12:20 with filledcurves lc rgb "grey50" title "10-90th range", \
     ARG1 using 1:16 with lines lc "red" lw 2 title "Median (50th)", \
     ARG1 using 1:30 with lines lc "green" lw 2 title "Best (0th %)"
set term qt

while (1) {
    replot      # Redraws the plot with new data from the file
    pause 20     # Wait for 1 second (adjust time as needed)
}
