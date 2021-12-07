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

// file completed
// ~~~~~~~~~~~~~~
void Visited::fileCompleted (const unsigned Ordinal) {
    const std::lock_guard<decltype (Mut_)> _{Mut_};
    assert (!Done_ && "Must not call fileCompleted() after done()");
    assert (Ordinal >= GroupRange_.first && Ordinal < GroupRange_.second &&
            "Ordinal must lie within the range assigned to the current group");
    assert (Visited_.insert (Ordinal).second && "O must not have been previously visted");
    Waiting_.push (Ordinal);
    CV_.notify_one ();
}

// next
// ~~~~
std::optional<unsigned> Visited::next () {
    std::unique_lock<decltype (Mut_)> Lock{Mut_};
    for (;;) {
        const auto IsEmpty = Waiting_.empty ();
        if ((Done_ && IsEmpty) || Error_) {
            return std::nullopt;
        }
        if (!IsEmpty && Waiting_.top () == ConsumerOrdinal_) {
            Waiting_.pop ();
            return {ConsumerOrdinal_++};
        }
        CV_.wait (Lock);
    }
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
