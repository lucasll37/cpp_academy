#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <fmt/core.h>

#include <filesystem>
#include <stdexcept>
#include <vector>
#include <string>

// ============================================================
// EMBEDDED PYTHON — C++ como host do interpretador
//
// Sentido INVERSO ao que fizemos em src/python/:
//
//   src/python/     → Python importa C++  (extension module)
//   src/embed-python/ → C++ embarca Python  (embedded interpreter)
//
// Casos de uso reais:
//   - Scripting engine numa aplicação C++ (configs, plugins, automação)
//   - Hot-reload de lógica sem recompilar o host
//   - Expor Python como linguagem de extensão para usuários finais
//   - Rodar scripts de análise/visualização (matplotlib, pandas) dentro
//     de uma pipeline C++
//
// Fluxo obrigatório:
//   Py_Initialize()  →  usar API  →  Py_Finalize()
//
// Nunca chame Py_Finalize() antes de liberar todos os PyObject*,
// senão os decrementos de refcount vão acessar memória inválida.
// ============================================================

namespace fs = std::filesystem;

// ─── RAII guard para o interpretador ────────────────────────
// Garante que Py_Finalize() seja chamado mesmo em caso de exceção.
// Sem isso, a ordem de destruição de variáveis globais vs Finalize
// é indefinida e pode causar crashes em módulos com C extensions.
struct PythonGuard {
    PythonGuard()  { Py_Initialize(); }
    ~PythonGuard() { Py_Finalize();   }

    // não copiável — o interpretador é um recurso único por processo
    PythonGuard(const PythonGuard&)            = delete;
    PythonGuard& operator=(const PythonGuard&) = delete;
};

// ─── helpers de verificação de erro ─────────────────────────
// PyErr_Occurred() devolve a exceção ativa (ou nullptr se não há).
// PyErr_Print() imprime traceback completo no stderr — igual ao Python
// interativo — e limpa a exceção ativa.
static void check_python_error(const std::string& context) {
    if (PyErr_Occurred()) {
        fmt::print(stderr, "[embed] erro em: {}\n", context);
        PyErr_Print();   // imprime traceback e limpa o erro
        throw std::runtime_error("Python error in: " + context);
    }
}

// ─── 1. PyRun_SimpleString — executar código inline ─────────
// A forma mais simples: passar uma string C contendo código Python.
// Roda no namespace __main__.
// Retorna 0 em sucesso, -1 em erro.
// Limitação: não consegue capturar o valor de retorno de expressões.
static void demo_run_string() {
    fmt::print("\n── 1. PyRun_SimpleString ──────────────────────────────\n");

    const char* code = R"py(
import sys
print(f"Python {sys.version}")
print("hello from embedded Python!")
resultado = 6 * 7
print(f"6 * 7 = {resultado}")
)py";

    int rc = PyRun_SimpleString(code);
    if (rc != 0) {
        throw std::runtime_error("PyRun_SimpleString falhou");
    }
}

// ─── 2. Importar módulo e chamar função ─────────────────────
// PyImport_ImportModule  → equivale a `import math` em Python
// PyObject_GetAttrString → equivale a `math.sqrt`
// PyObject_CallOneArg    → equivale a chamar a função com um argumento
//
// Toda chamada retorna PyObject* — verificar nullptr após cada uma.
// Py_XDECREF libera a referência (X = seguro para nullptr).
static void demo_call_builtin() {
    fmt::print("\n── 2. Importar módulo e chamar função ─────────────────\n");

    // import math
    PyObject* math_mod = PyImport_ImportModule("math");
    check_python_error("import math");

    // func = math.sqrt
    PyObject* sqrt_fn = PyObject_GetAttrString(math_mod, "sqrt");
    check_python_error("getattr sqrt");

    // arg = 144.0  (cria float Python a partir de double C)
    PyObject* arg = PyFloat_FromDouble(144.0);

    // result = math.sqrt(144.0)
    PyObject* result = PyObject_CallOneArg(sqrt_fn, arg);
    check_python_error("call sqrt");

    // extrair o double C de volta do PyObject*
    double val = PyFloat_AsDouble(result);
    fmt::print("math.sqrt(144.0) = {}\n", val);

    // liberar referências na ordem inversa
    // NUNCA esquecer: todo PyObject* criado ou retornado com refcount=1
    // precisa de Py_XDECREF quando não for mais usado
    Py_XDECREF(result);
    Py_XDECREF(arg);
    Py_XDECREF(sqrt_fn);
    Py_XDECREF(math_mod);
}

