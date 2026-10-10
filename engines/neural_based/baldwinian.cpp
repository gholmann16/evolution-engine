// Network with reward-modulated learning on (see Brain::fire and Brain::reward). Learned weights live in the loaded CSR,
// so they carry across every run() of one score() and are dropped at the next load()
class Baldwinian : public Brain {
    public:
        // Hall of famers run like inference: no learning, no write-back. Checked by pointer so every caller (opponents, display, GUI) is covered
        void load(const void * code) override {
            void ** fame = State::hall_of_fame;
            learning = !(fame && std::find(fame, fame + State::total_famers, code) != fame + State::total_famers);
            Brain::load(code);
        }
};
