#ifndef VISITED_HPP
#define VISITED_HPP

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <unordered_set>
#include <vector>

class Visited {
public:
    Visited () = default;

    ///@{
    /// Producer API.

    /// Marks the file with the given ordinal as ready for layout.
    void fileCompleted (unsigned Ordinal);
    /// Signals that the last input from the last group has been completed. Wakes up any waiting
    /// threads.
    void done ();
    /// Signals that an error was encountered and wakes up any waiting threads.
    void error ();
    ///@}

    ///@{
    /// Consumer API.

    /// Blocks until an the next input is available.
    /// \returns Has the next ordinal value on success. If the optional<> has no value, either there
    /// was an error or the last file ordinal was already returned.
    std::optional<unsigned> next ();
    ///@}

    /// Returns true if an error was signalled via a call to error().
    bool hasError () const;

private:
    /// Mutex synchonizes access to members of this instance.
    mutable std::mutex Mut_;
    /// Synchonizes producer (symbol resolution) and consumer (layout) threads.
    std::condition_variable CV_;

    /// An ordered collection of the files ready for processing by layout.
    std::priority_queue<unsigned, std::vector<unsigned>, std::greater<unsigned>> Waiting_;
#ifndef NDEBUG
    std::unordered_set<unsigned> Visited_;
#endif // NDEBUG
    unsigned ConsumerOrdinal_ = 0U;
    bool Done_ = false;
    bool Error_ = false;
};

#endif // VISITED_HPP
