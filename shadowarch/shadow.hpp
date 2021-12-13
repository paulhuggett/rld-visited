#ifndef SHADOW_HPP
#define SHADOW_HPP

#include <atomic>

class symbol;
struct archdef;

namespace shadow {

    constexpr auto archdef_mask = std::uintptr_t{0x01};

    // Shadow pointer state transitions:
    //
    //            +------+
    //            | null |
    //            +------+
    //               |
    //               v
    //            +------+
    // +--------->| busy |<----------+
    // |          +------+           |
    // |(1)          |            (2)|
    // |       +-----+------+        |
    // |       v            v        |
    // |  +---------+  +----------+  |
    // +--| symbol* |  | archdef* |--+
    //    +---------+  +----------+
    //
    // Notes:
    // (1) State changes from an undef symbol to busy and back to the same undef symbol.
    // (2) We can go from an archdef to a defined symbol.

    inline archdef * as_archdef (void const * p) {
        auto const uintptr = reinterpret_cast<std::uintptr_t> (p);
        if (uintptr & archdef_mask) {
            return reinterpret_cast<archdef *> (uintptr & ~archdef_mask);
        }
        return nullptr;
    }

    inline void * tagged (symbol * const sym) {
        assert (as_archdef (sym) == nullptr);
        return sym;
    }
    inline void * tagged (archdef * const ad) {
        assert (as_archdef (ad) == nullptr);
        return reinterpret_cast<void *> (reinterpret_cast<std::uintptr_t> (ad) | archdef_mask);
    }

    using void_ptr = void *;
    using atomic_void_ptr = std::atomic<void *>;

    auto * const busy = reinterpret_cast<void_ptr> (std::numeric_limits<uintptr_t>::max ());

    namespace details {

        template <typename Create>
        inline bool null_to_final (atomic_void_ptr * const p, void_ptr & expected, Create create) {
            expected = nullptr;
            if (p->compare_exchange_strong (expected, busy, std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
                p->store (tagged (create ()), std::memory_order_release);
                return true;
            }
            return false;
        }

        // archdef* -> busy -> symbol*/archdef*
        template <typename CreateFromArchdef>
        inline bool archdef_to_final (atomic_void_ptr * const p, void_ptr & expected,
                                      CreateFromArchdef create_from_archdef) {
            archdef * const ad = as_archdef (expected);
            assert (ad != nullptr);
            if (p->compare_exchange_weak (expected, busy, std::memory_order_acq_rel,
                                          std::memory_order_relaxed)) {
                p->store (tagged (create_from_archdef (ad)), std::memory_order_release);
                return true;
            }
            return false;
        }

        // \tparam Update A function with signature symbol*(symbol*).
        // \param p  A pointer to the atomic to be set.
        // \param expected  On entry, it must hold a symbol pointer.
        // \param update  A function used to update the symbol to which \p expected points. This
        //   function may adjust the body of the symbol or point it to a different symbol instance
        //   altogether. \returns True if the operation was performed, false if retry is necessary.
        template <typename Update>
        inline bool symbol_to_final (atomic_void_ptr * const p, void_ptr & expected,
                                     Update const update) {
            assert (as_archdef (expected) == nullptr);
            symbol * const sym = reinterpret_cast<symbol *> (expected);
            // symbol* -> busy -> symbol*
            if (p->compare_exchange_weak (expected, busy, std::memory_order_acq_rel,
                                          std::memory_order_relaxed)) {
                symbol * const sym2 = update (sym);
                p->store (tagged (sym2), std::memory_order_release);
                return true;
            }
            return false;
        }

        inline void * spin_while_busy (atomic_void_ptr * const p) {
            void * expected = busy;
            while ((expected = p->load (std::memory_order_acquire)) == busy) {
                std::this_thread::yield ();
            }
            return expected;
        }

    } // end namespace details

    // \tparam Update A function with signature symbol*(symbol*).
    // \param update  A function used to update the symbol to which \p expected points. This
    //   function may adjust the body of the symbol or point it to a different symbol instance
    //   altogether.
    template <typename Create, typename CreateFromArchdef, typename Update>
    void set (atomic_void_ptr * const p, Create const create,
              CreateFromArchdef const create_from_archdef, Update const update) {
        // null -> busy -> symbol*/archdef*
        void * expected = nullptr;
        if (details::null_to_final (p, expected, create)) {
            return;
        }
        for (;;) {
            if (expected == busy) {
                expected = details::spin_while_busy (p);
            }
            if (archdef * const ad = as_archdef (expected)) {
                // archdef* -> busy -> symbol*/archdef*
                if (details::archdef_to_final (p, expected, create_from_archdef)) {
                    return;
                }
            } else {
                // symbol* -> busy -> symbol*
                if (details::symbol_to_final (p, expected, update)) {
                    return;
                }
            }
        }
    }


    template <typename Create, typename CreateFromArchdef>
    void set (atomic_void_ptr * const p, Create const create,
              CreateFromArchdef const create_from_archdef) {
        // null -> busy -> symbol*
        void * expected = nullptr;
        if (details::null_to_final (p, expected, create)) {
            return;
        }
        for (;;) {
            if (expected == busy) {
                expected = details::spin_while_busy (p);
            }
            if (archdef * const ad = as_archdef (expected)) {
                // archdef* -> busy -> symbol*/archdef*
                if (details::archdef_to_final (p, expected, create_from_archdef)) {
                    return;
                }
            } else {
                // This is already a symbol*.
                return;
            }
        }
    }

} // end namespace shadow

#endif // SHADOW_HPP
