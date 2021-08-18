#include "engine/encoding.hpp"

#include "util/shortcuts.hpp"

using namespace plankton;

struct EncodingHelper {
    Encoding& encoding;
    const FlowGraph& graph;
    const EExpr& key;
    
    EncodingHelper(Encoding& encoding, const FlowGraph& graph, const EExpr& key)
            : encoding(encoding), graph(graph), key(key) {}
    EncodingHelper(Encoding& encoding, const FlowGraph& graph, const SymbolDeclaration& key)
            : EncodingHelper(encoding, graph, encoding.Encode(key)) {}
    
    inline EExpr MakeContains(EMode mode, bool contained) {
        auto result = plankton::MakeVector<EExpr>(graph.nodes.size());
        for (const auto& node : graph.nodes) {
            auto inKeyset = encoding.Encode(node.Keyset(mode))(key);
            auto logicallyContained = encoding.EncodeLogicallyContains(node, key, mode);
            if (!contained) logicallyContained = !logicallyContained;
            result.push_back(inKeyset && logicallyContained);
        }
        return encoding.MakeOr(result);
    }
    inline EExpr IsContained(EMode mode) { return MakeContains(mode, true); }
    inline EExpr IsNotContained(EMode mode) { return MakeContains(mode, false); }
    
    inline EExpr IsContainsUnchanged() {
        return (IsContained(EMode::PRE) == IsContained(EMode::POST)) &&
               (IsNotContained(EMode::PRE) == IsNotContained(EMode::POST));
    }
    
    inline EExpr OthersUnchanged() {
        return encoding.EncodeForAll(graph.config.GetFlowValueType().sort, [this](auto qv){
            EncodingHelper enc(encoding, graph, qv);
            return (qv != key) >> enc.IsContainsUnchanged();
        });
    }
};

EExpr Encoding::EncodeIsPure(const FlowGraph& graph) {
    return EncodeForAll(graph.config.GetFlowValueType().sort, [this, &graph](auto qv) {
        EncodingHelper enc(*this, graph, qv);
        return enc.IsContainsUnchanged();
    });
}

EExpr Encoding::EncodeContainsKey(const FlowGraph& graph, const SymbolDeclaration& key) {
    EncodingHelper enc(*this, graph, key);
    return enc.IsContained(EMode::PRE);
}

EExpr Encoding::EncodeNotContainsKey(const FlowGraph& graph, const SymbolDeclaration& key) {
    EncodingHelper enc(*this, graph, key);
    return enc.IsNotContained(EMode::PRE);
}

EExpr Encoding::EncodeIsInsertion(const FlowGraph& graph, const SymbolDeclaration& key) {
    EncodingHelper enc(*this, graph, key);
    return enc.IsNotContained(EMode::PRE) && enc.IsContained(EMode::POST) && enc.OthersUnchanged();
}

EExpr Encoding::EncodeIsDeletion(const FlowGraph& graph, const SymbolDeclaration& key) {
    EncodingHelper enc(*this, graph, key);
    return enc.IsContained(EMode::PRE) && enc.IsNotContained(EMode::POST) && enc.OthersUnchanged();
}
