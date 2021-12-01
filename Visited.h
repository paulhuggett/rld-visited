#ifndef VISITED_HPP
#define VISITED_HPP

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <vector>

class Visited {
public:
    Visited () = default;

    ///@{
    unsigned startGroup (unsigned GroupMembers);
    /// Marks given ordinal as visited.
    void visit (unsigned O);
    /// Signals that the last input has been completed and wakes up any waiting threads.
    void done ();
    /// Signals that an error was encountered and wakes up any waiting threads.
    void error ();
    ///@}

    /// Blocks until an the next input is available. Returns true if an error was
    /// signalled or all files have been completed, false otherwise.
    std::optional<unsigned> next ();
    /// Returns true if an error was signalled via a call to error().
    bool hasError () const;

private:
    mutable std::mutex Mut_;
    std::condition_variable CV_;

    std::priority_queue<unsigned, std::vector<unsigned>, std::greater<unsigned>> Visited_;
    unsigned Bias_ = 0U;

    unsigned ConsumerPos_ = 0U;
    bool Done_ = false;
    bool Error_ = false;
};

#endif // VISITED_HPP
