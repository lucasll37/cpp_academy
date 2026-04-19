# sandbox — Ambiente de Experimentação

## O que é este projeto?

Um espaço **livre para experimentar** features de C++ sem as restrições de um projeto estruturado. Atualmente demonstra serialização JSON com `nlohmann/json`, mas pode ser usado para qualquer experimento rápido.

---

## Conceitos atuais

O arquivo `main.cpp` atual demonstra o uso básico de `nlohmann/json`:

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

struct Aluno {
    std::string nome;
    int         idade;
    double      nota;
};

// Macro que gera from_json e to_json automaticamente
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Aluno, nome, idade, nota)

int main() {
    // 1. Criando JSON dinamicamente
    json j;
    j["curso"]  = "C++ moderno";
    j["alunos"] = json::array();
    j["alunos"].push_back(Aluno{"Lucas", 25, 9.5});
    j["alunos"].push_back(Aluno{"Ana",   22, 8.0});

    // Serializar para string (dump com indentação)
    std::cout << j.dump(4) << "\n";
    // {
    //     "alunos": [
    //         {"idade": 25, "nome": "Lucas", "nota": 9.5},
    //         {"idade": 22, "nome": "Ana",   "nota": 8.0}
    //     ],
    //     "curso": "C++ moderno"
    // }

    // 2. Deserializar de volta para struct
    for (auto& item : j["alunos"]) {
        Aluno a = item.get<Aluno>();
        std::cout << a.nome << " — nota: " << a.nota << "\n";
    }

    // 3. Parsing de string JSON
    json config = json::parse(R"({
        "debug": true,
        "max_alunos": 30
    })");

    bool debug = config.value("debug", false); // com valor padrão
    int  max   = config["max_alunos"].get<int>();
}
```

---

## Para que o sandbox serve

Use este projeto quando quiser:

- Testar uma feature de C++ rapidamente sem setup
- Experimentar uma biblioteca nova
- Verificar um comportamento antes de adicionar a outro projeto
- Rodar benchmarks simples

---

## Como usar

```bash
# Edite src/sandbox/main.cpp com seu experimento
meson setup dist
cd dist && ninja sandbox
./bin/sandbox
```

---

## `nlohmann/json` — referência rápida

```cpp
// Criação
json j = {{"chave", "valor"}, {"num", 42}};
json arr = json::array({1, 2, 3});
json obj = json::object();

// Leitura com tipo
std::string s = j["chave"].get<std::string>();
int n         = j["num"].get<int>();

// Com default (não lança se chave não existir)
int x = j.value("ausente", 0);

// Verificação de tipo
j["chave"].is_string();  // true
j["num"].is_number();    // true
j["arr"].is_array();     // se for array

// Iteração
for (auto& [key, val] : j.items()) { /* key = string, val = json */ }
for (auto& item : j_array)         { /* item = json */ }

// Parsing
json j = json::parse(str);            // lança parse_error se inválido
json j = json::parse(str, nullptr, false); // não lança, retorna json null em erro

// Serialização
std::string s = j.dump();     // sem espaços
std::string s = j.dump(4);    // indentado com 4 espaços
```
