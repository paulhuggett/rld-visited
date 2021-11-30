#include "Visited.h"

#include <cassert>

// start group
// ~~~~~~~~~~~
void Visited::startGroup (const GroupType Group, const FileIndexType FileMaxIndex) {
    const std::lock_guard<decltype (Mut_)> _{Mut_};
    assert (Group == NextGroup_ && "Group number was not correct");
#ifndef NDEBUG
    ++NextGroup_;
#endif
    assert (FileMaxIndex > 0U && "A group must contain at least one member");
    ;

    assert (Grid_.find (Group) == Grid_.end () && "addGroup() called on the same group twice!");
    Grid_[Group] = FileMaxIndex;
}

// visit
// ~~~~~
void Visited::visit (const Ordinal & O) {
    {
        const std::lock_guard<decltype (Mut_)> _{Mut_};
        assert (Grid_.find (O.first) != Grid_.end () &&
                "Group was not previously added with addGroup()");
        assert (Grid_.find (O.first)->second >= O.second &&
                "Y pos was larger than the group's maximum!");
        Visited_.push (O);
    }
    CV_.notify_one ();
}

// next
// ~~~~
std::optional<Ordinal> Visited::next () {
    std::unique_lock<decltype (Mut_)> Lock{Mut_};
    CV_.wait (Lock, [this] {
        return Error_ || Done_ || (!Visited_.empty () && Visited_.top () == ConsumerPos_);
    });

    if (Error_ || (Done_ && Grid_.empty () && Visited_.empty ())) {
        return std::nullopt;
    }

    assert (Visited_.top () == ConsumerPos_ && "ConsumerPos_ and Visited_ are not consistent");
    Visited_.pop ();

    const auto Result = ConsumerPos_;
    assert (Grid_.find (ConsumerPos_.first) != Grid_.end () && "Group was not found in the grid");
    const auto FileMaxIndex = Grid_.find (ConsumerPos_.first)->second;
    ++ConsumerPos_.second;
    // Have we exhausted all of the inputs in this group?
    if (ConsumerPos_.second >= FileMaxIndex) {
        Grid_.erase (ConsumerPos_.first);
        ++ConsumerPos_.first;
        ConsumerPos_.second = 0U;
    }
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
