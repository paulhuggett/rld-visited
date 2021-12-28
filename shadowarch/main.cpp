#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <tuple>

#include "context.hpp"
#include "group.hpp"
#include "print.hpp"
#include "shadow.hpp"
#include "symbol.hpp"

using namespace std::string_literals;
using namespace std::chrono_literals;

namespace {

    // Used to introduce artifical delays so that the timing can be perturbed.
    constexpr auto shadow_sleep = .0s;
    constexpr auto resolution_sleep = .2s;
    constexpr auto archive_sleep = .1s;

    enum { f, g, h, j };
    constexpr std::array<digest, 4> compilation_digests = {
        {{1453 /* f.o */}, {1459 /* g.o */}, {1471 /*h.o*/}, {1481 /*j.o*/}}};
    constexpr std::array<digest, 4> fragment_digests = {
        {{5641 /*f*/}, {5647 /*g*/}, {5651 /*h*/}, {5653 /*j*/}}};
    constexpr std::array<std::pair<address, char const *>, 4> const strings = {
        {{address{0}, "f"}, {address{8}, "g"}, {address{16}, "h"}, {address{24}, "j"}}};

    // | File | Source code                  |
    // | ---- | ---------------------------- |
    // | f.c  | `void f(void) { g(); h(); }` |
    // | g.c  | `void g(void) { j(); }`      |
    // | h.c  | `void h(void) {}`            |
    // | j.c  | `void j(void) {}`            |
    repository build_repository () {
        repository db;
        db.names = decltype (db.names){
            {strings[f].first, strings[f].second},
            {strings[g].first, strings[g].second},
            {strings[h].first, strings[h].second},
            {strings[j].first, strings[j].second},
        };
        db.fragments = decltype (db.fragments){
            {fragment_digests[f], fragment{strings[g].first, strings[h].first}}, // f -> g, h
            {fragment_digests[g], fragment{strings[j].first}},                   // g -> j
            {fragment_digests[h], fragment{}},                                   // h -> ∅
            {fragment_digests[j], fragment{}},                                   // j -> ∅
        };
        db.compilations = decltype (db.compilations){
            {compilation_digests[f], compilation{{strings[f].first, fragment_digests[f]}}},
            {compilation_digests[g], compilation{{strings[g].first, fragment_digests[g]}}},
            {compilation_digests[h], compilation{{strings[h].first, fragment_digests[h]}}},
            {compilation_digests[j], compilation{{strings[j].first, fragment_digests[j]}}},
        };

        for (auto const & cp : db.compilations) {
            compilation const & c = cp.second;
            for (auto const & definition : c.definitions) {
                db.size = std::max (db.size, static_cast<std::size_t> ((definition.name + sizeof (address)).raw ()));
            }
        }
        return db;
    }

    ios_printer print{std::cout, true /*enabled*/};


