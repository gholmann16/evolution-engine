// GTK4 + libadwaita front end for the evolver. Reuses the same State /
// Engine / Test / Evolver registries as cli.cpp -- this is just another
// driver on top of the existing architecture, not a parallel one.
#include <gtk/gtk.h>
#include <adwaita.h>
#include <cairo.h>
#include <state.hpp>
#include <engines/neural_based/brain.hpp>
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

// ---- GUI-only state (kept out of the State namespace on purpose: State is
// the shared contract with the engines/tests/evolvers, this is just widgets
// and bookkeeping for them) ----------------------------------------------

static GtkWidget *main_window;
static GtkWidget *nav_view;   // AdwNavigationView: root page + pushed detail views
static GtkWidget *engine_dropdown, *evolver_dropdown, *test_dropdown;
static GtkWidget *seed_spin, *children_spin, *famers_spin, *randomness_spin;
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

static void open_detail_view(const std::string &code_snapshot);

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
        gtk_label_set_wrap(GTK_LABEL(body_label), TRUE);
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

static void refresh_lists() {
    if (!state_ready) return;

    gtk_list_box_remove_all(GTK_LIST_BOX(top10_list));
    gtk_list_box_remove_all(GTK_LIST_BOX(fame_list));

    size_t shown = std::min<size_t>(10, State::total_creatures);
    for (size_t i = 0; i < shown; i++) {
        std::string header = "#" + std::to_string(i + 1) +
            "   score " + std::to_string(State::children[i].score) +
            "   size " + std::to_string(State::engine->size(State::children[i].code));
        std::string body = sanitize(State::test->display(State::children[i].code));
        gtk_list_box_append(GTK_LIST_BOX(top10_list), make_row(header, body, (int)i, false));
    }

    for (size_t i = 0; i < State::total_famers; i++) {
        bool claimed = i < fame_scores.size() && fame_scores[i] != SIZE_MAX;
        std::string header = claimed
            ? ("score " + std::to_string(fame_scores[i]) +
               "   size " + std::to_string(State::engine->size(State::hall_of_fame[i])))
            : "(unclaimed startup ancestor)   size " + std::to_string(State::engine->size(State::hall_of_fame[i]));
        std::string body = claimed ? sanitize(State::test->display(State::hall_of_fame[i])) : "";
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
static void draw_chart(GtkDrawingArea*, cairo_t *cr, int width, int height, gpointer) {
    if (score_history.size() < 2) {
        cairo_set_source_rgba(cr, 0.55, 0.55, 0.55, 0.9);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 13);
        cairo_move_to(cr, 12, height / 2.0);
        cairo_show_text(cr, "Not enough generations yet -- press Play.");
        return;
    }

    const size_t n = score_history.size();
    size_t max_val = 0, min_val = SIZE_MAX;
    for (auto &row : score_history) {
        max_val = std::max(max_val, row[0]);    // 100th percentile (worst)
        min_val = std::min(min_val, row[28]);   // 0th percentile (best)
    }
    if (max_val == min_val) max_val = min_val + 1;

    const double pad_l = 62, pad_r = 12, pad_t = 12, pad_b = 22;
    const double plot_w = width - pad_l - pad_r;
    const double plot_h = height - pad_t - pad_b;
    if (plot_w <= 1 || plot_h <= 1) return;

    auto xf = [&](size_t i) { return pad_l + plot_w * (double)i / (double)(n - 1); };
    auto yf = [&](size_t v) { return pad_t + plot_h * (1.0 - (double)(v - min_val) / (double)(max_val - min_val)); };

    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.35);
    cairo_move_to(cr, xf(0), yf(score_history[0][10]));
    for (size_t i = 1; i < n; i++) cairo_line_to(cr, xf(i), yf(score_history[i][10]));
    for (size_t i = n; i-- > 0; ) cairo_line_to(cr, xf(i), yf(score_history[i][18]));
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_set_line_width(cr, 2.0);
    cairo_set_source_rgb(cr, 0.85, 0.15, 0.15);
    cairo_move_to(cr, xf(0), yf(score_history[0][14]));
    for (size_t i = 1; i < n; i++) cairo_line_to(cr, xf(i), yf(score_history[i][14]));
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.15, 0.65, 0.25);
    cairo_move_to(cr, xf(0), yf(score_history[0][28]));
    for (size_t i = 1; i < n; i++) cairo_line_to(cr, xf(i), yf(score_history[i][28]));
    cairo_stroke(cr);

    cairo_set_source_rgba(cr, 0.55, 0.55, 0.55, 0.9);
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11);
    char buf[64];
    snprintf(buf, sizeof(buf), "%zu", max_val);
    cairo_move_to(cr, 2, pad_t + 9);
    cairo_show_text(cr, buf);
    snprintf(buf, sizeof(buf), "%zu", min_val);
    cairo_move_to(cr, 2, pad_t + plot_h);
    cairo_show_text(cr, buf);
    snprintf(buf, sizeof(buf), "gen 1..%zu", n);
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

            srand(State::seed + State::runs);
            State::evolver->evolve();
            State::evolver->score_all();
            State::evolver->sort();

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
    for (; init_done < target; init_done++) {
        if (init_done < State::total_famers) {
            State::hall_of_fame[init_done] = State::engine->ancestor_prog();
        } else {
            size_t ci = init_done - State::total_famers;
            State::children[ci].code = State::engine->ancestor_prog();
            State::children[ci].score = State::test->score(State::children[ci].code);
        }
    }

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

