#include "pipeline.hpp"
#include "pipelines_concretos.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace template_method {

// ── O TEMPLATE METHOD: a ordem mora aqui, e só aqui ──────────────────────────
std::vector<Registro> PipelineDeDados::executar(const std::string& fonte) {
    fmt::print("    [{}] iniciando\n", nome_);

    antes_de_tudo();                                     // hook

    auto registros = extrair(fonte);                     // obrigatório
    lidos_ = static_cast<int>(registros.size());
    fmt::print("    [{}] extraídos {} registros\n", nome_, lidos_);

    // Validar e transformar, nesta ordem — a base garante
    std::vector<Registro> saida;
    saida.reserve(registros.size());
    for (auto& r : registros) {
        if (!validar(r)) {                               // hook
            ++rejeitados_;
            continue;
        }
        transformar(r);                                  // obrigatório
        saida.push_back(std::move(r));
    }
    fmt::print("    [{}] {} rejeitados, {} transformados\n",
               nome_, rejeitados_, saida.size());

    if (deve_ordenar()) {                                // hook
        ordenar(saida);                                  // passo FIXO
        fmt::print("    [{}] ordenados\n", nome_);
    }

    depois_de_tudo();                                    // hook

    fmt::print("    [{}] concluído\n", nome_);
    return saida;
}

// Passo FIXO — privado, nenhum derivado alcança
void PipelineDeDados::ordenar(std::vector<Registro>& v) const {
    std::sort(v.begin(), v.end(),
              [](const Registro& a, const Registro& b) { return a.chave < b.chave; });
}

// ── CSV ──────────────────────────────────────────────────────────────────────
std::vector<Registro> PipelineCsv::extrair(const std::string& fonte) {
    std::vector<Registro> r;
    std::istringstream iss{fonte};
    std::string linha;
    while (std::getline(iss, linha)) {
        const auto virgula = linha.find(',');
        if (virgula == std::string::npos) continue;
        r.push_back({linha.substr(0, virgula),
                     std::stod(linha.substr(virgula + 1)), true});
    }
    return r;
}

void PipelineCsv::transformar(Registro& r) { r.valor *= 1.10; }   // +10%

// ── JSON ─────────────────────────────────────────────────────────────────────
std::vector<Registro> PipelineJson::extrair(const std::string& fonte) {
    // Parser simplificado: "chave":valor separados por ';'
    std::vector<Registro> r;
    std::istringstream iss{fonte};
    std::string par;
    while (std::getline(iss, par, ';')) {
        const auto sep = par.find(':');
        if (sep == std::string::npos) continue;
        r.push_back({par.substr(0, sep), std::stod(par.substr(sep + 1)), true});
    }
    return r;
}

void PipelineJson::transformar(Registro& r) { r.valor = std::round(r.valor); }

bool PipelineJson::validar(const Registro& r) const {
    return PipelineDeDados::validar(r) && r.valor >= 0;   // ← estende o default
}

// ── Banco ────────────────────────────────────────────────────────────────────
void PipelineBanco::antes_de_tudo() {
    conectado_ = true;
    fmt::print("    [Banco] conectado ao servidor\n");
}

void PipelineBanco::depois_de_tudo() {
    conectado_ = false;
    fmt::print("    [Banco] conexão fechada\n");
}

std::vector<Registro> PipelineBanco::extrair(const std::string&) {
    return {{"zeta", 30.0, true}, {"alfa", 10.0, true},
            {"beta", -5.0, true}, {"gama", 20.0, false}};
}

void PipelineBanco::transformar(Registro& r) { r.valor /= 2.0; }

bool PipelineBanco::validar(const Registro& r) const {
    return PipelineDeDados::validar(r) && r.valor > 0;
}

}  // namespace template_method