    void symbol_resolution (context & context, archdef * const compilationref,
                            unsigned const ordinal, group_set * const next_group) {
        auto const & compilations_index = context.repo.compilations;
        auto const & fragments_index = context.repo.fragments;
        print ("Symbol resolution for compilation ", compilationref->compilation, " (origin=\"",
               compilationref->origin, "\", ordinal=", ordinal, ')');

        compilation const & c = compilations_index.find (compilationref->compilation)->second;
        for (auto const & definition : c.definitions) {
            std::this_thread::sleep_for (resolution_sleep);

            auto const create = [&] () -> symbol * {
                print ("  Create def: ", context.name (definition.name));
                return new_symbol (context, definition.name, ordinal);
            };
            auto const create_from_archdef = [&] (std::atomic<void *> * /*p*/, archdef * /*ad*/) {
                print ("  Create def (overriding archdef): ", context.name (definition.name));
                context.undefs.erase (definition.name);
                return shadow::tagged_pointer{create ()};
            };
            auto const update = [&] (std::atomic<void *> *, symbol * const sym) {
                print ("  Undef to def: ", context.name (sym->name ()));
                assert (!sym->is_def ());
                context.undefs.erase (sym->name ());
                sym->set_ordinal (ordinal);
                return shadow::tagged_pointer{sym};
            };
            shadow::set (context.shadow_pointer (definition.name), create, create_from_archdef,
                         update);

            fragment const & f = fragments_index.find (definition.fragment)->second;
            for (address const ref : f.references) {
                std::this_thread::sleep_for (resolution_sleep);
                auto const create_undef = [&] () -> symbol * {
                    print ("  Create undef: ", context.name (ref));
                    // new symbol adds to the collection of undefs.
                    return new_symbol (context, ref);
                };
                // FIXME: name of this lambda.
                // Note that this function does not create an undef symbol, despite what its name
                // suggests. We need to keep the archdef record in the shadow memory in order that
                // the we can get the correct compilation when it comes time to turn 'next_group'
                // into the set of compilations for the next iteration. Bear in mind that a specific
                // archdef record can be replaced if we later find a definition in a library member
                // with an earlier position than the one we have here.
                auto const create_undef_from_archdef = [&] (std::atomic<void *> * const p,
                                                            archdef * const ad) {
                    print ("  archdef -> undef ", ad->position, ": ", context.name (ref));
                    next_group->insert (p);
                    context.undefs.add (ref);
                    return shadow::tagged_pointer{ad};
                };
                shadow::set (context.shadow_pointer (ref), create_undef, create_undef_from_archdef);
            }
        }
    }

    using library_member = std::tuple<digest, std::string, arch_position>;

    void archive_discovery (context & context, library_member const & lm,
                            group_set * const next_group) {
        auto const index = std::get<arch_position> (lm);
        print ("Archive Discovery for ", std::get<std::string> (lm), ", position ", index,
               ", compilation ", std::get<digest> (lm));

        auto const & compilations_index = context.repo.compilations;
        auto const & names_index = context.repo.names;


        compilation const & c = compilations_index.find (std::get<digest> (lm))->second;
        for (auto const & definition : c.definitions) {
            std::this_thread::sleep_for (archive_sleep);
            print ("  archdef: ", names_index.find (definition.name)->second);

            auto create = [&] {
                print ("    Create archdef: ", names_index.find (definition.name)->second);
                std::lock_guard<std::mutex> _{context.archdefs_mutex};
                context.archdefs.emplace_back (std::get<digest> (lm), std::get<std::string> (lm),
                                               std::get<arch_position> (lm));
                return &context.archdefs.back ();
            };
            auto const create_from_archdef = [&] (std::atomic<void *> *, archdef * const ad) {
                // There's an existing archdef for this symbol. Check the associated ordinal and
                // keep the one with the lower position.
                if (index < ad->position) {
                    print ("    Replace archdef for \"", names_index.find (definition.name)->second,
                           "\": ", ad->position, " with ", index);
                    return shadow::tagged_pointer{create ()};
                }
                print ("    Rejected: ", names_index.find (definition.name)->second,
                       " in favor of ", ad->position);
                return shadow::tagged_pointer{ad};
            };

            auto const update = [&] (std::atomic<void *> * const p, symbol * const sym) {
                auto const lock = sym->take_lock ();
                if (sym->is_def (lock)) {
                    return shadow::tagged_pointer{sym};
                }
                // A definition in an archive has matched with an undefined symbol. Turn the
                // undef into an archdef.
                assert (context.undefs.has (sym->name ()));
                next_group->insert (p);
                return shadow::tagged_pointer{create ()};
            };
            shadow::set (context.shadow_pointer (definition.name), create, create_from_archdef,
                         update);
        }
    }

    template <typename Iterator>
    void join_all (Iterator first, Iterator last) {
        std::for_each (first, last, [] (std::thread & t) {
            if (t.joinable ()) {
                t.join ();
            }
        });
    }


