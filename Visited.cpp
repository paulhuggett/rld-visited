#include "Visited.h"

#include <cassert>

// start group
// ~~~~~~~~~~~
unsigned Visited::startGroup (const unsigned FileMaxIndex) {
    const std::lock_guard<decltype (Mut_)> _{Mut_};
    assert (FileMaxIndex > 0U && "A group must contain at least one member");

    const auto Result = Bias_;
    Bias_ += FileMaxIndex;
    return Result;
}

// visit
// ~~~~~
void Visited::visit (const unsigned O) {
    const std::lock_guard<decltype (Mut_)> _{Mut_};
    Visited_.push (O);
    CV_.notify_one ();
}

// next
// ~~~~
std::optional<unsigned> Visited::next () {
    std::unique_lock<decltype (Mut_)> Lock{Mut_};
    CV_.wait (Lock, [this] {
        return Error_ || Done_ || (!Visited_.empty () && Visited_.top () == ConsumerPos_);
    });

    if (Error_ || (Done_ && Visited_.empty ())) {
        return std::nullopt;
    }

    assert (Visited_.top () == ConsumerPos_ && "ConsumerPos_ and Visited_ are not consistent");
    Visited_.pop ();

    const auto Result = ConsumerPos_;
    ++ConsumerPos_;
    return {Result};
}

// done
// ~~~~
void Visited::done () {
    const std::lock_guard<decltype (Mut_)> _{Mut_};
    Done_ = true;
    CV_.notify_all ();
}

// error
// ~~~~~
void Visited::error () {
    const std::lock_guard<decltype (Mut_)> _{Mut_};
    Error_ = true;
    CV_.notify_all ();
}

// has error
// ~~~~~~~~~
bool Visited::hasError () const {
    const std::lock_guard<decltype (Mut_)> _{Mut_};
    return Error_;
}
