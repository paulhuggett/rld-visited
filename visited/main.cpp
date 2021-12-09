#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>

#include "Visited.h"

using namespace std::chrono_literals;

namespace {

    using GroupContainer = std::vector<unsigned>;

    // I use calls to sleep_for() to simulate delays caused by work being done.
    constexpr auto ProducerDelay = .1s;
    constexpr auto ConsumerDelay = .1s;

    // Returned a randomly ordered range of values [Bias, Bias+FilesInGroup). This simulates
    // out-of-order completion of the files in the group.
    std::vector<unsigned> randomizedFileCompletions (const unsigned FilesInGroup,
                                                     const unsigned Bias) {
        static auto RNG = std::mt19937{std::random_device{}()};
        std::vector<unsigned> Files (std::size_t{FilesInGroup});
        auto First = std::begin (Files);
        auto Last = std::end (Files);
        std::iota (First, Last, Bias);
        std::shuffle (First, Last, RNG);
        return Files;
    }

    void producer (Visited * const V, GroupContainer && Groups) {
        auto Ordinal = 0U;
        for (const auto FilesInGroup : Groups) {
            assert (FilesInGroup >= 1 && "There must be at least one file per group");
            // Tell the consumer about each visited file with a delay to simulate work.
            for (const auto File : randomizedFileCompletions (FilesInGroup, Ordinal)) {
                std::this_thread::sleep_for (ProducerDelay);
                V->fileCompleted (Ordinal++);
            }
        }
        // Tell the consumer that we're done.
        // The linker signals done when all inputs are processed or there are no strong undefs
        // remaining.
        V->done ();
    }

    void consumer (Visited * const V) {
        auto separator = "";
        while (const std::optional<unsigned> InputOrdinal = V->next ()) {
            std::cout << separator << *InputOrdinal << std::flush;
            separator = " ";
            std::this_thread::sleep_for (ConsumerDelay);
        }
        std::cout << std::endl;
        if (V->hasError ()) {
            std::cerr << "There was an error.\n";
        }
    }

} // end anonymous namespace

// The expected output consists of integers in order from 0 to 60.
int main () {
    Visited V;
    // The number of groups, and the maximum file index within each of them is defined by the
    // container passed as the producer's second argument. The group container passed to the
    // producer thread defines the following groups:
    //
    // | Group #    | 0 | 1 |  2 |
    // | ---------- | - | - | -- |
    // | File Index | 0 | 1 | 41 |
    // |            |   | 2 | 42 |
    // |            |   | 3 | 43 |
    // |            |   | … |  … |
    //
    // The producer thread deliberately shuffles the order in which visit() is called for group
    // members to the simulate the unpredicable time taken for symbol resolution.
    std::thread P{producer, &V, GroupContainer{1, 40, 20}};
    std::thread C{consumer, &V};
    C.join ();
    P.join ();
}
