// GTK4 + libadwaita front end for the evolver. Reuses the same State /
// Engine / Test / Evolver registries as cli.cpp -- this is just another
// driver on top of the existing architecture, not a parallel one.
#include <gtk/gtk.h>
#include <adwaita.h>
#include <cairo.h>
#include <state.hpp>
#include <engines/neural_based/brain.hpp>
#include <tests/tictactoe_common.hpp>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <cctype>
#include <thread>
#include <atomic>

// ---- GUI-only state (kept out of the State namespace on purpose: State is
// the shared contract with the engines/tests/evolvers, this is just widgets
// and bookkeeping for them) ----------------------------------------------

static GtkWidget *main_window;
static GtkWidget *nav_view;   // AdwNavigationView: root page + pushed detail views
static GtkWidget *engine_dropdown, *evolver_dropdown, *test_dropdown;
static GtkWidget *seed_spin, *children_spin, *famers_spin, *randomness_spin;
static GtkWidget *max_runtime_scale, *max_runtime_value_label;
static GtkWidget *memory_estimate_label;
static GtkWidget *start_button, *play_button;
static GtkWidget *top10_list, *fame_list;
static GtkWidget *status_label;
static GtkWidget *code_title_label, *code_view;
static GtkWidget *chart_area;
static GtkWidget *detail_stack;   // AdwViewStack switching between "code" and "chart"

// One row per generation, mirroring runner.cpp's percentile columns (100th,
// 99th, 98th .. 1st, 0th -- 29 values). Kept in memory so the chart redraws
// instantly without re-parsing the data.txt file the GUI also writes (same
// on-disk format as the CLI, for parity between GUI and CLI runs).
static std::vector<std::vector<size_t>> score_history;
static std::ofstream history_file;   // kept open for the whole run instead of reopened every generation
static const int PERCENTILE_STEPS[] = {
    100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 80, 70, 60, 50,
    40, 30, 20, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
};

static bool state_ready = false;
static bool sim_running = false;
static guint idle_id = 0;

// One generation's evolve()+score_all()+sort() runs on this background
// thread instead of directly on the idle callback -- Tic_Off/TicTacToe's
// self-play against the whole hall of fame is far pricier per genome than
// the other tests (up to ~18x more Engine::run() calls), and running that
// synchronously on the GTK main thread meant the entire app -- not just
// evolution -- was unresponsive for however long one generation took: GTK
// couldn't even dispatch a Pause click until the callback returned. The
// idle callback now just polls gen_ready instead of computing directly, so
// the main loop keeps pumping events (Pause, list browsing, detail views)
// while a generation is in flight. gen_thread must be joined (see
// wait_for_generation()) before anything touches State::children/
// hall_of_fame from the main thread, since this thread reads/writes them.
static std::thread gen_thread;
static std::atomic<bool> gen_ready{false};
static bool gen_in_flight = false;

// Blocks until any in-flight background generation finishes. Only called
// from do_start() and on_window_destroy() -- both are about to free/replace
// State::children/hall_of_fame, which would race with the background
// thread if it were still running. Pause itself never calls this: it just
// stops scheduling new generations, so the button stays instant even if one
// happens to be mid-flight (that computation quietly finishes on its own
// and its result is picked up -- or discarded -- next time something polls
// gen_in_flight again).
static void wait_for_generation() {
    if (!gen_in_flight) return;
    gen_thread.join();
    gen_in_flight = false;
    gen_ready = false;
}

// Chunked async population init -- building 10000+ ancestors and scoring
// them synchronously on the button-click handler is what caused the "Start
// freezes the app" symptom (GTK never got the main loop back until the
// whole population was built). Spreading it across idle ticks keeps the UI
// alive and shows progress instead.
static bool initializing = false;
static size_t init_total = 0;
static size_t init_done = 0;
static const size_t INIT_CHUNK = 100;

// Parallel to State::hall_of_fame: the score each inducted champion had at
// the moment it was inducted (hall_of_fame itself only stores code blobs).
// SIZE_MAX marks a slot still holding its startup ancestor_prog() filler.
static std::vector<size_t> fame_scores;

static const char ** null_terminate(const char * const * names, int count) {
    const char ** out = new const char*[count + 1];
    for (int i = 0; i < count; i++) out[i] = names[i];
    out[count] = nullptr;
    return out;
}

static std::string sanitize(const std::string &s) {
    std::string out;
    for (unsigned char c : s) {
        if (c >= 32 && c < 127)
            out += (char)c;
        else {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\x%02X", c);
            out += buf;
        }
    }
    if (out.empty())
        out = "(empty)";
    return out;
}

// Parses whatever hex-digit characters appear in `text` into `out[256]`,
// zero-padding the rest -- ignores everything else (spaces, separators); a
// stray trailing nibble from an odd count of digits is just dropped rather
// than guessed at. Shared by the Brainfuck walkthrough's and neural node
// view's input boxes.
static void parse_hex_input(const char *text, char out[256]) {
    std::fill(out, out + 256, 0);
    size_t out_pos = 0;
    int high_nibble = -1;
    for (const char *p = text; *p && out_pos < 256; p++) {
        if (!isxdigit((unsigned char)*p)) continue;
        int v = isdigit((unsigned char)*p) ? *p - '0' : (tolower((unsigned char)*p) - 'a' + 10);
        if (high_nibble < 0) {
            high_nibble = v;
        } else {
            out[out_pos++] = (char)((high_nibble << 4) | v);
            high_nibble = -1;
        }
    }
}

static void open_detail_view(const std::string &code_snapshot);
static size_t runtime_from_tick(double tick);
static size_t estimated_population_bytes(int engine_idx, size_t children, size_t famers);
static size_t available_memory_bytes();

// Rows carry just a (list, index) pair -- not a cloned genome -- so clicking
// is the only time we ever pay for a snapshot. This used to clone every
// visible row's code on every single generation (10 + up to total_famers
// heap copies, generation after generation, whether or not anything was
// ever clicked), which was the main cost behind the GUI slowing down over a
// run. The lookup happens on the GTK main thread with no evolve step
// in flight (row-activated only fires between idle ticks), so indexing the
// live arrays here is safe and always current.
static void on_row_activated(GtkListBox*, GtkListBoxRow *row, gpointer) {
    if (!state_ready) return;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "idx"));
    bool fame = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "fame")) != 0;
    const void *code = fame ? State::hall_of_fame[idx] : State::children[idx].code;
    open_detail_view(*static_cast<const std::string*>(code));
}

static GtkWidget * make_row(const std::string &header, const std::string &body, int idx, bool fame) {
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(row_box, 8);
    gtk_widget_set_margin_end(row_box, 8);
    gtk_widget_set_margin_top(row_box, 6);
    gtk_widget_set_margin_bottom(row_box, 6);

    GtkWidget *header_label = gtk_label_new(header.c_str());
    gtk_label_set_xalign(GTK_LABEL(header_label), 0.0f);
    gtk_widget_add_css_class(header_label, "heading");
    gtk_box_append(GTK_BOX(row_box), header_label);

    if (!body.empty()) {
        GtkWidget *body_label = gtk_label_new(body.c_str());
        gtk_label_set_xalign(GTK_LABEL(body_label), 0.0f);
        // One line only -- bounded_row_body() already truncates the text
        // itself, this just guarantees it regardless (e.g. a body that's
        // short in characters but happens to be wide for the row).
        gtk_label_set_wrap(GTK_LABEL(body_label), FALSE);
        gtk_label_set_ellipsize(GTK_LABEL(body_label), PANGO_ELLIPSIZE_END);
        gtk_widget_add_css_class(body_label, "monospace");
        gtk_box_append(GTK_BOX(row_box), body_label);
    }

    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
    g_object_set_data(G_OBJECT(row), "idx", GINT_TO_POINTER(idx));
    g_object_set_data(G_OBJECT(row), "fame", GINT_TO_POINTER(fame ? 1 : 0));

    return row;
}

// The raw program: brainfuck source for the BF-family engines, the pruned
// synapse wiring ("code graph") for the neural engine -- whatever
// Engine::clean_debug() considers this genome's source representation.
static void refresh_code_panel() {
    if (!state_ready) return;
    std::string code_header = "Champion code — generation " + std::to_string(State::runs) +
        "   score " + std::to_string(State::children[0].score) +
        "   size " + std::to_string(State::engine->size(State::children[0].code));
    gtk_label_set_text(GTK_LABEL(code_title_label), code_header.c_str());
    std::string code_text = State::engine->clean_debug(State::children[0].code);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(code_view)), code_text.c_str(), -1);
}

// Catches up the code panel the moment the user switches back to it, since
// refresh_lists() skips recomputing it while the Progress tab is showing.
static void on_detail_stack_page_changed(GObject*, GParamSpec*, gpointer) {
    const char *visible_page = adw_view_stack_get_visible_child_name(ADW_VIEW_STACK(detail_stack));
    if (visible_page && strcmp(visible_page, "code") == 0)
        refresh_code_panel();
}

// Row bodies show a one-line teaser of the genome's raw debug() dump --
// full detail belongs in the champion code panel (refresh_code_panel(),
// a real GtkTextView meant for exactly that), not in a list of up to 110
// rows re-rendered every generation. Also caps what reaches Pango: a
// neural genome's dump is one line per synapse, up to ANCESTOR_SYNAPSES =
// 16,384 of them, and sanitize() escapes every real '\n' into a literal
// "\x0A" -- so an untruncated dump becomes one giant unbroken ~500KB
// line. Confirmed via perf while this was reportedly "stuck": 94.83% of
// main-thread samples were inside libpango's text layout, reached through
// gtk_widget_allocate, not anywhere near scoring or memory allocation.
constexpr size_t MAX_ROW_BODY_CHARS = 80;
static std::string bounded_row_body(const std::string &raw) {
    // sanitize() only ever grows text (each byte maps to 1-4 output
    // chars), so a MAX_ROW_BODY_CHARS*4-byte raw prefix is always enough
    // to cover MAX_ROW_BODY_CHARS sanitized chars -- lets this truncate
    // (and skip sanitizing) a ~500KB genome dump without ever touching
    // more than a few hundred bytes of it.
    std::string prefix = raw.substr(0, std::min(raw.size(), MAX_ROW_BODY_CHARS * 4));
    std::string clean = sanitize(prefix);
    bool truncated = prefix.size() < raw.size() || clean.size() > MAX_ROW_BODY_CHARS;
    if (clean.size() > MAX_ROW_BODY_CHARS)
        clean.resize(MAX_ROW_BODY_CHARS);
    return truncated ? clean + "..." : clean;
}

