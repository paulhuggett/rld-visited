#ifndef COMPILATIONREF_HPP
#define COMPILATIONREF_HPP

#include <string>
#include <ostream>

#include "repo.hpp"

using arch_position = std::pair<unsigned, unsigned>;

inline std::ostream & operator<< (std::ostream & os, arch_position const & position) {
    return os << '(' << position.first << ',' << position.second << ')';
}

struct compilationref {
    explicit compilationref (digest const compilation_, std::string origin_,
                             arch_position const position_) noexcept
            : compilation{compilation_}
            , origin{std::move (origin_)}
            , position{position_} {}

    digest const compilation;
    std::string const origin;
    arch_position const position;
};

#endif // COMPILATIONREF_HPP
