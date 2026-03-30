#define PY_SSIZE_T_CLEAN  // boa prática: garante que índices usem Py_ssize_t (tipo correto para tamanhos)
#include <Python.h>       // tudo da CPython API vem daqui — PyObject, PyArg_ParseTuple, etc.

// ============================================================
// FILOSOFIA CENTRAL DA CPYTHON API
//
// No Python, TUDO é um objeto — int, float, string, função...
// Em C, esses objetos são representados como PyObject*
// (um ponteiro para uma struct interna do interpretador).
//
// Toda função exposta ao Python tem sempre a mesma assinatura:
//
//   static PyObject* nome_da_funcao(PyObject* self, PyObject* args)
//
//   - self:  a instância (relevante para métodos de classe, aqui é nullptr)
//   - args:  tupla Python contendo os argumentos que o Python passou
//
// O fluxo de toda função é sempre o mesmo:
//   1. Extrair os args Python → tipos C  (PyArg_ParseTuple)
//   2. Executar a lógica em C puro
//   3. Converter o resultado C → PyObject*  (PyLong_FromLong, etc.)
//   4. Retornar o PyObject*
//
// Se algo der errado, seta um erro com PyErr_SetString e retorna nullptr.
// O interpretador Python detecta o nullptr e lança a exceção correspondente.
// ============================================================


// ============================================================
// somar(a, b) -> int
// ============================================================
static PyObject* py_somar(PyObject* self, PyObject* args) {

    int a, b;

    // PyArg_ParseTuple: lê a tupla `args` e popula as variáveis C
    // "ii" é a format string: i = int, d = double, s = string, etc.
    // retorna 0 (falso) se os tipos não baterem — nesse caso retornamos
    // nullptr para sinalizar erro ao interpretador (exceção já foi setada)
    if (!PyArg_ParseTuple(args, "ii", &a, &b)) {
        return nullptr;
    }

    int resultado = a + b;  // lógica C pura, sem nada de Python aqui

    // PyLong_FromLong: converte um long C de volta para um PyObject* (int Python)
    // essa é a "embalagem" do valor para o mundo Python
    return PyLong_FromLong(resultado);
}


// ============================================================
// multiplicar(a, b) -> int
// ============================================================
static PyObject* py_multiplicar(PyObject* self, PyObject* args) {

    int a, b;

    if (!PyArg_ParseTuple(args, "ii", &a, &b)) {
        return nullptr;
    }

    // podemos fazer a operação diretamente no return —
    // não precisamos de variável intermediária quando a lógica é simples
    return PyLong_FromLong(a * b);
}


// ============================================================
// fatorial(n) -> int
// ============================================================
static PyObject* py_fatorial(PyObject* self, PyObject* args) {

    int n;

    if (!PyArg_ParseTuple(args, "i", &n)) {
        return nullptr;
    }

    // validação de domínio: fatorial não existe para negativos
    // PyErr_SetString: seta uma exceção Python a partir do C
    //   - primeiro arg:  o tipo da exceção (PyExc_ValueError, PyExc_TypeError, etc.)
    //   - segundo arg:   a mensagem de erro
    // após setar o erro, retornamos nullptr — o Python vai lançar a exceção
    if (n < 0) {
        PyErr_SetString(PyExc_ValueError, "fatorial não definido para negativos");
        return nullptr;
    }

    long resultado = 1;
    for (int i = 2; i <= n; i++) {
        resultado *= i;
    }

    return PyLong_FromLong(resultado);
}


// ============================================================
// TABELA DE MÉTODOS
//
// O Python precisa saber quais funções C estão disponíveis no módulo
// e com quais nomes elas aparecem no lado Python.
//
// Cada entrada é: { "nome_python", funcao_c, flags, "docstring" }
//
// METH_VARARGS: os argumentos chegam como uma tupla (o padrão)
// A última entrada é sempre {nullptr, ...} — sentinela que marca o fim
// ============================================================
static PyMethodDef AcademyMethods[] = {
    {"somar",       py_somar,       METH_VARARGS, "Soma dois inteiros."},
    {"multiplicar", py_multiplicar, METH_VARARGS, "Multiplica dois inteiros."},
    {"fatorial",    py_fatorial,    METH_VARARGS, "Calcula o fatorial de n."},
    {nullptr, nullptr, 0, nullptr}  // sentinela obrigatória — marca o fim da tabela
};


// ============================================================
// DEFINIÇÃO DO MÓDULO
//
// Struct que descreve o módulo para o interpretador Python:
//   - PyModuleDef_HEAD_INIT: macro obrigatória de inicialização interna
//   - "academy":             nome usado no `import academy`
//   - nullptr:               docstring do módulo (opcional)
//   - -1:                    o módulo não guarda estado por subinterpretador
//                            (o caso normal — use -1 a menos que saiba o que faz)
//   - AcademyMethods:        a tabela de funções definida acima
// ============================================================
static struct PyModuleDef academymodule = {
    PyModuleDef_HEAD_INIT,
    "academy",
    nullptr,
    -1,
    AcademyMethods
};


// ============================================================
// PONTO DE ENTRADA DO MÓDULO
//
// Quando o Python executa `import academy`, ele procura numa .so
// uma função com o nome exato PyInit_<nome_do_modulo>.
//
// Essa função é chamada uma única vez, na primeira importação.
// Ela cria e retorna o objeto módulo usando a struct acima.
//
// PyMODINIT_FUNC: macro que define o tipo de retorno correto
// e garante que a função seja visível (exported) na .so
// ============================================================
PyMODINIT_FUNC PyInit_academy(void) {
    return PyModule_Create(&academymodule);
}