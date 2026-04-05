#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <omp.h>

namespace py = pybind11;
using namespace pybind11::literals;

// ============================================================
// FUNÇÕES C++ PURAS — sem nenhuma referência ao Python
//
// Essa é a maior diferença em relação à CPython API:
// a lógica de negócio fica completamente separada do binding.
// Na CPython API, a lógica estava misturada com PyArg_ParseTuple,
// PyLong_FromLong, etc. Aqui são funções C++ normais.
// ============================================================

int somar(int a, int b) {
    return a + b;
}

int multiplicar(int a, int b) {
    return a * b;
}

// CPython: PyErr_SetString(PyExc_ValueError, "...") + return nullptr
// pybind11: throw py::value_error("...") — mapeado automaticamente para ValueError
long fatorial(int n) {
    if (n < 0) throw py::value_error("fatorial não definido para negativos");
    long resultado = 1;
    for (int i = 2; i <= n; i++) resultado *= i;
    return resultado;
}

// ============================================================
// multiplicar_vetor — OpenMP + GIL
//
// CPython:
//   - copiar: loop com PyList_GetItem + PyFloat_AsDouble
//   - GIL:    Py_BEGIN_ALLOW_THREADS / Py_END_ALLOW_THREADS
//   - montar:  loop com PyList_SetItem + PyFloat_FromDouble
//
// pybind11:
//   - copiar: pybind11/stl.h faz automaticamente list → std::vector
//   - GIL:    py::gil_scoped_release (RAII — readquire ao sair do escopo)
//   - montar: std::vector retornado → pybind11/stl.h converte para list
// ============================================================
std::vector<double> multiplicar_vetor(const std::vector<double>& v, double escalar) {
    int n = (int)v.size();
    std::vector<double> resultado(n);

    {
        py::gil_scoped_release release;  // libera o GIL

        #pragma omp parallel for
        for (int i = 0; i < n; i++) {
            resultado[i] = v[i] * escalar;
        }

    } // destrutor de release → GIL readquirido automaticamente

    return resultado;
}

// ============================================================
// soma_vetor — reduction paralela
// ============================================================
double soma_vetor(const std::vector<double>& v) {
    int n    = (int)v.size();
    double total = 0.0;

    {
        py::gil_scoped_release release;

        #pragma omp parallel for reduction(+:total)
        for (int i = 0; i < n; i++) {
            total += v[i];
        }
    }

    return total;
}

// ============================================================
// info_threads — retorna dict com info do ambiente OpenMP
//
// CPython: PyDict_New() + PyDict_SetItemString() manual
// pybind11: py::dict com sintaxe de kwargs via _a literal
// ============================================================
py::dict info_threads() {
    return py::dict(
        "max_threads"_a = omp_get_max_threads(),
        "num_procs"_a   = omp_get_num_procs()
    );
}

// ============================================================
// PYBIND11_MODULE — substitui toda a burocracia CPython:
//
// CPython precisava de:
//   static PyMethodDef AcademyMethods[] = { ... };   ← tabela de métodos
//   static struct PyModuleDef academymodule = { ... }; ← definição do módulo
//   PyMODINIT_FUNC PyInit_academy() { ... }           ← ponto de entrada
//
// pybind11: um único bloco declarativo
// ============================================================
PYBIND11_MODULE(academy_pb, m) {
    m.doc() = "Módulo academy reimplementado com pybind11 + OpenMP";

    m.def("somar",             &somar,
          "Soma dois inteiros",
          py::arg("a"), py::arg("b"));

    m.def("multiplicar",       &multiplicar,
          "Multiplica dois inteiros",
          py::arg("a"), py::arg("b"));

    m.def("fatorial",          &fatorial,
          "Calcula o fatorial de n",
          py::arg("n"));

    m.def("multiplicar_vetor", &multiplicar_vetor,
          "Multiplica cada elemento por escalar (OpenMP)",
          py::arg("v"), py::arg("escalar"));

    m.def("soma_vetor",        &soma_vetor,
          "Soma todos os elementos (OpenMP reduction)",
          py::arg("v"));

    m.def("info_threads",      &info_threads,
          "Informações sobre threads OpenMP disponíveis");
}