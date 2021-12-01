#include "Visited.h"

#include <cassert>

// start group
// ~~~~~~~~~~~
unsigned Visited::startGroup (const unsigned GroupMembers) {
    const std::lock_guard<decltype (Mut_)> _{Mut_};
    assert (GroupMembers > 0U && "A group must contain at least one member");
    const auto Result = Bias_;
#ifndef NDEBUG
    GroupRange_ = std::make_pair (Result, Result + GroupMembers);
#endif // NDEBUG
    Bias_ += GroupMembers;
    return Result;
}

// visit
// ~~~~~
void Visited::visit (const unsigned O) {
    const std::lock_guard<decltype (Mut_)> _{Mut_};
    assert (O >= GroupRange_.first && O < GroupRange_.second &&
            "O must lie within the ordinals assigned to the current group");
    ;
    Waiting_.push (O);
    CV_.notify_one ();
}

// next
// ~~~~
std::optional<unsigned> Visited::next () {
    std::unique_lock<decltype (Mut_)> Lock{Mut_};
    CV_.wait (Lock, [this] {
        return Error_ || Done_ || (!Waiting_.empty () && Waiting_.top () == ConsumerOrdinal_);
    });

    if (Error_ || (Done_ && Waiting_.empty ())) {
        return std::nullopt;
    }

    assert (!Waiting_.empty () && Waiting_.top () == ConsumerOrdinal_ &&
            "ConsumerOrdinal_ and Visited_ are not consistent");
    Waiting_.pop ();

    const auto Result = ConsumerOrdinal_;
    ++ConsumerOrdinal_;
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
