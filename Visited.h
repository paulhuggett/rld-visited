#ifndef VISITED_HPP
#define VISITED_HPP

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

using Ordinal = std::pair<std::uint16_t, std::uint16_t>;

class Visited {
public:
    using GroupType = Ordinal::first_type;
    using FileIndexType = Ordinal::second_type;

    Visited () = default;

    ///@{
    void startGroup (GroupType Group, FileIndexType FileMaxIndex);
    /// Marks given ordinal as visited.
    void visit (const Ordinal & O);
    /// Signals that the last input has been completed and wakes up any waiting threads.
    void done ();
    /// Signals that an error was encountered and wakes up any waiting threads.
    void error ();
    ///@}

    /// Blocks until a specified index is visited. Returns true if an error was
    /// signalled, false otherwise.
    std::optional<Ordinal> next ();
    /// Returns true if an error was signalled via a call to error().
    bool hasError () const;

private:
    mutable std::mutex Mut_;
    std::condition_variable CV_;

    std::priority_queue<Ordinal, std::vector<Ordinal>, std::greater<Ordinal>> Visited_;
    std::unordered_map<GroupType, FileIndexType> Grid_;
#ifndef NDEBUG
    /// The next value that should be passed as the first argument to addGroup().
    GroupType NextGroup_ = 0;
#endif
    Ordinal ConsumerPos_;
    bool Done_ = false;
    bool Error_ = false;
};

#endif // VISITED_HPP