static void on_start_clicked(GtkButton*, gpointer) {
    if (initializing) return;
    set_running(false);
    free_state();

    guint eidx = gtk_drop_down_get_selected(GTK_DROP_DOWN(engine_dropdown));
    guint vidx = gtk_drop_down_get_selected(GTK_DROP_DOWN(evolver_dropdown));
    guint tidx = gtk_drop_down_get_selected(GTK_DROP_DOWN(test_dropdown));

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
    size_t seed_val = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(seed_spin));
    State::seed = seed_val ? seed_val : (size_t)time(nullptr);
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

static void on_play_clicked(GtkButton*, gpointer) {
    if (!state_ready) return;
    set_running(!sim_running);
}

static void on_window_destroy(GtkWidget*, gpointer) {
    set_running(false);
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

    GtkWidget *source_view = nullptr;
    GtkTextTag *pc_tag = nullptr;
    GtkWidget *memory_view = nullptr;
    GtkWidget *output_label = nullptr;
    GtkWidget *status_label = nullptr;
    GtkWidget *play_button = nullptr;
    guint timeout_id = 0;
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
        case ',': v.memory[v.pointer] = 0; v.in_dex++; break;   // GUI always feeds zeroed input
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
    gtk_button_set_label(GTK_BUTTON(v->play_button), "⏸ Pause");
}

static void bf_on_destroy(GtkWidget*, gpointer data) {
    BFView *v = static_cast<BFView*>(data);
    bf_stop_play(v);
    delete v;
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
    GtkWidget *step_btn = gtk_button_new_with_label("Step");
    g_signal_connect(step_btn, "clicked", G_CALLBACK(bf_on_step), v);
    gtk_box_append(GTK_BOX(controls), step_btn);

    v->play_button = gtk_button_new_with_label("▶ Play");
    gtk_widget_add_css_class(v->play_button, "suggested-action");
    g_signal_connect(v->play_button, "clicked", G_CALLBACK(bf_on_play), v);
    gtk_box_append(GTK_BOX(controls), v->play_button);

    GtkWidget *reset_btn = gtk_button_new_with_label("Reset");
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(bf_on_reset), v);
    gtk_box_append(GTK_BOX(controls), reset_btn);

    gtk_box_append(GTK_BOX(content), controls);

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
// Shared between node_layout() and node_draw() so the column headers always
// line up with the nodes actually drawn under them.
static const double NODE_COL_X[3] = {70, 380, 690};
static const char * const NODE_COL_LABELS[3] = {"Input", "Hidden", "Output"};
static const double NODE_TOP_MARGIN = 50;   // leaves room for the column header row

struct NodeLayout {
    double x, y;
};

struct NodeView {
    std::vector<Synapse> wiring;
    std::vector<TraceRound> rounds;
    std::map<unsigned short, NodeLayout> pos;
    size_t round_idx = 0;

    GtkWidget *canvas = nullptr;
    GtkWidget *status_label = nullptr;
    GtkWidget *play_button = nullptr;
    guint timeout_id = 0;
};