static void refresh_lists() {
    if (!state_ready) return;

    gtk_list_box_remove_all(GTK_LIST_BOX(top10_list));
    gtk_list_box_remove_all(GTK_LIST_BOX(fame_list));

    size_t shown = std::min<size_t>(10, State::total_creatures);
    for (size_t i = 0; i < shown; i++) {
        std::string header = "#" + std::to_string(i + 1) +
            "   score " + std::to_string(State::children[i].score) +
            "   size " + std::to_string(State::engine->size(State::children[i].code)) +
            "   " + State::test->display(State::children[i].code);
        // The code itself, in place of what used to be display()'s text --
        // the win/correct count is more useful up in the header now that
        // every test reports one, and watching the raw genome grow across
        // generations is its own kind of interesting even when it's not
        // exactly readable. Plain debug(), not clean_debug(): the clean
        // variant walks the whole synapse list to a fixed point for the
        // neural engine (see reachable_from_outputs() in brain.hpp) and is
        // meant for one genome on demand, not up to 110 of them every
        // generation.
        std::string body = bounded_row_body(State::engine->debug(State::children[i].code));
        gtk_list_box_append(GTK_LIST_BOX(top10_list), make_row(header, body, (int)i, false));
    }

    for (size_t i = 0; i < State::total_famers; i++) {
        bool claimed = i < fame_scores.size() && fame_scores[i] != SIZE_MAX;
        std::string header = claimed
            ? ("score " + std::to_string(fame_scores[i]) +
               "   size " + std::to_string(State::engine->size(State::hall_of_fame[i])) +
               "   " + State::test->display(State::hall_of_fame[i]))
            : "(unclaimed startup ancestor)   size " + std::to_string(State::engine->size(State::hall_of_fame[i]));
        std::string body = bounded_row_body(State::engine->debug(State::hall_of_fame[i]));
        gtk_list_box_append(GTK_LIST_BOX(fame_list), make_row(header, body, (int)i, true));
    }

    std::string status = "Generation " + std::to_string(State::runs) +
        "   best score " + std::to_string(State::children[0].score) +
        "   stagnant for " + std::to_string(State::repetitions) + " generations" +
        (sim_running ? "   (running)" : "   (paused)");
    gtk_label_set_text(GTK_LABEL(status_label), status.c_str());

    // clean_debug() walks the whole synapse list to a fixed point (see
    // brain.hpp) -- worth doing once a generation, not worth doing when the
    // Code tab isn't even the one on screen. refresh_code_panel() re-runs
    // whenever the user actually switches to it (see the stack's
    // notify::visible-child-name handler in activate()).
    const char *visible_page = detail_stack ? adw_view_stack_get_visible_child_name(ADW_VIEW_STACK(detail_stack)) : nullptr;
    if (!visible_page || strcmp(visible_page, "code") == 0)
        refresh_code_panel();

    gtk_widget_queue_draw(chart_area);
}

// Same percentile slice runner.cpp's runner() logs to data.txt, kept
// in-memory for the live chart and appended to State::output so a GUI run
// leaves the same on-disk trail a CLI run would (fan_chart.plot etc. still
// work against it).
static void record_generation() {
    std::vector<size_t> row;
    row.reserve(std::size(PERCENTILE_STEPS));
    for (int p : PERCENTILE_STEPS) {
        size_t idx = (size_t)p * (State::total_creatures - 1) / 100;
        row.push_back(State::children[idx].score);
    }
    score_history.push_back(row);

    history_file << State::runs;
    for (size_t v : row)
        history_file << " " << v;
    history_file << "\n" << std::flush;
}

// Same visual paradigm as fan_chart.plot: a grey 10th-90th percentile band,
// a red median line, a green best-score line -- just drawn live from
// score_history instead of gnuplot polling data.txt off disk.
// How many trailing generations the chart plots. Scaling the y-axis to the
// whole run's min/max meant one bad early generation (or one big early
// improvement) pinned the axis forever -- every later generation, however
// much it actually varied, flattened out near one edge. A trailing window
// keeps the axis meaningful as the run progresses, at the cost of not
// seeing the long-run trend in the same view.
static constexpr size_t CHART_WINDOW = 300;

static void draw_chart(GtkDrawingArea*, cairo_t *cr, int width, int height, gpointer) {
    if (score_history.size() < 2) {
        cairo_set_source_rgba(cr, 0.55, 0.55, 0.55, 0.9);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 13);
        cairo_move_to(cr, 12, height / 2.0);
        cairo_show_text(cr, "Not enough generations yet -- press Play.");
        return;
    }

    const size_t n_total = score_history.size();
    const size_t start = (n_total > CHART_WINDOW) ? n_total - CHART_WINDOW : 0;
    const size_t n = n_total - start;

    // Axis range tracks the 90th percentile (the shaded band's own top
    // edge) rather than the absolute worst (100th percentile, index 0).
    // The single worst genome in a generation can sit on a near-constant
    // penalty score for a long stretch even as the band/median/best all
    // improve -- anchoring the axis to it made the chart "way zoomed
    // out", with all the actual movement squeezed into a sliver at the
    // bottom. Nothing currently draws the 100th percentile as its own
    // line anyway, so excluding it from the range costs nothing visually.
    size_t max_val = 0, min_val = SIZE_MAX;
    for (size_t i = start; i < n_total; i++) {
        max_val = std::max(max_val, score_history[i][10]);   // 90th percentile
        min_val = std::min(min_val, score_history[i][28]);   // 0th percentile (best)
    }
    if (max_val == min_val) max_val = min_val + 1;

    const double pad_l = 68, pad_r = 12, pad_t = 12, pad_b = 22;
    const double plot_w = width - pad_l - pad_r;
    const double plot_h = height - pad_t - pad_b;
    if (plot_w <= 1 || plot_h <= 1) return;

    auto xf = [&](size_t i) { return pad_l + plot_w * (double)i / (double)(n - 1); };
    auto yf = [&](size_t v) { return pad_t + plot_h * (1.0 - (double)(v - min_val) / (double)(max_val - min_val)); };
    auto row = [&](size_t i) -> const std::vector<size_t>& { return score_history[start + i]; };

    // A handful of horizontal gridlines with their actual values, instead of
    // just the top and bottom of the range.
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11);
    constexpr int GRID_LINES = 4;
    char buf[64];
    for (int g = 0; g <= GRID_LINES; g++) {
        double frac = (double)g / GRID_LINES;
        double y = pad_t + plot_h * frac;
        size_t val = max_val - (size_t)(frac * (double)(max_val - min_val));

        cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.15);
        cairo_move_to(cr, pad_l, y);
        cairo_line_to(cr, width - pad_r, y);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, 0.55, 0.55, 0.55, 0.9);
        snprintf(buf, sizeof(buf), "%zu", val);
        cairo_move_to(cr, 2, y + 4);
        cairo_show_text(cr, buf);
    }

    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.35);
    cairo_move_to(cr, xf(0), yf(row(0)[10]));
    for (size_t i = 1; i < n; i++) cairo_line_to(cr, xf(i), yf(row(i)[10]));
    for (size_t i = n; i-- > 0; ) cairo_line_to(cr, xf(i), yf(row(i)[18]));
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_set_line_width(cr, 2.0);
    cairo_set_source_rgb(cr, 0.85, 0.15, 0.15);
    cairo_move_to(cr, xf(0), yf(row(0)[14]));
    for (size_t i = 1; i < n; i++) cairo_line_to(cr, xf(i), yf(row(i)[14]));
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.15, 0.65, 0.25);
    cairo_move_to(cr, xf(0), yf(row(0)[28]));
    for (size_t i = 1; i < n; i++) cairo_line_to(cr, xf(i), yf(row(i)[28]));
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, 0.55, 0.55, 0.55, 0.9);
    snprintf(buf, sizeof(buf), "gen %zu..%zu", start + 1, n_total);
    cairo_move_to(cr, pad_l, height - 6);
    cairo_show_text(cr, buf);

    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.6);
    cairo_rectangle(cr, pad_l + 6, pad_t + 4, 10, 10);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.85, 0.15, 0.15);
    cairo_rectangle(cr, pad_l + 96, pad_t + 4, 10, 10);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.15, 0.65, 0.25);
    cairo_rectangle(cr, pad_l + 172, pad_t + 4, 10, 10);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.55, 0.55, 0.55, 0.9);
    cairo_move_to(cr, pad_l + 20, pad_t + 13);
    cairo_show_text(cr, "10-90th");
    cairo_move_to(cr, pad_l + 110, pad_t + 13);
    cairo_show_text(cr, "median");
    cairo_move_to(cr, pad_l + 186, pad_t + 13);
    cairo_show_text(cr, "best");
}

static void free_state() {
    if (!state_ready) return;

    for (size_t i = 0; i < State::total_creatures; i++)
        delete static_cast<std::string*>(State::children[i].code);
    delete[] State::children;
    State::children = nullptr;

    for (size_t i = 0; i < State::total_famers; i++)
        delete static_cast<std::string*>(State::hall_of_fame[i]);
    free(State::hall_of_fame);
    State::hall_of_fame = nullptr;

    fame_scores.clear();
    state_ready = false;
}

static void set_running(bool run) {
    sim_running = run;
    gtk_button_set_label(GTK_BUTTON(play_button), run ? "⏸ Pause" : "▶ Play");
    if (run && idle_id == 0) {
        idle_id = g_idle_add([](gpointer) -> gboolean {
            if (!sim_running) return G_SOURCE_REMOVE;

            if (gen_in_flight) {
                if (!gen_ready.load()) return G_SOURCE_CONTINUE;
                gen_thread.join();
                gen_in_flight = false;
                gen_ready = false;
            } else {
                // State::engine/comp are thread_local (see state.hpp) so
                // that ScorePool's persistent workers each keep their own
                // clone -- which means this brand-new thread starts with
                // both null and needs its own copy of whatever the main
                // thread's pointers currently are before calling anything
                // that dereferences them.
                Engine *e = State::engine, *c = State::comp;
                gen_in_flight = true;
                gen_thread = std::thread([e, c] {
                    State::engine = e;
                    State::comp = c;
                    srand(State::seed + State::runs);
                    State::evolver->evolve();
                    State::evolver->score_all();
                    State::evolver->sort();
                    gen_ready = true;
                });
                return G_SOURCE_CONTINUE;
            }

            // Mirrors cli_interpret()'s hall-of-fame bookkeeping in runner.cpp,
            // minus the file/console logging (the GUI shows this live instead).
            bool repeat = false;
            for (size_t i = 0; i < State::total_famers; i++) {
                if (State::engine->equal(State::children[0].code, State::hall_of_fame[i])) {
                    repeat = true;
                    break;
                }
            }
            if (repeat) {
                State::repetitions++;
            } else if (State::total_famers) {
                size_t last = State::total_famers - 1;
                void *save = State::hall_of_fame[last];
                std::memmove(State::hall_of_fame + 1, State::hall_of_fame, last * sizeof(void*));
                State::hall_of_fame[0] = save;
                State::engine->copy_into(State::children[0].code, State::hall_of_fame[0]);

                fame_scores.pop_back();
                fame_scores.insert(fame_scores.begin(), State::children[0].score);

                State::repetitions = 0;
            }
            State::runs++;

            record_generation();
            refresh_lists();

            // Same give-up condition as cli_interpret() in runner.cpp: once
            // this many consecutive stagnant generations pass, the
            // randomness passed to evolve() (State::def_rand minus how long
            // it's been stuck) would bottom out -- a local minimum this
            // stuck is a legitimate outcome, not something to spin on
            // forever. Unlike the CLI, the GUI had no such check at all, so
            // a long-stalled run would keep going until repetitions caught
            // up to def_rand exactly and evolve() hit a live SIGFPE.
            const char *stop_reason = nullptr;
            if (State::children[0].score == 0)
                stop_reason = "Solved!";
            else if (State::repetitions >= State::def_rand)
                stop_reason = "Gave up -- stuck at a local minimum.";

            if (stop_reason) {
                sim_running = false;
                idle_id = 0;
                gtk_button_set_label(GTK_BUTTON(play_button), "▶ Play");
                std::string status = std::string(stop_reason) +
                    "   Generation " + std::to_string(State::runs) +
                    "   best score " + std::to_string(State::children[0].score);
                gtk_label_set_text(GTK_LABEL(status_label), status.c_str());
                return G_SOURCE_REMOVE;
            }

            return sim_running ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
        }, nullptr);
    } else if (!run && idle_id != 0) {
        g_source_remove(idle_id);
        idle_id = 0;
    }
}

