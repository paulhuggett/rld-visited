#include "symbol.hpp"

#include "context.hpp"

// create a defined symbol.
symbol * new_symbol (context & context, address const name, unsigned const ordinal) {
    std::lock_guard<std::mutex> _{context.symbols_mutex};
    context.symbols.emplace_back (name, ordinal);
    return &context.symbols.back ();
}

// create an undef symbol.
symbol * new_symbol (context & context, address const name) {
    std::lock_guard<std::mutex> _{context.symbols_mutex};
    context.symbols.emplace_back (name);
    context.undefs.add (name);
    return &context.symbols.back ();
}
