#pragma once

#include "tradecore/parser/book_update.hpp"
#include "tradecore/common/assert.hpp"

namespace tradecore::parser {

class BookUpdateValidator {
public:
    // Returns true if valid, false if invalid.
    [[nodiscard]] static constexpr bool validate(const BookUpdate& update) noexcept {
#ifdef NDEBUG
        // In release mode, compile out or keep minimal if requested.
        // We keep it full here for fuzzing effectiveness as per instructions, 
        // or just let branch predictor fly through it.
#endif
        if (TRADECORE_UNLIKELY(update.price == 0 && update.type != UpdateType::CancelOrder && update.type != UpdateType::DeleteOrder)) {
            return false;
        }
        
        if (TRADECORE_UNLIKELY(update.qty == 0 && update.type != UpdateType::DeleteOrder)) {
            return false;
        }

        if (TRADECORE_UNLIKELY(update.side > 1)) {
            return false;
        }

        if (TRADECORE_UNLIKELY(static_cast<uint8_t>(update.type) < 1 || static_cast<uint8_t>(update.type) > 8)) {
            return false;
        }

        return true;
    }
};

} // namespace tradecore::parser
