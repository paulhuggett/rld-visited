#ifndef ARCHDEF_HPP
#define ARCHDEF_HPP

#include <string>
#include <ostream>

#include "repo.hpp"

using arch_position = std::pair<unsigned, unsigned>;

inline std::ostream & operator<< (std::ostream & os, arch_position const & position) {
    return os << '(' << position.first << ',' << position.second << ')';
}

struct archdef {
    explicit archdef (digest const compilation_, std::string origin_,
                      arch_position const position_) noexcept
            : compilation{compilation_}
            , origin{std::move (origin_)}
            , position{position_} {}
    archdef (archdef const &) = delete;
    archdef (archdef &&) noexcept = delete;

    ~archdef () noexcept = default;

    archdef & operator= (archdef const &) = delete;
    archdef & operator= (archdef &&) noexcept = delete;

    digest const compilation;
    std::string const origin;
    arch_position const position;
};

#endif // ARCHDEF_HPP
