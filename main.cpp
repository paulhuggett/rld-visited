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

    // I use calls to sleep_for() to simulate delays caused by work being done.
    constexpr auto ProducerDelay = .1s;
    constexpr auto ConsumerDelay = .1s;

    void producer (Visited * const V, std::vector<unsigned> && Groups) {
        for (const auto FilesInGroup : Groups) {
            assert (FilesInGroup >= 1 && "There must be at least one file per group");
            const unsigned Bias = V->startGroup (FilesInGroup);

            // A randomly ordered range of values [Bias, Bias+FilesInGroup). The shuffle simulates
            // out-of-order completion of each  input files in the group.
            std::vector<unsigned> Files (std::size_t{FilesInGroup}, 0U);
            std::iota (std::begin (Files), std::end (Files), Bias);
            std::shuffle (std::begin (Files), std::end (Files),
                          std::mt19937{std::random_device{}()});

            // Tell the consumer about each visited file with a delay to simulate work.
            for (const auto File : Files) {
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
        for (;;) {
            const std::optional<unsigned> InputOrdinal = V->next ();
            if (!InputOrdinal) {
                // Either we're done or there was an error.
                break;
            }
            std::cout << *InputOrdinal << ' ' << std::flush;
            std::this_thread::sleep_for (ConsumerDelay);
        }
        std::cout << std::endl;
        if (V->hasError ()) {
            std::cerr << "There was an error.\n";
        }
    }

} // end anonymous namespace


int main () {
    // The expected output is:
    // 0 1 2 3 4 5 6
    Visited V;
    // The number of groups, and the maximum file index within each of them is defined by the
    // container passed as the producer's second argument.
    //
    // Group #    | 0  1  3
    // --------------------
    // File Index | 0  1  5
    //            |    2  6
    //            |    3
    //            |    4
    std::thread P{producer, &V, std::vector<unsigned>{1, 4, 2}};
    std::thread C{consumer, &V};
    C.join ();
    P.join ();
}
