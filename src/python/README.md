# python — Módulos de extensão: C++ por dentro do Python

## O que é este projeto?

O **mesmo** módulo Python implementado duas vezes em C++, para comparar linha a linha:
`academy_cpython.cpp` → módulo **`academy_cp`**, na **CPython API** crua (tudo à mão);
`academy_pybind.cpp` → módulo **`academy_pb`**, com **pybind11** (quase tudo automático).
Os dois exportam as mesmas seis funções e os dois usam **OpenMP**.

```bash
# a partir da raiz do repositório
make configure          # só na primeira vez (Conan + Meson)
make build              # compila; os .so ficam em build/src/python/

PYTHONPATH=build/src/python python3 src/python/main_cpython.py
PYTHONPATH=build/src/python python3 src/python/main_pybind.py

# ou, com o venv 3.12 gerido pelo Makefile local:
cd src/python && make run
```

**A direção importa** — este repositório tem os dois sentidos, e confundi-los é comum:

```
  src/python/   (aqui)   python3  ──import──▶  academy_pb.so (C++)
  src/embed-python/      ./embed_python  ──Py_Initialize──▶  interpretador CPython
```

O inverso está em [`embed-python`](../embed-python/README.md).

## Antes de começar: os oito conceitos

### 1. O que é um "módulo de extensão"

`import json` faz o interpretador procurar um `json.py`. `import academy_pb` faz ele
procurar, **na mesma lista de diretórios**, um objeto compartilhado (`.so` no Linux, `.pyd`
no Windows): código de máquina que o interpretador carrega com `dlopen` e passa a tratar como
módulo Python comum. Depois do `make build`, `ls -1 build/src/python/*.so` mostra os dois — e
o nome do arquivo não é decoração:

```
academy_pb . cpython-312 . x86_64-linux-gnu . so
    │             │              │             └─ objeto compartilhado (ELF)
    │             │              └─ arquitetura + SO + libc
    │             └─ implementação (CPython) e VERSÃO (3.12)
    └─ o nome que você usa no `import`
```

> ⚠ **Um `.so` de Python 3.12 não importa em 3.11.** Não é política: a ABI muda entre versões
> menores (layout de `PyObject`, tamanho de structs, funções que somem). O interpretador nem
> tenta — só carrega o `.so` cujo sufixo bate com o dele, o que
> `python3-config --extension-suffix` informa.

### 2. `PyObject*` — no Python, tudo é objeto

Em C, um objeto Python é sempre visto pelo mesmo tipo: **`PyObject*`**, ponteiro para uma
struct interna do interpretador. Você nunca a desreferencia; mexe nela pelas funções da API.

| Você tem | C → `PyObject*` | `PyObject*` → C |
|---|---|---|
| `long` / `double` | `PyLong_FromLong` / `PyFloat_FromDouble` | `PyLong_AsLong` / `PyFloat_AsDouble` |
| `const char*` | `PyUnicode_FromString` | `PyUnicode_AsUTF8` |
| lista | `PyList_New` + `PyList_SetItem` | `PyList_Size` + `PyList_GetItem` |
| dict | `PyDict_New` + `PyDict_SetItemString` | `PyDict_GetItemString` |

Toda função exposta ao Python tem a **mesma assinatura**, e devolver `nullptr` significa
"deu erro" — o interpretador lança então a exceção registrada com `PyErr_SetString`:

```cpp
static PyObject* py_somar(PyObject* self, PyObject* args);
//                        └─ o módulo   └─ TUPLA com os argumentos da chamada
```

### 3. Contagem de referências — a conta que a CPython API cobra de você

O núcleo da API, e o que o pybind11 esconde. Todo `PyObject*` tem um contador: enquanto ele
for maior que zero o objeto vive; ao chegar a zero, é destruído. `Py_INCREF(o)` soma 1,
`Py_DECREF(o)` subtrai 1. (`Py_XDECREF` tolera `nullptr`; `Py_DECREF` com `nullptr` é crash.)

Cada função da API devolve **um** de dois tipos de referência, e a doc oficial diz qual.
Errar é vazamento (nunca chega a zero) ou crash (chega cedo demais):

| Tipo | Significa | Sua obrigação | Exemplos |
|---|---|---|---|
| **new reference** | o contador já foi incrementado para você | `Py_DECREF` ao terminar, **ou** devolver o objeto ao Python | `PyLong_FromLong`, `PyFloat_FromDouble`, `PyList_New`, `PyDict_New` |
| **borrowed reference** | ninguém incrementou; vive porque outro o segura | **não** dar `Py_DECREF` | `PyList_GetItem`, `PyDict_GetItem` |
| **steals a reference** | a função assume a **sua** referência | **não** dar `Py_DECREF` depois | `PyList_SetItem`, `PyTuple_SetItem` |

Duas linhas com a mesma cara e donos opostos:

```cpp
PyList_SetItem(lista, i, PyFloat_FromDouble(x));   // ROUBA     → nada a fazer
PyDict_SetItemString(d, "k", PyLong_FromLong(x));  // NÃO rouba → VAZAMENTO
```

A segunda vazava neste repositório; a medição está adiante. Se `unique_ptr` já é familiar, a
analogia é exata: a CPython API é gerência **manual**, estilo `new`/`delete`; o `py::object`
do pybind11 é o smart pointer que faz a conta ([`smart-pointers`](../smart-pointers/README.md)).

### 4. O GIL — e o que acontece se você não liberá-lo

O **GIL** (*Global Interpreter Lock*) é um mutex global: **só uma thread executa bytecode
Python por vez**. Ele existe para proteger justamente os contadores da seção anterior. Você
entra na sua função C++ **segurando** o GIL, e daí saem duas consequências:

> ⚠ **O GIL não é opcional — não liberá-lo congela o processo inteiro.** Enquanto sua função
> não devolver o GIL, **nenhuma outra thread Python roda**: nem o servidor que atende
> requisições, nem a interface, nem o `KeyboardInterrupt`. Função de 300 ms = tudo travado por
> 300 ms. É por isso que NumPy e qualquer extensão séria liberam o GIL nos laços pesados.

> ⚠ **Liberar o GIL sem tirar os `PyObject*` do caminho é UB.** Sem o GIL, tocar em
> **qualquer** `PyObject*` — criar, ler, `Py_DECREF` — é corrupção silenciosa de heap: os
> contadores deixam de ser atômicos. Não dá segfault na hora; dá um crash inexplicável dez
> minutos depois, em outra thread.

O padrão correto tem cinco passos, e é o que `academy_cpython.cpp` faz:

```
  1. copiar dados Python → array C          (1 thread, COM o GIL)
  2. Py_BEGIN_ALLOW_THREADS                 ← solta o GIL
  3. processar em C puro, em paralelo       (N threads, SEM o GIL)
  4. Py_END_ALLOW_THREADS                   ← retoma o GIL
  5. empacotar array C → objetos Python     (1 thread, COM o GIL)
```

Em pybind11 os passos 2 e 4 viram um objeto RAII: `{ py::gil_scoped_release release; ... }`
— construtor solta, destrutor retoma.

### 5. A burocracia obrigatória da CPython API

Três coisas que todo módulo em C precisa ter, e que o pybind11 gera para você.

**(a) A tabela de métodos** — quais funções C existem e com que nome aparecem. Termina numa
**sentinela** de `nullptr`; sem ela o Python lê memória além do array.

```cpp
static PyMethodDef AcademyMethods[] = {
    {"somar", py_somar, METH_VARARGS, "Soma dois inteiros."},
    {nullptr, nullptr, 0, nullptr}   // sentinela obrigatória
};
```

O terceiro campo é a **convenção de chamada**: `METH_VARARGS` (o padrão — os argumentos
chegam numa tupla, assinatura `(self, args)`); `METH_VARARGS | METH_KEYWORDS` (tupla + dict
de `kwargs`, assinatura `(self, args, kwargs)`); `METH_O` (**um** argumento, direto, sem
tupla); e `METH_NOARGS` (nenhum). Os dois últimos são mais rápidos porque pulam a criação da
tupla — este subprojeto usa `METH_VARARGS` em tudo, inclusive em `info_threads()`, que não
recebe nada e portanto pediria `METH_NOARGS`.

**(b) A definição do módulo**, onde o campo `-1` merece uma frase inteira:

```cpp
static struct PyModuleDef academymodule = {
    PyModuleDef_HEAD_INIT, "academy_cp", nullptr,
    -1,                    // ← tamanho do estado por interpretador
    AcademyMethods, nullptr, nullptr, nullptr, nullptr
};
```