// ─── 3. Manipular __main__ namespace e ler variáveis ────────
// PyDict_GetItemString permite ler variáveis do namespace __main__
// após executar código que as definiu.
// Útil para: ler resultado de scripts, passar dados entre C++ e Python.
static void demo_read_globals() {
    fmt::print("\n── 3. Ler variáveis do namespace __main__ ─────────────\n");

    // definir variáveis em __main__
    PyRun_SimpleString(R"py(
import math
circunferencia = 2 * math.pi * 5.0   # raio = 5
mensagem = "calculado em Python"
lista_quadrados = [x**2 for x in range(1, 6)]
)py");

    // obter o dicionário de __main__
    // PyImport_AddModule devolve referência emprestada — NÃO decrementar
    PyObject* main_mod  = PyImport_AddModule("__main__");
    PyObject* main_dict = PyModule_GetDict(main_mod);  // também emprestada

    // ler variável float
    // PyDict_GetItemString → referência EMPRESTADA (não decrementar)
    PyObject* circ = PyDict_GetItemString(main_dict, "circunferencia");
    if (circ) {
        fmt::print("circunferencia (r=5): {:.4f}\n", PyFloat_AsDouble(circ));
    }

    // ler variável string
    PyObject* msg = PyDict_GetItemString(main_dict, "mensagem");
    if (msg) {
        fmt::print("mensagem: {}\n", PyUnicode_AsUTF8(msg));
    }

    // ler lista e iterar
    PyObject* lista = PyDict_GetItemString(main_dict, "lista_quadrados");
    if (lista && PyList_Check(lista)) {
        fmt::print("quadrados: [");
        Py_ssize_t n = PyList_Size(lista);
        for (Py_ssize_t i = 0; i < n; i++) {
            PyObject* item = PyList_GetItem(lista, i);  // referência emprestada
            fmt::print("{}{}", PyLong_AsLong(item), (i < n-1) ? ", " : "");
        }
        fmt::print("]\n");
    }
}

// ─── 4. Passar dados C++ → Python e receber resultado ───────
// Cenário realista: C++ tem os dados, Python tem a lógica
// (ex: modelo estatístico, geração de relatório, plot).
// Injetamos variáveis no namespace __main__ antes de rodar o script.
static void demo_pass_data() {
    fmt::print("\n── 4. Passar dados C++ → Python ───────────────────────\n");

    // dados gerados pelo C++
    std::vector<double> dados = { 2.5, 4.0, 1.5, 8.0, 3.0, 6.5, 5.0 };

    // ── converter std::vector → Python list ──────────────────
    PyObject* py_lista = PyList_New(static_cast<Py_ssize_t>(dados.size()));
    for (size_t i = 0; i < dados.size(); i++) {
        // PyList_SetItem ROUBA a referência do item — não chamar Py_DECREF depois
        PyList_SetItem(py_lista, static_cast<Py_ssize_t>(i),
                       PyFloat_FromDouble(dados[i]));
    }

    // ── injetar no namespace __main__ ────────────────────────
    // PyObject_SetAttrString: equivale a `__main__.sensor_data = py_lista`
    PyObject* main_mod = PyImport_AddModule("__main__");
    PyObject_SetAttrString(main_mod, "sensor_data", py_lista);
    Py_DECREF(py_lista);  // __main__ agora é dono; liberamos nossa ref

    // ── rodar script Python que processa os dados ─────────────
    PyRun_SimpleString(R"py(
import statistics

n       = len(sensor_data)
media   = statistics.mean(sensor_data)
mediana = statistics.median(sensor_data)
desvio  = statistics.stdev(sensor_data)
minimo  = min(sensor_data)
maximo  = max(sensor_data)

print(f"  n       = {n}")
print(f"  média   = {media:.3f}")
print(f"  mediana = {mediana:.3f}")
print(f"  desvio  = {desvio:.3f}")
print(f"  min/max = {minimo} / {maximo}")
)py");
    check_python_error("demo_pass_data script");
}

