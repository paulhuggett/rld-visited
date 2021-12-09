#include "Visited.h"

#include <cassert>

// file completed
// ~~~~~~~~~~~~~~
void Visited::fileCompleted (const unsigned Ordinal) {
    const std::lock_guard<decltype (Mut_)> _{Mut_};
    assert (!Done_ && "Must not call fileCompleted() after done()");
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
