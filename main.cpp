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

    constexpr auto ProducerDelay = 0.5s;
    constexpr auto ConsumerDelay = 0.3s;

    void producer (Visited * const V, const std::vector<Visited::FileIndexType> & Groups) {
        auto Group = Visited::GroupType{0};
        for (const auto FilesInGroup : Groups) {
            assert (FilesInGroup >= 1 && "There must be at least one file per group");
            V->startGroup (Group, FilesInGroup);

            // A randomly ordered array from 0 to FilesInGroup. Simulates out-of-order completion of
            // each of the input files in a group.
            std::vector<Visited::FileIndexType> Files (std::size_t{FilesInGroup},
                                                       Visited::FileIndexType{0});
            std::iota (std::begin (Files), std::end (Files), Visited::FileIndexType{0});
            std::shuffle (std::begin (Files), std::end (Files),
                          std::mt19937{std::random_device{}()});

            // Tell the consumer about each visited file with a delay to simulate work.
            for (const auto File : Files) {
                std::this_thread::sleep_for (ProducerDelay);
                V->visit (Ordinal{Group, File});
            }
            ++Group;
        }
        // Tell the consumer that we're done.
        std::this_thread::sleep_for (3s);
        V->done ();
    }

    void consumer (Visited * const V) {
        for (;;) {
            const std::optional<Ordinal> O = V->next ();
            if (!O) {
                break;
            }
            std::cout << O->first << ',' << O->second << ' ';
            std::cout.flush ();
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
    // 0,0 1,0 1,1 1,2 1,3 2,0 2,1
    Visited V;
    // The number of groups, and the maximum file index within each of them is defined by the
    // 'Groups' container.
    std::vector<Visited::FileIndexType> Groups{1, 4, 2};
    std::thread P{producer, &V, std::cref (Groups)};
    std::thread C{consumer, &V};
    C.join ();
    P.join ();
}
