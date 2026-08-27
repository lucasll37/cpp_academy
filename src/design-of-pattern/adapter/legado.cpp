#include "legado.hpp"

#include <fmt/core.h>

#include <cstring>
#include <string>
#include <vector>

namespace adapter::legado {

// A implementação "de terceiros". O conteúdo não importa — o que importa é
// que a INTERFACE dela é o que ela é, e não temos como mudar.
struct LegacyLogger {
    std::string              name;
    std::vector<std::string> buffer;
    bool                     open = true;
    static constexpr std::size_t MAX = 5;
};

LegacyLogger* legacy_logger_create(const char* name) {
    return new LegacyLogger{name ? name : "unnamed", {}, true};
}

void legacy_logger_destroy(LegacyLogger* h) { delete h; }

LegacyResult legacy_logger_write(LegacyLogger* h, int level,
                                 const char* msg, std::size_t len) {
    if (!h || !h->open)              return LEGACY_NOT_OPEN;
    if (level < 0 || level > 3)      return LEGACY_BAD_LEVEL;
    if (h->buffer.size() >= LegacyLogger::MAX) return LEGACY_BUFFER_FULL;

    static const char* nivel[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    h->buffer.push_back(fmt::format("[{}][{}] {}", h->name, nivel[level],
                                    std::string{msg, len}));
    return LEGACY_OK;
}

LegacyResult legacy_logger_flush(LegacyLogger* h) {
    if (!h || !h->open) return LEGACY_NOT_OPEN;
    for (const auto& l : h->buffer) fmt::print("      {}\n", l);
    h->buffer.clear();
    return LEGACY_OK;
}

std::size_t legacy_logger_count(const LegacyLogger* h) {
    return h ? h->buffer.size() : 0;
}

const char* legacy_result_string(LegacyResult r) {
    switch (r) {
        case LEGACY_OK:          return "ok";
        case LEGACY_NOT_OPEN:    return "logger não está aberto";
        case LEGACY_BUFFER_FULL: return "buffer cheio";
        case LEGACY_BAD_LEVEL:   return "nível inválido";
    }
    return "erro desconhecido";
}

}  // namespace adapter::legado
