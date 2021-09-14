#include "engine/util.hpp"

#include "util/shortcuts.hpp"
#include "util/log.hpp"

using namespace plankton;


void plankton::AddPureCheck(const FlowGraph& graph, Encoding& encoding, const std::function<void(bool)>& acceptPurity) {
    auto isPure = encoding.EncodeIsPure(graph);
    encoding.AddCheck(isPure, acceptPurity);
}

void plankton::AddPureSpecificationCheck(const FlowGraph& graph, Encoding& encoding, const ObligationAxiom& obligation,
                                         const std::function<void(std::optional<bool>)>& acceptFulfillment) {
    auto isPure = encoding.EncodeIsPure(graph);
    auto isKeyContained = encoding.EncodeContainsKey(graph, obligation.key->Decl());
    auto isKeyNotContained = encoding.EncodeNotContainsKey(graph, obligation.key->Decl());
    
    auto isContained = isPure && isKeyContained && !isKeyNotContained;
    auto isNotContained = isPure && isKeyNotContained && !isKeyContained;
    
    encoding.AddCheck(isContained, [spec = obligation.spec, callback = acceptFulfillment](bool holds) {
        // DEBUG(" ** isContained=" << holds << std::endl)
        if (!holds) { callback(std::nullopt); return; }
        switch (spec) {
            case Specification::CONTAINS: callback(true); break;
            case Specification::INSERT: callback(false); break;
            case Specification::DELETE: callback(std::nullopt); break;
        }
    });
    encoding.AddCheck(isNotContained, [spec = obligation.spec, callback = acceptFulfillment](bool holds) {
        // DEBUG(" ** isNotContained=" << holds << std::endl)
        if (!holds) { callback(std::nullopt); return; }
        switch (spec) {
            case Specification::CONTAINS: callback(false); break;
            case Specification::INSERT: callback(std::nullopt); break;
            case Specification::DELETE: callback(false); break;
        }
    });
}

void plankton::AddImpureSpecificationCheck(const FlowGraph& graph, Encoding& encoding, const ObligationAxiom& obligation,
                                           const std::function<void(std::optional<bool>)>& acceptFulfillment) {
    auto isUpdate = encoding.Bool(false);
    switch (obligation.spec) {
        case Specification::CONTAINS: return;  // always pure
        case Specification::INSERT: isUpdate = encoding.EncodeIsInsertion(graph, obligation.key->Decl()); break;
        case Specification::DELETE: isUpdate = encoding.EncodeIsDeletion(graph, obligation.key->Decl()); break;
    }
    encoding.AddCheck(isUpdate, [callback = acceptFulfillment](bool isDesiredUpdate) {
        if (isDesiredUpdate) callback(true);
        else callback(std::nullopt);
    });
}
