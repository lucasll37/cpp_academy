# python — Módulos de Extensão C++ para Python

## O que é este projeto?

Implementa o mesmo **módulo de extensão Python** de duas formas diferentes:
1. **CPython API** (`academy_cpython.cpp`) — API C de baixo nível
2. **PyBind11** (`academy_pybind.cpp`) — biblioteca moderna C++ sobre CPython

Ambos criam módulos que Python pode importar como `import academy_cp` / `import academy_pb`.

---

## Direção desta integração

```
src/python/        → Python importa C++   (extension module)
                      $ python3 -c "import academy_pb; print(academy_pb.somar(2,3))"

src/embed-python/  → C++ embarca Python   (embedded interpreter)
                      $ ./embed_python
```

---

## Estrutura de arquivos

```
python/
├── academy_cpython.cpp  ← módulo via CPython API (verboso, didático)
├── academy_pybind.cpp   ← módulo via PyBind11 (moderno, conciso)
└── meson.build
```

---

## Funções exportadas (ambas as versões)

| Função Python | C++ equivalente | Descrição |
|---|---|---|
| `somar(a, b)` | `int somar(int a, int b)` | Soma dois inteiros |
| `multiplicar(a, b)` | `int multiplicar(int a, int b)` | Multiplicação |
| `fatorial(n)` | `long fatorial(int n)` | Fatorial com validação |
| `multiplicar_vetor(v, k)` | `vector<double> multiplicar_vetor(...)` | Paralelo com OpenMP |
| `soma_vetor(v)` | `double soma_vetor(vector<double>)` | Reduction OpenMP |
| `info_threads()` | `py::dict info_threads()` | Info sobre OpenMP |

---

## PyBind11 — implementação moderna

```cpp
// Lógica C++ pura — nenhuma referência ao Python aqui
int somar(int a, int b) { return a + b; }
int multiplicar(int a, int b) { return a * b; }

long fatorial(int n) {
    // pybind11 converte automaticamente: throw py::value_error() → Python ValueError
    if (n < 0) throw py::value_error("fatorial não definido para negativos");
    long r = 1;
    for (int i = 2; i <= n; i++) r *= i;
    return r;
}

// OpenMP + GIL
// GIL (Global Interpreter Lock): mutex que protege o runtime CPython.
// Operações longas em C++ devem liberar o GIL para não bloquear outras threads Python.
std::vector<double> multiplicar_vetor(const std::vector<double>& v, double escalar) {
    std::vector<double> resultado(v.size());
    {
        py::gil_scoped_release release;  // libera GIL → outras threads Python podem rodar
        #pragma omp parallel for
        for (int i = 0; i < (int)v.size(); i++)
            resultado[i] = v[i] * escalar;
    } // destrutor → GIL readquirido automaticamente (RAII)
    return resultado;
}

// Declaração do módulo — sintaxe limpa e declarativa
PYBIND11_MODULE(academy_pb, m) {
    m.doc() = "Módulo academy com PyBind11 + OpenMP";

    m.def("somar",    &somar,    "Soma dois inteiros",    py::arg("a"), py::arg("b"));
    m.def("fatorial", &fatorial, "Calcula fatorial de n", py::arg("n"));

    // Conversão automática: std::vector<double> ↔ Python list
    m.def("multiplicar_vetor", &multiplicar_vetor,
          "Multiplica por escalar (OpenMP)",
          py::arg("v"), py::arg("escalar"));
}
```

---

## CPython API — implementação de baixo nível (para comparação)

```cpp
// Com CPython API, a lógica fica misturada com a burocracia do binding:

static PyObject* py_fatorial(PyObject* self, PyObject* args) {
    int n;
    // PyArg_ParseTuple: parseia argumento da chamada Python
    // "i" = int, ":fatorial" = nome para mensagens de erro
    if (!PyArg_ParseTuple(args, "i:fatorial", &n))
        return nullptr;

    if (n < 0) {
        PyErr_SetString(PyExc_ValueError, "fatorial não definido para negativos");
        return nullptr;
    }

    long r = 1;
    for (int i = 2; i <= n; i++) r *= i;

    return PyLong_FromLong(r); // converte long C → Python int
}

// Tabela de métodos — obrigatória
static PyMethodDef AcademyMethods[] = {
    {"fatorial", py_fatorial, METH_VARARGS, "Calcula fatorial de n"},
    {nullptr, nullptr, 0, nullptr} // sentinela
};

// Definição do módulo
static struct PyModuleDef academymodule = {
    PyModuleDef_HEAD_INIT,
    "academy_cp",   // nome do módulo
    nullptr,        // doc string
    -1,             // -1 = módulo sem estado por interpretador
    AcademyMethods
};

// Ponto de entrada: Python chama esta função ao importar o módulo
PyMODINIT_FUNC PyInit_academy_cp(void) {
    return PyModule_Create(&academymodule);
}
```

---

## Comparação: CPython API vs PyBind11

| Aspecto | CPython API | PyBind11 |
|---|---|---|
| Linhas de código | ~5x mais | Conciso |
| Lógica vs binding | Misturados | Separados |
| Conversão de tipos | Manual (PyLong_From*, etc.) | Automática |
| Exceções | `PyErr_SetString` + return nullptr | `throw` C++ normal |
| GIL | `Py_BEGIN_ALLOW_THREADS` macro | `py::gil_scoped_release` RAII |
| STL containers | Manual | `#include <pybind11/stl.h>` |
| Numpy arrays | Muito verboso | `#include <pybind11/numpy.h>` |
| Overhead runtime | Zero (API nativa) | Mínimo (wrapper fino) |

---

## Usando em Python

```python
import academy_pb as ac

# Inteiros
print(ac.somar(2, 3))      # 5
print(ac.fatorial(10))     # 3628800

# Exceção C++ propagada para Python
try:
    ac.fatorial(-1)
except ValueError as e:
    print(e)  # "fatorial não definido para negativos"

# Vetor paralelo (OpenMP)
v = [1.0, 2.0, 3.0, 4.0, 5.0]
resultado = ac.multiplicar_vetor(v, 2.5)
print(resultado)  # [2.5, 5.0, 7.5, 10.0, 12.5]

# Info de threads
info = ac.info_threads()
print(f"max_threads: {info['max_threads']}, procs: {info['num_procs']}")
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja academy_cpython academy_pybind

# Adiciona o diretório ao PYTHONPATH para importar
PYTHONPATH=dist/lib python3 -c "import academy_pb; print(academy_pb.somar(2,3))"
```

---

## Dependências

- `Python.h` — CPython development headers
- `pybind11` — binding moderno C++/Python (header-only)
- OpenMP — paralelismo (flag `-fopenmp`)
