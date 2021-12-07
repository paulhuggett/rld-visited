#ifndef ARCHDEF_HPP
#define ARCHDEF_HPP

#include <string>

#include "repo.hpp"

struct archdef {
    explicit archdef (digest const compilation_, std::string origin_) noexcept
            : compilation{compilation_}
            , origin{std::move (origin_)} {}
    archdef (archdef const &) = delete;
    archdef (archdef &&) noexcept = delete;

    ~archdef () noexcept = default;

    archdef & operator= (archdef const &) = delete;
    archdef & operator= (archdef &&) noexcept = delete;

    digest const compilation;
    std::string const origin;
};
static_assert (alignof (archdef) > 1U);

constexpr auto archdef_mask = std::uintptr_t{0x01};

#endif // ARCHDEF_HPP
