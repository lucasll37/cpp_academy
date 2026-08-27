#include "relatorio.hpp"

#include <fmt/core.h>

namespace builder {

std::string Relatorio::descrever() const {
    std::string s = fmt::format("'{}' [{}]", titulo_, formato_);
    if (!colunas_.empty()) {
        s += fmt::format(" colunas={}", colunas_.size());
    }
    s += fmt::format(" cab={} rod={} sep='{}' dec={}",
                     cabecalho_ ? "sim" : "nao",
                     rodape_ ? "sim" : "nao",
                     separador_, casas_decimais_);
    if (limite_ > 0) s += fmt::format(" limite={}", limite_);
    if (codificacao_ != "utf-8") s += fmt::format(" enc={}", codificacao_);
    return s;
}

}  // namespace builder
