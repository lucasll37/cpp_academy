#pragma once
#include "iterator.hpp"
#include "song.hpp"
#include <vector>
#include <stdexcept>

namespace patterns {

// ── percorre do índice 0 até o fim ──────────────────────────────────────────
struct ForwardIterator : Iterator<Song> {
    explicit ForwardIterator(const std::vector<Song>& songs)
        : songs_(songs), index_(0) {}

    bool has_next() const override {
        return index_ < songs_.size();
    }

    Song next() override {
        if (!has_next())
            throw std::out_of_range("ForwardIterator: sem mais elementos");
        return songs_[index_++];
    }

private:
    const std::vector<Song>& songs_;
    size_t                   index_;
};

// ── percorre do último até o índice 0 ───────────────────────────────────────
struct ReverseIterator : Iterator<Song> {
    explicit ReverseIterator(const std::vector<Song>& songs)
        : songs_(songs), index_(songs.size()) {}

    bool has_next() const override {
        return index_ > 0;
    }

    Song next() override {
        if (!has_next())
            throw std::out_of_range("ReverseIterator: sem mais elementos");
        return songs_[--index_];
    }

private:
    const std::vector<Song>& songs_;
    size_t                   index_;
};

} // namespace patterns
