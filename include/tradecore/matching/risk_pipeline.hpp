#pragma once

#include "tradecore/matching/order_action.hpp"
#include <string>
#include <tuple>

namespace tradecore::matching {

struct RiskResult {
    bool ok{true};
    std::string reason{};
};

// Base interface concept (implicit via template now, but documented here)
// class RiskRule {
//     [[nodiscard]] RiskResult evaluate(const OrderAction& order) const noexcept;
// };

// Decoupled Risk Pipeline using compile-time polymorphism (variadic templates)
// Unrolls the pipeline loop, completely avoiding vtable lookups and indirect branching.
template <typename... Rules>
class RiskPipeline {
public:
    explicit RiskPipeline(Rules... rules) : rules_(std::move(rules)...) {}

    [[nodiscard]] RiskResult evaluate(const OrderAction& order) const noexcept {
        return evaluate_impl(order, std::index_sequence_for<Rules...>{});
    }

private:
    std::tuple<Rules...> rules_;

    template <std::size_t... Is>
    [[nodiscard]] RiskResult evaluate_impl(const OrderAction& order, std::index_sequence<Is...>) const noexcept {
        RiskResult result{};
        
        // Fold expression: short-circuiting logical AND over all rule evaluations
        bool passed = (... && (
            result = std::get<Is>(rules_).evaluate(order),
            result.ok
        ));

        if (!passed) {
            return result;
        }
        
        return RiskResult{true, ""};
    }
};

// Example Risk Rules

class MaxOrderSizeCheck {
public:
    explicit MaxOrderSizeCheck(uint32_t max_qty) : max_qty_(max_qty) {}

    [[nodiscard]] RiskResult evaluate(const OrderAction& order) const noexcept {
        if (order.qty > max_qty_) {
            return RiskResult{false, "Order quantity exceeds maximum allowed size"};
        }
        return RiskResult{true, ""};
    }

private:
    uint32_t max_qty_;
};

class FatFingerPriceCheck {
public:
    explicit FatFingerPriceCheck(uint32_t min_price, uint32_t max_price) 
        : min_price_(min_price), max_price_(max_price) {}

    [[nodiscard]] RiskResult evaluate(const OrderAction& order) const noexcept {
        // Market orders have price 0, so bypass check
        if (order.type == OrderType::Market) return RiskResult{true, ""};
        
        if (order.price < min_price_ || order.price > max_price_) {
            return RiskResult{false, "Fat finger price violation"};
        }
        return RiskResult{true, ""};
    }

private:
    uint32_t min_price_;
    uint32_t max_price_;
};

// Convenience alias for no risk checks
using EmptyRiskPipeline = RiskPipeline<>;

} // namespace tradecore::matching
