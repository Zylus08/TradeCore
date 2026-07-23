#pragma once

#include "tradecore/matching/trade.hpp"
#include "tradecore/common/static_vector.hpp"

namespace tradecore::matching {

enum class ExecutionStatus : uint8_t {
    Accepted,
    RejectedRisk,
    RejectedInvalid,
    Filled,
    PartiallyFilled
};

struct ExecutionResult {
    ExecutionStatus status;
    
    // We use a StaticVector to avoid heap allocations. 1000 trades per match is highly conservative.
    static constexpr std::size_t kMaxTradesPerMatch = 1000;
    tradecore::common::StaticVector<Trade, kMaxTradesPerMatch> trades; 
};

} // namespace tradecore::matching