    template <typename T>
    struct reverse_wrapper {
        T & iterable;
    };
    template <typename T>
    auto begin (reverse_wrapper<T> w) {
        return std::rbegin (w.iterable);
    }
    template <typename T>
    auto end (reverse_wrapper<T> w) {
        return std::rend (w.iterable);
    }
    template <typename T>
    reverse_wrapper<T> reverse (T && iterable) {
        return {iterable};
    }


    std::vector<std::thread> create_arch_threads (context & context,
                                                  std::vector<library_member> const & archives,
                                                  group_set * const next_group) {
        std::vector<std::thread> arch_threads;
        arch_threads.reserve (archives.size ());
        for (auto const & arch : reverse (archives)) {
            arch_threads.emplace_back (archive_discovery, std::ref (context),
                                                  std::cref (arch), next_group);
        }
        return arch_threads;
    };


    void show_compilation_group (unsigned const ngroup, std::vector<archdef *> const & group) {
        std::vector<digest> group_compilations;

        group_compilations.reserve (group.size ());
        std::transform (std::begin (group), std::end (group),
                        std::back_inserter (group_compilations),
                        [] (archdef const * const ad) { return ad->compilation; });
        print ("Group ", ngroup, " compilations: ",
               make_range (std::cbegin (group_compilations), std::cend (group_compilations)));
    }

} // end anonymous namespace

int main () {
    print ("Main Thread");

    context context{build_repository};

    std::list<archdef> x;
    archdef * fptr = &x.emplace_back (compilation_digests[f], "f.o"s, arch_position{0, 0});
    std::vector<archdef *> const ticketed_compilations{fptr};

    // | Archive | File members | Position
    // | ------- | ------------ | ------------
    // | liba.a  | g.o j.o      | (1,0), (1,1)
    // | libb.a  | h.o          | (2,0)
    // | libc.a  | g.o          | (3,0)
    //
    // These are provided (albeit indirectly) on the command-line.

    // Position x=0 is assigned to the ticket files on the command line.
    const auto liba = 1U;
    const auto libb = 2U;
    const auto libc = 3U;
    assert ((arch_position{0, 1} < arch_position{1, 0}));

    std::vector<library_member> const archives{
        {compilation_digests[g], "liba.a(g.o)"s, std::make_pair (liba, 0U)},
        {compilation_digests[j], "liba.a(j.o)"s, std::make_pair (liba, 1U)},

        {compilation_digests[h], "libb.a(h.o)"s, std::make_pair (libb, 0U)},

        {compilation_digests[g], "libc.a(g.o)"s, std::make_pair (libc, 0U)},
    };
    bool archives_joined = false;

    auto group = ticketed_compilations;
    auto ordinal = 0U;
    bool more = false;

    auto ngroup = 0U;
    group_set next_group;

    auto archive_threads = create_arch_threads (context, archives, &next_group);
    do {
        show_compilation_group (ngroup, group);
        ++ngroup;

        std::vector<std::thread> workers;
        for (archdef * const & compilation : group) {
            workers.emplace_back (symbol_resolution, std::ref (context), compilation,
                                             ordinal++, &next_group);
        }
        join_all (std::begin (workers), std::end (workers));

        if (!archives_joined) {
            print ("Join Archive Discovery");
            join_all (std::begin (archive_threads), std::end (archive_threads));
            archives_joined = true;
        }

        group.clear ();
        next_group.for_each ([&group] (std::atomic<void *> * const p) {
            if (archdef * const ad = shadow::as_archdef (*p)) {
                group.emplace_back (ad);
            }
        });
        more = next_group.clear ();
    } while (more && !context.undefs.empty ());

    int exit_code = EXIT_SUCCESS;
    bool first = true;
    context.undefs.for_each ([&] (address const name) {
        if (first) {
            first = false;
            print ("Error. Undefined symbols:");
        }
        print (context.name (name));
        exit_code = EXIT_FAILURE;
    });
    if (exit_code == EXIT_SUCCESS) {
        print ("We have success!");
    }
    return exit_code;
}
