#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include <atomic>
#include <list>
#include <mutex>
#include <vector>

#include "archdef.hpp"
#include "repo.hpp"
#include "symbol.hpp"

struct context {
    template <typename RepoBuilderFn>
    explicit context (RepoBuilderFn const build_repository)
            : repo{build_repository ()}
            , shadow (repo.size) {}

    auto shadow_pointer (address const address) noexcept {
        return reinterpret_cast<std::atomic<void *> *> (shadow.data () + address.raw ());
    }

    std::string name (address n) const { return repo.names.find (n)->second; }

    repository repo;
    std::vector<uint8_t> shadow;

    std::mutex symbols_mutex;
    std::list<symbol> symbols;

    std::list<archdef> archdefs;
    undefined_symbols undefs;
};

#endif // CONTEXT_HPP
