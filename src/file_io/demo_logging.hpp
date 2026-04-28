#pragma once
// =============================================================================
//  demo_logging.hpp — Logging: stdout/stderr, redirecionamento e Logger caseiro
// =============================================================================
//
//  Conceitos cobertos:
//
//  A) Redirecionamento de fd em C++ (equivalente ao shell 1> 2> 2>&1):
//       • std::cout.rdbuf()  — troca o buffer de cout
//       • std::cerr.rdbuf()  — troca o buffer de cerr
//       • freopen()          — nível C (global, até para printf)
//       • dup2() via POSIX   — nível de file descriptor (fd=1, fd=2)
//
//  B) Logger caseiro com:
//       • Níveis: TRACE < DEBUG < INFO < WARN < ERROR < FATAL
//       • Saída simultânea: terminal (colorido) + arquivo rotativo
//       • Thread-safe via std::mutex
//       • Timestamps
//
//  C) Equivalentes shell:
//       ./prog 1>out.txt           — redireciona stdout para arquivo
//       ./prog 2>err.txt           — redireciona stderr para arquivo
//       ./prog 1>out.txt 2>&1      — ambos para o mesmo arquivo
//       ./prog 2>&1 | tee log.txt  — vê no terminal E salva
//
// =============================================================================

#include <fmt/core.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <chrono>
#include <mutex>
#include <memory>
#include <vector>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <functional>

