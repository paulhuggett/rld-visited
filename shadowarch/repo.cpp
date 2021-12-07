#include "repo.hpp"

std::ostream & operator<< (std::ostream & os, digest const d) {
    return os << d.v;
}
