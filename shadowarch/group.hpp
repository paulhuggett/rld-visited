#ifndef GROUP_HPP
#define GROUP_HPP

#include <mutex>
#include <unordered_set>

#include "repo.hpp"

class group_set {
public:
    void insert (std::atomic<void *> * const ref) {
        std::lock_guard<std::mutex> _{mutex};
        m.insert (ref);
    }

    bool clear () {
        std::lock_guard<std::mutex> _{mutex};
        bool more = !m.empty ();
        m.clear ();
        return more;
    }

    template <typename Function>
    void for_each (Function function) {
        std::lock_guard<std::mutex> _{mutex};
        for (auto const & s : m) {
            function (s);
        }
    }

private:
    std::unordered_set<std::atomic<void *> *> m;
    std::mutex mutex;
};

#endif // GROUP_HPP