static double node_layout(NodeView &v) {
    std::set<unsigned short> ids;
    if (!v.rounds.empty())
        for (const NeuronSample &n : v.rounds[0].neurons)
            ids.insert(n.neuron);

    std::map<int, std::vector<unsigned short>> columns;
    for (unsigned short id : ids) {
        int col = (id < INPUT_NEURONS) ? 0 : (id < EXCLUDING ? 1 : 2);
        columns[col].push_back(id);
    }

    const double spacing = 54;   // circles now run up to ~46px across; keep rows from overlapping
    double max_y = 200;
    for (auto &[col, list] : columns) {
        for (size_t i = 0; i < list.size(); i++) {
            double y = NODE_TOP_MARGIN + i * spacing;
            v.pos[list[i]] = {NODE_COL_X[col], y};
            max_y = std::max(max_y, y + 34);
        }
    }
    return max_y;
}

static void node_render(NodeView *v) {
    // round_idx now IS "how many rounds have run" -- 0 is the resting state
    // trace() prepends, so no +1 here (see trace()'s round-0 comment).
    std::string status = "Round " + std::to_string(v->round_idx) +
        " / " + std::to_string(v->rounds.empty() ? 0 : v->rounds.size() - 1) +
        "   nodes shown " + std::to_string(v->pos.size()) +
        "   synapses shown " + std::to_string(v->wiring.size());
    gtk_label_set_text(GTK_LABEL(v->status_label), status.c_str());
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
    for (int col = 0; col < 3; col++) {
        cairo_text_extents_t ext;
        cairo_text_extents(cr, NODE_COL_LABELS[col], &ext);
        cairo_move_to(cr, NODE_COL_X[col] - ext.width / 2.0 - ext.x_bearing, 20);
        cairo_show_text(cr, NODE_COL_LABELS[col]);
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
    gtk_button_set_label(GTK_BUTTON(v->play_button), "⏸ Pause");
}

static void node_on_destroy(GtkWidget*, gpointer data) {
    NodeView *v = static_cast<NodeView*>(data);
    node_stop_play(v);
    delete v;
}

static void push_node_view(const std::string &code) {
    Brain *brain = dynamic_cast<Brain*>(State::engine);
    if (!brain) return;

    NodeView *v = new NodeView();
    v->wiring = brain->clean_synapses(&code);
    // Trace against whatever the current test actually scores this genome
    // against (e.g. Output's "Hello papa"), not an arbitrary all-zero
    // placeholder -- otherwise the input column never lights up regardless
    // of what's really driving the champion.
    char trace_input[256] = {0};
    State::test->reference_input(trace_input);
    v->rounds = brain->trace(&code, trace_input);
    double content_h = node_layout(*v);

    GtkWidget *toolbar_view = adw_toolbar_view_new();
    GtkWidget *header = adw_header_bar_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(content, 10);
    gtk_widget_set_margin_end(content, 10);
    gtk_widget_set_margin_top(content, 10);
    gtk_widget_set_margin_bottom(content, 10);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    v->canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(v->canvas), 820);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(v->canvas), (int)content_h);
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

    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *step_btn = gtk_button_new_with_label("Step");
    g_signal_connect(step_btn, "clicked", G_CALLBACK(node_on_step), v);
    gtk_box_append(GTK_BOX(controls), step_btn);

    v->play_button = gtk_button_new_with_label("▶ Play");
    gtk_widget_add_css_class(v->play_button, "suggested-action");
    g_signal_connect(v->play_button, "clicked", G_CALLBACK(node_on_play), v);
    gtk_box_append(GTK_BOX(controls), v->play_button);

    GtkWidget *reset_btn = gtk_button_new_with_label("Reset");
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(node_on_reset), v);
    gtk_box_append(GTK_BOX(controls), reset_btn);

    gtk_box_append(GTK_BOX(content), controls);

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

    seed_spin = gtk_spin_button_new_with_range(0, 2000000000, 1);
    children_spin = gtk_spin_button_new_with_range(4, 200000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(children_spin), 10000);
    famers_spin = gtk_spin_button_new_with_range(1, 10000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(famers_spin), 100);
    randomness_spin = gtk_spin_button_new_with_range(1, 100000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(randomness_spin), 250);

    gtk_box_append(GTK_BOX(config_box), labeled_column("Engine", engine_dropdown));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Evolver", evolver_dropdown));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Test", test_dropdown));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Seed (0 = random)", seed_spin));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Children", children_spin));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Hall of Fame size", famers_spin));
    gtk_box_append(GTK_BOX(config_box), labeled_column("Randomness", randomness_spin));

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
