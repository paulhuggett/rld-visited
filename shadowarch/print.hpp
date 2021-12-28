#ifndef PRINT_HPP
#define PRINT_HPP

#include <atomic>
#include <iterator>
#include <algorithm>
#include <mutex>
#include <ostream>
#include <utility>

class ios_printer {
public:
    template <typename Iterator>
    struct range {
        Iterator begin;
        Iterator end;
    };

    explicit ios_printer (std::ostream & os, bool enabled = true) noexcept
            : os_{os}
            , enabled_{enabled} {}
    ios_printer (ios_printer const &) = delete;
    ios_printer (ios_printer &&) = delete;

    ios_printer & operator= (ios_printer const &) = delete;
    ios_printer & operator= (ios_printer &&) = delete;

    /// Writes one or more values to the output stream followed by a newline.
    template <typename... Args>
    std::ostream & operator() (Args &&... args) {
        std::lock_guard<std::mutex> _{mutex_};
        if (!enabled_) {
            return os_;
        }
        os_ << thread_id () << "> ";
        return this->print_one (std::forward<Args> (args)...) << '\n';
    }

private:
    static unsigned thread_id () {
        static std::atomic<unsigned> thread_count{0};
        static unsigned thread_local const id =
            thread_count.fetch_add (1U, std::memory_order_relaxed);
        return id;
    }


    std::ostream & print_one () { return os_; }

    template <typename Iterator, typename... Args>
    std::ostream & print_one (range<Iterator> && r, Args &&... args) {
        std::copy (
            r.begin, r.end,
            std::ostream_iterator<typename std::iterator_traits<Iterator>::value_type> (os_, " "));
        return print_one (std::forward<Args> (args)...);
    }

    template <typename A0, typename... Args>
    std::ostream & print_one (A0 && a0, Args &&... args) {
        os_ << a0;
        return print_one (std::forward<Args> (args)...);
    }

    std::mutex mutex_;
    std::ostream & os_;
    bool enabled_ = false;
};

template <typename Iterator>
auto make_range (Iterator begin, Iterator end) {
    return ios_printer::range<Iterator>{begin, end};
};

#endif // PRINT_HPP
