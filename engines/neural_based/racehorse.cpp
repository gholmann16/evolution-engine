// A faster racehorse, changes synaptic weight
class Racehorse : public Brain {
    public:
        void evolve(const void * parent, void * child, size_t randomness) override {
            return;
        }

        size_t run(char input[256], char output[256], size_t max) override {
            return 0;
        }
};
