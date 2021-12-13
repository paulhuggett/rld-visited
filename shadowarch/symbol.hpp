#ifndef SYMBOL_HPP
#define SYMBOL_HPP

#include <cassert>
#include <optional>
#include <mutex>
#include <unordered_set>
#include <type_traits>

#include "repo.hpp"

template <typename T>
struct remove_cvref {
    typedef std::remove_cv_t<std::remove_reference_t<T>> type;
};
template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

template <typename LockType>
struct is_lock_type : public std::false_type {};
template <>
struct is_lock_type<std::lock_guard<std::mutex>> : public std::true_type {};
template <>
struct is_lock_type<std::unique_lock<std::mutex>> : public std::true_type {};

template <typename T>
inline constexpr bool is_lock_type_v = is_lock_type<T>::value;


struct context;

class symbol {
public:
    // Create an undef symbol.
    explicit symbol (address const name)
            : name_{name}
            , ordinal_{std::nullopt} {}
    // Create a defined symbol.
    symbol (address const name, unsigned const ordinal)
            : name_{name}
            , ordinal_{ordinal} {}

    void set_ordinal (unsigned ordinal) {
        std::lock_guard<std::mutex> _{mutex_};
        assert (!this->ordinal_.has_value ());
        this->ordinal_ = ordinal;
    }

    template <typename LockType,
              typename = typename std::enable_if_t<is_lock_type_v<remove_cvref_t<LockType>>>>
    bool is_def (LockType &&) const noexcept {
        return this->ordinal_.has_value ();
    }
    bool is_def () const noexcept { return this->is_def (std::lock_guard<std::mutex>{mutex_}); }

    constexpr address name () const noexcept { return name_; }
    std::unique_lock<std::mutex> take_lock () { return std::unique_lock<std::mutex>{mutex_}; }

private:
    mutable std::mutex mutex_;
    address const name_;
    std::optional<unsigned> ordinal_;
};


// create a defined symbol.
symbol * new_symbol (context & context, address const name, unsigned const ordinal);
// create an undef symbol.
symbol * new_symbol (context & context, address const name);



class undefined_symbols {
public:
    void erase (address const d) {
        std::lock_guard<std::mutex> _{mutex_};
        assert (undefs_.find (d) != undefs_.end ());
        undefs_.erase (d);
    }

    void add (address const d) {
        std::lock_guard<std::mutex> _{mutex_};
        undefs_.insert (d);
    }

    bool empty () const {
        std::lock_guard<std::mutex> _{mutex_};
        return undefs_.empty ();
    }

    template <typename Function>
    void iterate (Function function) {
        std::lock_guard<std::mutex> _{mutex_};
        for (auto const & d : undefs_) {
            function (d);
        }
    }

private:
    mutable std::mutex mutex_;
    std::unordered_set<address> undefs_;
};


#endif // SYMBOL_HPP
