# embed-python — C++ como Host do Interpretador Python

## O que é este projeto?

Demonstra como **embarcar o interpretador Python dentro de uma aplicação C++**, permitindo que código Python seja executado como parte de um programa C++. Este é o caso inverso do projeto `python/` (onde Python importa módulos C++).

---

## Direções de integração C++ ↔ Python

```
src/python/       →  Python importa C++   (extension module)
                     $ python3 -c "import academy_pybind"

src/embed-python/ →  C++ embarca Python   (embedded interpreter)
                     $ ./embed-python  (C++ chama código Python internamente)
```

---

## Casos de uso reais

- **Scripting engine**: permitir que usuários finais personalizem o comportamento com Python
- **Hot-reload**: recarregar lógica Python sem recompilar o host C++
- **Pipeline de análise**: usar pandas/numpy/matplotlib dentro de uma pipeline C++
- **Configuração**: arquivos de config em Python puro (dinâmico, não apenas YAML)
- **Plugins**: plugins escritos em Python para uma aplicação C++

---

## Estrutura de arquivos

```
embed-python/
├── main.cpp      ← 6 demos progressivos de integração
└── meson.build
```

---

## Fluxo obrigatório

```cpp
Py_Initialize()   // inicializa o interpretador
    │
    ├── usar API Python
    │
Py_Finalize()     // finaliza o interpretador
```

**Regra crítica**: todos os `PyObject*` devem ser liberados com `Py_XDECREF` **antes** de `Py_Finalize()`.

---

## RAII Guard — garantindo a finalização

```cpp
struct PythonGuard {
    PythonGuard()  { Py_Initialize(); }
    ~PythonGuard() { Py_Finalize();   }

    // Não copiável — o interpretador é único por processo
    PythonGuard(const PythonGuard&)            = delete;
    PythonGuard& operator=(const PythonGuard&) = delete;
};

int main() {
    PythonGuard guard; // Initialize aqui
    // ... toda a API Python disponível aqui ...
    // Finalize automático no destrutor, mesmo com exceções
}
```

---

## Demo 1: `PyRun_SimpleString` — executar código inline

```cpp
const char* code = R"py(
import sys
print(f"Python {sys.version}")
resultado = 6 * 7
print(f"6 * 7 = {resultado}")
)py";

int rc = PyRun_SimpleString(code);
// rc == 0 → sucesso
// rc == -1 → erro (detalhes em PyErr_*)
```

**Limitação**: não consegue capturar valores de retorno de expressões — apenas executa.

---

## Demo 2: Importar módulo e chamar função

```cpp
// Equivale a: import math; result = math.sqrt(144.0)

PyObject* math_mod = PyImport_ImportModule("math");  // import math
PyObject* sqrt_fn  = PyObject_GetAttrString(math_mod, "sqrt"); // math.sqrt
PyObject* arg      = PyFloat_FromDouble(144.0);  // cria float Python
PyObject* result   = PyObject_CallOneArg(sqrt_fn, arg); // chama

double val = PyFloat_AsDouble(result); // extrai double C
fmt::print("sqrt(144) = {}\n", val);  // 12.0

// Libera referências (ordem: resultado → argumentos → função → módulo)
Py_XDECREF(result);
Py_XDECREF(arg);
Py_XDECREF(sqrt_fn);
Py_XDECREF(math_mod);
```

### Contagem de referências

O CPython usa **reference counting** para gerenciar memória. Cada `PyObject*` tem um contador:
- **Referência própria** (new reference): você é responsável por `Py_DECREF`
- **Referência emprestada** (borrowed reference): não decremente — o dono cuida

```cpp
// New reference → DECREF necessário
PyObject* mod = PyImport_ImportModule("math");  // new ref

// Borrowed reference → NÃO decrementar
PyObject* main = PyImport_AddModule("__main__");  // borrowed
PyObject* dict = PyModule_GetDict(main);           // borrowed
PyObject* item = PyList_GetItem(list, 0);          // borrowed
```

---

## Demo 3: Ler variáveis do namespace `__main__`