// One chunk of ancestor construction: the first total_famers units fill the
// hall of fame, the remaining total_creatures units fill children. Spread
// across idle ticks so the main loop keeps breathing (see INIT_CHUNK note).
static gboolean init_step(gpointer) {
    if (!initializing) return G_SOURCE_REMOVE;

    size_t target = std::min(init_total, init_done + INIT_CHUNK);
    // score_from/score_to track this chunk's slice of State::children[] (if
    // any) so it can be scored as one parallel_score() call instead of one
    // genome at a time -- for the neural engine, a single score() call is
    // expensive enough that serial scoring made a 10,000-genome population
    // take minutes instead of seconds.
    size_t score_from = State::total_creatures, score_to = 0;
    for (; init_done < target; init_done++) {
        if (init_done < State::total_famers) {
            State::hall_of_fame[init_done] = State::engine->ancestor_prog();
        } else {
            size_t ci = init_done - State::total_famers;
            State::children[ci].code = State::engine->ancestor_prog();
            score_from = std::min(score_from, ci);
            score_to = ci + 1;
        }
    }
    if (score_to > score_from)
        parallel_score(score_from, score_to);

    std::string status = "Initializing population... " + std::to_string(init_done) +
        " / " + std::to_string(init_total);
    gtk_label_set_text(GTK_LABEL(status_label), status.c_str());

    if (init_done < init_total)
        return G_SOURCE_CONTINUE;

    initializing = false;
    state_ready = true;
    gtk_widget_set_sensitive(play_button, TRUE);
    gtk_widget_set_sensitive(start_button, TRUE);
    refresh_lists();
    return G_SOURCE_REMOVE;
}

static void do_start(guint eidx, guint vidx, guint tidx) {
    set_running(false);
    wait_for_generation();
    free_state();

    // Previous run's engine/comp are never referenced again once we're past
    // this point (NodeView etc. copy out the data they need up front), so
    // free them here instead of leaking a pair on every Start click.
    delete State::engine;
    delete State::comp;
    State::engine = make_engine(eidx);
    State::comp = make_engine(eidx);
    State::evolver = evolvers[vidx];
    State::test = tests[tidx];

    State::total_creatures = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(children_spin));
    State::total_famers = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(famers_spin));
    State::def_rand = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(randomness_spin));
    State::max_runtime = runtime_from_tick(gtk_range_get_value(GTK_RANGE(max_runtime_scale)));
    size_t seed_val = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(seed_spin));
    State::seed = seed_val ? seed_val : (size_t)time(nullptr);
    // Write the actual seed back into the spinner -- if the user left it at
    // 0 (random), this is the only place they'd ever see which seed a good
    // run actually used, so they can reproduce it later.
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(seed_spin), (double)State::seed);
    State::repetitions = 0;
    State::runs = 0;

    score_history.clear();
    history_file.close();
    history_file.open(State::output, std::ofstream::out | std::ofstream::trunc);

    // Deliberately NOT calling srand() here: cli.cpp doesn't either, ancestor
    // generation runs on whatever the process's rand() stream currently is.
    // Matching that keeps GUI runs comparable to CLI runs.
    // Value-initialized/calloc'd so every not-yet-built slot's code pointer
    // is null -- free_state() can then safely run mid-init (delete on a null
    // pointer is a no-op) if the window closes before init finishes.
    State::children = new Program[State::total_creatures]();
    State::hall_of_fame = (void**)calloc(State::total_famers, sizeof(void*));
    fame_scores.assign(State::total_famers, SIZE_MAX);

    initializing = true;
    init_total = State::total_famers + State::total_creatures;
    init_done = 0;
    gtk_widget_set_sensitive(start_button, FALSE);
    gtk_widget_set_sensitive(play_button, FALSE);
    g_idle_add(init_step, nullptr);
}

static void on_start_confirm_response(GObject *dialog, GAsyncResult *result, gpointer user_data) {
    guint packed = GPOINTER_TO_UINT(user_data);
    int button = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(dialog), result, nullptr);
    if (button == 1) // "Start Anyway"
        do_start(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF);
}

static void on_start_clicked(GtkButton*, gpointer) {
    if (initializing) return;

    guint eidx = gtk_drop_down_get_selected(GTK_DROP_DOWN(engine_dropdown));
    guint vidx = gtk_drop_down_get_selected(GTK_DROP_DOWN(evolver_dropdown));
    guint tidx = gtk_drop_down_get_selected(GTK_DROP_DOWN(test_dropdown));
    size_t children = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(children_spin));
    size_t famers = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(famers_spin));

    size_t estimate = estimated_population_bytes((int)eidx, children, famers);
    size_t available = available_memory_bytes();

    // Warn instead of refusing outright -- MemAvailable is a moment-in-time
    // number (other apps, reclaimable cache, and swap all move it), so a
    // hard block would sometimes be wrong in either direction. This is what
    // used to just silently stall/OOM-kill partway through population init
    // with no explanation at all.
    if (available > 0 && estimate > available * 7 / 10) {
        char msg[256];
        snprintf(msg, sizeof(msg),
            "This population needs about %.2f GB, but only about %.2f GB is currently available. "
            "It may stall or be killed by the system out-of-memory handler partway through init. "
            "Consider lowering Children/Hall of Fame size, or closing other apps first.",
            estimate / (1024.0 * 1024 * 1024), available / (1024.0 * 1024 * 1024));
        GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", msg);
        const char *buttons[] = {"Cancel", "Start Anyway", nullptr};
        gtk_alert_dialog_set_buttons(dialog, buttons);
        gtk_alert_dialog_set_cancel_button(dialog, 0);
        gtk_alert_dialog_set_default_button(dialog, 0);
        guint packed = eidx | (vidx << 8) | (tidx << 16);
        gtk_alert_dialog_choose(dialog, GTK_WINDOW(main_window), nullptr, on_start_confirm_response, GUINT_TO_POINTER(packed));
        g_object_unref(dialog);
        return;
    }

    do_start(eidx, vidx, tidx);
}

static void on_play_clicked(GtkButton*, gpointer) {
    if (!state_ready) return;
    set_running(!sim_running);
}

static void on_window_destroy(GtkWidget*, gpointer) {
    set_running(false);
    wait_for_generation();
    initializing = false;
    free_state();
}

static GtkWidget * labeled_column(const char *label, GtkWidget *widget) {
    GtkWidget *col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_add_css_class(lbl, "caption");
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_box_append(GTK_BOX(col), lbl);
    gtk_box_append(GTK_BOX(col), widget);
    return col;
}

// The max-runtime slider is a tick count, not the raw iteration cap -- each
// tick is a 4x multiple of the last, so one widget comfortably spans what
// used to be five different hardcoded per-test constants (100K to 1e9)
// without needing a 9-figure spin button.
static size_t runtime_from_tick(double tick) {
    return (size_t)(100.0 * std::pow(4.0, tick));
}

static void on_max_runtime_changed(GtkRange *range, gpointer) {
    size_t v = runtime_from_tick(gtk_range_get_value(range));
    gtk_label_set_text(GTK_LABEL(max_runtime_value_label), (std::to_string(v) + " iterations").c_str());
}

// A fresh ancestor genome's size for the given engine type -- a neural
// genome starts at ANCESTOR_SYNAPSES * sizeof(Synapse) (128 KB); a
// Brainfuck-family one starts as the 1-byte string "+" (see
// Brainfuck_Base::ancestor_prog()). That ~131,000x gap never showed up
// anywhere in the UI, so a population size that's perfectly fine for
// Brainfuck can quietly ask for gigabytes on Network. Built from a
// throwaway engine instance rather than calling ancestor_prog() itself,
// so this can't perturb the shared rand() stream that evolution's own
// determinism depends on (see on_start_clicked's comment on that).
static size_t estimated_bytes_per_genome(int engine_idx) {
    Engine * probe = make_engine(engine_idx);
    size_t bytes = dynamic_cast<Brain*>(probe) ? (size_t)ANCESTOR_SYNAPSES * sizeof(Synapse) : 1;
    delete probe;
    return bytes;
}

static size_t estimated_population_bytes(int engine_idx, size_t children, size_t famers) {
    return (children + famers) * estimated_bytes_per_genome(engine_idx);
}

static void update_memory_estimate() {
    guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(engine_dropdown));
    size_t children = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(children_spin));
    size_t famers = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(famers_spin));
    size_t total_bytes = estimated_population_bytes((int)idx, children, famers);

    char buf[64];
    if (total_bytes >= 1024ull * 1024 * 1024)
        snprintf(buf, sizeof(buf), "≈ %.2f GB genomes", total_bytes / (1024.0 * 1024 * 1024));
    else if (total_bytes >= 1024 * 1024)
        snprintf(buf, sizeof(buf), "≈ %.1f MB genomes", total_bytes / (1024.0 * 1024));
    else
        snprintf(buf, sizeof(buf), "≈ %.1f KB genomes", total_bytes / 1024.0);
    gtk_label_set_text(GTK_LABEL(memory_estimate_label), buf);
}

static void on_memory_estimate_engine_changed(GObject*, GParamSpec*, gpointer) {
    update_memory_estimate();
}

static void on_memory_estimate_spin_changed(GtkSpinButton*, gpointer) {
    update_memory_estimate();
}

