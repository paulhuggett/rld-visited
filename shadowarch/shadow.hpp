#ifndef SHADOW_HPP
#define SHADOW_HPP

#include <atomic>

class symbol;
struct compilationref;

namespace shadow {

    // Shadow pointer state transitions:
    //
    //            +------+
    //            | null |
    //            +------+
    //               |
    //               v
    //            +------+
    // +--------->| busy |<--------------+
    // |          +------+               |
    // |(1)          |                (2)|
    // |       +-----+------+            |
    // |       v            v            |
    // |  +---------+  +-----------------+
    // +--| symbol* |  | compilationref* |
    //    +---------+  +-----------------+
    //
    // Notes:
    // (1) State changes from an undef symbol to busy and back to the same undef symbol.
    // (2) We can go from an compilationref to a defined symbol.

    static_assert (
        alignof (symbol) > 1U,
        "The LSB of pointers is used to distinguish between compilationref* and symbol*");
    static_assert (
        alignof (compilationref) > 1U,
        "The LSB of pointers is used to distinguish between compilationref* and symbol*");
    constexpr auto compilationref_mask = std::uintptr_t{0x01};

    inline compilationref * as_compilationref (void const * p) {
        auto const uintptr = reinterpret_cast<std::uintptr_t> (p);
        if (uintptr & compilationref_mask) {
            return reinterpret_cast<compilationref *> (uintptr & ~compilationref_mask);
        }
        return nullptr;
    }


    class tagged_pointer {
    public:
        explicit tagged_pointer (symbol * sym)
                : ptr_{tagged (sym)} {}
        explicit tagged_pointer (compilationref * cr)
                : ptr_{tagged (cr)} {}

        void * as_void_pointer () { return ptr_; }

    private:
        void * ptr_;

        static inline void * tagged (symbol * const sym) {
            assert (as_compilationref (sym) == nullptr);
            return sym;
        }
        static inline void * tagged (compilationref * const cr) {
            assert (as_compilationref (cr) == nullptr);
            return reinterpret_cast<void *> (reinterpret_cast<std::uintptr_t> (cr) |
                                             compilationref_mask);
        }
    };

    using void_ptr = void *;
    using atomic_void_ptr = std::atomic<void *>;


    auto * const busy = reinterpret_cast<void_ptr> (std::numeric_limits<uintptr_t>::max ());

    namespace details {

        /// Performs a nullptr -> busy -> symbol*/compilationref* state transition.
        ///
        /// \tparam Create  A function with signature tagged_pointer().
        /// \param p  A pointer to the atomic to be set. This should lie within the repository
        ///   shadow memory area.
        /// \param expected  On return, the value contained by the atomic.
        /// \param create  A function called to create a new symbol or compilationref.
        template <typename Create>
        inline bool null_to_final (atomic_void_ptr * const p, void_ptr & expected, Create create) {
            expected = nullptr;
            if (p->compare_exchange_strong (expected, busy, std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
                expected = create ().as_void_pointer ();
                p->store (expected, std::memory_order_release);
                return true;
            }
            return false;
        }

        /// Performs a compilationref* -> busy -> symbol*/compilationref* state transition.
        ///
        /// \tparam CreateFromCompilationRef  A function with signature
        ///   tagged_pointer(std::atomic<void*>*, compilationref *).
        /// \param p  A pointer to the atomic to be set. This should lie within the repository
        ///   shadow memory area.
        /// \param [in,out] expected  On entry, must point to an compilationref
        ///   pointer.
        /// \param create_from_compilation_ref  A function called to update a compilationref or to
        ///   create a symbol based on the input compilationref.
        ///
        /// \note This function expected to be called from within a loop which checks the value of
        ///   expected. This enables use of compare_exchange_weak() which may be slightly faster
        ///   than compare_exchange_strong() alternative on some platforms.
        template <typename CreateFromCompilationRef>
        inline bool compilationref_to_final (atomic_void_ptr * const p, void_ptr & expected,
                                             CreateFromCompilationRef create_from_compilation_ref) {
            compilationref * const cr = as_compilationref (expected);
            assert (cr != nullptr);
            if (p->compare_exchange_weak (expected, busy, std::memory_order_acq_rel,
                                          std::memory_order_relaxed)) {
                expected = create_from_compilation_ref (p, cr).as_void_pointer ();
                p->store (expected, std::memory_order_release);
                return true;
            }
            return false;
        }

        /// Performs a symbol* -> busy -> symbol* state transition.
        ///
        /// \tparam Update  A function with signature tagged_pointer(std::atomic<void*>*, symbol*).
        /// \param p  A pointer to the atomic to be set. This should lie within the repository
        ///   shadow memory area.
        /// \param [in,out] expected  On entry, must point to a symbol pointer.
        /// \param update  A function used to update the symbol to which \p expected points. This
        ///   function may adjust the body of the symbol or point it to a different symbol instance
        ///   altogether.
        /// \returns True if the operation was performed, false if retry is necessary.
        ///
        /// \note This function expected to be called from within a loop which checks the value of
        ///   expected. This enables use of compare_exchange_weak() which may be slightly faster
        ///   than compare_exchange_strong() alternative on some platforms.
        template <typename Update>
        inline bool symbol_to_final (atomic_void_ptr * const p, void_ptr & expected,
                                     Update const update) {
            assert (as_compilationref (expected) == nullptr);
            symbol * const sym = reinterpret_cast<symbol *> (expected);
            if (p->compare_exchange_weak (expected, busy, std::memory_order_acq_rel,
                                          std::memory_order_relaxed)) {
                expected = update (p, sym).as_void_pointer ();
                p->store (expected, std::memory_order_release);
                return true;
            }
            return false;
        }

        /// \param p  A pointer to the atomic to be set. This should lie within the repository
        ///   shadow memory area.
        /// \returns The value contained within the atomic.
        inline void * spin_while_busy (atomic_void_ptr * const p) {
            void * expected = busy;
            while ((expected = p->load (std::memory_order_acquire)) == busy) {
                std::this_thread::yield ();
            }
            return expected;
        }

    } // end namespace details

    /// \tparam Create  A function with signature tagged_pointer().
    /// \tparam CreateFromCompilationRef  A function with signature
    ///   tagged_pointer(std::atomic<void*>*, compilationref *).
    /// \tparam Update A function with signature tagged_pointer(std::atomic<void*>*, symbol*).
    ///
    /// \param p  A pointer to the atomic to be set. This should lie within the repository shadow
    ///   memory area.
    /// \param create  A function called to create a new symbol or compilationref.
    /// \param create_from_compilation_ref  A function called to update an compilationref or to
    /// create a symbol
    ///   based on the input compilationref.
    /// \param update  A function used to update the symbol to which \p expected points. This
    ///   function may adjust the body of the symbol or point it to a different symbol instance
    ///   altogether.
    template <typename Create, typename CreateFromCompilationRef, typename Update>
    void set (atomic_void_ptr * const p, Create const create,
              CreateFromCompilationRef const create_from_compilation_ref, Update const update) {
        // null -> busy -> symbol*/compilationref*
        void * expected = nullptr;
        if (details::null_to_final (p, expected, create)) {
            return;
        }
        for (;;) {
            if (expected == busy) {
                expected = details::spin_while_busy (p);
            }
            if (as_compilationref (expected) != nullptr) {
                // compilationref* -> busy -> symbol*/compilationref*
                if (details::compilationref_to_final (p, expected, create_from_compilation_ref)) {
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

} // end namespace shadow

#endif // SHADOW_HPP
