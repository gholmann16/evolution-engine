set title "My Line Graph"      # Sets the main title
set xlabel "X Axis Label"      # Sets the x-axis label
set ylabel "Y Axis Label"      # Sets the y-axis label
f(x) = a*x**2 + b*x + c
fit f(x) "data.txt" via a, b, c
plot "data.txt" using 1:2 with points title "Data Series 1", \
    f(x) with lines title "Trendline"
set term qt
