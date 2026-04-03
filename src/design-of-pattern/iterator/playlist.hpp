#pragma once
#include "iterable.hpp"
#include "playlist_iterators.hpp"
#include "song.hpp"
#include <vector>
#include <string>

namespace patterns {

struct Playlist : Iterable<Song> {
    explicit Playlist(std::string name) : name_(std::move(name)) {}

    void add(Song song) {
        songs_.push_back(std::move(song));
    }

    const std::string& name() const { return name_; }

    std::unique_ptr<Iterator<Song>> forward_iterator() const override {
        return std::make_unique<ForwardIterator>(songs_);
    }

    std::unique_ptr<Iterator<Song>> reverse_iterator() const override {
        return std::make_unique<ReverseIterator>(songs_);
    }

private:
    std::string        name_;
    std::vector<Song>  songs_;
};

} // namespace patterns
