# embed-python — C++ Hospedando o Interpretador Python

## O que é este projeto?

Um programa **C++** que carrega o **interpretador do Python para dentro do próprio
processo** e passa a executar código Python como parte da sua execução. Não há `python3`
rodando ao lado, nem troca de mensagens entre dois programas: é **um** executável, com o
CPython dentro dele.

Seis demos, do mínimo ao realista: executar uma string, chamar uma função de um módulo, ler
variáveis de volta, empurrar dados do C++ para o Python, rodar um arquivo `.py` do disco e
capturar uma exceção Python como um dado que o C++ pode inspecionar.

```bash
# ⚠ este alvo vem DESATIVADO no build (ele exige os headers do Python).
#    Passo 1 — descomente a linha em src/meson.build:
sed -i "s|^# subdir('embed-python')|subdir('embed-python')|" src/meson.build

#    Passo 2 — compile e instale
make build && make install

#    Passo 3 — rode A PARTIR DA RAIZ do repositório
./dist/bin/embed_python
```

> ⚠ **Rode a partir da raiz.** A Demo 5 procura o script em
> `./src/embed-python/scripts/hello.py`, que é um caminho **relativo ao diretório atual**.
> De outro diretório a demo é pulada com um aviso. Para rodar de qualquer lugar, passe o
> caminho: `./dist/bin/embed_python /caminho/para/script.py`.

> O executável se chama `embed_python` (com **underscore**), embora o diretório seja
> `embed-python` (com hífen). É o nome que está em
> [`meson.build`](meson.build): `executable('embed_python', ...)`.

---

## Antes de começar

Esta seção define **todo** o vocabulário usado no resto do arquivo. Se você já conhece o
termo, pule; nada aqui depende de leitura externa.

### As duas direções da integração C++ ↔ Python

São coisas **opostas**, e confundi-las é o primeiro tropeço:

| | Quem inicia o processo | Quem é a biblioteca | Neste repositório |
|---|---|---|---|
| **Extension module** | o `python3` do sistema | o seu C++, num `.so` | [`../python/`](../python/) |
| **Embedding** (aqui) | o **seu executável C++** | o CPython, em `libpython3.x.so` | este diretório |

```
../python/          →  Python importa C++     $ python3 -c "import academy_pybind"
./ (embed-python)   →  C++ embarca Python     $ ./dist/bin/embed_python
```

### O que significa "embarcar", em nível de link

Embarcar não é uma metáfora: o seu executável passa a **linkar contra a biblioteca
compartilhada do CPython**. Dá para ver:

```bash
$ ldd ./dist/bin/embed_python | grep python
	libpython3.12.so.1.0 => /lib/x86_64-linux-gnu/libpython3.12.so.1.0
```

É isso que o [`meson.build`](meson.build) pede com uma única palavra:

```meson
py = import('python').find_installation('python3')

executable('embed_python', ['main.cpp'],
    dependencies: [fmt_dep, py.dependency(embed: true)],  # ← embed: true
    install: true,
)
```

Sem `embed: true`, o Meson entrega só os **headers** (`Python.h`) e omite a biblioteca —
que é o certo para um *extension module*, onde quem já carregou a `libpython` é o processo
`python3`. Aqui somos nós o processo principal, então precisamos da biblioteca de verdade.

Para compilar à mão, o equivalente é `python3-config --embed`:

```bash
g++ -std=c++20 $(python3-config --includes) main.cpp $(python3-config --embed --ldflags)
```

O `--embed` é o que acrescenta o `-lpython3.12`. Sem ele, o link falha com
`undefined reference to 'Py_Initialize'`.

### `PyObject*` — o tipo de tudo

Na API C do CPython **todo** valor Python (um int, uma lista, um módulo, uma função, uma
exceção) é um `PyObject*`. Não há tipos separados: a distinção é feita em tempo de execução
por funções como `PyList_Check(obj)`.

### Contagem de referências — *new* vs *borrowed*

O CPython libera memória por **contagem de referências**: cada objeto tem um contador; quando
ele chega a zero, o objeto morre. `Py_INCREF` soma 1, `Py_DECREF` subtrai 1, `Py_XDECREF` é a
mesma coisa mas aceita `nullptr` sem estourar.

Cada função da API devolve o ponteiro de uma das duas formas — e **só a documentação diz
qual**:

| | O que significa | Sua obrigação |
|---|---|---|
| **new reference** (referência própria) | o contador já foi incrementado **para você** | chamar `Py_DECREF` quando terminar |
| **borrowed reference** (emprestada) | outro objeto é o dono; você só está olhando | **não** chamar `Py_DECREF` |

```cpp
PyObject* mod  = PyImport_ImportModule("math");   // NEW      → precisa de DECREF
PyObject* main = PyImport_AddModule("__main__");  // BORROWED → NÃO decrementar
PyObject* dict = PyModule_GetDict(main);          // BORROWED
PyObject* item = PyList_GetItem(lista, 0);        // BORROWED
```

> ⚠ **Os dois erros simétricos.** Esquecer o `DECREF` de uma *new reference* é **vazamento**.
> Dar `DECREF` numa *borrowed reference* é **destruir um objeto que ainda tem dono** — o
> próximo uso é *use-after-free*, e o sintoma costuma aparecer longe da causa. Quando a
> dúvida existir, a regra que não erra é: **olhe a documentação da função**; a API não tem
> nenhuma pista sintática que diferencie os dois casos.

