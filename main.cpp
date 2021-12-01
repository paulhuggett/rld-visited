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

    // A randomly ordered range of values [Bias, Bias+FilesInGroup). The shuffle simulates
    // out-of-order completion of each  input files in the group.
    std::vector<unsigned> randomGroupCompletions (const unsigned FilesInGroup,
                                                  const unsigned Bias) {
        std::vector<unsigned> Files (std::size_t{FilesInGroup}, 0U);
        std::iota (std::begin (Files), std::end (Files), Bias);
        std::shuffle (std::begin (Files), std::end (Files), std::mt19937{std::random_device{}()});
        return Files;
    }

    void producer (Visited * const V, GroupContainer && Groups) {
        for (const auto FilesInGroup : Groups) {
            assert (FilesInGroup >= 1 && "There must be at least one file per group");
            const unsigned Bias = V->startGroup (FilesInGroup);
            // Tell the consumer about each visited file with a delay to simulate work.
            for (const auto File : randomGroupCompletions (FilesInGroup, Bias)) {
                std::this_thread::sleep_for (ProducerDelay);
                V->visit (File);
            }
        }
        // Tell the consumer that we're done.
        // The linker signals done when all inputs are processed or there are no strong undefs
        // remaining.
        V->done ();
    }

    void consumer (Visited * const V) {
        while (const std::optional<unsigned> InputOrdinal = V->next ()) {
            std::cout << *InputOrdinal << ' ' << std::flush;
            std::this_thread::sleep_for (ConsumerDelay);
        }
        std::cout << std::endl;
        if (V->hasError ()) {
            std::cerr << "There was an error.\n";
        }
    }

} // end anonymous namespace

// The expected output is:
// 0 1 2 3 4 5 6
int main () {
    Visited V;
    // The number of groups, and the maximum file index within each of them is defined by the
    // container passed as the producer's second argument. The group container passed to the
    // producer thread defines the following groups:
    //
    // Group #    | 0  1  2
    // --------------------
    // File Index | 0  1  5
    //            |    2  6
    //            |    3
    //            |    4
    //
    // The producer thread deliberately shuffles the order in which visit() is called for group
    // members to the simulate the unpredicable time taken for symbol resolution.
    std::thread P{producer, &V, GroupContainer{1, 4, 2}};
    std::thread C{consumer, &V};
    C.join ();
    P.join ();
}
