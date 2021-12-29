#ifndef GROUP_HPP
#define GROUP_HPP

#include <mutex>
#include <unordered_set>

#include "repo.hpp"

class group_set {
public:
    void insert (std::atomic<void *> * const ref) {
        std::lock_guard<std::mutex> _{mutex_};
        m_.insert (ref);
    }

    bool clear () {
        std::lock_guard<std::mutex> _{mutex_};
        bool more = !m_.empty ();
        m_.clear ();
        return more;
    }

    template <typename Function>
    void for_each (Function function) {
        std::lock_guard<std::mutex> _{mutex_};
        for (auto const & s : m_) {
            function (s);
        }
    }

private:
    std::unordered_set<std::atomic<void *> *> m_;
    std::mutex mutex_;
};

#endif // GROUP_HPP