Existe um terceiro caso, mais raro, que aparece na Demo 4: funções que **roubam** a
referência (*steal*), como `PyList_SetItem`. Você entrega o ponteiro e deixa de ser
responsável por ele — dar `DECREF` depois é erro.

### RAII

Padrão do C++ em que **adquirir** um recurso é construir um objeto e **liberá-lo** é destruí-lo.
Como o destrutor roda sozinho ao sair do escopo — inclusive quando uma exceção é lançada —
o par "abre/fecha" nunca fica desemparelhado. É o mesmo mecanismo de `std::unique_ptr` e
`std::lock_guard` ([`../smart-pointers/`](../smart-pointers/)). Aqui ele garante o par
`Py_Initialize()` / `Py_Finalize()`.

### `__main__`

Quando você roda `python3 script.py`, o código do topo não executa "no vazio": ele executa
dentro de um módulo chamado `__main__`, cujo dicionário é o **escopo global** daquele
programa. É o mesmo namespace do REPL interativo. Aqui ele é o **quadro-negro compartilhado**
entre C++ e Python: o C++ escreve variáveis nele, o Python as lê, e vice-versa.

### `Py_ssize_t`

O inteiro com sinal que o CPython usa para tamanhos e índices — o equivalente de
`std::ptrdiff_t`. `PyList_Size` devolve `Py_ssize_t`, e é por isso que os laços do código
usam esse tipo em vez de `size_t` (evita o `-Wsign-compare`). O
`#define PY_SSIZE_T_CLEAN` no topo do `main.cpp` é obrigatório desde o Python 3.10: ele diz
que as funções de parsing de argumentos devem usar `Py_ssize_t` em vez de `int`.

### `R"py( ... )py"` — *raw string literal*

Sintaxe de C++11 para uma string **sem escapes**: dentro dela, `\n` é uma contrabarra
seguida de um `n`, e não uma quebra de linha. Ideal para código Python, que é cheio de
contrabarras e aspas. O `py` é apenas um **delimitador escolhido pelo autor**: a string
termina no primeiro `)py"`. Poderia ser `R"código( ... )código"` — só não pode aparecer
dentro do texto.

### Traceback

O relatório que o Python imprime quando uma exceção não é tratada: a pilha de chamadas, o
arquivo e a linha de cada quadro, e por fim o tipo e a mensagem do erro. `ZeroDivisionError`
é a exceção de divisão por zero — usada na Demo 6 justamente por ser fácil de provocar.

---

## Por que estudar isto?

Comece pelo problema, não pela API.

**O problema:** você tem um programa C++ compilado — um servidor, um pipeline de dados, um
simulador — e alguma parte da lógica **muda mais rápido do que o ciclo de release**. A regra
de negócio de um cliente, o formato de um relatório, o filtro de um sinal. Recompilar e
redistribuir o binário a cada mudança dessas é caro; pedir para o usuário mexer no C++ é
inviável.

**As saídas que quase funcionam:**

| Saída | Por que dói |
|---|---|
| arquivo de configuração (YAML/JSON) | resolve valores, não **lógica**: não tem `if`, nem laço, nem função |
| inventar uma mini-linguagem própria | você acaba escrevendo um interpretador — e depurando-o |
| chamar `python3` como subprocesso | dados vão e voltam por texto; cada chamada custa um `fork` + import; e não dá para compartilhar objetos |

**A saída deste subprojeto:** hospedar o CPython no seu processo. A lógica vira um `.py`
editável, os dados atravessam a fronteira **em memória** (sem serializar), e a linguagem de
extensão já vem com sintaxe, biblioteca padrão e um ecossistema inteiro prontos.

**Ao terminar este README você saberá:**

1. inicializar e finalizar o interpretador **sem vazar** e sem crash na saída;
2. distinguir *new* de *borrowed reference* — a única fonte real de bugs nesta API;
3. mandar dados C++ para o Python e trazer resultados de volta, com checagem de erro correta;
4. carregar um `.py` do disco como plugin, e por que o `import` dele pode falhar;
5. transformar uma exceção Python em uma **decisão** do C++, em vez de um traceback no stderr;
6. chamar Python de uma thread C++ sem travar o processo (o GIL).

Casos de uso reais: **scripting engine** (usuário final personaliza o comportamento),
**hot-reload** de lógica sem recompilar, **pipeline de análise** que chama numpy/pandas no
meio de um fluxo C++, **configuração executável**, **plugins**.

---

## Estrutura de arquivos

```
src/embed-python/
├── main.cpp          ← as 6 demos, o RAII guard e o helper check_python_error()
├── scripts/
│   └── hello.py      ← script externo executado pela Demo 5
├── meson.build       ← py.dependency(embed: true) — o que linka a libpython
└── README.md
```

---

## O fluxo obrigatório

```
Py_Initialize()          cria o interpretador no processo
      │
      ├── toda a API Python disponível
      │   (criar objetos, importar módulos, executar código)
      │
      └── liberar TODOS os PyObject* que você possui
      │
Py_Finalize()            destrói o interpretador
```

> ⚠ **Todo `PyObject*` que você possui precisa morrer antes de `Py_Finalize()`.** Depois do
> `Finalize`, as estruturas internas do interpretador não existem mais; um `Py_DECREF`
> atrasado escreve em memória liberada. Isso vale só para as referências **próprias** — as
> emprestadas nunca são suas para liberar.
>
> A **ordem** entre objetos independentes **não importa**. Liberar o resultado antes ou
> depois do módulo é indiferente; o que importa é que todos sejam liberados.

