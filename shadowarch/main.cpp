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


    void symbol_resolution (context & context, digest const compilation_digest,
                            unsigned const ordinal, group_set * const next_group) {
        auto const & compilations_index = context.repo.compilations;
        auto const & fragments_index = context.repo.fragments;
        print ("Symbol resolution for compilation ", compilation_digest, " (ordinal=", ordinal,
               ')');

        compilation const & c = compilations_index.find (compilation_digest)->second;
        for (auto const & definition : c.definitions) {
            std::this_thread::sleep_for (resolution_sleep);

            auto const create = [&] () -> symbol * {
                print ("  Create def: ", context.name (definition.name));
                return new_symbol (context, definition.name, ordinal);
            };
            auto const create_from_archdef = [&] (archdef * const ad) {
                print ("  Create def from archdef: ", context.name (definition.name));
                next_group->insert (ad->compilation);
                return create ();
            };
            auto const update = [&] (symbol * const sym) -> symbol * {
                print ("  Undef to def: ", context.name (sym->name ()));
                address const name = sym->name ();
                context.undefs.erase (name);
                sym->set_ordinal (ordinal);
                return sym;
            };
            shadow::set (context.shadow_pointer (definition.name), create, create_from_archdef,
                         update);

            fragment const & f = fragments_index.find (definition.fragment)->second;
            for (address const ref : f.references) {
                std::this_thread::sleep_for (resolution_sleep);
                auto const create_undef = [&] () -> symbol * {
                    print ("  Create undef: ", context.name (ref));
                    return new_symbol (context, ref);
                };
                auto const create_undef_from_archdef = [&] (archdef * const ad) {
                    print ("  archdef -> undef ", ad->position, ": ", context.name (ref));
                    next_group->insert (ad->compilation);
                    return create_undef ();
                };
                shadow::set (context.shadow_pointer (ref), create_undef, create_undef_from_archdef);
            }
        }
    }

    using library_member = std::tuple<digest, std::string, arch_position>;

    void archive_discovery (context & context, library_member const & lm,
                            group_set * const next_group) {
        auto const index = std::get<arch_position> (lm);
        print ("Archive Discovery for ", std::get<std::string> (lm), " (position ", index, ')');

        auto const & compilations_index = context.repo.compilations;
        auto const & names_index = context.repo.names;


        compilation const & c = compilations_index.find (std::get<digest> (lm))->second;
        for (auto const & definition : c.definitions) {
            std::this_thread::sleep_for (archive_sleep);
            print ("archdef: ", names_index.find (definition.name)->second);

            auto create = [&] {
                print ("  Create archdef: ", names_index.find (definition.name)->second);
                std::lock_guard<std::mutex> _{context.archdefs_mutex};
                context.archdefs.emplace_back (std::get<digest> (lm), std::get<std::string> (lm),
                                               std::get<arch_position> (lm));
                return &context.archdefs.back ();
            };
            auto const create_from_archdef = [&] (archdef * const ad) {
                // There's an existing archdef for this symbol. Check the associated ordinal and
                // keep the one with the lower position.
                if (index < ad->position) {
                    return create ();
                }
                return ad;
            };
            auto const update = [&] (symbol * const sym) {
                auto const lock = sym->take_lock ();
                if (!sym->is_def (lock)) {
                    archdef * const ad = create ();
                    next_group->insert (ad->compilation);
                }
                return sym;
            };
            shadow::set (context.shadow_pointer (definition.name), create, create_from_archdef,
                         update);
        }
    }

    template <typename Iterator>
    void join_all (Iterator first, Iterator last) {
        std::for_each (first, last, [] (std::thread & t) { t.join (); });
    }

} // end anonymous namespace

int main () {
    print ("Main Thread");

    context context{build_repository};

    std::vector<digest> const ticketed_compilations{compilation_digests[f]};

    // | Archive | File members |
    // | ------- | ------------ |
    // | liba.a  | g.o j.o      |
    // | libb.a  | h.o          |
    // | libc.a  | g.o          |
    //
    // These are provided (albeit indirectly) on the command-line.

    const auto liba = 0U;
    const auto libb = 1U;
    const auto libc = 2U;
    assert ((arch_position{0, 1} < arch_position{1, 0}));
    // assert (std::make_pair (0, 0) < std::make_pair (0, 1));

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

    std::vector<std::thread> arch_threads;
    for (auto const & arch : archives) {
        arch_threads.emplace_back (archive_discovery, std::ref (context), std::cref (arch),
                                   &next_group);
    }

    // std::thread archive{archive_discovery, std::ref (context), std::cref (archives),
    // &next_group};
    do {
        print ("Group ", ngroup++, " compilations: ",
               ios_printer::range<decltype (group)::const_iterator>{std::cbegin (group),
                                                                    std::cend (group)});

        std::vector<std::thread> workers;
        for (auto const & compilation : group) {
            workers.emplace_back (symbol_resolution, std::ref (context), compilation, ordinal++,
                                  &next_group);
        }
        join_all (std::begin (workers), std::end (workers));
        if (!archives_joined) {
            print ("Join Archive Discovery");
            join_all (std::begin (arch_threads), std::end (arch_threads));
            archives_joined = true;
        }

        group.clear ();
        next_group.iterate ([&group] (digest const compilation) {
            group.emplace_back (compilation);
        });
        more = next_group.clear ();
    } while (more && !context.undefs.empty ());

    int exit_code = EXIT_SUCCESS;
    bool first = true;
    context.undefs.iterate ([&] (address const name) {
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
