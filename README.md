# Evolution Engine — a plug and play genetic-programming platform

A C++20 platform for evolving programs (and small neural networks) against fitness tests. The `Engine` is a module which runs a genome which can be in one of the several formats documented below; each generation, a population of genomes is mutated, scored against a `Test`, and culled by an `Evolver` strategy. Ships as a CLI (`monolith.out`) and a GTK4/libadwaita GUI (`gui.out`) that share the same core functionality.

The system is fully pluggable across three independent axes:

- **Engine** — what a genome *is* and how it's executed (`engines/`)
- **Evolver** — how one generation becomes the next (`evolvers/`)
- **Test** — what problem a genome is being scored against (`tests/`)

You pick one of each to start a run. Reproduction is entirely asexual, with the current generation's elite massively overrepresented in each successive gene pool.

All tests currently feed the networks a lot of noisy data in my recent experiments with using these genetic algorithms to clean up data, and so the results will vary, but the nice part is this allows you to play with the programs as you will see in the gui section.

## Documentation

For information on the api that I've constructed and how the program generally works, check out the [Docs](DOCS.md).

## Building

Requires a C++20 compiler (g++), and for the GUI, GTK4 + libadwaita
development. (Arch: `gtk4`,`libadwaita`; Debian/Ubuntu: `libgtk-4-dev`, `libadwaita-1-dev`).

```sh
make            # builds monolith.out, the default target
make gui.out    # builds the GUI (needs gtk4 + libadwaita-1 via pkg-config)
make clean      # removes *.o and *.out
```

Both binaries link the same four object files (`engines.o`, `tests.o`,
`evolvers.o`, plus `cli.o` or `gui.o`), built with `-O3 -march=native -flto -std=c++20 -pthread` so that generations can be sub second instead of sub minute. Each `.o` is a packages together all of that axis, for example engines.o compiles in all engines using includes.

## Visualization tools

### gnuplot fan charts

To run these charts use:
```sh
gnuplot -c fan_chart.plot data.txt      # best/median/10-90th band over time
gnuplot -c time_score.plot data.txt     # every one of the 29 percentile lines individually
```

- **`fan_chart.plot`**: plots a grey filled band between columns 12 and 20
  (90th/10th percentile), a red line for column 16 (median), and a green
  line for column 30 (best score) — the same visual the GUI's built-in
  chart draws.
- **`time_score.plot`**: plots all 29 percentile columns as individual
  labeled lines, useful for watching every percentile band's trajectory
  rather than just the best/median/worst summary.

## GUI (`gui.out`)

A GTK4 + libadwaita front end with a lot of cool tools to poke these generated programs. Includes a custom Cairo-drawn fan chart over the trailing 300 generations — a grey filled band between the 10th and 90th percentile, a red median line, and a green best-score line — the same visual as `fan_chart.plot`,

![Demonstration of gui highlighting the current elites (best performing programs of a given generation), the former champions, as well as a chart at the bottom](<gui.png>)


**Interactive brain demos** — clicking any row in the Top 10 or Hall of Fame list pushes a detail page for that specific genome:

- **Brainfuck/Skipfuck stepping interpreter** — the source is shown with the current instruction pointer highlighted and auto-scrolled into view, alongside a live 16×16 hex dump of memory centered on the tape pointer. Step one instruction at a time, or Play at a watchable pace. An input box lets you type hex bytes that feed the interpreter's `,` (read) instructions live so you can talk to [`Output`](DOCS.md#output) or attempt to [`Add`](DOCS.md#add) numbers

![Visual displaying the interactive brainfuck interpreter](<input.png>)

- **Neural node view** — for the neural engines, a cleaned version of the already Sparse Neural Network is shown.Each neuron is a circle colored by how close its charge is to firing threshold. Synapses are drawn dim in the background  and bright on top for whichever ones actually fired that round — green for excitatory weights, red for inhibitory, with thickness scaled by weight magnitude. Step/Play re-run the network's actual propagation for whatever input you type in, round by round.

![Demo of the neural graphs you can play with and see in-app](<neural.png>)

- **Tic-Tac-Toe playground** — for non input based tests, a grid is shown alongside either detail view that lets you play with the compiled genome in live time.This is a "see how it plays" sandbox, not a faithful replay of that test's training-time scoring convention, which subjects it to a much larger battery of tests

![Picture showing the computer thinking of a response to a user's play in tictactoe](<game.png>)

There is currently no save/load or export UI, instead, we use seeds if you want to replicate behavior on multiple computers.

### Graphviz synapse graphs

[`brain_to_graphiz.c`](brain_to_graphiz.c) converts a neural engine's
`graph.txt` dump into Graphviz edge statements — one line per synapse,
colored green for a positive weight / red for negative, with edge
thickness (`penwidth`) proportional to `|weight|`.

```sh
gcc -O2 -o brain_to_graphiz brain_to_graphiz.c -lm
./monolith.out -n Network -e Squarelite -t Output   # let it run a bit, then Ctrl+C
./brain_to_graphiz graph.txt
```

You can paste the output into an online or terminal based graphiz generator such as [this Graphviz web editor](https://dreampuf.github.io/GraphvizOnline/). In the past, I was able to evolve a neural network that solved tictactoe down to 4 losing games using 146 nodes, the results are shown below:

![Graph displaying a neural network generated by Evolution Engine nearly succeeding at solving tictactoe](<1200 + 146.svg>)