### O guard RAII

```cpp
struct PythonGuard {
    PythonGuard()  { Py_Initialize(); }
    ~PythonGuard() { Py_Finalize();   }

    // Não copiável: o interpretador é um recurso ÚNICO por processo.
    // Uma cópia significaria dois Py_Finalize() para um Py_Initialize().
    PythonGuard(const PythonGuard&)            = delete;
    PythonGuard& operator=(const PythonGuard&) = delete;
};

int main() {
    PythonGuard guard;   // Py_Initialize() aqui
    // ... toda a API Python disponível ...
}                        // Py_Finalize() aqui, mesmo se uma exceção subir
```

O `= delete` não é decoração. Sem ele, `PythonGuard g2 = g1;` compilaria e você teria dois
destrutores para uma inicialização — o segundo `Py_Finalize()` é *undefined behavior*.

### `Py_Initialize` e a API moderna

`Py_Initialize()` é a forma clássica e suficiente para aprender. Desde o Python 3.8 existe a
API de configuração da [PEP 587](https://peps.python.org/pep-0587/), que é o caminho para
qualquer host sério:

```cpp
PyConfig config;
PyConfig_InitPythonConfig(&config);
PyConfig_SetBytesString(&config, &config.home, "/caminho/do/venv");
Py_InitializeFromConfig(&config);   // devolve PyStatus, e não aborta em erro
PyConfig_Clear(&config);
```

Ela permite escolher `home`, `sys.path`, `argv`, isolamento e tratamento de erros — coisas
que a versão sem argumentos herda do ambiente e você não controla.

> ⚠ **`Py_Finalize()` não é um "reset" confiável.** Módulos de extensão em C podem guardar
> estado estático que não é desfeito; reinicializar o interpretador no mesmo processo
> funciona para código Python puro e falha de formas criativas com bibliotecas nativas
> (numpy entre elas). Trate embarcar como **uma inicialização por processo**.

---

## O estado de erro do interpretador

Esta é a parte que o código usa **em todas as demos** e que precisa vir antes delas.

O CPython não usa códigos de retorno para propagar erros: ele mantém, **por thread**, uma
"exceção ativa". Quando uma função da API falha, ela devolve `nullptr` (ou `-1`) **e**
registra a exceção nesse estado. As três funções que importam:

| Função | O que faz |
|---|---|
| `PyErr_Occurred()` | **consulta** se há exceção ativa; devolve borrowed ref ou `nullptr`. **Não** limpa |
| `PyErr_Print()` | imprime o traceback no stderr **e limpa** a exceção |
| `PyErr_Fetch(&t,&v,&tb)` | **retira** tipo, valor e traceback do estado (limpando-o) e entrega a você |

O helper que aparece em todo o `main.cpp`:

```cpp
static void check_python_error(const std::string& context) {
    if (PyErr_Occurred()) {
        fmt::print(stderr, "[embed] erro em: {}\n", context);
        PyErr_Print();   // imprime o traceback e LIMPA o erro
        throw std::runtime_error("Python error in: " + context);
    }
}
```

Ele converte a convenção de erro do Python na convenção do C++ (uma exceção), e o
`PythonGuard` garante o `Py_Finalize()` na subida da pilha. **Os blocos de código deste
README chamam esse helper** — ele mora no `main.cpp`, não é da biblioteca.

> ⚠ **Uma exceção deixada pendente contamina a chamada seguinte.** Se você ignora o `nullptr`
> de uma função e chama outra, a segunda pode falhar com o erro da primeira — ou, pior,
> "ter sucesso" com o estado sujo, e o erro aparecer páginas depois. Cheque **imediatamente**.

---

## Demo 1: `PyRun_SimpleString` — o mínimo que funciona

O menor programa útil possível. Uma string entra, código Python roda no namespace `__main__`.

```cpp
#define PY_SSIZE_T_CLEAN
#include <Python.h>

int main() {
    Py_Initialize();
    int rc = PyRun_SimpleString("print('olá do Python embarcado')");
    Py_Finalize();
    return rc;                       // 0 = sucesso, -1 = erro
}
```

O que o `main.cpp` executa:

```cpp
const char* code = R"py(
import sys
print(f"Python {sys.version}")
resultado = 6 * 7
print(f"6 * 7 = {resultado}")
)py";

int rc = PyRun_SimpleString(code);
if (rc != 0) throw std::runtime_error("PyRun_SimpleString falhou");
```

**As duas limitações**, que motivam todas as demos seguintes:

1. **Não devolve valor.** `PyRun_SimpleString("6 * 7")` executa e joga o `42` fora. Para
   recuperar o resultado é preciso guardá-lo numa variável (`resultado = 6 * 7`) e ler o
   namespace depois — que é a Demo 3.
2. **Ela mesma trata o erro.** Em caso de exceção, `PyRun_SimpleString` chama `PyErr_Print()`
   internamente: o traceback vai para o stderr e a exceção é **limpa**. Conveniente para
   scripts, inútil quando o C++ precisa decidir algo — que é a Demo 6.

---

## Demo 2: importar um módulo e chamar uma função

Aqui aparecem as referências pela primeira vez. Cada linha equivale a uma linha de Python:

```cpp
// import math
PyObject* math_mod = PyImport_ImportModule("math");        // NEW ref
check_python_error("import math");

// fn = math.sqrt
PyObject* sqrt_fn = PyObject_GetAttrString(math_mod, "sqrt");  // NEW ref
check_python_error("getattr sqrt");

// arg = 144.0  (double C  →  float Python)
PyObject* arg = PyFloat_FromDouble(144.0);                 // NEW ref

// result = math.sqrt(144.0)
PyObject* result = PyObject_CallOneArg(sqrt_fn, arg);      // NEW ref
check_python_error("call sqrt");

// float Python  →  double C
double val = PyFloat_AsDouble(result);
if (val == -1.0 && PyErr_Occurred()) check_python_error("PyFloat_AsDouble");
fmt::print("math.sqrt(144.0) = {}\n", val);                // 12

// as quatro são NEW refs: as quatro precisam de DECREF
Py_XDECREF(result);
Py_XDECREF(arg);
Py_XDECREF(sqrt_fn);
Py_XDECREF(math_mod);
```

> ⚠ **`PyFloat_AsDouble` devolve `-1.0` tanto para o valor `-1.0` quanto para erro.** Não há
> valor de retorno reservado para falha, porque `-1.0` é um double perfeitamente legítimo.
> A checagem correta é o par: `if (val == -1.0 && PyErr_Occurred())`. Vale para toda a
> família — `PyLong_AsLong` devolve `-1`, `PyLong_AsSsize_t` idem. Chamar `PyErr_Occurred()`
> **incondicionalmente** também funciona, mas custa uma leitura do estado por conversão.

> ⚠ **`PyObject_CallOneArg` exige CPython 3.9+.** Em toolchains mais antigos use
> `PyObject_CallFunctionObjArgs(sqrt_fn, arg, NULL)` — repare no `NULL` terminador, que é
> obrigatório e não é verificado pelo compilador.

---

## Demo 3: ler variáveis de volta do `__main__`

A resposta para a limitação nº 1 da Demo 1: se `PyRun_SimpleString` não devolve valor, guarde
o valor numa variável e leia o namespace.

```cpp
PyRun_SimpleString(R"py(
import math
circunferencia  = 2 * math.pi * 5.0   # raio = 5
mensagem        = "calculado em Python"
lista_quadrados = [x**2 for x in range(1, 6)]
)py");

// o namespace __main__ e o seu dicionário — AMBOS emprestados
PyObject* main_mod  = PyImport_AddModule("__main__");   // BORROWED
PyObject* main_dict = PyModule_GetDict(main_mod);       // BORROWED

// PyDict_GetItemString também devolve BORROWED — não decremente
PyObject* circ = PyDict_GetItemString(main_dict, "circunferencia");
if (circ) fmt::print("circunferencia (r=5): {:.4f}\n", PyFloat_AsDouble(circ));

PyObject* lista = PyDict_GetItemString(main_dict, "lista_quadrados");
if (lista && PyList_Check(lista)) {
    Py_ssize_t n = PyList_Size(lista);                  // Py_ssize_t, não size_t
    for (Py_ssize_t i = 0; i < n; ++i) {
        PyObject* item = PyList_GetItem(lista, i);      // BORROWED
        fmt::print("{} ", PyLong_AsLong(item));
    }
}
// nenhum Py_DECREF nesta demo: TUDO aqui é emprestado.
```

Repare que a demo inteira não tem um `Py_DECREF` sequer — e está correta. Colocar um
`Py_XDECREF(main_dict)` "por simetria" derrubaria o interpretador.

> ⚠ **`PyDict_GetItemString` devolve `nullptr` quando a chave não existe — e *não* levanta
> exceção.** É o único caso comum em que `nullptr` não significa erro. Por isso o `if (circ)`
> antes de usar. Se você quer distinguir "não existe" de "existe e é `None`", compare com
> `Py_None`.

---

## Demo 4: empurrar dados C++ → Python

O caso realista: **o C++ tem os dados, o Python tem a lógica**. Nada é serializado — a lista
Python é construída em memória a partir do `std::vector`.

```cpp
std::vector<double> dados = { 2.5, 4.0, 1.5, 8.0, 3.0, 6.5, 5.0 };

PyObject* py_lista = PyList_New(static_cast<Py_ssize_t>(dados.size()));   // NEW ref
for (size_t i = 0; i < dados.size(); ++i) {
    // ⚠ PyList_SetItem ROUBA a referência do item: depois desta linha o
    //   float criado pertence à lista. NÃO chame Py_DECREF nele.
    PyList_SetItem(py_lista, static_cast<Py_ssize_t>(i),
                   PyFloat_FromDouble(dados[i]));
}

// __main__.sensor_data = py_lista
PyObject* main_mod = PyImport_AddModule("__main__");    // BORROWED
PyObject_SetAttrString(main_mod, "sensor_data", py_lista);
Py_DECREF(py_lista);   // o módulo incrementou a sua própria; largamos a nossa

PyRun_SimpleString(R"py(
import statistics
print(f"  média   = {statistics.mean(sensor_data):.3f}")
print(f"  desvio  = {statistics.stdev(sensor_data):.3f}")
)py");
check_python_error("demo_pass_data script");
```

Três detalhes que decidem se isso vaza ou não:

| Linha | O que acontece |
|---|---|
| `PyList_New(n)` | devolve **new ref** com `n` posições `NULL` — todas precisam ser preenchidas antes do uso |
| `PyList_SetItem` | **rouba** a referência do item (por isso o `PyFloat_FromDouble` inline não vaza) |
| `PyObject_SetAttrString` | **não** rouba: incrementa por conta própria, então a sua ref continua sua — daí o `Py_DECREF` explícito |

> ⚠ **`PyList_SetItem` é o irmão perigoso de `PyList_SET_ITEM`.** O primeiro checa limites; o
> segundo é uma macro sem checagem, para listas recém-criadas. E **nenhum dos dois** libera o
> valor antigo da posição de forma segura em lista já povoada — para isso existe
> `PyList_SetSlice` ou `PySequence_SetItem`.

### O caminho de volta: Python → C++ em tipos compostos

A tabela do fim deste arquivo cobre escalares. Para compostos, o padrão é o mesmo: uma
função de leitura por tipo, `nullptr` em erro, `PyErr_Occurred()` no meio quando o valor de
retorno é ambíguo.

```cpp
// dicionário: {"nome": "sensor-3", "erro": 0.02}
PyObject* d = PyDict_GetItemString(main_dict, "config");     // BORROWED
if (d && PyDict_Check(d)) {
    PyObject* nome = PyDict_GetItemString(d, "nome");        // BORROWED
    if (nome) fmt::print("{}\n", PyUnicode_AsUTF8(nome));
}

// tupla: (código, mensagem)
if (PyTuple_Check(t) && PyTuple_Size(t) == 2) {
    long        cod = PyLong_AsLong(PyTuple_GetItem(t, 0));  // BORROWED
    const char* msg = PyUnicode_AsUTF8(PyTuple_GetItem(t, 1));
}

// qualquer iterável, sem saber o tipo concreto
PyObject* it = PyObject_GetIter(obj);                        // NEW ref
while (PyObject* item = PyIter_Next(it)) {                   // NEW ref por item
    /* ... */
    Py_DECREF(item);
}
Py_DECREF(it);
```

> ⚠ **`PyUnicode_AsUTF8` devolve um ponteiro para o buffer INTERNO do objeto**, não uma cópia.
> Ele é válido enquanto o objeto Python viver. Guardá-lo num `const char*` que sobrevive ao
> `Py_DECREF` é *use-after-free*. Copie para `std::string` se for guardar.

Para arrays numéricos grandes (numpy) a ponte correta **não** é converter elemento a
elemento: é o *buffer protocol* (`PyObject_GetBuffer`), que dá acesso direto ao bloco
contíguo — zero cópia.

---

## Demo 5: rodar um `.py` do disco — a base de um sistema de plugins

O que torna um plugin útil não é executar um arquivo; é o arquivo poder **ler** o que o host
preparou. As demos 4 e 5 juntas fazem isso: a 4 injeta `sensor_data` no `__main__`, a 5
executa um script que o encontra lá.

```cpp
static void demo_run_file(const fs::path& script_path) {
    if (!fs::exists(script_path)) {          // checar ANTES é mais claro que
        /* aviso e retorna */                // deixar o fopen falhar
        return;
    }
    FILE* fp = fopen(script_path.c_str(), "r");   // PyRun_SimpleFile quer FILE*
    if (!fp) return;

    int rc = PyRun_SimpleFile(fp, script_path.c_str());
    fclose(fp);
    if (rc != 0) throw std::runtime_error("PyRun_SimpleFile falhou");
}
```

O caminho padrão é `./src/embed-python/scripts/hello.py`, e `argv[1]` o sobrescreve:

```cpp
fs::path script = (argc > 1) ? fs::path(argv[1])
                             : fs::path("./src/embed-python/scripts/hello.py");
```

O script — [`scripts/hello.py`](scripts/hello.py) — termina justamente lendo a variável que
o C++ deixou no `__main__`:

```python
import sys, platform, datetime

print(f"  rodando como script externo!")
print(f"  Python    : {sys.version.split()[0]}")
print(f"  plataforma: {platform.system()} {platform.machine()}")

# o namespace __main__ é compartilhado: o C++ injetou isto na Demo 4
if 'sensor_data' in dir():
    print(f"  sensor_data disponível: {sensor_data[:3]}...")
```

E o caminho de volta é o mesmo do Demo 3: depois do `PyRun_SimpleFile`, o host lê do
`main_dict` a variável que o plugin definiu. **Injetar antes, ler depois** — é todo o
contrato de um plugin.

### ⚠ O interpretador embarcado usa o Python do SISTEMA

Este é o erro nº 1 de quem embarca, e ele não dá nenhum aviso: falha só na hora do `import`.

```bash
$ ./dist/bin/embed_python /tmp/probe.py
  sys.executable = /usr/bin/python3
  sys.prefix     = /usr
  sys.path[0:3]  = ['/usr/lib/python312.zip', '/usr/lib/python3.12',
                    '/usr/lib/python3.12/lib-dynload']
```

Duas consequências, ambas medidas neste repositório:

**1. Um pacote instalado num *virtualenv* não é encontrado.** O `sys.prefix` do processo é
`/usr`, e não o do venv — ativar o venv no shell **não** muda isso, porque quem define o
prefixo é a `libpython` linkada, não a variável `PATH`. Um `import numpy` que funciona no
`python3` do venv falha aqui.

**2. O diretório do script *não* entra no `sys.path`.** Rodando `python3 usa.py`, o Python
insere a pasta do script em `sys.path[0]`. O interpretador embarcado **não faz isso** — a
lista acima começa direto nos diretórios do sistema:

```bash
$ python3 /tmp/modt/usa.py          # usa.py faz "import meu_modulo"
  VALOR = 7                          # ✓ acha o vizinho

$ ./dist/bin/embed_python /tmp/modt/usa.py
ModuleNotFoundError: No module named 'meu_modulo'      # ✗ mesmo arquivo
```

**As três saídas**, da mais simples à mais correta:

```cpp
// (a) acrescentar o diretório do plugin ao sys.path, no próprio C++
PyRun_SimpleString("import sys; sys.path.insert(0, '/caminho/dos/plugins')");

// (b) apontar o interpretador para o venv, ANTES de Py_Initialize
setenv("PYTHONHOME", "/caminho/do/venv", 1);
setenv("PYTHONPATH", "/caminho/do/venv/lib/python3.12/site-packages", 1);

// (c) a forma moderna e explícita — PEP 587
PyConfig config;
PyConfig_InitPythonConfig(&config);
PyConfig_SetBytesString(&config, &config.home, "/caminho/do/venv");
Py_InitializeFromConfig(&config);
PyConfig_Clear(&config);
```

A (a) resolve o caso do plugin e é o que a maioria dos hosts faz. A (c) é a única que
resolve o venv de forma robusta e sem depender do ambiente do processo.

---

## Demo 6: transformar uma exceção Python numa decisão do C++

**Por que esta demo existe:** `PyRun_SimpleString` já imprime o traceback sozinho. Se tudo o
que você quer é ver o erro na tela, não precisa de nada disto. `PyErr_Fetch` existe para o
caso em que o **C++ precisa decidir com base no tipo do erro** — tentar de novo, cair num
fallback, emitir um log estruturado, desabilitar aquele plugin.

> ⚠ **`PyRun_SimpleString` não serve para isto.** Em caso de erro ele chama `PyErr_Print()`
> internamente, que **limpa** o estado. Um `PyErr_Fetch` depois dele encontra `nullptr` e você
> imprime `tipo: desconhecido`. Use `PyRun_String`, que devolve `nullptr` e deixa a exceção
> **pendente** para você.

```cpp
PyObject* main_mod  = PyImport_AddModule("__main__");   // BORROWED
PyObject* main_dict = PyModule_GetDict(main_mod);       // BORROWED

// Py_file_input = "trecho de arquivo" (aceita statements, não só expressões).
// Devolve NEW ref em sucesso, nullptr em erro — sem imprimir, sem limpar.
PyObject* res = PyRun_String("resultado_invalido = 1 / 0",
                             Py_file_input, main_dict, main_dict);
if (res == nullptr) {
    PyObject *ptype = nullptr, *pvalue = nullptr, *ptraceback = nullptr;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);           // retira e limpa
    PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);

    // nome do tipo: ptype.__name__
    const char* type_name = "desconhecido";
    if (ptype) {
        PyObject* name = PyObject_GetAttrString(ptype, "__name__");   // NEW
        if (name) type_name = PyUnicode_AsUTF8(name);
        Py_XDECREF(name);
    }

    // mensagem: str(pvalue)
    std::string msg = "(sem mensagem)";
    if (pvalue) {
        PyObject* pstr = PyObject_Str(pvalue);                        // NEW
        if (pstr) { msg = PyUnicode_AsUTF8(pstr); Py_DECREF(pstr); }
    }

    // ── o ponto da demo: DECIDIR ──────────────────────────────────
    if (ptype && PyErr_GivenExceptionMatches(ptype, PyExc_ZeroDivisionError)) {
        fmt::print("    ação: divisão por zero é recuperável → uso 0.0\n");
    } else {
        fmt::print("    ação: erro não previsto → propagar\n");
    }

    Py_XDECREF(ptype); Py_XDECREF(pvalue); Py_XDECREF(ptraceback);
} else {
    Py_DECREF(res);   // sucesso: PyRun_String devolveu uma NEW ref
}
```

**O que cada peça faz:**

| | |
|---|---|
| `PyErr_Fetch` | **retira** os três ponteiros do estado de erro da thread; depois dele não há mais exceção ativa. Devolve **new refs** — os três precisam de `Py_XDECREF` |
| `PyErr_NormalizeException` | o `Fetch` pode devolver a exceção "crua" (a **classe**, sem instância). O Normalize garante que `pvalue` seja uma instância de `ptype` — sem ele, `str(pvalue)` pode não dar a mensagem |
| `PyErr_GivenExceptionMatches` | compara **respeitando herança**: `ZeroDivisionError` também casa com `PyExc_ArithmeticError`. Comparar `ptype == PyExc_ZeroDivisionError` com `==` erraria subclasses |
| `PyErr_Restore` | o inverso do `Fetch`: devolve a exceção ao estado, se você decidiu **não** tratá-la |

> ⚠ **Nota de versão.** `PyErr_Fetch` / `PyErr_Restore` / `PyErr_NormalizeException` estão
> **depreciados a partir do CPython 3.12** em favor de `PyErr_GetRaisedException()` e
> `PyErr_SetRaisedException()`, que trabalham com um único `PyObject*` já normalizado:
> ```cpp
> PyObject* exc = PyErr_GetRaisedException();   // NEW ref, já normalizada
> if (PyErr_GivenExceptionMatches(exc, PyExc_ZeroDivisionError)) { /* ... */ }
> Py_DECREF(exc);
> ```
> Continuam funcionando no 3.12/3.13; o código acima usa a forma clássica porque é a que
> você encontrará na maior parte do material existente.

---

## Chamar Python de uma thread C++ — o GIL

Tudo acima é *single-thread*. O caso realmente interessante — o host C++ é concorrente e
alguma thread precisa chamar o script — depende de um conceito só.

**GIL** (*Global Interpreter Lock*) é um mutex **global do interpretador**: para tocar em
**qualquer** `PyObject*`, uma thread precisa segurá-lo. É o que torna a contagem de
referências correta sem um atômico por objeto. Consequência direta: apenas **uma** thread
executa bytecode Python por vez, por mais núcleos que a máquina tenha.

A thread que chamou `Py_Initialize()` **já segura o GIL**. Qualquer outra thread precisa
pedi-lo:

```cpp
#include <thread>

void trabalho_em_thread() {
    PyGILState_STATE estado = PyGILState_Ensure();   // adquire o GIL (e registra
                                                     // a thread no interpretador)
    PyRun_SimpleString("print('olá de uma thread C++')");
    PyGILState_Release(estado);                      // devolve — sempre, em par
}

int main() {
    PythonGuard guard;                    // Py_Initialize(): a main segura o GIL

    PyThreadState* salvo = PyEval_SaveThread();   // a main SOLTA o GIL
    std::thread t{trabalho_em_thread};            // ...para a outra poder pegá-lo
    t.join();
    PyEval_RestoreThread(salvo);                  // a main pega de volta
}                                                 // Py_Finalize() precisa do GIL
```

| Função | Para quem |
|---|---|
| `PyGILState_Ensure()` / `Release()` | qualquer thread que **não sabe** se já tem o GIL — é reentrante e o caso comum |
| `PyEval_SaveThread()` / `RestoreThread()` | a thread que **tem** o GIL e vai passar um tempo em C++ puro |

> ⚠ **Esquecer o `PyGILState_Ensure` não dá erro de compilação nem exceção.** Duas threads
> mexendo em refcounts sem o lock corrompem os contadores: o sintoma é um *crash* aleatório,
> minutos depois, num objeto sem relação com o bug. E o `Release` **precisa** acontecer em
> todos os caminhos de saída — inclusive no de exceção. É o cenário clássico para um guard
> RAII:
> ```cpp
> struct GilGuard {
>     PyGILState_STATE s;
>     GilGuard()  : s(PyGILState_Ensure()) {}
>     ~GilGuard() { PyGILState_Release(s); }
>     GilGuard(const GilGuard&) = delete;
>     GilGuard& operator=(const GilGuard&) = delete;
> };
> ```

> ⚠ **A `main` precisa soltar o GIL antes de bloquear em C++.** Se ela fica presa num
> `t.join()` segurando o GIL e a thread `t` chama `PyGILState_Ensure()`, os dois esperam um
> pelo outro: **deadlock**. O `PyEval_SaveThread()` acima é o que evita isso.

**Sobre custo:** cada travessia C++ → Python paga a aquisição do GIL, a criação dos objetos
de argumento e o *dispatch* do interpretador. Num laço quente, chamar uma função Python por
iteração é a decisão errada — **passe o lote inteiro de uma vez** (uma lista, ou melhor, um
buffer via *buffer protocol*) e deixe o laço acontecer do lado Python. É a mesma lógica de
qualquer travessia de fronteira: minimize o **número** de idas, não o tamanho de cada uma.

---

## Tabela de conversões C++ ↔ Python

| C++ | C++ → Python | Python → C++ | Cuidado |
|---|---|---|---|
| `double` | `PyFloat_FromDouble(d)` | `PyFloat_AsDouble(o)` | `-1.0` é valor **e** erro → `PyErr_Occurred()` |
| `long` | `PyLong_FromLong(l)` | `PyLong_AsLong(o)` | `-1` é valor **e** erro; pode dar `OverflowError` |
| `const char*` | `PyUnicode_FromString(s)` | `PyUnicode_AsUTF8(o)` | devolve ponteiro **interno**: não sobrevive ao objeto |
| `std::string` | `PyUnicode_FromStringAndSize(s.data(), s.size())` | `PyUnicode_AsUTF8AndSize(o, &n)` | aceita `\0` no meio |
| `bool` | `PyBool_FromLong(b)` | `PyObject_IsTrue(o)` | `IsTrue` devolve `-1` em erro |
| `std::vector<T>` | `PyList_New(n)` + `PyList_SetItem` | `PyList_Size` + `PyList_GetItem` | `SetItem` **rouba**; `GetItem` **empresta** |
| `std::map` | `PyDict_New()` + `PyDict_SetItemString` | `PyDict_GetItemString` | `SetItemString` **não** rouba; `GetItemString` empresta |

**`None` não é a conversão de `nullptr`** — merece linha própria porque as duas direções são
assimétricas:

```cpp
// testar se um valor recebido é None (Py_None é um singleton: compare por ponteiro)
if (obj == Py_None) { /* ... */ }

// devolver None de uma função C: a macro faz o Py_INCREF por você
return Py_RETURN_NONE;     // equivale a: Py_INCREF(Py_None); return Py_None;
```

O `Py_INCREF` é exigido ao **devolver** `None` (porque quem recebe vai dar `DECREF`), nunca
ao **ler**. E `nullptr` na API C significa **erro**, não `None` — são coisas diferentes.

---

## Como validar

```bash
# 1. ativar o alvo (vem comentado) e compilar
sed -i "s|^# subdir('embed-python')|subdir('embed-python')|" src/meson.build
make build && make install

# 2. rodar A PARTIR DA RAIZ do repositório
./dist/bin/embed_python
```

Saída esperada (a versão do Python e a hora variam com a máquina):

```
╔══════════════════════════════════════════════╗
║        embed-python — C++ embarca Python     ║
╚══════════════════════════════════════════════╝

── 1. PyRun_SimpleString ──────────────────────────────
Python 3.12.3 (main, Jun 19 2026, 12:46:00) [GCC 13.3.0]
hello from embedded Python!
6 * 7 = 42

── 2. Importar módulo e chamar função ─────────────────
math.sqrt(144.0) = 12

── 3. Ler variáveis do namespace __main__ ─────────────
circunferencia (r=5): 31.4159
mensagem: calculado em Python
quadrados: [1, 4, 9, 16, 25]

── 4. Passar dados C++ → Python ───────────────────────
  n       = 7
  média   = 4.357
  mediana = 4.000
  desvio  = 2.304
  min/max = 1.5 / 8.0

── 5. Executar arquivo .py externo ────────────────────
  executando: ./src/embed-python/scripts/hello.py
  rodando como script externo!
  Python    : 3.12.3
  plataforma: Linux x86_64
  hora      : 14:37:58
  sensor_data disponível: [2.5, 4.0, 1.5]...

── 6. Tratar exceção Python em C++ ────────────────────
  exceção capturada em C++:
    tipo: ZeroDivisionError
    msg:  division by zero
    ação: divisão por zero é recuperável → uso 0.0

✓ embed-python concluído
```

Três verificações que valem mais que a saída:

```bash
# (a) o processo realmente linka a libpython — é isto que "embarcar" significa
ldd ./dist/bin/embed_python | grep python
#   libpython3.12.so.1.0 => /lib/x86_64-linux-gnu/libpython3.12.so.1.0

# (b) o argv[1] sobrescreve o script padrão
./dist/bin/embed_python src/embed-python/scripts/hello.py

# (c) fora da raiz, a Demo 5 é pulada — é a lição do caminho relativo
cd /tmp && /caminho/para/cpp_academy/dist/bin/embed_python
#   [aviso] script './src/embed-python/scripts/hello.py' não encontrado — pulando demo
```

E o teste que fecha o assunto de memória — nenhum vazamento **seu**:

```bash
valgrind --leak-check=full --suppressions=$(python3 -c \
  "import sysconfig,os;print(os.path.join(sysconfig.get_paths()['data'],'share/doc/python3/valgrind-python.supp'))") \
  ./dist/bin/embed_python
```

> ⚠ Sem o arquivo de supressões, o Valgrind acusa centenas de blocos "ainda alcançáveis" que
> são **do próprio CPython** (pools de inteiros pequenos, strings internalizadas, arenas do
> alocador). Eles são liberados no `Py_Finalize` ou intencionalmente mantidos até o fim do
> processo — não são bugs seus. Procure por `definitely lost` em quadros do seu `main.cpp`.

---

## Dependências

| | Pacote | Para quê |
|---|---|---|
| `Python.h` + `libpython3.x` | `python3-dev` (Debian/Ubuntu) · `python3-devel` (Fedora/RHEL) | headers e biblioteca do CPython |
| `fmt` | via Conan, já no repositório | formatação da saída |

Sem o pacote `-dev` o Meson falha logo no `find_installation` — é por isso que este alvo vem
comentado em `src/meson.build`: ele não deve quebrar o build de quem clona o repositório sem
os headers do Python.

---

## Referências

**Neste repositório:**

- [`../python/`](../python/) — o caminho **inverso**: Python importando um módulo C++ (pybind11)
- [`../smart-pointers/`](../smart-pointers/) — RAII e `unique_ptr`, o padrão do `PythonGuard`
- [`../memory/`](../memory/) — posse, vazamento e *use-after-free*, o vocabulário do refcount
- [`../concurrency/`](../concurrency/) — `std::thread`, mutex e deadlock, o vocabulário do GIL
- [`../errors/`](../errors/) — exceções vs valores de erro, o mesmo dilema da fronteira C/Python

**Documentação oficial do CPython:**

- [Embedding Python in Another Application](https://docs.python.org/3/extending/embedding.html) — o tutorial canônico deste assunto
- [Initialization, Finalization, and Threads](https://docs.python.org/3/c-api/init.html) — `Py_Initialize`, `PyGILState_*`, `PyEval_SaveThread`
- [Reference Counting](https://docs.python.org/3/c-api/refcounting.html) e [Objects, Types and Reference Counts](https://docs.python.org/3/c-api/intro.html#objects-types-and-reference-counts) — *new* vs *borrowed*, com a lista de quem devolve o quê
- [Exception Handling](https://docs.python.org/3/c-api/exceptions.html) — `PyErr_Fetch`, `PyErr_GetRaisedException`, `PyErr_GivenExceptionMatches`
- [PEP 587 — Python Initialization Configuration](https://peps.python.org/pep-0587/) — `PyConfig` e `Py_InitializeFromConfig`
- [Buffer Protocol](https://docs.python.org/3/c-api/buffer.html) — a ponte sem cópia para arrays numéricos