```cpp
// Executa código que define variáveis em __main__
PyRun_SimpleString(R"py(
import math
circunferencia = 2 * math.pi * 5.0
lista = [x**2 for x in range(1, 6)]
)py");

// Acessa o namespace __main__
PyObject* main_mod  = PyImport_AddModule("__main__"); // borrowed
PyObject* main_dict = PyModule_GetDict(main_mod);     // borrowed

// Lê variáveis (referências emprestadas — não decrementar)
PyObject* circ = PyDict_GetItemString(main_dict, "circunferencia");
double val = PyFloat_AsDouble(circ); // 31.4159...

PyObject* lista = PyDict_GetItemString(main_dict, "lista");
for (Py_ssize_t i = 0; i < PyList_Size(lista); i++) {
    PyObject* item = PyList_GetItem(lista, i); // borrowed
    fmt::print("{} ", PyLong_AsLong(item));
}
// Saída: 1 4 9 16 25
```

---

## Demo 4: Passar dados C++ → Python

```cpp
std::vector<double> dados = { 2.5, 4.0, 1.5, 8.0, 3.0, 6.5, 5.0 };

// Cria lista Python a partir de std::vector
PyObject* py_lista = PyList_New(dados.size());
for (size_t i = 0; i < dados.size(); i++) {
    // PyList_SetItem ROUBA a referência → não chamar Py_DECREF depois
    PyList_SetItem(py_lista, i, PyFloat_FromDouble(dados[i]));
}

// Injeta no namespace __main__ como variável "sensor_data"
PyObject* main_mod = PyImport_AddModule("__main__");
PyObject_SetAttrString(main_mod, "sensor_data", py_lista);
Py_DECREF(py_lista); // __main__ é dono agora

// Python processa os dados
PyRun_SimpleString(R"py(
import statistics
print(f"média   = {statistics.mean(sensor_data):.3f}")
print(f"desvio  = {statistics.stdev(sensor_data):.3f}")
)py");
```

---

## Demo 5: Executar arquivo `.py` externo

```cpp
FILE* fp = fopen("config/setup.py", "r");
if (fp) {
    PyRun_SimpleFile(fp, "config/setup.py");
    fclose(fp);
}
```

Útil para: scripts de configuração, plugins, lógica de inicialização.

---

## Demo 6: Capturar exceções Python em C++

```cpp
int rc = PyRun_SimpleString("resultado = 1 / 0"); // ZeroDivisionError
if (rc != 0) {
    PyObject *ptype = nullptr, *pvalue = nullptr, *ptraceback = nullptr;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback); // extrai e limpa o erro
    PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);

    // Extrai nome do tipo de exceção
    PyObject* name = PyObject_GetAttrString(ptype, "__name__");
    fmt::print("tipo: {}\n", PyUnicode_AsUTF8(name));
    Py_XDECREF(name);

    // Extrai mensagem
    PyObject* msg = PyObject_Str(pvalue);
    fmt::print("msg: {}\n", PyUnicode_AsUTF8(msg));
    Py_DECREF(msg);

    Py_XDECREF(ptype); Py_XDECREF(pvalue); Py_XDECREF(ptraceback);
}
```

---

## Tabela de conversões C++ ↔ Python

| C++ | Para Python | De Python para C++ |
|---|---|---|
| `double` | `PyFloat_FromDouble(d)` | `PyFloat_AsDouble(obj)` |
| `long` | `PyLong_FromLong(l)` | `PyLong_AsLong(obj)` |
| `const char*` | `PyUnicode_FromString(s)` | `PyUnicode_AsUTF8(obj)` |
| `std::vector` | `PyList_New(n)` + `PyList_SetItem` | `PyList_Size` + `PyList_GetItem` |
| `bool` | `PyBool_FromLong(b)` | `PyObject_IsTrue(obj)` |
| `nullptr` | — | `Py_None` (com `Py_INCREF`) |

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja embed_python
./bin/embed_python
# com script externo:
./bin/embed_python caminho/para/script.py
```

---

## Dependências

- `Python.h` (CPython development headers) — `python3-dev` ou `python3-devel`
- `fmt` — formatação de saída
