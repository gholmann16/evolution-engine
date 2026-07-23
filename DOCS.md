## Core architecture

Everything lives behind three abstract interfaces and one global mutable state, all defined in [`state.hpp`](state.hpp) and shared by every module (CLI, GUI, every engine/evolver/test).

## Contents

- [Program](#program) the structure used to transport data
- [Engines](#engines) — Brainfuck, JitFuck, Skipfuck, Network, Racehorse
- [Evolvers](#evolvers) — Squarelite, Above_Average, Top_10_Percent
- [Tests](#tests) — Crc8, Output, Tic_Off, TicTacToe, Add
- [Output files](#output-files) — `data.txt`, `graph.txt`
- [Other files in the tree](#other-files-in-the-tree) — `sucrisc/`, `loader.bf`, `main.c`, `tester.cpp`, `crcbf.h`, stray artifacts
- [Known quirks / rough edges](#known-quirks--rough-edges)

### `Program`

```cpp
struct Program { void * code; size_t score; };
```

Genomes are stored as programs that are type independent so as to be handled by the same evolvers and scorers.`code` is a type-erased genome — its real type depends entirely on which `Engine` is active (a `std::string*` for the brainfuck-family engines, a packed `Synapse[]` byte buffer for the neural engines — see [Engines](#engines)).
`score` is a **cost, lower is better**; `0` means "solved."

### `Engine` => the thing that runs the genome

Interfaces for interpretting different kinds of genomes, like brainfuck or neural network.
(`Brainfuck`, `JitFuck`, `Skipfuck`, `Network`, `Racehorse`):

```cpp
class Engine {
public:
    // Parse/JIT-compile a genome into whatever internal form `run()` needs.
    virtual void load(const void * code) = 0;
    /* 
        * input read only, output should be non freeable memory, too many allocs otherwise.
        * Return looptime/runtime, cost capped at `max`. What that cost actually counts differs per engine
        * (in future versions I'd like a sanity check like time or raw assembly ops)
        */
    virtual size_t run(char input[256], char output[256], size_t max) = 0;

    // Creates a default genome to start evolving from.
    virtual void * ancestor_prog() = 0;
    // Evolve `parent` into `child`. The lower the more aggressive
    virtual void evolve(const void * parent, void * child, size_t randomness) = 0;

    // Human-readable dump of a genome, should be an allocated string
    virtual std::string debug(const void * code) = 0;
    // Same as debug(), but with unreachable structure pruned
    virtual std::string clean_debug(const void * code) { return debug(code); }
    // Allocated string ready to be passed to assembler. Currently unused/unfinished in every engine
    virtual std::string compile(const void * code) = 0;
    // Possibily fuzzy equality check, used for hall-of-fame repeat detection and fitness pool diversity checks.
    virtual bool equal(const void * first, const void * second) = 0;
    // A size metric, used as a fitness tie-breaker.
    virtual size_t size(const void * code) = 0;
    // Wipe an allocated program and copy parent code into it.
    virtual void copy_into(const void * parent, void * child) = 0;
}
```

Engines are selected by name through a small factory registry in [`engines/engines.cpp`](engines/engines.cpp). Each worker thread has its own engine instance while `load()` fills the engine with JIT machine code, CSR synapse arrays, etc.

### `Test` => handles scoring

```cpp
class Test {
    // lower is better
    virtual size_t score(const void * code) const = 0;
    // explains more in depth how a given genome did, e.g. how many times did it do a perfect job
    virtual std::string display(const void * code) const { return ""; }
};
```

### `Evolver` => defines how selection works

```cpp
class Evolver {
    // Decide how to allocate children slots and how many genomes survive
    virtual void evolve() const = 0;
    // Runs through and scores the genomes
    virtual void score_all() const = 0;
    // Decides which genomes "win" in a given generation, processing both randomness and diversity
    virtual void sort() const = 0;
};
```

### The generation loop — `runner()` (`runner.cpp`)

`runner()` is the shared evolutionary loop the CLI calls directly and the
GUI re-implements one generation at a time via a GTK idle callback (see
[GUI](#gui-guiout)). Each generation we:

1. Reseed `rand()` from `State::seed + State::runs` (deterministic)
2. `evolver->evolve()`, `evolver->score_all()`, `evolver->sort()`
3. Append one line to `State::output` (`data.txt`): generation number plus 29 percentile-sampled scores of the sorted population
4. Overwrite `graph.txt` with the `engine->debug()` of the new champion
5. `cli_interpret()`: check the new champion against the hall of fame.
   - If it matches an existing hall-of-famer, `repetitions++`. This stops cycles and increases randomness until we see progress
   - Otherwise, add this champion to the hall of fame
   - Prints a one-line generation summary (score, size, wins, seed, timing).
   - Stops the run if `score == 0` (solved) or `repetitions >= def_rand` (local minimum)

`SIGINT` (Ctrl+C) is caught and finishes the *current* generation before exiting cleanly, rather than killing the process mid-generation.

## CLI (`monolith.out`)

```sh
./monolith.out --help
./monolith.out --list
./monolith.out -n Brainfuck -e Squarelite -t Output -v
./monolith.out --engine JitFuck --evolver Above_Average --test Crc8 -c 5000 -f 50 -r 300 -s 42 -o data.txt -m 200000
```

`--engine`/`--evolver`/`--test` take the exact (case-sensitive) names from `--list`, and are **mandatory** — the process exits with an error if any of the three is unset.

| Flag | Long form | Effect |
|---|---|---|
| `-h` | `--help` | General help |
| `-l` | `--list` | Prints available engines, evolvers, and tests. |
| `-n <name>` | `--engine <name>` | Select the engine (see [Engines](#engines)). |
| `-e <name>` | `--evolver <name>` | Select the evolver (see [Evolvers](#evolvers)). |
| `-t <name>` | `--test <name>` | Select the test (see [Tests](#tests)). |
| `-v` | `--verbose` | Timing and other information. |
| `-c <N>` | `--children <N>` | Population size (`State::total_creatures`, default 10000). |
| `-f <N>` | `--famers <N>` | Hall-of-fame size (`State::total_famers`, default 100). |
| `-r <N>` | `--randomness <N>` | Baseline mutation randomness / stagnation threshold (`State::def_rand`, default 250). Higher = calmer mutation and more tolerance for repeated generations before giving up. |
| `-s <N>` | `--seed <N>` | RNG seed. Random seed otherwise. |
| `-o <path>` | `--output <path>` | Path for the score log (default `data.txt`). |
| `-m <N>` | `--max-runtime <N>` | `Engine::run()` iteration cap (`State::max_runtime`, default 100000). |

Each flag that takes a value expects it as the *next* argv token (not in 
`--flag=value` form). Unrecognized arguments print a warning and are
otherwise ignored.

Using these options, the first generation is built using `ancestor_prog()` as well as a default hall of fame, and then the program begins the evolutionary cycle.

## Engines

An engine defines what a genome *is* (its representation) and how it's
executed.

### `Brainfuck` => classic interpreter

Genome: raw Brainfuck source (`+ - < > [ ] . ,`, plus a vestigial `0` "clear cell" instruction that evolution can never actually produce, since it's not in the mutable-character alphabet). Straightforward interpreter over a 65536-byte tape (uses `unsigned short` and wrapping) with matching 8-bit I/O cursors. `max_runtime` counts **loop-condition (`]`) evaluations only**. A genome with mismatched brackets is punished with a flat `max_runtime` cost rather than running.

### `JitFuck` => the same language, JIT-compiled

Implements the identical opcode set as `Brainfuck` (the name refers to the *implementation strategy*, not a different dialect). Compiles source directly to x86-64 machine code in an `mmap`'d executable buffer (classic single-pass JIT with backpatched jump offsets for `[`/`]`) for optimization which comes into play for longer genomes.

### `Skipfuck` => a bracket-free dialect

Genome alphabet: `+ - < > , .` plus a new instruction `*`, and **no** `[`/`]` at all. `*` reads the current cell as a signed byte and jumps forward/backward by that many *instructions*, relative to just after the `*`; an unstructured, data-addressed relative jump. Loops are built by arranging for some cell to hold a negative value at the right point. Because there's no bracket-matching requirement, **every string over its 7-character alphabet is a valid program**. That removes a whole class of mutations that would otherwise be wasted on syntactically dead brainfuck genomes, and turns "which instruction runs next" into a smooth, continuously-tunable numeric property instead of a brittle structural one which is theoretically friendlier to incremental mutation - but I have not experimentally seen too much difference. Also JIT-compiled to x86-64, via one shared jump trampoline rather
than per-loop backpatching. `max_runtime` counts `*` (jump) executions
only.

### `Network` — sparse spiking neural network

Genome: a flat array of `Synapse { unsigned short input, output; float multiplier; }` edges over a fixed pool of 8192 neurons: 2048 input neurons (one per bit of the 256-byte input buffer), 4096 worker neurons, and 2048 output neurons (one per bit of the 256-byte output buffer). There are no fixed layers; any neuron can connect to any other, subject to the input/output conventions above. Evaluation runs 30 fixed propagation rounds: each round, neuron potentials leak (decay toward zero), any neuron at/above threshold fires (pushing weighted charge onto its targets and resetting itself), and after all rounds the output neurons are bit-packed back into a 256-byte output buffer. `max_runtime` counts total synapse *firings* across all 30 rounds (and so in naturally way higher than the brainfuck interpreters). `ancestor_prog()` guarantees every input neuron starts with at least one random outgoing synapse, so evolution always has a gradient to discover an input's usefulness rather than starting from a totally dead input. `equal()` uses fuzzy comparison (≥90% of wiring — not weights — must match) for hall-of-fame/diversity purposes, since nearly every mutation nudges nearly every weight.

### `Racehorse` (in progress)

A `Network`/`Brain` variant intended (per its own comment) to mutate synaptic *weights* in live time as a brain develops a "memory". Not sure how to conicide with evolution, but maybe it just remembers round by round.

## Evolvers

An evolver decides, each generation, how many  and which of the sorted-by-score population survive as an unmodified "elite," and how the rest are regenerated. All three build on the shared thread logic in [`evolvers/standard.hpp`](evolvers/standard.hpp). Some tests cache a per-generation "answer" the first call, and so we score one genome before launching all 8 workers. Each worker gets its own cloned `Engine` instance via `thread_local` state, and writes only to its own slice of `State::children`, no locking needed thanks to 0 overlap.

### `Squarelite`

Elite size = `sqrt(total_creatures)`. After sorting, runs a de-duplication pass over the elite window: scans forward keeping only genomes that are fail the `Engine::equal()` test, so the elite pool can't accidentally fill up with clones of one lucky winner. If fewer than `sqrt(N)` distinct genomes exist, pads the remainder by copying the #1 champion.

### `Above_Average`

Elite size starts at `total_creatures / 2`, and shrinks further as `repetitions` (stagnation) grows, on top of `Standard::evolve`'s own mutation-rate escalation — a second, independent stagnation-escape lever. No de-duplication pass.

### `Top_10_Percent`

Elite size is a fixed `total_creatures / 10`, independent of `def_rand`/ `repetitions` — the simplest of the three, with neither adaptive shrinkage nor de-duplication.

## Tests

A test defines the fitness challenge a genome is scored against. Select
with `-t <name>` on the CLI. Score is always a cost: **lower is better**,
`0` means solved.

### `Crc8`

Evolve a program that computes an 8-bit CRC-8/DVB-S2 checksum. Each generation, 3 random 255-byte trial inputs (plus their reference CRC) are cached and reused across the whole population. Score = total runtime across all 3 trials, plus a 5× program-size parsimony penalty *iff* every trial's first output byte matches the expected CRC exactly. Any timeout or wrong byte on any trial scores a flat, harsh penalty (`max_runtime × 50`) instead. This was meant as an optimization exercise as it is clearly an unevolvable test.

### `Output`

Evolve a program that prints the literal string `"Hello World"`, regardless of input — 10 different random-noise input buffers are used per generation specifically so that hardcoding the output (rather than reading input) is the winning strategy. Score is a weighted sum of length mismatch and per-byte value differences across all 10 trials, plus a size penalty.

### `Tic_Off`

"1 v 1 tic-tac-toe," co-evolutionary: a candidate genome plays concrete games against every genome currently in the hall of fame, once as first player (X) and once as second (O). Score is a simple win/loss tally against the current hall-of-fame roster. 

### `TicTacToe`

A stricter, roster-independent variant: instead of playing concrete games, it recursively and exhaustively searches **every possible** opponent response line from both the first-move and second-move perspective, weighting a loss by how many possible games that branch represented. Score is `0` only if the candidate provably never loses on any line, playing either side. Because tic tac toe is such a simple game, this genuinely isn't even that expensive.

### `Add`

Evolve a program that computes the 8-bit sum of two random input bytes,across 10 trials per generation. Score is runtime plus a 5× size penalty plus 100 times the summed absolute byte-value error across trials.

## Output files

Both the CLI and the GUI write the same two artifacts (GUI writes only the
first):

### `data.txt` => per-generation score distribution

Overwritten (truncated) at the start of a run, then one line appended per
generation. Each line has **30 whitespace-separated columns**:

1. Generation number (`State::runs`)
2. through 30: the population's score at percentile ranks `100, 99, 98,
   ..., 91, 90, 80, 70, ..., 10, 9, 8, ..., 1, 0` (29 values), read off the
   already-sorted-ascending population. Column 2 is the *worst* score in
   the population (100th percentile); column 30 is the *best* (0th
   percentile, i.e. the champion); column 16 is the median.

Change the path with `-o/--output` on the CLI (GUI always uses `data.txt`).

### `graph.txt` => current champion dump (CLI only)

Overwritten every generation with `engine->debug(children[0].code)` — raw Brainfuck/Skipfuck source for those engine families, or a list of `{input, output, multiplier},` synapse triples for the neural engines. This is the expected input format for `brain_to_graphiz`.

## Known quirks / rough edges

- **`Engine::compile()`** is unfinished/unused
- **`Racehorse`** is completely unimplemented despite being present and visible.

### Other
There's also a deprecated `sucrisc/` engine (a tiny custom RISC ISA + a compiler to x86-64 assembly)
