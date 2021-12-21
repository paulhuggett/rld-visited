#ifndef GROUP_HPP
#define GROUP_HPP

#include <mutex>
#include <unordered_set>

#include "repo.hpp"

class group_set {
public:
    void insert (digest const compilation) {
        std::lock_guard<std::mutex> _{mutex};
        m.insert (compilation);
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
    std::unordered_set<digest> m;
    std::mutex mutex;
};

#endif // GROUP_HPP