// ─── 5. Executar arquivo .py externo ────────────────────────
// PyRun_SimpleFile abre e executa um arquivo .py do disco.
// Útil para scripts de configuração, plugins, etc.
static void demo_run_file(const fs::path& script_path) {
    fmt::print("\n── 5. Executar arquivo .py externo ────────────────────\n");

    if (!fs::exists(script_path)) {
        fmt::print("  [aviso] script '{}' não encontrado — pulando demo\n",
                   script_path.string());
        return;
    }

    // fopen necessário pois PyRun_SimpleFile espera FILE*
    FILE* fp = fopen(script_path.c_str(), "r");
    if (!fp) {
        fmt::print("  [aviso] não foi possível abrir '{}'\n",
                   script_path.string());
        return;
    }

    fmt::print("  executando: {}\n", script_path.string());
    int rc = PyRun_SimpleFile(fp, script_path.c_str());
    fclose(fp);

    if (rc != 0) {
        throw std::runtime_error("PyRun_SimpleFile falhou");
    }
}

// ─── 6. Tratar exceção Python em C++ ────────────────────────
// PyErr_Fetch/Restore permite inspecionar a exceção sem imprimir
// automaticamente — útil para logar estruturadamente ou tomar
// decisões com base no tipo de erro.
static void demo_handle_exception() {
    fmt::print("\n── 6. Tratar exceção Python em C++ ────────────────────\n");

    // código Python que vai lançar ZeroDivisionError
    int rc = PyRun_SimpleString("resultado_invalido = 1 / 0");
    if (rc != 0) {
        // Py_FetchErrror → extrai tipo, valor e traceback
        // (limpa o estado de erro internamente)
        PyObject *ptype = nullptr, *pvalue = nullptr, *ptraceback = nullptr;
        PyErr_Fetch(&ptype, &pvalue, &ptraceback);
        PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);

        // extrair nome do tipo de exceção
        const char* type_name = "desconhecido";
        if (ptype) {
            PyObject* name = PyObject_GetAttrString(ptype, "__name__");
            if (name) type_name = PyUnicode_AsUTF8(name);
            Py_XDECREF(name);
        }

        // extrair mensagem de erro
        std::string msg = "(sem mensagem)";
        if (pvalue) {
            PyObject* pstr = PyObject_Str(pvalue);
            if (pstr) {
                msg = PyUnicode_AsUTF8(pstr);
                Py_DECREF(pstr);
            }
        }

        fmt::print("  exceção capturada em C++:\n");
        fmt::print("    tipo: {}\n", type_name);
        fmt::print("    msg:  {}\n", msg);

        Py_XDECREF(ptype);
        Py_XDECREF(pvalue);
        Py_XDECREF(ptraceback);
    }
}

// ─── entry point ────────────────────────────────────────────
int main(int argc, char* argv[]) {
    fmt::print("╔══════════════════════════════════════════════╗\n");
    fmt::print("║        embed-python — C++ embarca Python     ║\n");
    fmt::print("╚══════════════════════════════════════════════╝\n");

    // PythonGuard inicializa o interpretador via RAII
    // A partir daqui, toda a CPython API está disponível
    PythonGuard guard;

    try {
        demo_run_string();
        demo_call_builtin();
        demo_read_globals();
        demo_pass_data();

        // tenta executar script externo se passado como argumento
        fs::path script = (argc > 1)
            ? fs::path(argv[1])
            : fs::path("./src/embed-python/scripts/hello.py");
        demo_run_file(script);

        demo_handle_exception();

    } catch (const std::exception& e) {
        fmt::print(stderr, "[embed] fatal: {}\n", e.what());
        return 1;
    }

    fmt::print("\n✓ embed-python concluído\n");
    return 0;
}