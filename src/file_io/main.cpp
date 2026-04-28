// =============================================================================
//  src/file_io/main.cpp — Ponto de entrada do subprojeto file_io
// =============================================================================
//
//  Subprojeto: file_io
//  Objetivo:   Explorar toda a hierarquia de I/O em C++ de forma didática
//
//  Demos:
//    1. iostream  — cin, cout, cerr, clog, manipuladores de formato
//    2. fstream   — ifstream, ofstream, fstream, modo binário, seek
//    3. sstream   — ostringstream, istringstream, stringstream
//    4. logging   — rdbuf redirect, TeeStream, Logger com níveis
//    5. filesystem — std::filesystem: path, create, iterate, metadata
//
// =============================================================================

#include <fmt/core.h>
#include <string>

#include "demo_iostream.hpp"
#include "demo_fstream.hpp"
#include "demo_sstream.hpp"
#include "demo_logging.hpp"
#include "demo_filesystem.hpp"

// ─────────────────────────────────────────────────────────────────────────────
//  Separador visual
// ─────────────────────────────────────────────────────────────────────────────
static void cabecalho(const std::string& titulo) {
    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║  {:^56}  ║\n", titulo);
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
}

int main() {
    fmt::print("\n");
    fmt::print("┌─────────────────────────────────────────────────────────┐\n");
    fmt::print("│          cpp_academy — file_io subproject               │\n");
    fmt::print("│   Streams, File I/O, Logging e std::filesystem          │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── Demo 1: iostream ──────────────────────────────────────────────────
    cabecalho("1. iostream — cin / cout / cerr / clog");
    demo_iostream::run();

    // ── Demo 2: fstream ───────────────────────────────────────────────────
    cabecalho("2. fstream — arquivo texto e binário");
    demo_fstream::run();

    // ── Demo 3: sstream ───────────────────────────────────────────────────
    cabecalho("3. sstream — string streams");
    demo_sstream::run();

    // ── Demo 4: logging ───────────────────────────────────────────────────
    cabecalho("4. logging — rdbuf, Tee, Logger, stderr");
    demo_logging::run();

    // ── Demo 5: filesystem ────────────────────────────────────────────────
    cabecalho("5. std::filesystem — C++17");
    demo_filesystem::run();

    fmt::print("\n✓ file_io — todos os demos concluídos\n\n");
    return 0;
}
