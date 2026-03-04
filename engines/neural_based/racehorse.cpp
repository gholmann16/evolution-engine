// A faster racehorse
class Racehorse : public BrainWrapper {
    public:
        void evolve(const void * parent, void * child, size_t randomness) override {
            return;
        }

        size_t run(const void * code, char input[256], char output[256], size_t max) override {
            return 0;
        }
};
