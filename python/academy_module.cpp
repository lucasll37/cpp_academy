#define PY_SSIZE_T_CLEAN  // boa prática: garante que índices usem Py_ssize_t (tipo correto para tamanhos)
#include <Python.h>       // tudo da CPython API vem daqui — PyObject, PyArg_ParseTuple, etc.
#include <omp.h>          // API do OpenMP — pragmas de paralelismo e funções utilitárias (omp_get_max_threads, etc.)

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
// O GIL E O OPENMP — o ponto mais importante desta seção
//
// O Python tem o GIL (Global Interpreter Lock): um mutex global
// que garante que apenas UMA thread execute código Python por vez.
// Isso protege os PyObject* de corrupção em ambientes multi-thread.
//
// O OpenMP cria múltiplas threads — parece conflito, mas NÃO É,
// desde que você respeite uma regra simples:
//
//   DENTRO de um bloco paralelo OpenMP:
//     ✅ operar apenas em memória C pura (double*, int*, arrays nativos)
//     ❌ NUNCA tocar em PyObject* (criar, ler, modificar, decrementar)
//
// O padrão correto é sempre:
//   1. copiar dados Python → arrays C      (single-thread, com GIL)
//   2. liberar o GIL  →  Py_BEGIN_ALLOW_THREADS
//   3. processar em paralelo com OpenMP    (multi-thread, sem GIL)
//   4. reconquistar o GIL  →  Py_END_ALLOW_THREADS
//   5. empacotar resultado C → objetos Python  (single-thread, com GIL)
//
// Py_BEGIN_ALLOW_THREADS e Py_END_ALLOW_THREADS são macros da CPython API
// que fazem exatamente esse gerenciamento do GIL.
// É exatamente assim que o NumPy opera internamente.
// ============================================================


