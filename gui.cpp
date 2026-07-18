// GTK4 + libadwaita front end for the evolver. Reuses the same State /
// Engine / Test / Evolver registries as cli.cpp -- this is just another
// driver on top of the existing architecture, not a parallel one.
#include <gtk/gtk.h>
#include <adwaita.h>
#include <state.hpp>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>

// ---- GUI-only state (kept out of the State namespace on purpose: State is
// the shared contract with the engines/tests/evolvers, this is just widgets
// and bookkeeping for them) ----------------------------------------------

static GtkWidget *engine_dropdown, *evolver_dropdown, *test_dropdown;
static GtkWidget *seed_spin, *children_spin, *famers_spin, *randomness_spin;
static GtkWidget *start_button, *play_button;
static GtkWidget *top10_list, *fame_list;
static GtkWidget *status_label;
static GtkWidget *code_title_label, *code_view;

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

static GtkWidget * make_row(const std::string &header, const std::string &body) {
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

    return row_box;
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
        gtk_list_box_append(GTK_LIST_BOX(top10_list), make_row(header, body));
    }

    for (size_t i = 0; i < State::total_famers; i++) {
        bool claimed = i < fame_scores.size() && fame_scores[i] != SIZE_MAX;
        std::string header = claimed
            ? ("score " + std::to_string(fame_scores[i]) +
               "   size " + std::to_string(State::engine->size(State::hall_of_fame[i])))
            : "(unclaimed startup ancestor)   size " + std::to_string(State::engine->size(State::hall_of_fame[i]));
        std::string body = claimed ? sanitize(State::test->display(State::hall_of_fame[i])) : "";
        gtk_list_box_append(GTK_LIST_BOX(fame_list), make_row(header, body));
    }

    std::string status = "Generation " + std::to_string(State::runs) +
        "   best score " + std::to_string(State::children[0].score) +
        "   stagnant for " + std::to_string(State::repetitions) + " generations" +
        (sim_running ? "   (running)" : "   (paused)");
    gtk_label_set_text(GTK_LABEL(status_label), status.c_str());

    // The raw program: brainfuck source for the BF-family engines, the
    // synapse wiring ("code graph") for the neural engine -- whatever
    // Engine::debug() considers this genome's source representation.
    std::string code_header = "Champion code — generation " + std::to_string(State::runs) +
        "   score " + std::to_string(State::children[0].score) +
        "   size " + std::to_string(State::engine->size(State::children[0].code));
    gtk_label_set_text(GTK_LABEL(code_title_label), code_header.c_str());
    std::string code_text = State::engine->debug(State::children[0].code);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(code_view)), code_text.c_str(), -1);
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

    State::engine = engines[eidx];
    State::comp = competitors[eidx];
    State::evolver = evolvers[vidx];
    State::test = tests[tidx];

    State::total_creatures = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(children_spin));
    State::total_famers = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(famers_spin));
    State::def_rand = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(randomness_spin));
    size_t seed_val = (size_t)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(seed_spin));
    State::seed = seed_val ? seed_val : (size_t)time(nullptr);
    State::repetitions = 0;
    State::runs = 0;

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

    GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_vexpand(main_paned, TRUE);
    gtk_paned_set_start_child(GTK_PANED(main_paned), lists_box);
    gtk_paned_set_resize_start_child(GTK_PANED(main_paned), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(main_paned), FALSE);
    gtk_paned_set_end_child(GTK_PANED(main_paned), code_col);
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
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), toolbar_view);

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