O CPython permite criar **sub-interpretadores**: vários ambientes Python isolados no mesmo
processo (é o que o `mod_wsgi` usa para separar aplicações). Um módulo pode guardar estado
**por** sub-interpretador, e aí o campo diria quantos bytes. `-1` significa "não uso esse
mecanismo; meu estado, se houver, é global ao processo" — o valor certo enquanto não houver
estado mutável. A alternativa moderna é a [PEP 489](https://peps.python.org/pep-0489/).

**(c) O ponto de entrada:** `PyMODINIT_FUNC PyInit_academy_cp(void) { return
PyModule_Create(&academymodule); }`. Ao carregar o `.so`, o interpretador procura o símbolo
com esse nome **exato**. Nome errado = `ImportError: dynamic module does not define module
export function`.

### 6. `PyArg_ParseTuple` e a string de formato

Desmonta a tupla de argumentos em variáveis C. É **variádica**, no estilo `scanf`: string de
formato, depois **ponteiros** para as variáveis de saída.

```cpp
int a, b;
if (!PyArg_ParseTuple(args, "ii", &a, &b))   // ← observe os &
    return nullptr;                          // 0 = falhou; a exceção JÁ foi setada
```

Os códigos: `i`/`l` (int → `int*`/`long*`), `d` (float → `double*`), `s` (str →
`const char**`, UTF-8, não liberar), `O` (qualquer objeto → `PyObject**`, **borrowed**),
`O!` (objeto de um tipo, passa-se `PyTypeObject*` e depois `PyObject**`), `|` (daqui em
diante os argumentos são opcionais) e `:nome` (nome da função nas mensagens de erro). Assim,
`"Od:multiplicar_vetor"` lê um objeto e um double, e erros dirão `multiplicar_vetor() ...`.

> ⚠ Por ser variádica, **não há verificação de tipo em compilação**. Escrever `"d"` e passar
> um `int*` compila sem um único aviso e corrompe a pilha em execução — o mesmo risco do
> `printf`, e exatamente o que o pybind11 elimina ao deduzir tudo da assinatura C++.

`#define PY_SSIZE_T_CLEAN` antes do `#include <Python.h>` é obrigatório desde o 3.10: faz os
códigos de tamanho (`s#`, `y#`) usarem `Py_ssize_t` em vez de `int`.

### 7. pybind11 — o que ele é e o que custa

Biblioteca **header-only**: nada para linkar, o `#include <pybind11/pybind11.h>` traz tudo.
Usa templates para gerar, em compilação, o código que você escreveria à mão.

- **`PYBIND11_MODULE(nome, m)`** — substitui tabela de métodos, `PyModuleDef` e `PyInit_`.
  O `nome` **precisa** bater com o nome do `.so`.
- **`m.def("nome", &funcao, "docstring", ...)`** — expõe uma função C++.
- **`py::arg("a")`** — dá **nome** ao parâmetro do lado Python, habilitando `somar(a=2, b=3)`;
  sem isso, só por posição. O literal `"a"_a` (com `using namespace pybind11::literals`) é a
  forma curta do mesmo.
- **`#include <pybind11/stl.h>`** — conversões automáticas entre `std::vector`, `std::map`,
  `std::optional` e os tipos Python.
- **`throw py::value_error("...")`** vira `ValueError`; `std::invalid_argument` também vira
  `ValueError`, `std::out_of_range` vira `IndexError`, `std::runtime_error` vira
  `RuntimeError`.

> ⚠ **O preço do header-only é tempo de compilação.** Medido nesta máquina, `g++ -O2 -c`:
> `academy_cpython.cpp` leva **0,32 s**; `academy_pybind.cpp`, **4,48 s**. Catorze vezes
> mais, para um arquivo com metade das linhas. Em cinquenta arquivos de binding, isso decide
> o seu dia.

### 8. OpenMP em três linhas

Paraleliza laços por anotação: você marca, o compilador gera as threads. A flag é
`-fopenmp` (o Meson a acrescenta via `omp_dep`).

```cpp
#pragma omp parallel for
for (int i = 0; i < n; i++)
    dados[i] *= escalar;        // cada thread pega uma fatia dos índices
```

Só é correto porque as iterações são **independentes**. Com uma acumuladora seria uma *data
race* — e para isso existe a cláusula **`reduction`**:

```cpp
double total = 0.0;
#pragma omp parallel for reduction(+:total)
for (int i = 0; i < n; i++) total += dados[i];
```

`reduction(+:total)` dá a **cada thread uma cópia privada** de `total`, inicializada com o
elemento neutro do operador (`0` para `+`), e ao fim soma todas as cópias na original — sem
mutex, sem race. Existem reduções para `+ - * & | ^ && || min max`.

Duas utilitárias aparecem no código: `omp_get_max_threads()` (quantas threads o runtime pode
usar; respeita `OMP_NUM_THREADS`) e `omp_get_num_procs()` (processadores lógicos). O assunto
a fundo está em [`mandelbrot_openmp`](../mandelbrot_openmp/README.md).

## Por que estudar isto?

O problema: **Python é produtivo e lento; C++ é rápido e verboso.** Um módulo de extensão
permite escolher os dois, arquivo a arquivo.

| Sem extensão | Com extensão |
|---|---|
| o laço quente roda em bytecode interpretado | roda como código de máquina, com SIMD/OpenMP |
| o GIL serializa tudo | o trecho C++ libera o GIL e usa os N núcleos |
| uma biblioteca C++ que já existe é inalcançável | expõe-se a API dela em dezenas de linhas |
| SDK de hardware sem binding = fim da linha | você escreve o binding |

As três razões reais, em ordem de frequência: **(1) reusar uma biblioteca C++ que já
existe** — o caso mais comum, e o ganho não é velocidade, é *acesso* (foi assim que OpenCV,
PyTorch e o ONNX Runtime chegaram ao Python); **(2) acelerar um laço quente que não se
vetoriza** — autômato celular, máquina de estados, dependência entre iterações; **(3) falar
com hardware ou SDK proprietário** que só publica cabeçalhos C.

### ⚠ Quando **não** vale a pena

Antes de escrever uma linha de C++, elimine as alternativas, nesta ordem:

| Alternativa | Quando serve |
|---|---|
| **NumPy vetorizado** | operação elemento a elemento sobre arrays. Medido abaixo: **33× mais rápido** que a extensão deste diretório, sem compilar nada |
| **`ctypes` / `cffi`** | a biblioteca C já existe como `.so` e você só quer chamá-la |
| **`subprocess`** | o trabalho é um programa inteiro, não uma função |
| **Cython / Numba** | acelerar Python existente sem trocar de linguagem |

> ⚠ **Se a função faz pouco trabalho por chamada, a travessia come o ganho.** Cruzar a
> fronteira Python→C++ custa entre **78 e 281 ns** (medido adiante) *antes* de a sua função
> começar — uma função que soma dois inteiros nunca compensa. A pergunta certa não é "isto é
> lento em Python?", é **"quanto trabalho eu faço por travessia?"**

## Exemplo mínimo (5 minutos)

Antes de olhar qualquer arquivo daqui, faça um módulo do zero — são cinco linhas:

```cpp
// mini.cpp
#include <pybind11/pybind11.h>
int somar(int a, int b) { return a + b; }
PYBIND11_MODULE(mini, m) { m.def("somar", &somar); }
```

Compilar exige três coisas na linha de comando: cabeçalhos do Python, cabeçalhos do pybind11
e o sufixo de nome correto.

```bash
c++ -O2 -shared -std=c++20 -fPIC \
    $(PKG_CONFIG_PATH=$PWD/build pkg-config --cflags pybind11) \
    $(python3 -c "import sysconfig; print('-I' + sysconfig.get_paths()['include'])") \
    mini.cpp -o mini$(python3-config --extension-suffix)

python3 -c "import mini; print(mini.somar(2, 3))"     # → 5
```

Aqui o pybind11 vem do Conan, daí o `pkg-config`; se você o instalou com
`pip install pybind11`, a forma canônica é `$(python3 -m pybind11 --includes)`, que já traz
os cabeçalhos do Python junto. Note três coisas: o nome em `PYBIND11_MODULE(mini, ...)` bate
com o nome do arquivo; o `import` funcionou porque o `.so` está no diretório atual (que está
no `sys.path`); e não houve **uma única linha de código Python**.

## Estrutura de arquivos

```
python/
├── README.md              ← este arquivo
├── academy_cpython.cpp    ← academy_cp: CPython API crua, refcount à mão, GIL por macro
├── academy_pybind.cpp     ← academy_pb: o MESMO módulo em pybind11
├── main_cpython.py        ← exercita academy_cp; imprime de onde o .so foi carregado
├── main_pybind.py         ← exercita academy_pb + o benchmark comparativo dos dois
├── meson.build            ← py.extension_module() para os dois alvos
├── Makefile               ← venv 3.12 local: `make setup`, `make run`, `make wheel`
├── requirements.txt       ← numpy, meson-python, ninja, ipykernel (para o venv)
└── standalone/            ← empacotar a extensão como wheel instalável por pip
    ├── meson.build        ← projeto Meson independente da árvore do repositório
    └── pyproject.toml     ← build-backend = "mesonpy"
```

O `.venv/` que aparece no `ls` é criado pelo `make setup` e não é versionado.

## A CPython API em três degraus

`academy_cpython.cpp` está escrito nesta progressão — cada degrau acrescenta **uma**
dificuldade.

### Degrau 1 — `somar`: parse e retorno, mais nada

```cpp
static PyObject* py_somar(PyObject* self, PyObject* args) {
    (void)self;
    int a, b;
    if (!PyArg_ParseTuple(args, "ii", &a, &b))   // Python → C
        return nullptr;                          // erro: exceção já setada
    return PyLong_FromLong(a + b);               // C → Python (new reference)
}
```

Quatro passos, sempre os mesmos: desmontar os argumentos, calcular em C puro, embrulhar o
resultado, devolver. `PyLong_FromLong` cria uma *new reference*, e devolvê-la transfere a
dívida ao interpretador — por isso não há `Py_DECREF` aqui.

### Degrau 2 — `fatorial`: acrescenta o erro

```cpp
if (n < 0) {
    PyErr_SetString(PyExc_ValueError, "fatorial não definido para negativos");
    return nullptr;
}
```

O protocolo de erro tem duas metades e **as duas são obrigatórias**: registrar a exceção
(`PyErr_SetString`, com o tipo — `PyExc_ValueError`, `PyExc_TypeError`...) **e** devolver
`nullptr`. Registrar sem devolver `nullptr` deixa uma exceção pendente que estoura na próxima
chamada, num lugar sem relação com a causa. Do lado Python vira um `ValueError` comum.

### Degrau 3 — `multiplicar_vetor`: lista, refcount, GIL e OpenMP

Todos os conceitos se encontram aqui — os cinco passos do conceito 4, no código real:

```cpp
int n = (int)PyList_Size(lista);
std::vector<double> dados(n);          // buffer C com DONO (RAII)

// FASE 1 — copiar Python → C, com o GIL, uma thread
for (int i = 0; i < n; i++) {
    dados[i] = PyFloat_AsDouble(PyList_GetItem(lista, i));  // GetItem = borrowed
    if (dados[i] == -1.0 && PyErr_Occurred())
        return nullptr;                // o vector se destrói sozinho — RAII
}

// FASE 2/3 — sem GIL, N threads, só memória C
Py_BEGIN_ALLOW_THREADS
    #pragma omp parallel for
    for (int i = 0; i < n; i++) dados[i] *= escalar;
Py_END_ALLOW_THREADS

// FASE 4 — de volta ao mundo Python, com o GIL
PyObject* resultado = PyList_New(n);   // new reference
if (resultado == nullptr) return nullptr;
for (int i = 0; i < n; i++)
    PyList_SetItem(resultado, i, PyFloat_FromDouble(dados[i]));  // SetItem ROUBA
return resultado;
```

Três decisões que valem ser ditas em voz alta:

- **`std::vector<double>`, não `new double[n]`.** A versão anterior usava `new[]`/`delete[]`,
  e o `return nullptr` da fase 1 teria vazado a alocação. É o RAII que
  [`smart-pointers`](../smart-pointers/README.md) e [`memory`](../memory/README.md) ensinam,
  aplicado dentro de uma API C.
- **`PyFloat_AsDouble` devolve `-1.0` em caso de erro**, que é um double válido. Por isso a
  checagem é `valor == -1.0 && PyErr_Occurred()`: só o par significa falha. Com isso,
  `[1.0, "texto"]` vira `TypeError: must be real number, not str` em vez de lixo silencioso.
- **`PyList_SetItem` sem `Py_DECREF`**, porque ela rouba a referência de
  `PyFloat_FromDouble`. Um `Py_DECREF` ali destruiria o float enquanto a lista ainda o
  aponta — crash na primeira leitura.

> ⚠ **O que NÃO fazer dentro do bloco OpenMP.** Entre `Py_BEGIN_ALLOW_THREADS` e
> `Py_END_ALLOW_THREADS` só existe memória C. É proibido, sem exceção: criar ou ler um
> `PyObject*`, chamar `Py_INCREF`/`Py_DECREF` ou `PyErr_SetString`, ou deixar uma exceção C++
> atravessar o bloco — qualquer um corrompe o heap do interpretador **sem falhar na hora**.
> Para reportar um erro detectado no laço, grave uma flag num `int` local e trate **depois**
> do `Py_END_ALLOW_THREADS`.

### ⚠ O vazamento que este subprojeto tinha (medido)

`info_threads()` monta um dict. A versão anterior era esta:

```cpp
PyDict_SetItemString(dict, "max_threads", PyLong_FromLong(max_threads));  // ← VAZA
```

Parece idêntica ao `PyList_SetItem` acima. Não é: `PyDict_SetItemString` **incrementa por
conta própria** em vez de roubar. O `PyLong` ficava com contador 2 e nunca era destruído.

```cpp
PyObject* v_max = PyLong_FromLong(max_threads);   // new reference
PyDict_SetItemString(dict, "max_threads", v_max); // o dict pega a dele
Py_DECREF(v_max);                                 // a nossa acaba aqui
```

E aqui está a parte instrutiva: **com os valores padrão o vazamento é invisível**, porque
inteiros pequenos (−5 a 256) são objetos cacheados e imortais no CPython 3.12 — nada é
alocado. Forçando `OMP_NUM_THREADS=99999` e contando os blocos com
`sys.getallocatedblocks()` em 100 mil chamadas, ele aparece: **100 001 blocos vazados antes
da correção, 1 depois**. Exatamente um por chamada — o `max_threads = 99999`; o
`num_procs = 24` continuava cacheado e não vazava.

Nenhum `-Wall` pega isso, nenhum sanitizer padrão pega isso, e o programa nunca falha: só
cresce. **É este o custo cognitivo que o pybind11 elimina** — em `academy_pybind.cpp` não há
um único `Py_INCREF`/`Py_DECREF`, e não porque alguém foi cuidadoso.

## O mesmo módulo em pybind11

A lógica vira C++ normal, sem uma linha de Python:

```cpp
// compare com o degrau 2: sem PyErr_SetString, sem `return nullptr`
long fatorial(int n) {
    if (n < 0) throw py::value_error("fatorial não definido para negativos");
    long r = 1;  for (int i = 2; i <= n; i++) r *= i;  return r;
}

std::vector<double> multiplicar_vetor(const std::vector<double>& v, double escalar) {
    std::vector<double> resultado(v.size());
    {
        py::gil_scoped_release release;          // solta o GIL
        #pragma omp parallel for
        for (int i = 0; i < (int)v.size(); i++)
            resultado[i] = v[i] * escalar;
    }                                            // destrutor retoma o GIL
    return resultado;
}

PYBIND11_MODULE(academy_pb, m) {
    m.doc() = "Módulo academy reimplementado com pybind11 + OpenMP";
    m.def("somar",    &somar,    "Soma dois inteiros",      py::arg("a"), py::arg("b"));
    m.def("fatorial", &fatorial, "Calcula o fatorial de n", py::arg("n"));
    m.def("multiplicar_vetor", &multiplicar_vetor,
          "Multiplica cada elemento por escalar (OpenMP)",
          py::arg("v"), py::arg("escalar"));
}
```

A conversão `list ↔ std::vector<double>` vem de `#include <pybind11/stl.h>` e é **cópia**, não
vista: a biblioteca percorre a lista e desembrulha cada `PyFloat`, exatamente como a fase 1
escrita à mão. Não há almoço grátis; há menos código onde errar.

### As funções exportadas

As mesmas seis nos dois módulos. Repare que a **assinatura C++ difere**: na CPython API toda
função tem a mesma forma, e é a string de formato que carrega os tipos.

| Função Python | Em `academy_cp` (CPython API) | Em `academy_pb` (pybind11) |
|---|---|---|
| `somar(a, b)` | `static PyObject* py_somar(PyObject*, PyObject*)` | `int somar(int, int)` |
| `multiplicar(a, b)` | `static PyObject* py_multiplicar(PyObject*, PyObject*)` | `int multiplicar(int, int)` |
| `fatorial(n)` | `static PyObject* py_fatorial(PyObject*, PyObject*)` | `long fatorial(int)` |
| `multiplicar_vetor(v, escalar)` | `static PyObject* py_multiplicar_vetor(PyObject*, PyObject*)` | `std::vector<double> multiplicar_vetor(const std::vector<double>&, double)` |
| `soma_vetor(v)` | `static PyObject* py_soma_vetor(PyObject*, PyObject*)` | `double soma_vetor(const std::vector<double>&)` |
| `info_threads()` | `static PyObject* py_info_threads(PyObject*, PyObject*)` | `py::dict info_threads()` |

### Comparação

| Aspecto | CPython API | pybind11 |
|---|---|---|
| Linhas efetivas (sem comentários/vazias) | **144** | **67** |
| Tempo de `g++ -O2 -c` | **0,32 s** | **4,48 s** |
| Lógica vs. binding | misturados | separados |
| Conversão de tipos | manual (`PyLong_From*`, `PyArg_ParseTuple`) | deduzida da assinatura C++ |
| Verificação de tipo em compilação | nenhuma (formato variádico) | total (templates) |
| Contagem de referências | sua responsabilidade | invisível |
| Exceções | `PyErr_SetString` + `return nullptr` | `throw` C++ normal |
| GIL | macros `Py_BEGIN/END_ALLOW_THREADS` | `py::gil_scoped_release` (RAII) |
| STL e NumPy | à mão, item a item | `<pybind11/stl.h>`, `<pybind11/numpy.h>` |
| Overhead por chamada, `-O2` | **78 ns** (medido) | **281 ns** (medido) |

## O preço do wrapper, medido

Nada aqui é estimativa: tudo saiu de `main_pybind.py` nesta máquina — **AMD Ryzen 9 5900X,
24 threads, GCC 13.3.0, Python 3.12.3, Linux (WSL2)** — sobre uma lista Python de
10 000 000 de `double`. Cada número é o **menor de 5** execuções.

**Compilado com `-O2`:**

| Operação | `academy_cp` | `academy_pb` | Python puro |
|---|---|---|---|
| `multiplicar_vetor(v, 2.0)` | 329 ms | 333 ms | 322 ms (`[x*2.0 for x in v]`) |
| `soma_vetor(v)` | 60 ms | 126 ms | 56 ms (`sum(v)`) |
| `somar(2, 3)`, por chamada | 78 ns | 281 ns | 77 ns |

**Compilado com `-O0`, que é o padrão deste repositório** (`make build` usa
`buildtype=debug`). O pybind11 é quase todo template inlinável; sem otimização cada camada
vira uma chamada de verdade:

| Operação | `academy_cp` | `academy_pb` | Fator |
|---|---|---|---|
| `multiplicar_vetor` | 317 ms | 1411 ms | **4,5×** |
| `soma_vetor` | 57 ms | 880 ms | **15×** |
| `somar`, por chamada | 80 ns | 1035 ns | **13×** |

> ⚠ **Nunca meça uma extensão pybind11 em build de debug.** A biblioteca não é "lenta"; ela
> depende do inliner, e `-O0` desliga a única coisa que a torna barata.

### As três lições dos números

**1. A extensão não ganhou do Python puro em `multiplicar_vetor`** — 333 ms contra 322 ms.
Porque a multiplicação não é o trabalho: o trabalho é a **travessia**, desembrulhar 10 milhões
de `PyFloat` na entrada e **criar 10 milhões de `PyFloat` novos** na saída. Medido
isoladamente, só criar esses objetos custa **312 ms** (`[x + 0.0 for x in v]`); a
multiplicação paralela em 24 threads sobre um array C é ruído perto disso.

**2. `soma_vetor` é 5× mais rápida que `multiplicar_vetor` no mesmo vetor** — 60 ms contra
329 ms. A diferença: ela devolve **um** float, não dez milhões. Isso isola a conta:

```
  soma_vetor        = ler 10M PyFloat (≈55 ms) + somar em paralelo (≈5 ms)
  multiplicar_vetor = ler (≈55) + multiplicar (≈5) + CRIAR 10M PyFloat (≈270 ms)
```

**A regra que sai daí: devolva o mínimo possível ao Python.** Se o resultado final é um
escalar, uma estatística ou um índice, calcule-o do lado C++ e devolva só ele.

**3. NumPy faz o mesmo trabalho em 10 ms** — 33× mais rápido que a nossa extensão. Não porque
o laço dele seja melhor, mas porque **ele nunca cria dez milhões de objetos Python**: um
`ndarray` é um único objeto com um bloco contíguo de `double` dentro. Mesma lição por outro
ângulo. Se a sua extensão vai trabalhar sobre arrays numéricos, receba e devolva `ndarray`
via `#include <pybind11/numpy.h>`, não `std::vector`.

## Onde o Python encontra o módulo

É aqui que se trava depois de compilar com sucesso. O `import` percorre, **em ordem**, a
lista `sys.path`:

```
sys.path = [ diretório do script, ou "" (cwd) no modo -c/-m ]
         + [ o que estiver em PYTHONPATH, na ordem escrita ]
         + [ a biblioteca padrão ]
         + [ site-packages do interpretador ou do venv ]
```

O primeiro diretório que contiver `academy_pb.cpython-312-x86_64-linux-gnu.so` vence. Três
formas de colocá-lo lá, da mais descartável para a definitiva:

**(a) `PYTHONPATH` apontando para a árvore de build**, para desenvolver:
`PYTHONPATH=build/src/python python3 -c "import academy_pb; print(academy_pb.somar(2,3))"`.

**(b) Depois do `make install`.** O Meson instala as extensões no diretório de pacotes
correspondente ao prefixo (`--prefix=dist`) — e **não** em `dist/lib`:
`make install && ls dist/local/lib/python3.12/dist-packages/` mostra os dois `.so` lá.

**(c) Instalando como wheel** — a forma que dispensa `PYTHONPATH`; é a próxima seção.
Para descobrir de onde um módulo veio, pergunte a ele (é o que `main_cpython.py` imprime no
fim): `import academy_cp; print(academy_cp.__file__)`.

> ⚠ `ModuleNotFoundError: No module named 'academy_pb'` tem quase sempre uma de três causas:
> o `make build` não rodou; o `PYTHONPATH` aponta para o lugar errado; ou o `python3` que você
> usa não é o mesmo para o qual o `.so` foi compilado (compare
> `python3-config --extension-suffix` com o nome do arquivo).

## `standalone/` — empacotar como wheel

Mexer em `PYTHONPATH` funciona na sua máquina e em nenhuma outra. A forma de **distribuir**
uma extensão é o **wheel**: um `.zip` com o `.so` dentro e metadados que o `pip` entende.
`standalone/` é um projeto Meson **independente** da árvore do repositório, com dois arquivos,
e o `pyproject.toml` é o que faz o `pip` saber compilar C++:

```toml
[build-system]
requires = ["meson-python"]
build-backend = "mesonpy"     # ← o pip delega a compilação ao Meson
```

`meson-python` é o *build backend*: encontrada essa linha, o `pip` executa `meson setup` +
`meson compile` e empacota o resultado sozinho.

```console
$ cd src/python && make setup      # cria .venv 3.12 com meson-python + ninja
$ make wheel                       # equivale ao pip install abaixo
$ .venv/bin/pip install --no-build-isolation ./standalone
  Created wheel: academy_cp-1.0.0-cp312-cp312-linux_x86_64.whl
  Successfully installed academy_cp-1.0.0
```

Repare no nome do wheel — `cp312-cp312-linux_x86_64` — é a mesma informação de ABI do nome
do `.so`, agora no pacote: ele só instala em CPython 3.12 / Linux / x86-64. (Wheels portáveis
existem: são os `manylinux`, construídos num contêiner com glibc antiga.) A partir daí, nada
de `PYTHONPATH`: o `.so` fica em `.venv/lib/python3.12/site-packages/` e `import academy_cp`
funciona de qualquer diretório.

> ⚠ `standalone/meson.build` compila **apenas `academy_cp`**; o alvo `academy_pb` está
> comentado de propósito, porque fora da árvore do repositório não há o `pkg-config` do Conan
> e `dependency('pybind11')` falharia. Para empacotar também o módulo pybind11, acrescente
> `pybind11` em `[build-system] requires` e resolva o include pelo pacote pip.
> `--no-build-isolation` reusa o que já está no venv em vez de baixar tudo num ambiente
> temporário — mais rápido, e funciona offline.

## Como validar

```bash
make configure && make build
ls -1 build/src/python/*.so     # dois arquivos, com o sufixo do SEU Python
PYTHONPATH=build/src/python python3 src/python/main_cpython.py
```

```
=== CPython API ===
7
12
120
erro capturado: fatorial não definido para negativos
{'max_threads': 24, 'num_procs': 24}
[10.0, 20.0, 30.0]
15.0

carregado de: /home/.../build/src/python/academy_cp.cpython-312-x86_64-linux-gnu.so
```

`7` = `somar(3,4)`; `12` = `multiplicar(3,4)`; `120` = `fatorial(5)`; a linha do erro prova
que a exceção atravessou para o Python; `[10.0, 20.0, 30.0]` é o OpenMP; `15.0` é a redução.
**Só `max_threads` e `num_procs` variam com a sua máquina.**

`main_pybind.py` imprime as mesmas sete linhas com `=== pybind11 ===` no topo, seguidas do
benchmark. Os tempos variam por máquina; o que deve se **manter** é a forma do resultado:
`soma_vetor` bem mais rápida que `multiplicar_vetor`, e `academy_cp` na frente de
`academy_pb` num build de debug.

Pelo `Makefile` local: `make setup` (cria o venv, idempotente), `make run` (roda os dois
scripts com o `PYTHONPATH` correto), `make wheel`, `make help`. Depois do `make install`:
`PYTHONPATH=dist/local/lib/python3.12/dist-packages python3 -c "import academy_pb;
print(academy_pb.somar(2,3))"` → `5`.

E para confirmar que o vazamento não voltou — esperado `blocos vazados: 1`; se voltar
`100001`, alguém tirou os `Py_DECREF` de `py_info_threads`:

```bash
OMP_NUM_THREADS=99999 PYTHONPATH=build/src/python python3 -c "
import sys, academy_cp as cp
for _ in range(1000): cp.info_threads()
a = sys.getallocatedblocks()
for _ in range(100000): cp.info_threads()
print('blocos vazados:', sys.getallocatedblocks() - a)"
```

## Referências

### Neste repositório

- [`embed-python`](../embed-python/README.md) — a direção inversa: C++ hospeda o
  interpretador, com guarda RAII para `Py_Initialize`/`Py_Finalize`.
- [`mandelbrot_openmp`](../mandelbrot_openmp/README.md) — OpenMP a fundo: `schedule`,
  granularidade, escalabilidade medida.
- [`smart-pointers`](../smart-pointers/README.md) e [`memory`](../memory/README.md) — o RAII
  e a posse que a CPython API obriga você a fazer à mão.
- [`concurrency`](../concurrency/README.md) — mutex e data race; o GIL é uma instância disso.
- [`linkage`](../linkage/README.md) — `extern "C"` e símbolos exportados: por que o nome
  `PyInit_academy_cp` precisa aparecer no `.so`.
- [`templates`](../templates/README.md) — a dedução de tipos que gera o binding do pybind11.
- [`meson`](../meson/README.md) — o `py.extension_module()` usado no `meson.build` daqui.
- [`yolo`](../yolo/README.md) — um caso real da razão nº 1: o ONNX Runtime só existe em
  Python porque alguém escreveu o binding.

### Fora daqui

- [Extending and Embedding the Python Interpreter](https://docs.python.org/3/extending/index.html)
  — o tutorial oficial; leia o capítulo 1 inteiro e a seção *Reference Counts*.
- [Python/C API Reference Manual](https://docs.python.org/3/c-api/index.html) — a referência
  por função; diz, em cada entrada, se o retorno é *new* ou *borrowed*.
- [pybind11 — documentação](https://pybind11.readthedocs.io/) — *First steps*, *Functions*
  (`py::arg`, políticas de retorno), *STL conversions* e *Thread safety / GIL*.
- [PEP 489 — Multi-phase extension module initialization](https://peps.python.org/pep-0489/)
  — a alternativa moderna ao `-1` do `PyModuleDef`.
- [meson-python](https://mesonbuild.com/meson-python/) — o backend usado em `standalone/`.
- [OpenMP 5.2](https://www.openmp.org/specifications/) — `parallel for`, `reduction`.