// /proc/meminfo's MemAvailable -- the kernel's own estimate of how much can
// be allocated without swapping (unlike MemFree, it accounts for reclaimable
// buff/cache), which is exactly the number that decides whether a big
// population build sails through or starts thrashing. Returns 0 if it can't
// be read (missing/non-Linux), which callers treat as "unknown, don't block."
static size_t available_memory_bytes() {
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.rfind("MemAvailable:", 0) == 0) {
            size_t kb = 0;
            if (sscanf(line.c_str(), "MemAvailable: %zu kB", &kb) == 1)
                return kb * 1024ull;
            break;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------
// Brainfuck interpreter detail view. A from-scratch stepping interpreter
// (not the engine's own run()) so we can pause after every instruction and
// show a highlighted PC plus a live memory dump. Covers both BF-family
// dialects: bracket-matched loops for Brainfuck/JitFuck, and Skipfuck's
// offset-jump dialect -- Skipfuck's assembler emits one fixed-size (4-byte)
// machine-code chunk per source character, so its '*' jump is just "move by
// this many characters," directly translatable without touching x86.
// ---------------------------------------------------------------------
struct BFView {
    std::string source;
    bool skip_dialect = false;
    bool invalid = false;
    std::vector<long> match;   // bracket dialect only: match[i] = matching bracket index

    std::vector<unsigned char> memory = std::vector<unsigned char>(65536, 0);
    unsigned short pointer = 0;
    unsigned char in_dex = 0, out_dex = 0;
    std::string output;
    size_t pc = 0;
    size_t steps = 0;
    bool halted = false;

    // Same 256-byte input Engine::run() would get, editable live -- ','
    // reads from it below instead of a hardcoded zero, so stepping through
    // actually shows what this program does with the input you give it.
    char input[256] = {0};

    GtkWidget *source_view = nullptr;
    GtkTextTag *pc_tag = nullptr;
    GtkWidget *memory_view = nullptr;
    GtkWidget *output_label = nullptr;
    GtkWidget *status_label = nullptr;
    GtkWidget *play_button = nullptr;
    GtkWidget *controls = nullptr;   // Step/Play/Reset row, locked while a gameplay move is pending
    guint timeout_id = 0;

    // Fires once when a gameplay-triggered playback (see
    // ttt_apply_trace_to_bf_view) reaches halt, then gets cleared -- manual
    // Step/Play/replay afterward never re-trigger it.
    void (*on_playback_done)(gpointer) = nullptr;
    gpointer on_playback_done_data = nullptr;
};

static void bf_build_matches(BFView &v) {
    v.match.assign(v.source.size(), -1);
    std::vector<long> stack;
    for (size_t i = 0; i < v.source.size(); i++) {
        if (v.source[i] == '[')
            stack.push_back((long)i);
        else if (v.source[i] == ']') {
            if (stack.empty()) { v.invalid = true; return; }
            long open = stack.back();
            stack.pop_back();
            v.match[open] = (long)i;
            v.match[i] = open;
        }
    }
    if (!stack.empty())
        v.invalid = true;
}

static void bf_reset(BFView &v) {
    std::fill(v.memory.begin(), v.memory.end(), 0);
    v.pointer = 0;
    v.in_dex = 0;
    v.out_dex = 0;
    v.output.clear();
    v.pc = 0;
    v.steps = 0;
    v.halted = v.invalid;
}

static void bf_step(BFView &v) {
    if (v.halted) return;
    if (v.pc >= v.source.size()) { v.halted = true; return; }

    char c = v.source[v.pc];
    long next_pc = (long)v.pc + 1;
    switch (c) {
        case '>': v.pointer++; break;
        case '<': v.pointer--; break;
        case '+': v.memory[v.pointer]++; break;
        case '-': v.memory[v.pointer]--; break;
        case ',': v.memory[v.pointer] = v.input[v.in_dex++]; break;
        case '.': v.output += (char)v.memory[v.pointer]; v.out_dex++; break;
        case '0': v.memory[v.pointer] = 0; break;
        case '[':
            if (!v.skip_dialect && v.memory[v.pointer] == 0)
                next_pc = v.match[v.pc] + 1;
            break;
        case ']':
            if (!v.skip_dialect && v.memory[v.pointer] != 0)
                next_pc = v.match[v.pc] + 1;
            break;
        case '*':
            if (v.skip_dialect)
                next_pc = (long)(v.pc + 1) + (signed char)v.memory[v.pointer];
            break;
        default: break;
    }

    v.steps++;
    if (next_pc < 0 || next_pc >= (long)v.source.size() || v.steps > 300000) {
        v.pc = v.source.size();
        v.halted = true;
    } else {
        v.pc = (size_t)next_pc;
    }
}

static std::string bf_hex_dump(const BFView &v) {
    constexpr int ROWS = 16, COLS = 16;
    int start_row = (int)(v.pointer / COLS) - ROWS / 2;
    start_row = std::max(0, std::min(start_row, 65536 / COLS - ROWS));

    std::string out;
    char buf[16];
    for (int r = 0; r < ROWS; r++) {
        int addr = (start_row + r) * COLS;
        snprintf(buf, sizeof(buf), "%05X: ", addr);
        out += buf;
        std::string ascii;
        for (int c = 0; c < COLS; c++) {
            int idx = addr + c;
            bool cur = (idx == v.pointer);
            snprintf(buf, sizeof(buf), cur ? "[%02X]" : " %02X ", v.memory[idx]);
            out += buf;
            unsigned char ch = v.memory[idx];
            ascii += (ch >= 32 && ch < 127) ? (char)ch : '.';
        }
        out += " " + ascii + "\n";
    }
    return out;
}

static void bf_render(BFView *v) {
    GtkTextBuffer *src_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(v->source_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(src_buf, &start, &end);
    gtk_text_buffer_remove_tag(src_buf, v->pc_tag, &start, &end);
    if (!v->invalid && v->pc < v->source.size()) {
        GtkTextIter pc_start, pc_end;
        gtk_text_buffer_get_iter_at_offset(src_buf, &pc_start, (int)v->pc);
        gtk_text_buffer_get_iter_at_offset(src_buf, &pc_end, (int)v->pc + 1);
        gtk_text_buffer_apply_tag(src_buf, v->pc_tag, &pc_start, &pc_end);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(v->source_view), &pc_start, 0.1, FALSE, 0, 0);
    }

    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(v->memory_view)), bf_hex_dump(*v).c_str(), -1);
    gtk_label_set_text(GTK_LABEL(v->output_label), ("Output: " + sanitize(v->output)).c_str());

    std::string status = "pc " + std::to_string(v->pc) +
        "   pointer " + std::to_string(v->pointer) +
        "   steps " + std::to_string(v->steps);
    if (v->invalid) status += "   (invalid genome: mismatched brackets)";
    else if (v->halted) status += "   (halted)";
    gtk_label_set_text(GTK_LABEL(v->status_label), status.c_str());
}

static void bf_stop_play(BFView *v) {
    if (v->timeout_id) {
        g_source_remove(v->timeout_id);
        v->timeout_id = 0;
    }
    if (v->play_button)
        gtk_button_set_label(GTK_BUTTON(v->play_button), "▶ Play");
}

static gboolean bf_play_tick(gpointer data) {
    BFView *v = static_cast<BFView*>(data);
    if (v->halted) {
        bf_stop_play(v);
        if (v->on_playback_done) {
            gtk_widget_set_sensitive(v->controls, TRUE);
            void (*done)(gpointer) = v->on_playback_done;
            gpointer done_data = v->on_playback_done_data;
            v->on_playback_done = nullptr;
            v->on_playback_done_data = nullptr;
            done(done_data);
        }
        return G_SOURCE_REMOVE;
    }
    // One instruction per tick at a slower cadence -- fast enough to watch
    // the pc highlight move, unlike the old 20-steps-per-40ms blur.
    bf_step(*v);
    bf_render(v);
    return G_SOURCE_CONTINUE;
}

static void bf_on_step(GtkButton*, gpointer data) {
    BFView *v = static_cast<BFView*>(data);
    bf_step(*v);
    bf_render(v);
}

static void bf_on_reset(GtkButton*, gpointer data) {
    BFView *v = static_cast<BFView*>(data);
    bf_stop_play(v);
    bf_reset(*v);
    bf_render(v);
}

static void bf_on_play(GtkButton*, gpointer data) {
    BFView *v = static_cast<BFView*>(data);
    if (v->timeout_id) {
        bf_stop_play(v);
        return;
    }
    if (v->invalid) return;
    // Pressing Play once halted starts the program over instead of doing
    // nothing.
    if (v->halted) {
        bf_reset(*v);
        bf_render(v);
    }
    v->timeout_id = g_timeout_add(120, bf_play_tick, v);
    if (v->play_button)
        gtk_button_set_label(GTK_BUTTON(v->play_button), "⏸ Pause");
}

static void bf_on_destroy(GtkWidget*, gpointer data) {
    BFView *v = static_cast<BFView*>(data);
    // Just cancel the timer -- don't touch v->play_button. content's
    // "destroy" firing means this whole view is already being torn down,
    // so by the time this runs the button may already be gone (this was
    // the source of "gtk_button_set_label: assertion 'GTK_IS_BUTTON
    // (button)' failed": bf_stop_play() reset the label on a widget that
    // was mid-teardown). No need to reset a label that's about to
    // disappear anyway.
    if (v->timeout_id) {
        g_source_remove(v->timeout_id);
        v->timeout_id = 0;
    }
    // v->play_button is weak-pointer-tracked (see push_bf_view) precisely so
    // the above is safe even if bf_play_tick's timer somehow ticks once more
    // before g_source_remove takes effect -- but that tracking writes into
    // this struct on the button's actual destruction, so it must be torn
    // down before the struct holding the target address is freed below.
    if (v->play_button)
        g_object_remove_weak_pointer(G_OBJECT(v->play_button), (gpointer*)&v->play_button);
    delete v;
}

// Changing the input restarts the walkthrough from scratch -- in_dex may
// already be partway through the old input, so there's no sane way to keep
// going with the new bytes instead of just starting over.
static void bf_on_input_changed(GtkEditable *editable, gpointer data) {
    BFView *v = static_cast<BFView*>(data);
    bf_stop_play(v);
    parse_hex_input(gtk_editable_get_text(editable), v->input);
    bf_reset(*v);
    bf_render(v);
}

// Wires the TicTacToe playground to the Brainfuck/Skipfuck stepper sitting
// above it: called with the literal board State::engine->run() just saw, so
// a champion move replays and autoplays the exact instructions that produced
// it -- same idea as ttt_apply_trace_to_node_view, just for the BF-family
// walkthrough instead of the neural node view. on_done fires once the
// program halts -- ttt_playground_on_cell uses it to reveal the move only
// after the "thinking" finishes, not before.
static void ttt_apply_trace_to_bf_view(BFView *v, const char board[256], void (*on_done)(gpointer), gpointer on_done_data) {
    bf_stop_play(v);
    std::copy(board, board + 256, v->input);
    bf_reset(*v);
    bf_render(v);
    v->on_playback_done = on_done;
    v->on_playback_done_data = on_done_data;
    gtk_widget_set_sensitive(v->controls, FALSE);
    v->timeout_id = g_timeout_add(120, bf_play_tick, v);
    if (v->play_button)
        gtk_button_set_label(GTK_BUTTON(v->play_button), "⏸ Pause");
}

// ---------------------------------------------------------------------
// Live playground: appended to the bottom of both the Brainfuck interpreter
// view and the neural node view, whichever engine the champion is, when the
// active Test is TicTacToe/Tic_Off -- a real game against the champion via
// Engine::run(), same as score() would call it. Add/Crc8/Output don't get a
// separate playground: their walkthroughs (the tape stepper's ',' reads,
// the node view's trace) take the input directly instead, so there's one
// input box driving the actual step-by-step visualization rather than a
// second box off to the side just reporting a final result.
// ---------------------------------------------------------------------
static bool current_test_is_tic_tac_toe() {
    for (int i = 0; i < num_tests; i++) {
        if (tests[i] != State::test) continue;
        std::string name = test_names[i];
        return name == "TicTacToe" || name == "Tic_Off" || name == "Tournament_Toe";
    }
    return false;
}

// NodeView is defined further down (it needs Brain/trace() machinery that
// comes later in the file); forward-declared here so the playground can
// hold a pointer to the node view it's paired with and hand it fresh
// gameplay input, without reshuffling this whole section below NodeView.
struct NodeView;
static void ttt_apply_trace_to_node_view(NodeView *v, const char board[256], void (*on_done)(gpointer), gpointer on_done_data);

struct TTTPlayground {
    std::string code;
    alignas(256) char board[256];
    alignas(256) char seen_board[256];   // board as the engine actually saw it (flipped if champion_mark is 'O')
    GtkWidget *cells[9] = {nullptr};
    GtkWidget *status = nullptr;
    GtkWidget *reset_btn = nullptr;
    GtkWidget *champion_first_btn = nullptr;
    NodeView *node_view = nullptr;   // non-null only when the champion is a Brain
    BFView *bf_view = nullptr;       // non-null only when the champion is Brainfuck-family
    unsigned char pending_move = 0;  // champion's chosen move, staged while the paired view "thinks"
    size_t pending_runtime = 0;      // Engine::run()'s return value for pending_move above
    // Whoever moves first is X, whoever moves second is O -- standard
    // convention, and exactly what TicTacToe's score() itself tests (see
    // play_game's two call sites: player='X' when the champion goes first,
    // player='O' when it goes second). Settable per-game instead of always
    // hardcoding the human as X, so "let the champion go first" actually
    // exercises the half of training the human-always-first flow never did.
    char human_mark = 'X';
    char champion_mark = 'O';
};

static bool ttt_playground_won(const char board[9], char player) {
    static constexpr int LINES[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8},
        {0,3,6}, {1,4,7}, {2,5,8},
        {0,4,8}, {2,4,6},
    };
    for (auto &line : LINES)
        if (board[line[0]] == player && board[line[1]] == player && board[line[2]] == player)
            return true;
    return false;
}

static void ttt_playground_refresh(TTTPlayground *pg) {
    for (int i = 0; i < 9; i++)
        gtk_button_set_label(GTK_BUTTON(pg->cells[i]), std::string(1, pg->board[i]).c_str());
}

