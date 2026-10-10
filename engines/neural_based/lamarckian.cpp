// Same learning as Baldwinian, but every reward() writes the learned weights back into the genome so children inherit them
class Lamarckian : public Baldwinian {
    private:
        std::string * genome = nullptr;

    public:
        void load(const void * code) override {
            genome = const_cast<std::string *>(reinterpret_cast<const std::string *>(code));
            Baldwinian::load(code);
        }

        void reward(float cost) override {
            Brain::reward(cost);
            if (!learning)
                return;
            // Rewritten in CSR order (grouped by source); evolve() and equal() don't care about synapse order.
            // Synapses reward() zeroed are pruned. Zero never grows back, so survivors only shrink and always fit
            Synapse * data = reinterpret_cast<Synapse *>(genome->data());
            int at = 0;
            for (int i = 0; i < EXCLUDING; i++)
                for (int s = head[i]; s < tail[i]; s++)
                    if (synapses[s].weight != 0)
                        data[at++] = {(unsigned short)i, synapses[s].output, synapses[s].weight};
            genome->resize(at * sizeof(Synapse));
        }
};
