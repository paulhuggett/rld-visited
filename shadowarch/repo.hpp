#ifndef REPO_HPP
#define REPO_HPP

#include <functional>
#include <ostream>
#include <unordered_map>
#include <utility>
#include <vector>

struct address {
    constexpr std::uintptr_t raw () const noexcept { return v; }

    constexpr bool operator== (address const rhs) const noexcept { return v == rhs.v; }
    constexpr bool operator!= (address const rhs) const noexcept { return !operator== (rhs); }

    constexpr address operator+ (std::uintptr_t x) const noexcept { return {v + x}; }
    std::uintptr_t v;
};

template <>
struct std::hash<address> {
    std::size_t operator() (address addr) const noexcept {
        auto x = addr.v;
        return std::hash<decltype (x)>{}(x);
    }
};

struct digest {
    constexpr bool operator== (digest const rhs) const noexcept { return v == rhs.v; }
    constexpr bool operator!= (digest const rhs) const noexcept { return !operator== (rhs); }

    unsigned long v;
};

std::ostream & operator<< (std::ostream & os, digest const d);

template <>
struct std::hash<digest> {
    std::size_t operator() (digest d) const noexcept { return std::hash<decltype (d.v)>{}(d.v); }
};


struct fragment {
    fragment (std::initializer_list<address> && references_)
            : references{references_} {}

    std::vector<address> const references;
};

struct compilation {
    struct definition {
        constexpr definition (address const name_, digest const fragment_) noexcept
                : name{name_}
                , fragment{fragment_} {}
        address name;
        digest fragment;
    };

    explicit compilation (std::initializer_list<definition> && definitions_)
            : definitions{definitions_} {}

    std::vector<definition> const definitions;
};

struct repository {
    repository () = default;
    repository (repository const & rhs) = delete;
    repository (repository && rhs) noexcept
            : names{std::move (rhs.names)}
            , fragments{std::move (rhs.fragments)}
            , compilations{std::move (rhs.compilations)} {}

    ~repository () noexcept = default;

    repository & operator= (repository const &) = delete;
    repository & operator= (repository &&) noexcept = delete;

    std::unordered_map<address, std::string> names;
    std::unordered_map<digest, fragment> fragments;
    std::unordered_map<digest, compilation> compilations;
    std::size_t size = 0U;
};

#endif // REPO_HPP
