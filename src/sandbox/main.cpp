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