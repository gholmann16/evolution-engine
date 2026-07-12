#include <runner.hpp>
#include <time.h>
#include <string.h>
#include <ranges>
#include <cstdint>

constexpr uint32_t hash(const char * str) {
    uint32_t hash = 0x811c9dc5;
    while (*str != 0) {
        hash ^= *str;
        hash *= 0x01000193;
        str++;
    }
    return hash;
}

void help() {
    puts("Genetic-based algorithm platform");
    puts("Usage");
    puts("monolith.out [Options]");

    puts("Options:");
    puts("-h, --help\t\tPrints out this useful menu.");
    puts("-l, --list\t\tLists out the engines, evolvers, and tests you may select.");
    puts("-n, --engine\tSets the tpye of genetic algorithm, or engine, to use.");
    puts("-e, --evolver\tSets the evolver, aka the algorithm which chooses the direction of evolution.");
    puts("-t, --test\tSets the test the genetic algorithm is meant to overcome.");
    puts("-v, --verbose\tPrints out more information than normally.");
    puts("-c, --children\tSets the number of \"children\" per generation, or genetic algorithms.");
    puts("-f, --famers\tSets the number of \"Hall of Famers\", or old programs that new ones compare to for repetition checks, and fighting for some tests.");
    puts("-r, --randomness\tSets the default randomness value for evolution (higher is less random, but also more allowed repeat generations)");
    puts("-s, --seed\t\tSets the seed for a generation. Only used for debug purposes.");
    puts("-o, --output\t\tSets the output file for the span of score data.");
}

void list() {
    puts("Available Engines:");
    for (int i = 0; i < num_engines; i++)
        puts(engine_names[i]);

    puts("");
    puts("Available Evolvers:");
    for (int i = 0; i < num_evolvers; i++)
        puts(evolver_names[i]);

    puts("");
    puts("Available Tests");
    for (int i = 0; i < num_tests; i++)
        puts(test_names[i]);
}

int pos(const char ** map, char * name, int lim) {
    for (int i = 0; i < lim; i++) {
        if (strcmp(map[i], name) == 0)
            return i;
    }

    printf("Couldn't find %s\n", name);
    exit(-1);
}

int main(int argc, char * argv[]) {
    if (argc < 2) {
        help();
        return 0;
    }

    for (int i = 1; i < argc; i++)
        switch(hash(argv[i])) {
            case hash("-h"):
            case hash("--help"):
                help();
                return 0;
            case hash("-l"):
            case hash("--list"):
                list();
                return 0;
            case hash("-n"):
            case hash("--engine"):
                State::engine = engines[pos(engine_names, argv[++i], num_engines)];
                State::comp = competitors[pos(engine_names, argv[i], num_engines)];
                break;
            case hash("-e"):
            case hash("--evolver"):
                State::evolver = evolvers[pos(evolver_names, argv[++i], num_evolvers)];
                break;
            case hash("-t"):
            case hash("--test"):
                State::test = tests[pos(test_names, argv[++i], num_tests)];
                break;
            case hash("-v"):
            case hash("--verbose"):
                State::verbose = true;
                break;
            case hash("-c"):
            case hash("--children"):
                sscanf(argv[++i], "%zu", &State::total_creatures);
                break;
            case hash("-f"):
            case hash("--famers"):
                sscanf(argv[++i], "%zu", &State::total_famers);
                break;
            case hash("-r"):
            case hash("--randomness"):
                sscanf(argv[++i], "%zu", &State::def_rand);
                break;
            case hash("-s"):
            case hash("--seed"):
                sscanf(argv[++i], "%zu", &State::seed);
                break;
            case hash("-o"):
            case hash("--output"):
                State::output = argv[++i];
                break;
            default:
                printf("%s not a valid argument, maybe you forgot to preface it?\n", argv[i]);
                break;
        }

    if (State::engine == nullptr || State::evolver == nullptr || State::test == nullptr) {
        puts("Engine, evolver, and test must all be set these are mandatory arguments");
        return -1;
    }
    else if (!State::seed)
        State::seed = time(NULL);

    State::children = new Program[State::total_creatures];
    State::hall_of_fame = (void**)malloc(State::total_famers * sizeof(void *));

    for (size_t famer = 0; famer < State::total_famers; famer++)
        State::hall_of_fame[famer] = State::engine->ancestor_prog();

    for(size_t ancestor = 0; ancestor < State::total_creatures; ancestor++) {
        State::children[ancestor].code = State::engine->ancestor_prog();
        State::children[ancestor].score = State::test->score(State::children[ancestor].code);
    }

    runner();

    for (size_t ancestor = 0; ancestor < State::total_creatures; ancestor++) {
        delete static_cast<std::string*>(State::children[ancestor].code);
    }
    delete[] State::children;
    free(State::hall_of_fame);

    return 0;
}
