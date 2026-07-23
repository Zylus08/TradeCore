# Contributing to TradeCore

Thank you for your interest in contributing to TradeCore! We maintain an extremely high bar for latency bounds, determinism, and code clarity. 

## Pull Request Checklist

Before submitting a Pull Request, please ensure the following:

1. **Unit Tests Pass:** Run `ctest` and ensure 100% of unit tests pass.
2. **Performance Contracts:** Run the benchmark suite. Your PR must not violate the latency bounds defined in `scripts/validate_contracts.py`. Any regression > 5% will trigger an automated failure.
3. **No Heap Allocations:** We strictly enforce a zero-allocation policy after initialization. Do not introduce `new`, `std::vector::push_back` (where it exceeds capacity), `std::string`, or any standard library structures that hit the heap on the hot path.
4. **Sanitizers:** Your code must pass AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan) checks without warnings.
5. **Formatting:** Ensure your code is formatted according to the `.clang-format` rules.

## Architectural Changes
If you are proposing a significant architectural change, please open an Issue first to discuss the design rationale and expected performance impact before writing code.