static void ttt_playground_end(TTTPlayground *pg, const std::string &msg) {
    gtk_label_set_text(GTK_LABEL(pg->status), msg.c_str());
    for (int i = 0; i < 9; i++)
        gtk_widget_set_sensitive(pg->cells[i], FALSE);
}

// Applies the champion's already-decided move (staged as pending_move/
// pending_runtime by on_cell or start_game below). Called immediately when
// there's no paired view to animate, or once that view finishes playing
// back the moves/rounds that produced this move -- so the move is revealed
// after the "thinking", never before it.
static void ttt_playground_reveal_move(gpointer data) {
    TTTPlayground *pg = static_cast<TTTPlayground*>(data);
    gtk_widget_set_sensitive(pg->reset_btn, TRUE);
    gtk_widget_set_sensitive(pg->champion_first_btn, TRUE);
    unsigned char move = pg->pending_move;
    size_t runtime = pg->pending_runtime;

    if (runtime >= State::max_runtime || move >= 9 || pg->board[move] != ' ') {
        ttt_playground_end(pg, "Champion made an invalid move -- you win by forfeit.");
        return;
    }

    pg->board[move] = pg->champion_mark;
    ttt_playground_refresh(pg);
    if (ttt_playground_won(pg->board, pg->champion_mark)) { ttt_playground_end(pg, "Champion wins!"); return; }
    if (std::none_of(pg->board, pg->board + 9, [](char c) { return c == ' '; })) {
        ttt_playground_end(pg, "Draw.");
        return;
    }
    gtk_label_set_text(GTK_LABEL(pg->status), (std::string("Your move (") + pg->human_mark + ") -- click a square.").c_str());
    for (int i = 0; i < 9; i++)
        if (pg->board[i] == ' ')
            gtk_widget_set_sensitive(pg->cells[i], TRUE);
}

// Stages pending_move/pending_runtime (already computed by the caller) for
// reveal: locks the board, reset, and "champion goes first" controls, then
// hands off to whichever paired view can animate the thinking that produced
// it, or reveals immediately if there's no view to animate.
static void ttt_playground_stage_move(TTTPlayground *pg) {
    if (!pg->node_view && !pg->bf_view) {
        ttt_playground_reveal_move(pg);
        return;
    }
    // Block further clicks -- including a mid-thinking "New game" or
    // "let champion go first" -- until the paired view's playback (started
    // below, against the exact board run() just saw) finishes: otherwise
    // you could sneak in another move, or reset the board, while the
    // champion's move is still just staged, not yet on the board.
    for (int i = 0; i < 9; i++)
        gtk_widget_set_sensitive(pg->cells[i], FALSE);
    gtk_widget_set_sensitive(pg->reset_btn, FALSE);
    gtk_widget_set_sensitive(pg->champion_first_btn, FALSE);
    gtk_label_set_text(GTK_LABEL(pg->status), "Champion is thinking...");
    if (pg->node_view)
        ttt_apply_trace_to_node_view(pg->node_view, pg->seen_board, ttt_playground_reveal_move, pg);
    else
        ttt_apply_trace_to_bf_view(pg->bf_view, pg->seen_board, ttt_playground_reveal_move, pg);
}

// Shared by "New game" (champion_first=false) and "Let champion go first"
// (champion_first=true): clears the board, assigns marks for this game (see
// TTTPlayground::human_mark/champion_mark), and either waits for the
// human's first click or immediately stages the champion's opening move
// against the empty board -- through the same "thinking" flow as any other
// move, rather than just slapping a move down instantly.
static void ttt_playground_start_game(TTTPlayground *pg, bool champion_first) {
    std::fill(std::begin(pg->board), std::end(pg->board), 0);
    std::fill(pg->board, pg->board + 9, ' ');
    pg->champion_mark = champion_first ? 'X' : 'O';
    pg->human_mark = champion_first ? 'O' : 'X';
    ttt_playground_refresh(pg);
    for (int i = 0; i < 9; i++)
        gtk_widget_set_sensitive(pg->cells[i], TRUE);

    if (!champion_first) {
        gtk_label_set_text(GTK_LABEL(pg->status), (std::string("Your move (") + pg->human_mark + ") -- click a square.").c_str());
        return;
    }

    alignas(256) char output[256] = {0};
    std::copy(std::begin(pg->board), std::end(pg->board), std::begin(pg->seen_board));
    if (pg->champion_mark == 'O') flip(pg->seen_board);
    State::engine->load(&pg->code);
    pg->pending_runtime = State::engine->run(pg->seen_board, output, State::max_runtime);
    pg->pending_move = (unsigned char)output[0];
    ttt_playground_stage_move(pg);
}

static void ttt_playground_on_reset(GtkButton*, gpointer data) {
    ttt_playground_start_game(static_cast<TTTPlayground*>(data), false);
}

static void ttt_playground_on_champion_first(GtkButton*, gpointer data) {
    ttt_playground_start_game(static_cast<TTTPlayground*>(data), true);
}

static void ttt_playground_on_cell(GtkButton *btn, gpointer data) {
    TTTPlayground *pg = static_cast<TTTPlayground*>(data);
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "cell-idx"));
    if (pg->board[idx] != ' ') return;

    pg->board[idx] = pg->human_mark;
    ttt_playground_refresh(pg);
    if (ttt_playground_won(pg->board, pg->human_mark)) { ttt_playground_end(pg, "You win!"); return; }
    if (std::none_of(pg->board, pg->board + 9, [](char c) { return c == ' '; })) {
        ttt_playground_end(pg, "Draw.");
        return;
    }

    alignas(256) char output[256] = {0};
    std::copy(std::begin(pg->board), std::end(pg->board), std::begin(pg->seen_board));
    if (pg->champion_mark == 'O') flip(pg->seen_board);
    State::engine->load(&pg->code);
    pg->pending_runtime = State::engine->run(pg->seen_board, output, State::max_runtime);
    pg->pending_move = (unsigned char)output[0];
    ttt_playground_stage_move(pg);
}

static void ttt_playground_destroy(GtkWidget*, gpointer data) {
    delete static_cast<TTTPlayground*>(data);
}

static GtkWidget * build_ttt_playground(const std::string &code, NodeView *node_view, BFView *bf_view) {
    TTTPlayground *pg = new TTTPlayground();
    pg->code = code;
    pg->node_view = node_view;
    pg->bf_view = bf_view;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    GtkWidget *title = gtk_label_new("Play against this champion");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_add_css_class(title, "heading");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 4);
    for (int i = 0; i < 9; i++) {
        GtkWidget *btn = gtk_button_new_with_label(" ");
        gtk_widget_set_size_request(btn, 48, 48);
        g_object_set_data(G_OBJECT(btn), "cell-idx", GINT_TO_POINTER(i));
        g_signal_connect(btn, "clicked", G_CALLBACK(ttt_playground_on_cell), pg);
        gtk_grid_attach(GTK_GRID(grid), btn, i % 3, i / 3, 1, 1);
        pg->cells[i] = btn;
    }
    gtk_box_append(GTK_BOX(box), grid);

    pg->status = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(pg->status), 0.0f);
    gtk_box_append(GTK_BOX(box), pg->status);

    GtkWidget *button_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    pg->reset_btn = gtk_button_new_with_label("New game");
    g_signal_connect(pg->reset_btn, "clicked", G_CALLBACK(ttt_playground_on_reset), pg);
    gtk_box_append(GTK_BOX(button_row), pg->reset_btn);

    pg->champion_first_btn = gtk_button_new_with_label("Let champion go first");
    g_signal_connect(pg->champion_first_btn, "clicked", G_CALLBACK(ttt_playground_on_champion_first), pg);
    gtk_box_append(GTK_BOX(button_row), pg->champion_first_btn);
    gtk_box_append(GTK_BOX(box), button_row);

    g_signal_connect(box, "destroy", G_CALLBACK(ttt_playground_destroy), pg);

    ttt_playground_on_reset(nullptr, pg);
    return box;
}

static GtkWidget * build_playground_widget(const std::string &code, NodeView *node_view = nullptr, BFView *bf_view = nullptr) {
    return current_test_is_tic_tac_toe() ? build_ttt_playground(code, node_view, bf_view) : nullptr;
}

static void push_bf_view(const std::string &code) {
    BFView *v = new BFView();
    v->source = code;
    v->skip_dialect = code.find('*') != std::string::npos;
    bf_build_matches(*v);
    bf_reset(*v);

    GtkWidget *toolbar_view = adw_toolbar_view_new();
    GtkWidget *header = adw_header_bar_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(content, 10);
    gtk_widget_set_margin_end(content, 10);
    gtk_widget_set_margin_top(content, 10);
    gtk_widget_set_margin_bottom(content, 10);

    // TicTacToe/Tic_Off champions never read arbitrary bytes -- their ','
    // input is the board state the playground below deals out, not
    // free-form hex a user would type here, so the box is just noise.
    if (!current_test_is_tic_tac_toe()) {
        GtkWidget *input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *input_label = gtk_label_new("Input (hex, feeds ',' reads):");
        gtk_box_append(GTK_BOX(input_row), input_label);
        GtkWidget *input_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(input_entry), "e.g. 48 65 6C 6C 6F 20 70 61 70 61");
        gtk_widget_add_css_class(input_entry, "monospace");
        gtk_widget_set_hexpand(input_entry, TRUE);
        g_signal_connect(input_entry, "changed", G_CALLBACK(bf_on_input_changed), v);
        gtk_box_append(GTK_BOX(input_row), input_entry);
        gtk_box_append(GTK_BOX(content), input_row);
    }

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(paned, TRUE);

    GtkWidget *src_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *src_title = gtk_label_new(v->skip_dialect ? "Source (Skipfuck dialect)" : "Source");
    gtk_label_set_xalign(GTK_LABEL(src_title), 0.0f);
    gtk_widget_add_css_class(src_title, "heading");
    gtk_box_append(GTK_BOX(src_col), src_title);
    GtkWidget *src_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(src_scroll, TRUE);
    v->source_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(v->source_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(v->source_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(v->source_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(v->source_view), GTK_WRAP_CHAR);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(v->source_view)), v->source.c_str(), -1);
    v->pc_tag = gtk_text_buffer_create_tag(gtk_text_view_get_buffer(GTK_TEXT_VIEW(v->source_view)), "pc",
        "background", "#f9c440", "foreground", "#000000", nullptr);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(src_scroll), v->source_view);
    gtk_box_append(GTK_BOX(src_col), src_scroll);
    gtk_paned_set_start_child(GTK_PANED(paned), src_col);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);

    GtkWidget *mem_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *mem_title = gtk_label_new("Memory (hex, window around pointer)");
    gtk_label_set_xalign(GTK_LABEL(mem_title), 0.0f);
    gtk_widget_add_css_class(mem_title, "heading");
    gtk_box_append(GTK_BOX(mem_col), mem_title);
    GtkWidget *mem_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(mem_scroll, TRUE);
    v->memory_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(v->memory_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(v->memory_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(v->memory_view), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(mem_scroll), v->memory_view);
    gtk_box_append(GTK_BOX(mem_col), mem_scroll);
    gtk_paned_set_end_child(GTK_PANED(paned), mem_col);
    gtk_paned_set_resize_end_child(GTK_PANED(paned), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 480);

    gtk_box_append(GTK_BOX(content), paned);

    v->output_label = gtk_label_new("Output:");
    gtk_label_set_xalign(GTK_LABEL(v->output_label), 0.0f);
    gtk_widget_add_css_class(v->output_label, "monospace");
    gtk_label_set_wrap(GTK_LABEL(v->output_label), TRUE);
    gtk_box_append(GTK_BOX(content), v->output_label);

    v->status_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(v->status_label), 0.0f);
    gtk_box_append(GTK_BOX(content), v->status_label);

    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    v->controls = controls;
    GtkWidget *step_btn = gtk_button_new_with_label("Step");
    g_signal_connect(step_btn, "clicked", G_CALLBACK(bf_on_step), v);
    gtk_box_append(GTK_BOX(controls), step_btn);

    v->play_button = gtk_button_new_with_label("▶ Play");
    gtk_widget_add_css_class(v->play_button, "suggested-action");
    g_signal_connect(v->play_button, "clicked", G_CALLBACK(bf_on_play), v);
    gtk_box_append(GTK_BOX(controls), v->play_button);
    g_object_add_weak_pointer(G_OBJECT(v->play_button), (gpointer*)&v->play_button);

    GtkWidget *reset_btn = gtk_button_new_with_label("Reset");
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(bf_on_reset), v);
    gtk_box_append(GTK_BOX(controls), reset_btn);

    gtk_box_append(GTK_BOX(content), controls);

    GtkWidget *playground = build_playground_widget(code, nullptr, v);
    if (playground) {
        gtk_box_append(GTK_BOX(content), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
        gtk_box_append(GTK_BOX(content), playground);
    }

    g_signal_connect(content, "destroy", G_CALLBACK(bf_on_destroy), v);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), content);

    AdwNavigationPage *page = adw_navigation_page_new(toolbar_view,
        v->skip_dialect ? "Skipfuck interpreter" : "Brainfuck interpreter");
    adw_navigation_view_push(ADW_NAVIGATION_VIEW(nav_view), page);

    bf_render(v);
}

