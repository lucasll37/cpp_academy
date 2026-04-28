#pragma once
// =============================================================================
//  demo_sstream.hpp — String streams: ostringstream, istringstream, stringstream
// =============================================================================
//
//  String streams são streams que operam sobre std::string em memória.
//  Extremamente úteis para:
//    • Formatação de strings compostas (ostringstream)
//    • Parsing / deserialização de strings (istringstream)
//    • Conversão de tipos para/de string
//    • Tokenização de linhas CSV, comandos, configs
//    • Testes e simulação de cin sem bloquear o terminal
//
//  Hierarquia:
//    ostringstream  ←─ escrita em string  (herda ostream)
//    istringstream  ←─ leitura de string  (herda istream)
//    stringstream   ←─ leitura e escrita  (herda iostream)
//
// =============================================================================

#include <fmt/core.h>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <iomanip>

namespace demo_sstream {

// ─────────────────────────────────────────────────────────────────────────────
//  1. ostringstream — construção de strings formatadas
// ─────────────────────────────────────────────────────────────────────────────
void demo_ostringstream() {
    fmt::print("\n══ 1. ostringstream — construção de strings ══\n");

    // ── Concatenação eficiente (evita N alocações de + com string) ────────
    {
        std::ostringstream oss;
        oss << "Usuario=" << "lucas"
            << " | versao=" << 3
            << " | pi=" << std::fixed << std::setprecision(4) << 3.14159;

        std::string resultado = oss.str(); // extrai o conteúdo
        fmt::print("  resultado: {}\n", resultado);

        // str("") — limpa o buffer para reutilizar
        oss.str("");
        oss.clear(); // limpa bits de estado também
        oss << "reusado!";
        fmt::print("  após reutilização: {}\n", oss.str());
    }

    // ── Formatação de tabela ASCII ────────────────────────────────────────
    {
        std::ostringstream oss;
        oss << std::left;
        oss << std::setw(12) << "Nome"
            << std::setw(8)  << "Idade"
            << std::setw(12) << "Cidade" << "\n";
        oss << std::string(32, '-') << "\n";

        std::vector<std::tuple<std::string,int,std::string>> dados = {
            {"Alice", 30, "São Paulo"},
            {"Bob",   25, "Campinas"},
            {"Carol", 28, "Rio"},
        };
        for (auto& [nome, idade, cidade] : dados)
            oss << std::setw(12) << nome
                << std::setw(8)  << idade
                << std::setw(12) << cidade << "\n";

        fmt::print("  tabela:\n{}", oss.str());
    }

    // ── Conversão numérica → string (C++11: prefira std::to_string) ───────
    {
        auto to_str = [](double v, int prec = 2) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(prec) << v;
            return oss.str();
        };
        fmt::print("  to_str(3.14159, 4) = '{}'\n", to_str(3.14159, 4));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. istringstream — parsing e tokenização
// ─────────────────────────────────────────────────────────────────────────────
void demo_istringstream() {
    fmt::print("\n══ 2. istringstream — parsing e tokenização ══\n");

    // ── Parser de linha de configuração ──────────────────────────────────
    {
        auto parse_kv = [](const std::string& linha)
            -> std::pair<std::string, std::string>
        {
            std::istringstream iss(linha);
            std::string chave, valor;
            std::getline(iss, chave, '=');
            std::getline(iss, valor);
            return {chave, valor};
        };

        std::vector<std::string> config = {
            "host=localhost",
            "port=5432",
            "db=cpp_academy",
        };

        std::map<std::string,std::string> cfg;
        for (auto& linha : config) {
            auto [k, v] = parse_kv(linha);
            cfg[k] = v;
        }
        for (auto& [k, v] : cfg)
            fmt::print("  cfg['{}'] = '{}'\n", k, v);
    }

    // ── Tokenização CSV ───────────────────────────────────────────────────
    {
        fmt::print("  [CSV tokenização]\n");
        std::string csv_linha = "lucas,30,São Paulo,engenheiro";
        std::istringstream iss(csv_linha);
        std::string token;
        std::vector<std::string> campos;
        while (std::getline(iss, token, ','))
            campos.push_back(token);

        for (size_t i = 0; i < campos.size(); ++i)
            fmt::print("    campo[{}] = '{}'\n", i, campos[i]);
    }

    // ── Conversão string → tipo ───────────────────────────────────────────
    {
        auto from_str = [](const std::string& s) {
            double v;
            std::istringstream iss(s);
            iss >> v;
            return iss ? v : std::numeric_limits<double>::quiet_NaN();
        };
        fmt::print("  from_str('3.14') = {:.2f}\n", from_str("3.14"));
        fmt::print("  from_str('abc')  = {}\n",
            std::isnan(from_str("abc")) ? "NaN (falhou)" : "ok");
    }

    // ── Parser de expressão simples ───────────────────────────────────────
    {
        fmt::print("  [parser de números separados por espaço]\n");
        std::istringstream iss("10 20 30 40 50");
        int soma = 0, x;
        while (iss >> x) soma += x;
        fmt::print("  soma = {}\n", soma);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. stringstream — bidirecional (leitura e escrita)
// ─────────────────────────────────────────────────────────────────────────────
void demo_stringstream_bidirecional() {
    fmt::print("\n══ 3. stringstream — bidirecional ══\n");

    // ── Buffer compartilhado: escrita e releitura ─────────────────────────
    {
        std::stringstream ss;

        // Escreve dados de diferentes tipos
        ss << 100 << " " << 3.14 << " " << "hello";
        fmt::print("  conteúdo atual: '{}'\n", ss.str());

        // Volta ao início para reler
        ss.seekg(0, std::ios::beg);
        int i; double d; std::string s;
        ss >> i >> d >> s;
        fmt::print("  relido: i={} d={:.2f} s={}\n", i, d, s);
    }

    // ── Uso como buffer temporário para serialização ───────────────────────
    {
        struct Ponto { int x, y; };
        auto serializar = [](const Ponto& p) {
            std::ostringstream oss;
            oss << p.x << "," << p.y;
            return oss.str();
        };
        auto deserializar = [](const std::string& s) {
            Ponto p;
            char sep;
            std::istringstream iss(s);
            iss >> p.x >> sep >> p.y;
            return p;
        };

        Ponto original{42, 99};
        std::string serial = serializar(original);
        Ponto recuperado   = deserializar(serial);

        fmt::print("  serializado: '{}'\n", serial);
        fmt::print("  recuperado:  ({}, {})\n", recuperado.x, recuperado.y);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. view() vs str() — C++20
// ─────────────────────────────────────────────────────────────────────────────
void demo_view_cpp20() {
    fmt::print("\n══ 4. str() vs view() — cópia vs referência ══\n");

    std::ostringstream oss;
    oss << "dados importantes: " << 42;

    // str() — retorna CÓPIA da string interna (alocação)
    std::string copia = oss.str();
    fmt::print("  str() (cópia): '{}'\n", copia);

    // view() [C++20] — retorna string_view SEM alocar (mais eficiente)
    // ATENÇÃO: view() é invalidada se o stream for modificado
    std::string_view vista = oss.view();
    fmt::print("  view() (sem cópia): '{}'\n", vista);
    fmt::print("  (view() é invalidada ao modificar o stream!)\n");
}

inline void run() {
    demo_ostringstream();
    demo_istringstream();
    demo_stringstream_bidirecional();
    demo_view_cpp20();
}

} // namespace demo_sstream
