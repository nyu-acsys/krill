#include "default_solver.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


std::deque<std::unique_ptr<heal::Annotation>> DefaultSolver::MakeStable(std::deque<std::unique_ptr<heal::Annotation>> annotations,
                                                                        const std::deque<std::unique_ptr<HeapEffect>>& interferences) const {

    // Sind Effekt-Zyklen problematisch?
    // Betrachte Effekte e1 und e2. Falls e2 anwendbar in e1.post*e1.context aber nicht in e1.pre*e1.context, dann könnte es
    // passieren, dass e1 zu präzise ist und e1 und e2 Anwendbarkeit osziliert.
    // Problem am oszilieren: ist nicht interference-free, da immer noch ein Effekt angewendet werden kann dessen post nicht vom aktuellen implied
    // man kann den context eh nicht verwenden, da man nicht weiß ob der Effekt überhaupt angewendet wird

    // TODO: prune effects e which satisfy: e.post*e.context => e.pre*e.context ? They should not do anything


    SymbolicFactory factory;
    for (const auto& effect : interferences) {
        factory.Blacklist(*effect->pre);
        factory.Blacklist(*effect->post);
        factory.Blacklist(*effect->context);
    }


    // TODO: implement
    throw std::logic_error("not yet implemented: DefaultSolver::MakeStable");
}
