#pragma once
// =============================================================================
//  demo_iostream.hpp — Streams padrão: cin, cout, cerr, clog
// =============================================================================
//
//  Hierarquia de streams em C++:
//
//       ios_base
//          └── basic_ios<CharT>
//                 ├── basic_istream<CharT>   ←─ std::istream / std::cin
//                 ├── basic_ostream<CharT>   ←─ std::ostream / std::cout / cerr / clog
//                 └── basic_iostream<CharT>  ←─ leitura E escrita
//
//  Os 4 objetos globais pré-definidos:
//    • std::cin   — entrada padrão  (stdin,  geralmente teclado)
//    • std::cout  — saída padrão    (stdout, geralmente terminal) — BUFFERED
//    • std::cerr  — erro padrão     (stderr) — NÃO buffered (flush imediato)
//    • std::clog  — log padrão      (stderr) — BUFFERED
//
// =============================================================================

#include <fmt/core.h>
#include <iostream>
#include <iomanip>   // std::setw, std::setprecision, std::left, etc.
#include <limits>    // std::numeric_limits

namespace demo_iostream {

// ─────────────────────────────────────────────────────────────────────────────
//  1. cout — saída básica e formatação com manipuladores
// ─────────────────────────────────────────────────────────────────────────────
void demo_cout_formatacao() {
    fmt::print("\n══ 1. cout — formatação com manipuladores ══\n");

    // ── Inteiros em diferentes bases ──────────────────────────────────────
    int n = 255;
    std::cout << "decimal:     " << std::dec << n << "\n";   // 255
    std::cout << "hexadecimal: " << std::hex << n << "\n";   // ff
    std::cout << "octal:       " << std::oct << n << "\n";   // 377
    std::cout << std::dec; // volta para decimal!

    // ── std::showbase: prefixos 0x / 0 ───────────────────────────────────
    std::cout << "com showbase:\n";
    std::cout << std::showbase;
    std::cout << "  hex: " << std::hex << n << "\n";   // 0xff
    std::cout << "  oct: " << std::oct << n << "\n";   // 0377
    std::cout << std::noshowbase << std::dec;

    // ── Ponto flutuante ───────────────────────────────────────────────────
    double pi = 3.14159265358979;
    std::cout << "\nfloating point:\n";
    std::cout << "  default:    " << pi << "\n";                          // 3.14159
    std::cout << "  fixed(2):   " << std::fixed
                                  << std::setprecision(2) << pi << "\n"; // 3.14
    std::cout << "  scientific: " << std::scientific
                                  << std::setprecision(4) << pi << "\n"; // 3.1416e+00
    std::cout << "  defaultfloat: ";
    std::cout.unsetf(std::ios::floatfield);       // reseta para default
    std::cout << std::setprecision(6) << pi << "\n";

    // ── Largura, alinhamento e preenchimento ──────────────────────────────
    std::cout << "\nalinhamento:\n";
    std::cout << std::setfill('.');
    std::cout << std::left  << std::setw(20) << "esquerda"  << "|\n";
    std::cout << std::right << std::setw(20) << "direita"   << "|\n";
    std::cout << std::internal << std::setw(20) << -42      << "|\n";
    std::cout << std::setfill(' '); // reset fill

    // ── Booleanos ─────────────────────────────────────────────────────────
    std::cout << "\nbool:\n";
    std::cout << "  false = " << false << "  true = " << true << "\n";
    std::cout << std::boolalpha;
    std::cout << "  false = " << false << "  true = " << true << "\n";
    std::cout << std::noboolalpha;

    // ── flush vs endl ─────────────────────────────────────────────────────
    // std::endl = '\n' + flush (mais lento)
    // '\n'      = apenas newline (mais rápido, buffer envia depois)
    std::cout << "flush manual: " << std::flush;
    std::cout << "linha com endl" << std::endl;
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. cerr vs clog — erro e log
// ─────────────────────────────────────────────────────────────────────────────
void demo_cerr_clog() {
    fmt::print("\n══ 2. cerr vs clog ══\n");

    // cerr: stderr, sem buffer — garante entrega mesmo em crash
    std::cerr << "[ERRO] cerr aparece imediatamente (unbuffered)\n";

    // clog: stderr, buffered — mais eficiente para volume de logs
    std::clog << "[LOG ] clog é buffered (mais eficiente)\n";
    std::clog.flush(); // flush manual quando necessário

    // Redirecionamento no shell (veja README):
    //   ./programa 1>saida.txt 2>erros.txt   (stdout e stderr separados)
    //   ./programa 2>&1 | tee log.txt         (ambos juntos)

    fmt::print("  → No terminal: cerr e clog vão para stderr (fd=2)\n");
    fmt::print("  → Redirecione com: ./prog 1>out.txt 2>err.txt\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. cin — leitura com tratamento de erros
// ─────────────────────────────────────────────────────────────────────────────
void demo_cin_simulado() {
    fmt::print("\n══ 3. cin — leitura e tratamento de erros (simulado) ══\n");

    // Demonstração com istringstream simulando cin (para não bloquear demo)
    // Na prática: troque std::istringstream iss(...) por std::cin

    // ── Leitura simples ───────────────────────────────────────────────────
    {
        std::istringstream iss("42 3.14 hello");
        int i; double d; std::string s;
        iss >> i >> d >> s;
        fmt::print("  lido: i={} d={:.2f} s={}\n", i, d, s);
    }

    // ── Leitura de linha inteira com getline ──────────────────────────────
    {
        std::istringstream iss("linha com espaços\noutra linha");
        std::string linha;
        while (std::getline(iss, linha)) {
            fmt::print("  getline: '{}'\n", linha);
        }
    }

    // ── Estado do stream: failbit, badbit, eofbit ─────────────────────────
    {
        std::istringstream iss("abc");  // texto onde esperamos int
        int x;
        iss >> x;

        fmt::print("  após ler int de 'abc':\n");
        fmt::print("    fail()  = {}\n", iss.fail());   // true
        fmt::print("    bad()   = {}\n", iss.bad());    // false
        fmt::print("    eof()   = {}\n", iss.eof());    // false

        iss.clear();  // limpa os bits de erro
        std::string s;
        iss >> s;     // agora funciona
        fmt::print("    após clear() + leitura: s='{}'\n", s);
    }

    // ── Ignorar resíduo após leitura ──────────────────────────────────────
    {
        std::istringstream iss("42\nproxima linha");
        int n; iss >> n;
        iss.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // descarta até '\n'
        std::string resto;
        std::getline(iss, resto);
        fmt::print("  após ignore + getline: '{}'\n", resto);
    }
}

inline void run() {
    demo_cout_formatacao();
    demo_cerr_clog();
    demo_cin_simulado();
}

} // namespace demo_iostream
