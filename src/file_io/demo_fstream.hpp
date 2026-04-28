#pragma once
// =============================================================================
//  demo_fstream.hpp — Streams de arquivo: ifstream, ofstream, fstream
// =============================================================================
//
//  Classes:
//    • std::ofstream  — escrita em arquivo   (herda ostream)
//    • std::ifstream  — leitura de arquivo   (herda istream)
//    • std::fstream   — leitura E escrita    (herda iostream)
//
//  Modos de abertura (std::ios::*):
//    in        — abrir para leitura
//    out       — abrir para escrita (trunca por padrão)
//    app       — append: sempre escreve ao final
//    ate       — at-end: move ponteiro para o fim ao abrir
//    trunc     — trunca arquivo ao abrir (padrão de out)
//    binary    — modo binário (sem conversão de \n)
//
//  Combinações comuns:
//    ofstream f("x");                   → out | trunc
//    ofstream f("x", ios::app);         → out | app
//    fstream  f("x", ios::in|ios::out); → lê e escreve (não trunca)
//
// =============================================================================

#include <fmt/core.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

namespace demo_fstream {

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  1. ofstream — escrita de texto
// ─────────────────────────────────────────────────────────────────────────────
void demo_escrita_texto() {
    fmt::print("\n══ 1. ofstream — escrita de texto ══\n");

    const std::string path = "/tmp/cpp_academy_texto.txt";

    // ── Escrita simples (trunca arquivo se existir) ───────────────────────
    {
        std::ofstream ofs(path);            // modo padrão: out | trunc
        if (!ofs) {
            fmt::print("  [ERRO] não abriu {}\n", path);
            return;
        }

        ofs << "Linha 1: Hello, C++!\n";
        ofs << "Linha 2: " << 42 << " — " << 3.14 << "\n";
        ofs << "Linha 3: " << std::boolalpha << true << "\n";

        // ofs fecha e faz flush automaticamente ao sair de escopo (RAII)
        fmt::print("  escrito em {}\n", path);
    }

    // ── Append: adiciona sem truncar ──────────────────────────────────────
    {
        std::ofstream ofs(path, std::ios::app);
        ofs << "Linha 4: adicionada com ios::app\n";
        fmt::print("  append feito\n");
    }

    // ── Verificar tamanho resultante ──────────────────────────────────────
    fmt::print("  tamanho final: {} bytes\n", fs::file_size(path));
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. ifstream — leitura de texto
// ─────────────────────────────────────────────────────────────────────────────
void demo_leitura_texto() {
    fmt::print("\n══ 2. ifstream — leitura de texto ══\n");

    const std::string path = "/tmp/cpp_academy_texto.txt";

    // ── Leitura linha por linha ───────────────────────────────────────────
    {
        std::ifstream ifs(path);
        if (!ifs) { fmt::print("  [ERRO] arquivo não encontrado\n"); return; }

        fmt::print("  [linha por linha]\n");
        std::string linha;
        int i = 0;
        while (std::getline(ifs, linha)) {
            fmt::print("    {:2d}: {}\n", ++i, linha);
        }
    }

    // ── Leitura palavra por palavra (operator>>) ──────────────────────────
    {
        std::ifstream ifs(path);
        fmt::print("  [primeiras 5 palavras]\n  ");
        std::string palavra;
        for (int i = 0; i < 5 && ifs >> palavra; ++i) {
            fmt::print("'{}' ", palavra);
        }
        fmt::print("\n");
    }

    // ── Leitura do arquivo inteiro de uma vez ─────────────────────────────
    {
        std::ifstream ifs(path);
        // Técnica: move stream para o final, mede posição, volta ao início
        ifs.seekg(0, std::ios::end);
        std::streamsize tam = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        std::string conteudo(static_cast<size_t>(tam), '\0');
        ifs.read(conteudo.data(), tam);

        fmt::print("  [arquivo inteiro — {} bytes lidos]\n", tam);
        fmt::print("  primeiros 40 chars: {:.40}\n", conteudo);
    }

    // ── rdbuf — stream-to-stream sem intermediário ────────────────────────
    {
        std::ifstream ifs(path);
        std::ostringstream oss;
        oss << ifs.rdbuf();             // copia todo o buffer
        fmt::print("  [via rdbuf — {} bytes]\n", oss.str().size());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. fstream — leitura e escrita simultânea + posicionamento
// ─────────────────────────────────────────────────────────────────────────────
void demo_fstream_rw() {
    fmt::print("\n══ 3. fstream — leitura e escrita + seekg/seekp ══\n");

    const std::string path = "/tmp/cpp_academy_rw.txt";

    // ── Criar arquivo de 10 campos ────────────────────────────────────────
    {
        std::ofstream ofs(path);
        for (int i = 0; i < 10; ++i)
            ofs << "ITEM_" << i << "\n";
    }

    // ── Abrir para leitura e escrita sem truncar ──────────────────────────
    {
        std::fstream fs_rw(path, std::ios::in | std::ios::out);

        // Ponteiros de stream:
        //   tellg() / seekg() — get (leitura)
        //   tellp() / seekp() — put (escrita)

        // Ler primeira linha
        std::string linha;
        std::getline(fs_rw, linha);
        fmt::print("  1ª linha: '{}'\n", linha);
        fmt::print("  posição após leitura: {}\n", static_cast<long>(fs_rw.tellg()));

        // Escrever no início do arquivo (sobrescreve)
        fs_rw.seekp(0, std::ios::beg);
        fs_rw << "MODIF";  // sobrescreve os 5 primeiros bytes

        // Ir para o final e acrescentar
        fs_rw.seekp(0, std::ios::end);
        fs_rw << "ITEM_10\n";

        // Voltar e reler
        fs_rw.seekg(0, std::ios::beg);
        fmt::print("  após edição:\n");
        int i = 0;
        while (std::getline(fs_rw, linha) && i++ < 3)
            fmt::print("    {}\n", linha);
    }

    fmt::print("  [seekg()/seekp() — origem dos offsets]\n");
    fmt::print("    ios::beg = início do arquivo\n");
    fmt::print("    ios::cur = posição atual\n");
    fmt::print("    ios::end = fim do arquivo\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Modo binário — leitura e escrita de structs
// ─────────────────────────────────────────────────────────────────────────────
struct Registro {
    uint32_t id;
    float    valor;
    char     nome[16];
};

void demo_binario() {
    fmt::print("\n══ 4. Modo binário — structs no disco ══\n");

    const std::string path = "/tmp/cpp_academy_bin.dat";

    // ── Escrita binária ───────────────────────────────────────────────────
    {
        std::ofstream ofs(path, std::ios::binary);

        std::vector<Registro> dados = {
            {1, 3.14f, "alpha"},
            {2, 2.71f, "beta"},
            {3, 1.41f, "gamma"},
        };

        // write(const char* ptr, streamsize n)
        for (const auto& r : dados)
            ofs.write(reinterpret_cast<const char*>(&r), sizeof(Registro));

        fmt::print("  {} registros escritos ({} bytes cada, total={} bytes)\n",
            dados.size(), sizeof(Registro), dados.size() * sizeof(Registro));
    }

    // ── Leitura binária ───────────────────────────────────────────────────
    {
        std::ifstream ifs(path, std::ios::binary);

        Registro r;
        int count = 0;
        while (ifs.read(reinterpret_cast<char*>(&r), sizeof(Registro))) {
            fmt::print("  [{:2d}] id={:3} valor={:.2f} nome='{}'\n",
                ++count, r.id, r.valor, r.nome);
        }
        fmt::print("  gcount()={} (bytes lidos na última operação)\n",
            static_cast<long>(ifs.gcount()));
    }

    // ── Acesso aleatório — leitura do 2º registro diretamente ────────────
    {
        std::ifstream ifs(path, std::ios::binary);
        std::streamoff offset = 1 * sizeof(Registro); // 2º registro (índice 1)
        ifs.seekg(offset, std::ios::beg);

        Registro r;
        ifs.read(reinterpret_cast<char*>(&r), sizeof(Registro));
        fmt::print("  acesso aleatório → 2º registro: id={} nome='{}'\n",
            r.id, r.nome);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. RAII e verificação de erros
// ─────────────────────────────────────────────────────────────────────────────
void demo_erros_raii() {
    fmt::print("\n══ 5. RAII e estados de erro ══\n");

    // ── Arquivo inexistente ───────────────────────────────────────────────
    {
        std::ifstream ifs("/tmp/nao_existe.txt");
        if (!ifs.is_open()) {
            fmt::print("  arquivo não abriu (is_open() = false)\n");
        }

        // Bits de estado:
        fmt::print("  good()={} fail()={} bad()={} eof()={}\n",
            ifs.good(), ifs.fail(), ifs.bad(), ifs.eof());
    }

    // ── Exceções no stream ────────────────────────────────────────────────
    {
        std::ifstream ifs;
        // Ativa exceções para failbit e badbit
        ifs.exceptions(std::ios::failbit | std::ios::badbit);

        try {
            ifs.open("/tmp/nao_existe.txt");
        } catch (const std::ios_base::failure& e) {
            fmt::print("  exceção capturada: {}\n", e.what());
        }
    }

    // ── O destrutor fecha e faz flush automaticamente ─────────────────────
    fmt::print("  fstream fecha automaticamente no destrutor (RAII)\n");
    fmt::print("  mas use .close() explícito se precisar saber o resultado\n");
}

inline void run() {
    demo_escrita_texto();
    demo_leitura_texto();
    demo_fstream_rw();
    demo_binario();
    demo_erros_raii();
}

} // namespace demo_fstream