// ---------------------------------------------------------------------
// Neural node view: lays kept neurons out in input/hidden/output columns,
// draws the static pruned wiring dim in the background, and steps through
// Brain::trace()'s rounds highlighting synapses as they fire and coloring
// each node by its current potential -- "how it comes up with ideas."
// ---------------------------------------------------------------------
// Shared between node_layout() and node_draw() so the row labels always
// line up with the nodes actually drawn next to them.
static const double NODE_ROW_Y[3] = {70, 260, 450};
static const char * const NODE_COL_LABELS[3] = {"Input", "Hidden", "Output"};
static const double NODE_LEFT_MARGIN = 100;   // leaves room for the row label

// Oscillators share the Input row -- they're lower-numbered than every real
// input neuron, so they always sort to the front of it (see node_layout()).
static int node_group(unsigned short id) {
    if (id < HIDDEN_START) return 0;
    if (id < EXCLUDING) return 1;
    return 2;
}

// Small label drawn above a neuron's circle: which literal bit of the
// 256-byte buffer an input/output neuron is, or which oscillator number a
// clock neuron is -- plain hidden neurons don't get one.
static std::string node_neuron_label(unsigned short id) {
    if (id < OSCILLATOR_NEURONS)
        return "Osc " + std::to_string(id + 1);
    if (id < HIDDEN_START)
        return "bit " + std::to_string(id - INPUT_START);
    if (id >= EXCLUDING)
        return "bit " + std::to_string(id - EXCLUDING);
    return "";
}

struct NodeLayout {
    double x, y;
};

struct NodeView {
    std::string code;   // genome this trace is against -- kept to re-trace on input changes
    char input[256] = {0};
    std::vector<Synapse> wiring;
    std::vector<TraceRound> rounds;
    std::map<unsigned short, NodeLayout> pos;
    size_t round_idx = 0;

    GtkWidget *canvas = nullptr;
    GtkWidget *status_label = nullptr;
    GtkWidget *output_label = nullptr;   // decoded first VISIBLE_OUTPUT_BYTES output bytes, this round
    GtkWidget *play_button = nullptr;
    GtkWidget *controls = nullptr;   // Step/Play/Reset row, locked while a gameplay move is pending
    guint timeout_id = 0;

    // Fires once when a gameplay-triggered playback (see
    // ttt_apply_trace_to_node_view) reaches its last round, then gets
    // cleared -- manual Step/Play/replay afterward never re-trigger it.
    void (*on_playback_done)(gpointer) = nullptr;
    gpointer on_playback_done_data = nullptr;
};

static double node_layout(NodeView &v) {
    std::set<unsigned short> ids;
    if (!v.rounds.empty())
        for (const NeuronSample &n : v.rounds[0].neurons)
            ids.insert(n.neuron);

    std::map<int, std::vector<unsigned short>> rows;
    for (unsigned short id : ids)
        rows[node_group(id)].push_back(id);

    const double spacing = 54;   // circles now run up to ~46px across; keep neurons from overlapping
    double max_x = 200;
    for (auto &[row, list] : rows) {
        for (size_t i = 0; i < list.size(); i++) {
            double x = NODE_LEFT_MARGIN + i * spacing;
            v.pos[list[i]] = {x, NODE_ROW_Y[row]};
            max_x = std::max(max_x, x + 34);
        }
    }
    return max_x;
}

// Decodes the first VISIBLE_OUTPUT_BYTES output bytes from a trace round's
// neuron potentials -- the same bit-per-neuron, MSB-first scheme run()
// itself decodes with, so this is exactly what the champion is outputting
// as of this round (kept[] always includes these neurons, see
// reachable_from_outputs()).
static std::string node_decode_output(const TraceRound &round) {
    std::map<unsigned short, float> values;
    for (const NeuronSample &n : round.neurons)
        values[n.neuron] = n.value;

    std::string hex;
    for (int position = 0; position < VISIBLE_OUTPUT_BYTES; position++) {
        unsigned char byte = 0;
        for (int bit = 0; bit < 8; bit++) {
            auto it = values.find(EXCLUDING + position * 8 + bit);
            if (it != values.end() && it->second >= THRESHOLD)
                byte |= 128 >> bit;
        }
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X ", byte);
        hex += buf;
    }
    if (!hex.empty()) hex.pop_back();
    return hex;
}

static void node_render(NodeView *v) {
    // round_idx now IS "how many rounds have run" -- 0 is the resting state
    // trace() prepends, so no +1 here (see trace()'s round-0 comment).
    std::string status = "Round " + std::to_string(v->round_idx) +
        " / " + std::to_string(v->rounds.empty() ? 0 : v->rounds.size() - 1) +
        "   nodes shown " + std::to_string(v->pos.size()) +
        "   synapses shown " + std::to_string(v->wiring.size());
    gtk_label_set_text(GTK_LABEL(v->status_label), status.c_str());

    // Only a meaningful, freestanding byte stream for the non-game tests --
    // TicTacToe/Tic_Off/Tournament_Toe only ever read output[0] as a cell
    // index, and the playground already shows the move/game outcome, so a
    // raw hex dump here would just be noise for those.
    if (v->output_label) {
        std::string out = "Output (first " + std::to_string(VISIBLE_OUTPUT_BYTES) + " bytes): " +
            (v->rounds.empty() ? "" : node_decode_output(v->rounds[v->round_idx]));
        gtk_label_set_text(GTK_LABEL(v->output_label), out.c_str());
    }

    gtk_widget_queue_draw(v->canvas);
}

// "How full" a neuron is, as a fraction of THRESHOLD (the point it fires
// at): 0 = resting (dark), 1 = at/above threshold (bright). A single warm
// hue that lights up as charge builds -- like an ember heating up -- reads
// as "how much" more directly than a hue-shifting rainbow would.
static void heat_color(double t, double &r, double &g, double &b) {
    t = std::clamp(t, 0.0, 1.0);
    r = 0.09 + t * 0.89;
    g = 0.10 + t * 0.73;
    b = 0.14 + t * 0.18;
}

// Picks readable label text against whatever heat_color produced, instead
// of a fixed grey that loses contrast once the fill gets bright.
static bool fill_is_light(double r, double g, double b) {
    return (0.299 * r + 0.587 * g + 0.114 * b) > 0.55;
}

// Excitatory (positive weight) synapses draw green, inhibitory (negative)
// draw red -- same color language as the champion-code / chart panels use
// for "good"/"bad". Line width and saturation both scale with |weight| so
// strong synapses read as thicker/brighter, not just differently colored.
static void synapse_style(float multiplier, bool firing, double &r, double &g, double &b, double &a, double &width) {
    double mag = std::clamp((double)std::fabs(multiplier) / 3.0, 0.0, 1.0);
    if (multiplier >= 0) { r = 0.20; g = 0.75; b = 0.25; }
    else                 { r = 0.90; g = 0.20; b = 0.20; }
    if (firing) {
        a = 0.55 + mag * 0.45;
        width = 2.0 + mag * 3.5;
    } else {
        a = 0.10 + mag * 0.20;
        width = 1.0 + mag * 1.5;
    }
}

static void node_draw(GtkDrawingArea*, cairo_t *cr, int, int, gpointer data) {
    NodeView *v = static_cast<NodeView*>(data);
    if (v->rounds.empty() || v->round_idx >= v->rounds.size())
        return;
    const TraceRound &round = v->rounds[v->round_idx];

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13);
    cairo_set_source_rgba(cr, 0.75, 0.75, 0.75, 0.95);
    for (int row = 0; row < 3; row++) {
        cairo_text_extents_t ext;
        cairo_text_extents(cr, NODE_COL_LABELS[row], &ext);
        cairo_move_to(cr, 10, NODE_ROW_Y[row] - ext.height / 2.0 - ext.y_bearing);
        cairo_show_text(cr, NODE_COL_LABELS[row]);
    }

    std::map<unsigned short, float> values;
    for (const NeuronSample &n : round.neurons)
        values[n.neuron] = n.value;

    std::map<uint32_t, float> fired;              // (input,output) -> weight
    std::set<unsigned short> fired_neurons;       // neurons that fired this round
    for (const FiredSynapse &f : round.fired) {
        fired[((uint32_t)f.input << 16) | f.output] = f.multiplier;
        fired_neurons.insert(f.input);
    }

    // Dim background wiring first, then firing synapses on top and bright,
    // so an active path stands out against the whole static graph.
    for (const Synapse &s : v->wiring) {
        uint32_t key = ((uint32_t)s.input << 16) | s.output;
        if (fired.count(key)) continue;
        auto pi = v->pos.find(s.input), po = v->pos.find(s.output);
        if (pi == v->pos.end() || po == v->pos.end()) continue;
        double r, g, b, a, w;
        synapse_style(s.multiplier, false, r, g, b, a, w);
        cairo_set_source_rgba(cr, r, g, b, a);
        cairo_set_line_width(cr, w);
        cairo_move_to(cr, pi->second.x, pi->second.y);
        cairo_line_to(cr, po->second.x, po->second.y);
        cairo_stroke(cr);
    }
    for (const FiredSynapse &f : round.fired) {
        auto pi = v->pos.find(f.input), po = v->pos.find(f.output);
        if (pi == v->pos.end() || po == v->pos.end()) continue;
        double r, g, b, a, w;
        synapse_style(f.multiplier, true, r, g, b, a, w);
        cairo_set_source_rgba(cr, r, g, b, a);
        cairo_set_line_width(cr, w);
        cairo_move_to(cr, pi->second.x, pi->second.y);
        cairo_line_to(cr, po->second.x, po->second.y);
        cairo_stroke(cr);
    }

    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 9);
    for (auto &[id, p] : v->pos) {
        float val = 0;
        auto it = values.find(id);
        if (it != values.end()) val = it->second;
        double t = std::clamp((double)val / (double)THRESHOLD, 0.0, 1.0);
        bool firing_now = fired_neurons.count(id) != 0;
        // fire() resets a neuron's raw potential to THRESHOLD-1 (-0.3) the
        // instant it fires -- a refractory dip, not "cold." A neuron that
        // just fired is the hottest state there is (this is also why a
        // perpetually-active input, which never gets re-leaked, reads as
        // permanently dark under raw potential alone -- it's mid-refractory
        // every single round despite firing every single round).
        if (firing_now) t = 1.0;
        double radius = 15.0 + t * 8.0;

        double r, g, b;
        heat_color(t, r, g, b);
        cairo_set_source_rgb(cr, r, g, b);
        cairo_arc(cr, p.x, p.y, radius, 0, 2 * M_PI);
        cairo_fill(cr);

        // Bolding effect: a neuron that fired this round gets a heavy bright
        // outline instead of the normal thin border -- "just fired" reads as
        // a bold ring regardless of how charged (bright) it already looked.
        if (firing_now) {
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_set_line_width(cr, 3.2);
        } else {
            cairo_set_source_rgba(cr, 0, 0, 0, 0.55);
            cairo_set_line_width(cr, 1.0);
        }
        cairo_arc(cr, p.x, p.y, radius, 0, 2 * M_PI);
        cairo_stroke(cr);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", val);
        if (fill_is_light(r, g, b))
            cairo_set_source_rgba(cr, 0.05, 0.05, 0.05, 0.95);
        else
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.95);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, buf, &extents);
        cairo_move_to(cr, p.x - extents.width / 2.0 - extents.x_bearing,
                          p.y - extents.height / 2.0 - extents.y_bearing);
        cairo_show_text(cr, buf);

        std::string tag = node_neuron_label(id);
        if (!tag.empty()) {
            cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_font_size(cr, 9);
            cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.9);
            cairo_text_extents_t tag_ext;
            cairo_text_extents(cr, tag.c_str(), &tag_ext);
            cairo_move_to(cr, p.x - tag_ext.width / 2.0 - tag_ext.x_bearing, p.y - radius - 6);
            cairo_show_text(cr, tag.c_str());
            cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 9);
        }
    }
}

