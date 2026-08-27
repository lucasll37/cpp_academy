// =============================================================================
//  sandbox — nlohmann/json do zero
//
//  Sete demos numeradas, todas OFFLINE e deterministicas. Rode e compare com a
//  saida documentada no README.md deste diretorio.
//
//  Este arquivo tambem e o "rascunho" do repositorio: se voce quiser testar uma
//  ideia rapida de C++, apague o corpo do main() e escreva a sua. O alvo do
//  Meson chama-se `sandbox` e nao depende de mais nada alem de nlohmann_json.
// =============================================================================
#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using json = nlohmann::json;

// -----------------------------------------------------------------------------
// A macro NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(T, campos...) gera, no MESMO
// namespace de T, duas funcoes livres:
//
//     void to_json(json& j, const T& t);
//     void from_json(const json& j, T& t);
//
// A biblioteca as acha por ADL (argument-dependent lookup): ao ver `j = aluno`
// o compilador procura `to_json` tambem no namespace de `Aluno`.
// "NON_INTRUSIVE" = fora da classe; exige que os campos sejam PUBLICOS.
// -----------------------------------------------------------------------------
struct Aluno {
    std::string nome;
    int         idade;
    double      nota;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Aluno, nome, idade, nota)

// -----------------------------------------------------------------------------
// O que a macro gera, escrito A MAO. Nao ha magica nenhuma: sao estas duas
// funcoes. Escrevendo-as voce ganha controle (renomear chaves, valores padrao,
// validar) — que e exatamente o que a macro NAO deixa fazer.
// -----------------------------------------------------------------------------
struct Curso {
    std::string titulo;
    int         carga_horaria;
};

void to_json(json& j, const Curso& c) {
    j = json{{"titulo", c.titulo}, {"carga_horaria", c.carga_horaria}};
}

void from_json(const json& j, Curso& c) {
    j.at("titulo").get_to(c.titulo);
    // .value() nao lanca: se a chave faltar, usa o padrao.
    c.carga_horaria = j.value("carga_horaria", 0);
}

// =============================================================================
namespace {

void linha(const char* titulo) {
    std::cout << "\n=== " << titulo << " ===\n";
}

// -- Demo 1 -------------------------------------------------------------------
// Construir um json na mao. `json` e um TIPO-SOMA: a mesma variavel pode ser
// null, bool, numero, string, array ou objeto, e muda de tipo em runtime.
// Atribuir a j["curso"] transforma um json null em objeto automaticamente.
void demo1_construir() {
    linha("Demo 1 - construir um json");

    json j;                                    // comeca como null
    std::cout << "vazio e null?      " << std::boolalpha << j.is_null() << "\n";

    j["curso"]  = "C++ moderno";               // agora e um objeto
    j["alunos"] = json::array();
    j["alunos"].push_back(Aluno{"Lucas", 25, 9.5});
    j["alunos"].push_back(Aluno{"Ana", 22, 8.0});

    std::cout << "tipo agora:        " << j.type_name() << "\n";
    std::cout << "dump() compacto:\n" << j.dump() << "\n";
    std::cout << "dump(2) indentado:\n" << j.dump(2) << "\n";

    // As chaves saem em ordem ALFABETICA ("alunos" antes de "curso"), porque
    // nlohmann::json guarda objetos num std::map. Para preservar a ordem de
    // insercao existe nlohmann::ordered_json:
    nlohmann::ordered_json o;
    o["curso"]  = "C++ moderno";
    o["alunos"] = json::array({1, 2});
    std::cout << "ordered_json:      " << o.dump() << "\n";
}

// -- Demo 2 -------------------------------------------------------------------
// Desserializar: texto -> json. R"(...)" e um raw string literal (C++11): nada
// dentro dele precisa de escape, o que torna JSON embutido legivel.
void demo2_parse() {
    linha("Demo 2 - parse de uma string");

    json config = json::parse(R"({
        "debug": true,
        "max_alunos": 30,
        "tags": ["cpp", "json"],
        "limite": null
    })");

    std::cout << "debug (value):     " << config.value("debug", false) << "\n";
    std::cout << "max_alunos (get):  " << config["max_alunos"].get<int>() << "\n";
    std::cout << "tags[0]:           " << config["tags"][0].get<std::string>() << "\n";
    std::cout << "limite e null?     " << config["limite"].is_null() << "\n";

    // Iterar um OBJETO: .items() da (chave, valor). Structured bindings, C++17.
    std::cout << "items():\n";
    for (const auto& [chave, valor] : config.items())
        std::cout << "  " << chave << " = " << valor.dump() << "\n";
}

// -- Demo 3 -------------------------------------------------------------------
// struct <-> json, pelos dois caminhos: a macro (Aluno) e as funcoes escritas
// a mao (Curso). O codigo de USO e identico — e esse e o ponto.
void demo3_struct() {
    linha("Demo 3 - struct <-> json");

    Aluno a{"Lucas", 25, 9.5};
    json  ja = a;                     // chama to_json por ADL
    std::cout << "Aluno -> json:     " << ja.dump() << "\n";

    Aluno volta = ja.get<Aluno>();    // chama from_json por ADL
    std::cout << "json -> Aluno:     " << volta.nome << ", nota " << volta.nota << "\n";

    Curso c{"C++ do zero", 40};
    json  jc = c;                     // to_json escrita a mao, logo acima
    std::cout << "Curso -> json:     " << jc.dump() << "\n";

    // A versao a mao tolera a chave ausente porque usa .value(); a macro nao.
    Curso parcial = json::parse(R"({"titulo": "sem carga"})").get<Curso>();
    std::cout << "sem carga_horaria: " << parcial.titulo
              << " / " << parcial.carga_horaria << "\n";
}

