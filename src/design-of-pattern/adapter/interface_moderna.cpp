#include "interface_moderna.hpp"

namespace adapter {

std::string_view descrever(ErroDeLog e) {
    switch (e) {
        case ErroDeLog::NaoAberto:     return "logger não está aberto";
        case ErroDeLog::BufferCheio:   return "buffer cheio";
        case ErroDeLog::NivelInvalido: return "nível inválido";
        case ErroDeLog::Desconhecido:  return "erro desconhecido";
    }
    return "?";
}

}  // namespace adapter