static void node_stop_play(NodeView *v) {
    if (v->timeout_id) {
        g_source_remove(v->timeout_id);
        v->timeout_id = 0;
    }
    if (v->play_button)
        gtk_button_set_label(GTK_BUTTON(v->play_button), "▶ Play");
}

static gboolean node_play_tick(gpointer data) {
    NodeView *v = static_cast<NodeView*>(data);
    v->round_idx++;
    if (v->round_idx >= v->rounds.size()) {
        v->round_idx = v->rounds.empty() ? 0 : v->rounds.size() - 1;
        node_stop_play(v);
        node_render(v);
        if (v->on_playback_done) {
            gtk_widget_set_sensitive(v->controls, TRUE);
            void (*done)(gpointer) = v->on_playback_done;
            gpointer done_data = v->on_playback_done_data;
            v->on_playback_done = nullptr;
            v->on_playback_done_data = nullptr;
            done(done_data);
        }
        return G_SOURCE_REMOVE;
    }
    node_render(v);
    return G_SOURCE_CONTINUE;
}

static void node_on_step(GtkButton*, gpointer data) {
    NodeView *v = static_cast<NodeView*>(data);
    if (v->round_idx + 1 < v->rounds.size())
        v->round_idx++;
    node_render(v);
}

static void node_on_reset(GtkButton*, gpointer data) {
    NodeView *v = static_cast<NodeView*>(data);
    node_stop_play(v);
    v->round_idx = 0;
    node_render(v);
}

static void node_on_play(GtkButton*, gpointer data) {
    NodeView *v = static_cast<NodeView*>(data);
    if (v->timeout_id) {
        node_stop_play(v);
        return;
    }
    if (v->rounds.empty()) return;
    // Pressing Play at the end starts the replay over from round 0 instead
    // of doing nothing.
    if (v->round_idx + 1 >= v->rounds.size()) {
        v->round_idx = 0;
        node_render(v);
    }
    v->timeout_id = g_timeout_add(300, node_play_tick, v);
    if (v->play_button)
        gtk_button_set_label(GTK_BUTTON(v->play_button), "⏸ Pause");
}

static void node_on_destroy(GtkWidget*, gpointer data) {
    NodeView *v = static_cast<NodeView*>(data);
    // See bf_on_destroy's comment: cancel the timer only, don't touch
    // v->play_button -- it may already be mid-teardown by the time this
    // fires.
    if (v->timeout_id) {
        g_source_remove(v->timeout_id);
        v->timeout_id = 0;
    }
    // See bf_on_destroy's matching comment -- untrack the weak pointer
    // before freeing the struct it writes into.
    if (v->play_button)
        g_object_remove_weak_pointer(G_OBJECT(v->play_button), (gpointer*)&v->play_button);
    delete v;
}

// Same idea as the Brainfuck walkthrough's input box: re-traces from round
// 0 against whatever's typed, instead of the fixed all-zero placeholder
// trace() used to get -- so Step/Play actually show how this input
// propagates instead of playing back an arbitrary fixed demo.
static void node_on_input_changed(GtkEditable *editable, gpointer data) {
    NodeView *v = static_cast<NodeView*>(data);
    Brain *brain = dynamic_cast<Brain*>(State::engine);
    if (!brain) return;

    node_stop_play(v);
    parse_hex_input(gtk_editable_get_text(editable), v->input);
    v->rounds = brain->trace(&v->code, v->input);
    v->round_idx = 0;
    node_render(v);
}

// Wires the TicTacToe playground to the node view sitting above it: called
// with the literal board State::engine->run() just saw, so a champion move
// re-traces and autoplays through the rounds that actually produced it,
// instead of the node view sitting frozen on its initial all-zero-input
// trace while a real game gets played below it. on_done fires once playback
// reaches its last round -- ttt_playground_on_cell uses it to reveal the
// move only after the "thinking" finishes, not before. Only ever called
// with v non-null (see ttt_playground_on_cell); the dynamic_cast guard below
// exists for the same reason node_on_input_changed has one -- State::engine
// is a global that this view doesn't own.
static void ttt_apply_trace_to_node_view(NodeView *v, const char board[256], void (*on_done)(gpointer), gpointer on_done_data) {
    Brain *brain = dynamic_cast<Brain*>(State::engine);
    if (!brain) { on_done(on_done_data); return; }

    node_stop_play(v);
    std::copy(board, board + 256, v->input);
    v->rounds = brain->trace(&v->code, v->input);
    v->round_idx = 0;
    v->on_playback_done = on_done;
    v->on_playback_done_data = on_done_data;
    gtk_widget_set_sensitive(v->controls, FALSE);
    node_render(v);
    v->timeout_id = g_timeout_add(300, node_play_tick, v);
    if (v->play_button)
        gtk_button_set_label(GTK_BUTTON(v->play_button), "⏸ Pause");
}

static void push_node_view(const std::string &code) {
    Brain *brain = dynamic_cast<Brain*>(State::engine);
    if (!brain) return;

    NodeView *v = new NodeView();
    v->code = code;
    v->wiring = brain->clean_synapses(&code);
    v->rounds = brain->trace(&code, v->input);   // v->input starts all-zero
    double content_w = node_layout(*v);

    GtkWidget *toolbar_view = adw_toolbar_view_new();
    GtkWidget *header = adw_header_bar_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(content, 10);
    gtk_widget_set_margin_end(content, 10);
    gtk_widget_set_margin_top(content, 10);
    gtk_widget_set_margin_bottom(content, 10);

    // Same reasoning as push_bf_view: a TicTacToe/Tic_Off champion's input is
    // the board the playground below deals out, not user-typed hex, so
    // there's nothing meaningful for this box to drive.
    if (!current_test_is_tic_tac_toe()) {
        GtkWidget *input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *input_label = gtk_label_new("Input (hex, fed into the trace):");
        gtk_box_append(GTK_BOX(input_row), input_label);
        GtkWidget *input_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(input_entry), "e.g. 48 65 6C 6C 6F 20 70 61 70 61");
        gtk_widget_add_css_class(input_entry, "monospace");
        gtk_widget_set_hexpand(input_entry, TRUE);
        g_signal_connect(input_entry, "changed", G_CALLBACK(node_on_input_changed), v);
        gtk_box_append(GTK_BOX(input_row), input_entry);
        gtk_box_append(GTK_BOX(content), input_row);
    }

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    v->canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(v->canvas), (int)content_w);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(v->canvas), 520);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(v->canvas), node_draw, v, nullptr);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), v->canvas);
    gtk_box_append(GTK_BOX(content), scroll);

    GtkWidget *legend = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(legend),
        "<span foreground='#33bf40'>green synapse</span> = excitatory   "
        "<span foreground='#e63333'>red synapse</span> = inhibitory   "
        "(thicker/brighter = stronger weight)   "
        "node color = charge (blue -&gt; yellow -&gt; red as it nears/exceeds threshold)   "
        "<span foreground='#f2eb4d'>yellow ring</span> = fired this round");
    gtk_label_set_xalign(GTK_LABEL(legend), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(legend), TRUE);
    gtk_widget_add_css_class(legend, "caption");
    gtk_box_append(GTK_BOX(content), legend);

    v->status_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(v->status_label), 0.0f);
    gtk_box_append(GTK_BOX(content), v->status_label);

    if (!current_test_is_tic_tac_toe()) {
        v->output_label = gtk_label_new("");
        gtk_label_set_xalign(GTK_LABEL(v->output_label), 0.0f);
        gtk_widget_add_css_class(v->output_label, "monospace");
        gtk_box_append(GTK_BOX(content), v->output_label);
    }

    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    v->controls = controls;
    GtkWidget *step_btn = gtk_button_new_with_label("Step");
    g_signal_connect(step_btn, "clicked", G_CALLBACK(node_on_step), v);
    gtk_box_append(GTK_BOX(controls), step_btn);

    v->play_button = gtk_button_new_with_label("▶ Play");
    gtk_widget_add_css_class(v->play_button, "suggested-action");
    g_signal_connect(v->play_button, "clicked", G_CALLBACK(node_on_play), v);
    gtk_box_append(GTK_BOX(controls), v->play_button);
    g_object_add_weak_pointer(G_OBJECT(v->play_button), (gpointer*)&v->play_button);

    GtkWidget *reset_btn = gtk_button_new_with_label("Reset");
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(node_on_reset), v);
    gtk_box_append(GTK_BOX(controls), reset_btn);

    gtk_box_append(GTK_BOX(content), controls);

    GtkWidget *playground = build_playground_widget(code, v);
    if (playground) {
        gtk_box_append(GTK_BOX(content), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
        gtk_box_append(GTK_BOX(content), playground);
    }

    g_signal_connect(content, "destroy", G_CALLBACK(node_on_destroy), v);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), content);

    AdwNavigationPage *page = adw_navigation_page_new(toolbar_view, "Neural node view");
    adw_navigation_view_push(ADW_NAVIGATION_VIEW(nav_view), page);

    node_render(v);
}

static void open_detail_view(const std::string &code_snapshot) {
    if (dynamic_cast<Brain*>(State::engine))
        push_node_view(code_snapshot);
    else
        push_bf_view(code_snapshot);
}