// ============================================================
// somar(a, b) -> int
// ============================================================
static PyObject* py_somar(PyObject* self, PyObject* args) {
    (void)self;  // ← obrigatório pela API, não usado em funções de módulo

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
    (void)self;  // ← obrigatório pela API, não usado em funções de módulo

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
    (void)self;  // ← obrigatório pela API, não usado em funções de módulo

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
// multiplicar_vetor(lista, escalar) -> lista
//
// EXEMPLO BÁSICO DE OPENMP:
// recebe uma lista Python de floats e um escalar double,
// multiplica cada elemento em paralelo, retorna nova lista.
//
// Este é o padrão fundamental: copiar para C → paralelizar → devolver.
// ============================================================
static PyObject* py_multiplicar_vetor(PyObject* self, PyObject* args) {
    (void)self;  // ← obrigatório pela API, não usado em funções de módulo

    PyObject* lista;
    double escalar;

    // "Od" = um objeto Python qualquer (O) + um double (d)
    if (!PyArg_ParseTuple(args, "Od", &lista, &escalar)) {
        return nullptr;
    }

    // PyList_Check: valida que o objeto recebido é realmente uma lista Python
    if (!PyList_Check(lista)) {
        PyErr_SetString(PyExc_TypeError, "primeiro argumento deve ser uma lista");
        return nullptr;
    }

    int n = (int)PyList_Size(lista);

    // alocamos um array C para trabalhar — sair do mundo Python
    // é fundamental para poder paralelizar sem violar o GIL
    double* dados = new double[n];

    // FASE 1: copiar lista Python → array C
    // ainda single-thread pois estamos tocando em PyObject* (PyList_GetItem)
    for (int i = 0; i < n; i++) {
        dados[i] = PyFloat_AsDouble(PyList_GetItem(lista, i));
    }

    // FASE 2: liberar o GIL e paralelizar com OpenMP
    // a partir daqui NÃO podemos tocar em nenhum PyObject*
    Py_BEGIN_ALLOW_THREADS

        #pragma omp parallel for
        for (int i = 0; i < n; i++) {
            dados[i] *= escalar;  // memória C pura — seguro para paralelizar
        }

    // FASE 3: reconquistar o GIL antes de voltar ao mundo Python
    Py_END_ALLOW_THREADS

    // FASE 4: empacotar resultado C → lista Python (single-thread novamente)
    PyObject* resultado = PyList_New(n);
    for (int i = 0; i < n; i++) {
        // PyFloat_FromDouble: converte double C → float Python
        PyList_SetItem(resultado, i, PyFloat_FromDouble(dados[i]));
    }

    delete[] dados;  // liberar memória C — Python não gerencia isso

    return resultado;
}


// ============================================================
// soma_vetor(lista) -> float
//
// EXEMPLO DE REDUCAO PARALELA COM OPENMP:
// soma todos os elementos de um vetor em paralelo.
//
// Redução é o padrão onde múltiplas threads calculam resultados
// parciais que são combinados no final — um padrão clássico em
// computação paralela. OpenMP abstrai isso com a cláusula reduction.
// ============================================================
static PyObject* py_soma_vetor(PyObject* self, PyObject* args) {
    (void)self;  // ← obrigatório pela API, não usado em funções de módulo

    PyObject* lista;

    if (!PyArg_ParseTuple(args, "O", &lista)) {
        return nullptr;
    }

    if (!PyList_Check(lista)) {
        PyErr_SetString(PyExc_TypeError, "argumento deve ser uma lista");
        return nullptr;
    }

    int n = (int)PyList_Size(lista);
    double* dados = new double[n];

    for (int i = 0; i < n; i++) {
        dados[i] = PyFloat_AsDouble(PyList_GetItem(lista, i));
    }

    double total = 0.0;

    Py_BEGIN_ALLOW_THREADS

        // reduction(+:total): cada thread tem sua cópia local de `total`,
        // ao final do bloco paralelo o OpenMP soma todas as cópias
        // automaticamente no `total` original — sem race condition
        #pragma omp parallel for reduction(+:total)
        for (int i = 0; i < n; i++) {
            total += dados[i];
        }

    Py_END_ALLOW_THREADS

    delete[] dados;

    // PyFloat_FromDouble: converte double C → float Python
    return PyFloat_FromDouble(total);
}


// ============================================================
// info_threads() -> dict
//
// EXEMPLO UTILITÁRIO:
// retorna informações sobre o ambiente OpenMP como um dict Python.
//
// Demonstra como construir um dict Python a partir do C —
// útil para introspecção e debugging do paralelismo disponível.
// ============================================================
static PyObject* py_info_threads(PyObject* self, PyObject* args) {
    (void)self;  // ← obrigatório pela API, não usado em funções de módulo
    (void)args;  // ← essa função não recebe argumentos

    // omp_get_max_threads: quantas threads o OpenMP pode usar
    // (respeita OMP_NUM_THREADS se definido no ambiente)
    int max_threads = omp_get_max_threads();

    // omp_get_num_procs: quantos processadores lógicos existem no hardware
    int num_procs = omp_get_num_procs();

    // PyDict_New: cria um dict Python vazio — equivale a {} no Python
    PyObject* dict = PyDict_New();

    // PyDict_SetItemString: insere uma chave string → valor PyObject* no dict
    // PyLong_FromLong converte int C → int Python para ser o valor
    PyDict_SetItemString(dict, "max_threads", PyLong_FromLong(max_threads));
    PyDict_SetItemString(dict, "num_procs",   PyLong_FromLong(num_procs));

    return dict;  // equivale a retornar {"max_threads": N, "num_procs": M}
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
    {"somar",             py_somar,             METH_VARARGS, "Soma dois inteiros."},
    {"multiplicar",       py_multiplicar,       METH_VARARGS, "Multiplica dois inteiros."},
    {"fatorial",          py_fatorial,          METH_VARARGS, "Calcula o fatorial de n."},
    {"multiplicar_vetor", py_multiplicar_vetor, METH_VARARGS, "Multiplica cada elemento de uma lista por um escalar (OpenMP)."},
    {"soma_vetor",        py_soma_vetor,        METH_VARARGS, "Soma todos os elementos de uma lista (OpenMP reduction)."},
    {"info_threads",      py_info_threads,      METH_VARARGS, "Retorna informações sobre threads OpenMP disponíveis."},
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
    AcademyMethods,
    nullptr,  // m_slots    — para inicialização multifase (não usamos)
    nullptr,  // m_traverse — para garbage collector (não usamos)
    nullptr,  // m_clear    — para garbage collector (não usamos)
    nullptr,  // m_free     — destrutor do módulo    (não usamos)
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