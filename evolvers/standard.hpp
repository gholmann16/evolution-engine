#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace Standard {
    void evolve(int mod) {
        // Keep the winner around so you never regress (agamogenesis) and reset
        for (size_t creature = mod; creature < State::total_creatures; creature++)
            State::engine->evolve(State::children[creature % mod].code, State::children[creature].code, clamped_sub(State::def_rand, State::repetitions));
    }

    // Scoring is the expensive part of a generation (every new child runs
    // against the test, up to the test's own runtime cap) and, unlike
    // evolve(), touches no shared RNG stream -- Test::score() is a pure
    // function of the code blob plus whatever State::engine/State::comp it's
    // given. That makes it safe to fan out across worker threads without
    // disturbing the seed -> run determinism the rest of the codebase leans
    // on. Each worker gets its own clone_engine() of the current engine/comp
    // (load() mutates per-instance buffers -- CSR arrays, JIT'd machine code
    // -- that can't be shared across threads) via thread_local State::engine/comp.
    constexpr int WORKER_THREADS = 8;

    class ScorePool {
        private:
            std::mutex mtx;
            std::condition_variable cv_go, cv_done;
            std::vector<std::thread> workers;
            std::vector<Engine *> engine_clones;
            std::vector<Engine *> comp_clones;
            // Keyed by factory_id, not by Engine*: State::engine/comp get
            // delete'd and rebuilt on every GUI "Start" click (see
            // on_start_clicked), and a fresh allocation landing at the exact
            // address just freed is common, not exotic -- a raw-pointer
            // comparison here would then skip re-cloning and hand workers
            // stale clones of whatever engine type used to live at that
            // address. factory_id is stable proof of "same concrete type",
            // regardless of which instance or address it came from.
            int cloned_engine_factory_id = -1;
            int cloned_comp_factory_id = -1;
            bool have_comp_clones = false;

            size_t range_begin[WORKER_THREADS] = {0};
            size_t range_end[WORKER_THREADS] = {0};
            uint64_t generation = 0;
            int pending = 0;
            bool shutting_down = false;

            void worker_loop(int id) {
                std::unique_lock<std::mutex> lock(mtx);
                uint64_t seen = 0;
                while (true) {
                    cv_go.wait(lock, [&] { return shutting_down || generation != seen; });
                    if (shutting_down) return;
                    seen = generation;

                    size_t b = range_begin[id], e = range_end[id];
                    Engine * my_engine = engine_clones[id];
                    Engine * my_comp = comp_clones[id];
                    lock.unlock();

                    State::engine = my_engine;
                    State::comp = my_comp;
                    for (size_t i = b; i < e; i++)
                        State::children[i].score = State::test->score(State::children[i].code);

                    lock.lock();
                    if (--pending == 0)
                        cv_done.notify_one();
                }
            }

        public:
            ScorePool() {
                engine_clones.resize(WORKER_THREADS, nullptr);
                comp_clones.resize(WORKER_THREADS, nullptr);
                for (int i = 0; i < WORKER_THREADS; i++)
                    workers.emplace_back([this, i] { worker_loop(i); });
            }

            ~ScorePool() {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    shutting_down = true;
                }
                cv_go.notify_all();
                for (auto & t : workers)
                    t.join();
                for (auto * e : engine_clones) delete e;
                for (auto * e : comp_clones) delete e;
            }

            // Scores children[begin..total). Blocks until every worker is
            // done -- a synchronous barrier, same as the old serial loop from
            // the caller's point of view, just faster.
            void run(size_t begin, size_t total) {
                if (begin >= total) return;

                std::unique_lock<std::mutex> lock(mtx);

                Engine * real_engine = State::engine;
                Engine * real_comp = State::comp;
                if (cloned_engine_factory_id != real_engine->factory_id) {
                    for (auto * e : engine_clones) delete e;
                    for (int i = 0; i < WORKER_THREADS; i++)
                        engine_clones[i] = clone_engine(real_engine);
                    cloned_engine_factory_id = real_engine->factory_id;
                }
                bool want_comp_clones = real_comp != nullptr;
                int real_comp_factory_id = want_comp_clones ? real_comp->factory_id : -1;
                if (have_comp_clones != want_comp_clones || cloned_comp_factory_id != real_comp_factory_id) {
                    for (auto * e : comp_clones) delete e;
                    for (int i = 0; i < WORKER_THREADS; i++)
                        comp_clones[i] = want_comp_clones ? clone_engine(real_comp) : nullptr;
                    cloned_comp_factory_id = real_comp_factory_id;
                    have_comp_clones = want_comp_clones;
                }

                size_t span = total - begin;
                size_t chunk = (span + WORKER_THREADS - 1) / WORKER_THREADS;
                for (int i = 0; i < WORKER_THREADS; i++) {
                    range_begin[i] = begin + std::min(span, (size_t)i * chunk);
                    range_end[i] = begin + std::min(span, (size_t)(i + 1) * chunk);
                }

                pending = WORKER_THREADS;
                generation++;
                cv_go.notify_all();
                cv_done.wait(lock, [&] { return pending == 0; });
            }
    };

    inline ScorePool score_pool;

    void score_all(int mod) {
        if ((size_t)mod >= State::total_creatures)
            return;

        // Serial warm-up: some tests (Add, Crc8) cache a per-generation
        // "answer" in a shared static, rebuilt the first time score() sees a
        // new State::runs. Scoring one child on the calling thread first
        // forces that refresh to happen exactly once, before the worker
        // threads start reading those buffers concurrently -- otherwise two
        // threads could race rebuilding them (Crc8 even free()s and
        // re-mallocs, so a race there is a real double-free hazard, not just
        // stale data).
        State::children[mod].score = State::test->score(State::children[mod].code);
        if (State::verbose)
            printf("%zu\t", State::children[mod].score);

        score_pool.run(mod + 1, State::total_creatures);

        if (State::verbose)
            for (size_t i = mod + 1; i < State::total_creatures; i++)
                printf("%zu\t", State::children[i].score);
    }
}

// Declared in state.hpp so cli.cpp/gui.cpp can score the initial ancestor
// population without needing to know Standard::score_pool exists. Same
// serial-first-item pattern as Standard::score_all() above, for the same
// reason: forces Add/Crc8's per-generation cached-answer rebuild to happen
// once before worker threads could race rebuilding it concurrently.
void parallel_score(size_t begin, size_t total) {
    if (begin >= total) return;
    State::children[begin].score = State::test->score(State::children[begin].code);
    Standard::score_pool.run(begin + 1, total);
}