// -- Demo 4 -------------------------------------------------------------------
// O caso mais comum da vida real: ler e escrever um arquivo de configuracao.
// `arquivo >> j` e `json::parse(arquivo)` fazem a mesma coisa.
void demo4_arquivo() {
    linha("Demo 4 - arquivo");

    const std::filesystem::path p =
        std::filesystem::temp_directory_path() / "cpp_academy_sandbox.json";

    json cfg;
    cfg["nome"]   = "academy";
    cfg["versao"] = 3;

    {
        std::ofstream out(p);
        out << cfg.dump(2) << "\n";   // escrever = dump num ostream
    }

    json lido;
    {
        std::ifstream in(p);
        in >> lido;                   // ler = operator>> (ou json::parse(in))
    }

    std::cout << "arquivo:           " << p.string() << "\n";
    std::cout << "bytes escritos:    " << std::filesystem::file_size(p) << "\n";
    std::cout << "round-trip igual?  " << (lido == cfg) << "\n";
    std::filesystem::remove(p);
}

// -- Demo 5 -------------------------------------------------------------------
// Os TRES erros que a nlohmann lanca. Todos derivam de nlohmann::json::exception,
// que traz um `id` numerico estavel — use o id, nao o texto do what().
void demo5_erros() {
    linha("Demo 5 - os tres erros");

    // (a) parse_error: o texto nao e JSON valido.
    // json::parse e [[nodiscard]]: guarde o retorno mesmo quando so quer o erro.
    try {
        json descartado = json::parse(R"({"quebrado": )");
        (void)descartado;
    } catch (const json::parse_error& e) {
        std::cout << "parse_error  id=" << e.id << " byte=" << e.byte << "\n";
    }

    json j = json::parse(R"({"nome": "Lucas", "idade": 25})");

    // (b) type_error: o valor existe, mas nao e do tipo pedido.
    try {
        (void)j["nome"].get<int>();
    } catch (const json::type_error& e) {
        std::cout << "type_error   id=" << e.id << "\n";
    }

    // (c) out_of_range: .at() com chave inexistente.
    try {
        (void)j.at("ausente");
    } catch (const json::out_of_range& e) {
        std::cout << "out_of_range id=" << e.id << "\n";
    }

    // Capturar os tres de uma vez: pegue a base.
    try {
        json descartado = json::parse("nao e json");
        (void)descartado;
    } catch (const json::exception& e) {
        std::cout << "base exception id=" << e.id << "\n";
    }

    // Sem excecao nenhuma: o parser tolerante devolve um json discarded.
    json tolerante = json::parse("{ invalido", nullptr, /*allow_exceptions=*/false);
    std::cout << "is_discarded()     " << tolerante.is_discarded() << "\n";
}

// -- Demo 6 -------------------------------------------------------------------
// A armadilha numero 1 da biblioteca: num json NAO-const, j["x"] CRIA "x"
// com valor null se ela nao existir. Ler com [] pode mudar o documento.
void demo6_acesso() {
    linha("Demo 6 - [] cria a chave; at/value/contains nao");

    json j = json::parse(R"({"a": 1})");
    std::cout << "antes:             " << j.dump() << "\n";

    (void)j["b"];                      // <- INSERE "b": null
    std::cout << "depois de j[\"b\"]:  " << j.dump() << "\n";

    std::cout << "contains(\"c\"):     " << j.contains("c") << "\n";
    std::cout << "value(\"c\", 42):    " << j.value("c", 42) << "\n";
    std::cout << "ainda sem \"c\":     " << j.dump() << "\n";

    // null NAO e o mesmo que ausente:
    std::cout << "contains(\"b\"):     " << j.contains("b")
              << "  b e null? " << j["b"].is_null() << "\n";

    // Num json const, operator[] nao insere — e UB se a chave faltar.
    const json& cj = j;
    std::cout << "const j[\"a\"]:      " << cj["a"] << "\n";
}

// -- Demo 7 -------------------------------------------------------------------
// JSON e texto: legivel, mas gordo. A propria nlohmann serializa para formatos
// BINARIOS. Os numeros abaixo sao MEDIDOS neste programa, nao estimados.
void demo7_binario() {
    linha("Demo 7 - formatos binarios (medido)");

    json j = json::array();
    for (int i = 0; i < 100; ++i)
        j.push_back(Aluno{"Aluno" + std::to_string(i), 18 + i % 10, 5.0 + i % 5});

    const std::string               texto   = j.dump();
    const std::vector<std::uint8_t> cbor    = json::to_cbor(j);
    const std::vector<std::uint8_t> msgpack = json::to_msgpack(j);
    const std::vector<std::uint8_t> ubjson  = json::to_ubjson(j);

    std::cout << "json (texto):      " << texto.size()   << " bytes\n";
    std::cout << "cbor:              " << cbor.size()    << " bytes\n";
    std::cout << "msgpack:           " << msgpack.size() << " bytes\n";
    std::cout << "ubjson:            " << ubjson.size()  << " bytes\n";
    std::cout << "cbor volta igual?  " << (json::from_cbor(cbor) == j) << "\n";
}

}  // namespace

int main() {
    demo1_construir();
    demo2_parse();
    demo3_struct();
    demo4_arquivo();
    demo5_erros();
    demo6_acesso();
    demo7_binario();
    std::cout << "\nfim.\n";
    return 0;
}
