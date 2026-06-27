#pragma once

#include "tr069/SoapBuilder.hpp"
#include "tr069/Types.hpp"
#include "transformer/transformer_api.hpp"

namespace tr069 {

class RpcHandler {
public:
    explicit RpcHandler(transformer::TransformerApi& transformer)
        : transformer_(transformer) {}

    RpcResult handle(const ParsedRpc& rpc) const;

private:
    transformer::TransformerApi& transformer_;
};

} // namespace tr069