static void activate(GtkApplication *app, gpointer) {
    GtkWidget *window = adw_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Brainfuck Evolver");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 720);

    GtkWidget *toolbar_view = adw_toolbar_view_new();
    GtkWidget *header = adw_header_bar_new();
    adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), adw_window_title_new("Brainfuck Evolver", nullptr));
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header);

    GtkWidget *config_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(config_box, 10);
    gtk_widget_set_margin_end(config_box, 10);
    gtk_widget_set_margin_top(config_box, 10);
    gtk_widget_set_margin_bottom(config_box, 10);

    const char **engine_nt = null_terminate(engine_names, num_engines);
    const char **evolver_nt = null_terminate(evolver_names, num_evolvers);
    const char **test_nt = null_terminate(test_names, num_tests);

    engine_dropdown = gtk_drop_down_new_from_strings(engine_nt);
    evolver_dropdown = gtk_drop_down_new_from_strings(evolver_nt);
    test_dropdown = gtk_drop_down_new_from_strings(test_nt);

    // Default selection: JitFuck / Squarelite / Output -- by name rather
    // than a bare index, so this doesn't silently point at the wrong
    // thing if engine_names[]/evolver_names[]/test_names[] ever get
    // reordered.
    auto index_of = [](const char *const *names, int count, const char *target) -> guint {
        for (int i = 0; i < count; i++)
            if (strcmp(names[i], target) == 0)
                return (guint)i;
        return 0;
    };
    gtk_drop_down_set_selected(GTK_DROP_DOWN(engine_dropdown), index_of(engine_names, num_engines, "JitFuck"));
    gtk_drop_down_set_selected(GTK_DROP_DOWN(evolver_dropdown), index_of(evolver_names, num_evolvers, "Squarelite"));
    gtk_drop_down_set_selected(GTK_DROP_DOWN(test_dropdown), index_of(test_names, num_tests, "Output"));

    seed_spin = gtk_spin_button_new_with_range(0, 2000000000, 1);
    children_spin = gtk_spin_button_new_with_range(4, 200000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(children_spin), 10000);
    famers_spin = gtk_spin_button_new_with_range(1, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(famers_spin), 100);
    randomness_spin = gtk_spin_button_new_with_range(1, 100000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(randomness_spin), 250);

    // One shared cap instead of a different hardcoded constant per test
    // (100K to 1e9 across the old MAX_RUNTIME/LOOP_MAX/MAX_TIC_TAC_TOE/
    // MAX_TIC_OFF). Each tick is a 4x multiple of the last so that whole
    // range fits on one slider.
    GtkWidget *max_runtime_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    max_runtime_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 15, 1);
    gtk_scale_set_draw_value(GTK_SCALE(max_runtime_scale), FALSE);
    gtk_range_set_value(GTK_RANGE(max_runtime_scale), 5);
    gtk_widget_set_size_request(max_runtime_scale, 140, -1);
    g_signal_connect(max_runtime_scale, "value-changed", G_CALLBACK(on_max_runtime_changed), nullptr);
    max_runtime_value_label = gtk_label_new(std::to_string(runtime_from_tick(5)).append(" iterations").c_str());
    gtk_widget_add_css_class(max_runtime_value_label, "caption");
    gtk_box_append(GTK_BOX(max_runtime_box), max_runtime_scale);
    gtk_box_append(GTK_BOX(max_runtime_box), max_runtime_value_label);

    gtk_box_append(GTK_BOX(config_box), labeled_column("Engine", engine_dropdown));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Evolver", evolver_dropdown));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Test", test_dropdown));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Seed (0 = random)", seed_spin));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Children", children_spin));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Hall of Fame size", famers_spin));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Randomness", randomness_spin));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Max runtime", max_runtime_box));

    // Live estimate of the population's genome storage -- the dominant cost
    // of initializing a population, and the one that can quietly balloon
    // into gigabytes for the neural engine (see estimated_bytes_per_genome).
    // Updates as Engine/Children/Hall of Fame size change, before Start is
    // ever clicked, so an unreasonable population size for this machine is
    // visible up front instead of discovered as a stall five minutes in.
    memory_estimate_label = gtk_label_new("");
    g_signal_connect(engine_dropdown, "notify::selected", G_CALLBACK(on_memory_estimate_engine_changed), nullptr);
    g_signal_connect(children_spin, "value-changed", G_CALLBACK(on_memory_estimate_spin_changed), nullptr);
    g_signal_connect(famers_spin, "value-changed", G_CALLBACK(on_memory_estimate_spin_changed), nullptr);
    update_memory_estimate();
    gtk_box_append(GTK_BOX(config_box), labeled_column("Est. population size", memory_estimate_label));

    start_button = gtk_button_new_with_label("Start / Reset");
    gtk_widget_set_valign(start_button, GTK_ALIGN_END);
    gtk_widget_add_css_class(start_button, "suggested-action");
    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_clicked), nullptr);
    gtk_box_append(GTK_BOX(config_box), start_button);

    play_button = gtk_button_new_with_label("▶ Play");
    gtk_widget_set_valign(play_button, GTK_ALIGN_END);
    gtk_widget_set_sensitive(play_button, FALSE);
    g_signal_connect(play_button, "clicked", G_CALLBACK(on_play_clicked), nullptr);
    gtk_box_append(GTK_BOX(config_box), play_button);

    GtkWidget *lists_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_vexpand(lists_box, TRUE);
    gtk_widget_set_margin_start(lists_box, 10);
    gtk_widget_set_margin_end(lists_box, 10);

    GtkWidget *left_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(left_col, TRUE);
    GtkWidget *left_title = gtk_label_new("Top 10 Champions (this generation)");
    gtk_label_set_xalign(GTK_LABEL(left_title), 0.0f);
    gtk_widget_add_css_class(left_title, "title-4");
    gtk_box_append(GTK_BOX(left_col), left_title);
    GtkWidget *left_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(left_scroll, TRUE);
    top10_list = gtk_list_box_new();
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(top10_list), TRUE);
    g_signal_connect(top10_list, "row-activated", G_CALLBACK(on_row_activated), nullptr);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(left_scroll), top10_list);
    gtk_box_append(GTK_BOX(left_col), left_scroll);
    gtk_box_append(GTK_BOX(lists_box), left_col);

    gtk_box_append(GTK_BOX(lists_box), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    GtkWidget *right_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(right_col, TRUE);
    GtkWidget *right_title = gtk_label_new("Hall of Fame (previous champions)");
    gtk_label_set_xalign(GTK_LABEL(right_title), 0.0f);
    gtk_widget_add_css_class(right_title, "title-4");
    gtk_box_append(GTK_BOX(right_col), right_title);
    GtkWidget *right_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(right_scroll, TRUE);
    fame_list = gtk_list_box_new();
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(fame_list), TRUE);
    g_signal_connect(fame_list, "row-activated", G_CALLBACK(on_row_activated), nullptr);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(right_scroll), fame_list);
    gtk_box_append(GTK_BOX(right_col), right_scroll);
    gtk_box_append(GTK_BOX(lists_box), right_col);

    // Champion code panel: the raw genome behind children[0] -- brainfuck
    // source for the BF-family engines, the synapse wiring ("code graph")
    // for the neural engine. Whatever Engine::debug() returns.
    GtkWidget *code_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    code_title_label = gtk_label_new("Champion code");
    gtk_label_set_xalign(GTK_LABEL(code_title_label), 0.0f);
    gtk_widget_add_css_class(code_title_label, "title-4");
    gtk_widget_set_margin_start(code_title_label, 10);
    gtk_widget_set_margin_top(code_title_label, 6);
    gtk_box_append(GTK_BOX(code_col), code_title_label);

    GtkWidget *code_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(code_scroll, TRUE);
    gtk_widget_set_margin_start(code_scroll, 10);
    gtk_widget_set_margin_end(code_scroll, 10);
    gtk_widget_set_margin_bottom(code_scroll, 10);
    code_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(code_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(code_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(code_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(code_view), GTK_WRAP_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(code_scroll), code_view);
    gtk_box_append(GTK_BOX(code_col), code_scroll);

    // Progress chart: same fan-chart paradigm as fan_chart.plot (grey
    // 10-90th band, red median, green best), fed live from score_history
    // instead of gnuplot polling data.txt.
    GtkWidget *chart_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *chart_title = gtk_label_new("Score distribution by generation");
    gtk_label_set_xalign(GTK_LABEL(chart_title), 0.0f);
    gtk_widget_add_css_class(chart_title, "title-4");
    gtk_widget_set_margin_start(chart_title, 10);
    gtk_widget_set_margin_top(chart_title, 6);
    gtk_box_append(GTK_BOX(chart_col), chart_title);

    chart_area = gtk_drawing_area_new();
    gtk_widget_set_hexpand(chart_area, TRUE);
    gtk_widget_set_vexpand(chart_area, TRUE);
    gtk_widget_set_margin_start(chart_area, 10);
    gtk_widget_set_margin_end(chart_area, 10);
    gtk_widget_set_margin_bottom(chart_area, 10);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(chart_area), draw_chart, nullptr, nullptr);
    gtk_box_append(GTK_BOX(chart_col), chart_area);

    // Icon names taken from the system's own icon theme (checked against the
    // active Papirus-Light theme, both also present in the Adwaita fallback)
    // rather than guessed app-specific names, which is why the previous
    // icons weren't rendering.
    GtkWidget *right_stack = adw_view_stack_new();
    adw_view_stack_add_titled_with_icon(ADW_VIEW_STACK(right_stack), code_col, "code", "Code", "text-x-generic-symbolic");
    adw_view_stack_add_titled_with_icon(ADW_VIEW_STACK(right_stack), chart_col, "chart", "Progress", "utilities-system-monitor-symbolic");
    gtk_widget_set_vexpand(right_stack, TRUE);
    detail_stack = right_stack;
    g_signal_connect(right_stack, "notify::visible-child-name", G_CALLBACK(on_detail_stack_page_changed), nullptr);

    GtkWidget *switcher = adw_view_switcher_new();
    adw_view_switcher_set_policy(ADW_VIEW_SWITCHER(switcher), ADW_VIEW_SWITCHER_POLICY_WIDE);
    adw_view_switcher_set_stack(ADW_VIEW_SWITCHER(switcher), ADW_VIEW_STACK(right_stack));

    GtkWidget *detail_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(detail_col, 6);
    gtk_box_append(GTK_BOX(detail_col), switcher);
    gtk_box_append(GTK_BOX(detail_col), right_stack);

    GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_vexpand(main_paned, TRUE);
    gtk_paned_set_start_child(GTK_PANED(main_paned), lists_box);
    gtk_paned_set_resize_start_child(GTK_PANED(main_paned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(main_paned), FALSE);
    gtk_paned_set_end_child(GTK_PANED(main_paned), detail_col);
    gtk_paned_set_resize_end_child(GTK_PANED(main_paned), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(main_paned), FALSE);
    gtk_paned_set_position(GTK_PANED(main_paned), 380);

    status_label = gtk_label_new("Configure a run and click Start / Reset.");
    gtk_label_set_xalign(GTK_LABEL(status_label), 0.0f);
    gtk_widget_set_margin_start(status_label, 10);
    gtk_widget_set_margin_end(status_label, 10);
    gtk_widget_set_margin_top(status_label, 4);
    gtk_widget_set_margin_bottom(status_label, 10);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(content), config_box);
    gtk_box_append(GTK_BOX(content), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(content), main_paned);
    gtk_box_append(GTK_BOX(content), status_label);

    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), content);

    // AdwNavigationView gives us "click a champion -> push a detail page,
    // with a free back button" for the BF interpreter / neural node views
    // (see open_detail_view) without hand-rolling any navigation chrome.
    nav_view = adw_navigation_view_new();
    AdwNavigationPage *root_page = adw_navigation_page_new(toolbar_view, "Brainfuck Evolver");
    adw_navigation_view_add(ADW_NAVIGATION_VIEW(nav_view), root_page);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), nav_view);

    main_window = window;
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), nullptr);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    // adw_application_new (not gtk_application_new): without a real
    // AdwApplication, libadwaita's style manager/accent-color/theming never
    // gets wired up, so AdwApplicationWindow renders like a plain GTK window.
    AdwApplication *app = adw_application_new("dev.pantheum.brainfuck", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
