#include "playlist.hpp"
#include <fmt/core.h>

using namespace patterns;

static void play(Iterator<Song>& it) {
    while (it.has_next()) {
        auto [title, artist, dur] = it.next();
        fmt::println("  ♪  {:<30} {}  ({}:{:02d})",
            title, artist, dur / 60, dur % 60);
    }
}

int main() {
    Playlist pl("Road Trip");
    pl.add({"Hotel California",       "Eagles",            391});
    pl.add({"Bohemian Rhapsody",       "Queen",             354});
    pl.add({"Stairway to Heaven",      "Led Zeppelin",      482});
    pl.add({"Comfortably Numb",        "Pink Floyd",        382});
    pl.add({"November Rain",           "Guns N' Roses",     537});

    fmt::println("=== {} — ordem normal ===", pl.name());
    auto fwd = pl.forward_iterator();
    play(*fwd);

    fmt::println("\n=== {} — ordem reversa ===", pl.name());
    auto rev = pl.reverse_iterator();
    play(*rev);

    return 0;
}
