#include "hello_world.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

struct Aluno {
    std::string nome;
    int         idade;
    double      nota;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Aluno, nome, idade, nota)

int main() {
    // 1. Criando JSON
    json j;
    j["curso"]  = "C++ moderno";
    j["alunos"] = json::array();
    j["alunos"].push_back(Aluno{"Lucas", 25, 9.5});
    j["alunos"].push_back(Aluno{"Ana",   22, 8.0});

    std::cout << j.dump(4) << "\n\n";

    // 2. Lendo de volta
    for (auto& item : j["alunos"]) {
        Aluno a = item.get<Aluno>();
        std::cout << a.nome << " — nota: " << a.nota << "\n";
    }

    // 3. Parsing de string raw
    json config = json::parse(R"({
        "debug": true,
        "max_alunos": 30
    })");

    bool debug = config.value("debug", false);
    std::cout << "\ndebug mode: " << (debug ? "on" : "off") << "\n";

    return 0;
}

// #include "hello_world.hpp"
// #include <spdlog/spdlog.h>
// #include <spdlog/sinks/stdout_color_sinks.h>
// #include <spdlog/sinks/rotating_file_sink.h>

// int main() {
//     // Sink 1: console colorido
//     auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

//     // Sink 2: arquivo rotativo (5MB, 3 arquivos)
//     auto arquivo = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
//         "academy.log", 1024 * 1024 * 5, 3
//     );

//     // Logger com os dois sinks ao mesmo tempo
//     auto log = std::make_shared<spdlog::logger>(
//         "academy",
//         spdlog::sinks_init_list{console, arquivo}
//     );

//     log->set_level(spdlog::level::debug);

//     log->debug("iniciando...");
//     log->info("chamando to_greet");
//     log->warn("isso é um aviso: {}", 42);
//     log->error("isso seria um erro");

//     academy::to_greet("Lucas");

//     log->info("fim");
//     return 0;
// }

// // #include "greeter.hpp"
// // // #include "common.pb.h"

// // #include <fmt/core.h>

// // int main() {
// //     std::shared_ptr<academy::Greeter> a;
// //     // mlinference::common::Test b;
// //     // b = mlinference::common::Test::BOOL;

// //     {
// //         auto b = academy::make_greeter("Lucas");
// //         fmt::println("use_count dentro do bloco: ", b.use_count());

// //         a = b; // compartilha propriedade
// //         fmt::println("use_count após compartilhar: ", b.use_count());

// //         b->greet();
// //     } // b sai de escopo → use_count vai para 1, objeto NÃO é destruído

// //     fmt::println("use_count após bloco: ", a.use_count());
// //     a->greet(); // ainda válido!

// //     // a sai de escopo → use_count vai para 0 → objeto destruído aqui
// //     return 0;
// // }