namespace demo_logging {

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  Utilitário: timestamp legível
// ─────────────────────────────────────────────────────────────────────────────
static std::string agora() {
    auto now    = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms     = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    // std::put_time é thread-safe em implementações modernas
    oss << std::put_time(std::localtime(&time_t), "%H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. Redirecionamento de rdbuf — trocar o buffer de cout/cerr em C++
// ─────────────────────────────────────────────────────────────────────────────
void demo_rdbuf_redirect() {
    fmt::print("\n══ 1. rdbuf() — redirecionamento de streams em C++ ══\n");

    const std::string log_path = "/tmp/cpp_academy_rdbuf.txt";

    // ── Redirecionar cout para arquivo ────────────────────────────────────
    {
        std::ofstream arquivo(log_path);

        // Salva o buffer original de cout
        std::streambuf* buf_original = std::cout.rdbuf();

        // Substitui o buffer: cout agora escreve em 'arquivo'
        std::cout.rdbuf(arquivo.rdbuf());

        std::cout << "Esta linha foi para o arquivo!\n";
        std::cout << "E esta também.\n";

        // IMPORTANTE: restaurar antes de qualquer fmt::print ou destruição
        std::cout.rdbuf(buf_original);
    }

    fmt::print("  cout restaurado. Conteúdo do arquivo:\n");
    std::ifstream leitura(log_path);
    std::string linha;
    while (std::getline(leitura, linha))
        fmt::print("    > {}\n", linha);

    // ── Capturar saída em string (útil em testes) ─────────────────────────
    {
        std::ostringstream captura;
        std::streambuf* buf_original = std::cout.rdbuf(captura.rdbuf());

        std::cout << "linha capturada 1\n";
        std::cout << "linha capturada 2\n";

        std::cout.rdbuf(buf_original);
        fmt::print("  capturado em string: '{}'\n", captura.str().substr(0, 40));
    }

    // ── RAII helper: RedirectGuard ────────────────────────────────────────
    fmt::print("  [padrão RAII para redirecionamento seguro]\n");
    fmt::print("  → ver classe RedirectGuard abaixo\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  RedirectGuard — RAII para redirecionamento de stream
// ─────────────────────────────────────────────────────────────────────────────
class RedirectGuard {
public:
    // Redireciona 'stream' para 'destino'
    explicit RedirectGuard(std::ostream& stream, std::ostream& destino)
        : stream_(stream), buf_original_(stream.rdbuf(destino.rdbuf())) {}

    // Redireciona 'stream' para um arquivo
    explicit RedirectGuard(std::ostream& stream, const std::string& path)
        : stream_(stream), arquivo_(path), buf_original_(stream.rdbuf(arquivo_.rdbuf())) {}

    ~RedirectGuard() {
        stream_.rdbuf(buf_original_);   // restaura automaticamente
    }

    RedirectGuard(const RedirectGuard&)            = delete;
    RedirectGuard& operator=(const RedirectGuard&) = delete;

private:
    std::ostream&   stream_;
    std::ofstream   arquivo_;        // vazio se redirecionou para outro stream
    std::streambuf* buf_original_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  2. freopen() — redirecionamento C (afeta printf e streams C)
// ─────────────────────────────────────────────────────────────────────────────
void demo_freopen() {
    fmt::print("\n══ 2. freopen() — redirecionamento nível C ══\n");

    // NOTA: freopen altera stdin/stdout/stderr globalmente
    //       Use com cuidado — não é reversível facilmente
    fmt::print("  freopen(\"arquivo\", \"w\", stderr)  → redireciona stderr para arquivo\n");
    fmt::print("  freopen(\"/dev/tty\",  \"w\", stdout) → restaura stdout para terminal\n");
    fmt::print("  (não executado aqui para não quebrar o demo)\n\n");

    // Exemplo seguro: redirecionar para /dev/null temporariamente
    // FILE* old = freopen("/dev/null", "w", stdout);
    // printf("invisível\n");   // vai para /dev/null
    // freopen("/dev/tty", "w", stdout);  // restaura

    fmt::print("  Equivalente shell:\n");
    fmt::print("    ./prog > /dev/null        (descarta stdout)\n");
    fmt::print("    ./prog 2> /dev/null       (descarta stderr)\n");
    fmt::print("    ./prog > /dev/null 2>&1   (descarta tudo)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Logger — com níveis, timestamps e saída múltipla
// ─────────────────────────────────────────────────────────────────────────────
enum class Level : int {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
    FATAL = 5,
};

static const char* level_str(Level l) {
    switch (l) {
        case Level::TRACE: return "TRACE";
        case Level::DEBUG: return "DEBUG";
        case Level::INFO:  return "INFO ";
        case Level::WARN:  return "WARN ";
        case Level::ERROR: return "ERROR";
        case Level::FATAL: return "FATAL";
        default:           return "?????";
    }
}

// Cores ANSI para terminal
static const char* level_cor(Level l) {
    switch (l) {
        case Level::TRACE: return "\033[90m";   // cinza
        case Level::DEBUG: return "\033[36m";   // ciano
        case Level::INFO:  return "\033[32m";   // verde
        case Level::WARN:  return "\033[33m";   // amarelo
        case Level::ERROR: return "\033[31m";   // vermelho
        case Level::FATAL: return "\033[35m";   // magenta
        default:           return "\033[0m";
    }
}

class Logger {
public:
    // ── Configuração ─────────────────────────────────────────────────────
    explicit Logger(const std::string& log_path,
                    Level min_level   = Level::DEBUG,
                    bool  colorir_term = true,
                    bool  stderr_erros = true)
        : arquivo_(log_path, std::ios::app)
        , min_level_(min_level)
        , colorir_(colorir_term)
        , stderr_erros_(stderr_erros)
    {
        if (!arquivo_)
            throw std::runtime_error("Logger: não abriu " + log_path);
        log(Level::INFO, "Logger iniciado → " + log_path);
    }

    ~Logger() {
        log(Level::INFO, "Logger encerrado");
    }

    // ── API de logging ───────────────────────────────────────────────────
    void trace(std::string_view msg) { log(Level::TRACE, msg); }
    void debug(std::string_view msg) { log(Level::DEBUG, msg); }
    void info (std::string_view msg) { log(Level::INFO,  msg); }
    void warn (std::string_view msg) { log(Level::WARN,  msg); }
    void error(std::string_view msg) { log(Level::ERROR, msg); }
    void fatal(std::string_view msg) { log(Level::FATAL, msg); }

    void set_level(Level l) { min_level_ = l; }

private:
    void log(Level nivel, std::string_view msg) {
        if (nivel < min_level_) return;

        // Formata a linha de log
        std::ostringstream linha;
        linha << "[" << agora() << "] "
              << "[" << level_str(nivel) << "] "
              << msg;
        std::string texto = linha.str();

        std::lock_guard<std::mutex> lock(mutex_);

        // ── Saída no terminal ─────────────────────────────────────────────
        // ERROR e FATAL → stderr; o resto → stdout
        std::ostream& term = (stderr_erros_ && nivel >= Level::ERROR)
                                ? std::cerr
                                : std::cout;

        if (colorir_)
            term << level_cor(nivel) << texto << "\033[0m\n";
        else
            term << texto << "\n";

        // ── Saída no arquivo (sem cores ANSI) ────────────────────────────
        arquivo_ << texto << "\n";
        arquivo_.flush(); // garante escrita imediata (critical para crash)
    }

    std::ofstream arquivo_;
    Level         min_level_;
    bool          colorir_;
    bool          stderr_erros_;
    std::mutex    mutex_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  4. TeeStream — escreve simultaneamente em dois streams
// ─────────────────────────────────────────────────────────────────────────────
//  Equivalente ao `tee` do Unix: ./prog 2>&1 | tee log.txt
class TeeBuf : public std::streambuf {
public:
    TeeBuf(std::streambuf* a, std::streambuf* b) : a_(a), b_(b) {}

protected:
    int overflow(int c) override {
        if (c == EOF) return !EOF;
        if (a_->sputc(static_cast<char>(c)) == EOF) return EOF;
        if (b_->sputc(static_cast<char>(c)) == EOF) return EOF;
        return c;
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        a_->sputn(s, n);
        b_->sputn(s, n);
        return n;
    }

private:
    std::streambuf* a_;
    std::streambuf* b_;
};

void demo_tee_stream() {
    fmt::print("\n══ 4. TeeStream — cout + arquivo simultaneamente ══\n");

    const std::string log_path = "/tmp/cpp_academy_tee.txt";
    std::ofstream arquivo(log_path);

    // Cria o tee buffer e redireciona cout
    TeeBuf tee(std::cout.rdbuf(), arquivo.rdbuf());
    std::streambuf* buf_original = std::cout.rdbuf(&tee);

    std::cout << "[TEE] esta linha vai para o terminal E para o arquivo\n";
    std::cout << "[TEE] equivale a: ./prog 2>&1 | tee log.txt\n";

    // Restaura
    std::cout.rdbuf(buf_original);

    fmt::print("  conteúdo do arquivo:\n");
    std::ifstream ifs(log_path);
    std::string linha;
    while (std::getline(ifs, linha))
        fmt::print("    > {}\n", linha);
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Demonstração completa do Logger
// ─────────────────────────────────────────────────────────────────────────────
void demo_logger_completo() {
    fmt::print("\n══ 3. Logger completo com níveis e arquivo ══\n");

    const std::string log_path = "/tmp/cpp_academy_logger.log";
    Logger log(log_path, Level::TRACE, /*colorir=*/false);

    log.trace("inicializando subsistema...");
    log.debug("variável x = 42");
    log.info ("conexão estabelecida com sucesso");
    log.warn ("buffer 80% cheio — considere flush");
    log.error("falha ao abrir arquivo de configuração");
    log.fatal("estado inválido — encerrando");

    fmt::print("  log escrito em: {}\n", log_path);

    // Exibir conteúdo do arquivo de log
    fmt::print("  conteúdo:\n");
    std::ifstream ifs(log_path);
    std::string linha;
    while (std::getline(ifs, linha))
        fmt::print("    {}\n", linha);
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Dicas de shell — redirecionamento externo ao programa
// ─────────────────────────────────────────────────────────────────────────────
void demo_dicas_shell() {
    fmt::print("\n══ 5. Referência: redirecionamento shell ══\n");
    fmt::print(
        "  Redirecionamento de file descriptors:\n"
        "    fd 0 = stdin   (leitura)\n"
        "    fd 1 = stdout  (cout, printf)\n"
        "    fd 2 = stderr  (cerr, fprintf(stderr,...))\n\n"

        "  Exemplos:\n"
        "    ./prog > out.txt            — stdout → arquivo (trunca)\n"
        "    ./prog >> out.txt           — stdout → arquivo (append)\n"
        "    ./prog 2> err.txt           — stderr → arquivo\n"
        "    ./prog > out.txt 2> err.txt — stdout e stderr separados\n"
        "    ./prog > out.txt 2>&1       — stderr redireciona para onde stdout aponta\n"
        "    ./prog 2>&1 > out.txt       — CUIDADO: ordem importa!\n"
        "                                  (stderr fica no terminal, stdout no arquivo)\n"
        "    ./prog 2>&1 | tee log.txt   — tee: terminal + arquivo simultaneamente\n"
        "    ./prog < input.txt          — stdin lê de arquivo\n"
        "    ./prog < in.txt > out.txt   — pipeline completo\n\n"

        "  No código C++:\n"
        "    cout → fd 1 (stdout)\n"
        "    cerr → fd 2 (stderr) — unbuffered\n"
        "    clog → fd 2 (stderr) — buffered\n"
    );
}

inline void run() {
    demo_rdbuf_redirect();
    demo_freopen();
    demo_logger_completo();
    demo_tee_stream();
    demo_dicas_shell();
}

} // namespace demo_logging
