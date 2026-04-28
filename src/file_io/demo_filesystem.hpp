#pragma once
// =============================================================================
//  demo_filesystem.hpp — std::filesystem (C++17): manipulação de arquivos e dirs
// =============================================================================
//
//  std::filesystem provê uma API portável (Linux/macOS/Windows) para:
//    • Criar, mover, copiar e deletar arquivos e diretórios
//    • Iterar por diretórios (recursivo ou não)
//    • Inspecionar metadados: tamanho, permissões, timestamps
//    • Manipular paths de forma segura (sem concatenação de strings)
//
//  Cabeçalho: <filesystem>
//  Namespace: std::filesystem (alias: namespace fs = std::filesystem;)
//
// =============================================================================

#include <fmt/core.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace demo_filesystem {

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  Utilitário: formatar file_time_type como string
// ─────────────────────────────────────────────────────────────────────────────
static std::string fmt_time(fs::file_time_type ftime) {
    auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                   ftime - fs::file_time_type::clock::now()
                         + std::chrono::system_clock::now());
    auto tt = std::chrono::system_clock::to_time_t(sys);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. path — manipulação de caminhos
// ─────────────────────────────────────────────────────────────────────────────
void demo_path() {
    fmt::print("\n══ 1. fs::path — composição e decomposição de caminhos ══\n");

    fs::path p = "/tmp/cpp_academy/subdir/arquivo.txt";

    fmt::print("  path completo:  {}\n", p.string());
    fmt::print("  root_name():    {}\n", p.root_name().string());    // vazio no Linux
    fmt::print("  root_dir():     {}\n", p.root_directory().string()); // /
    fmt::print("  parent_path():  {}\n", p.parent_path().string());  // /tmp/cpp_academy/subdir
    fmt::print("  filename():     {}\n", p.filename().string());     // arquivo.txt
    fmt::print("  stem():         {}\n", p.stem().string());         // arquivo
    fmt::print("  extension():    {}\n", p.extension().string());    // .txt

    // ── Composição com operator/ ───────────────────────────────────────
    fs::path base  = "/tmp/cpp_academy";
    fs::path sub   = base / "logs" / "2024" / "app.log";
    fmt::print("  base / logs / 2024 / app.log:\n    {}\n", sub.string());

    // ── Modificação de extensão ────────────────────────────────────────
    fs::path p2 = p;
    p2.replace_extension(".bak");
    fmt::print("  replace_extension(.bak): {}\n", p2.filename().string());

    // ── Paths relativos vs absolutos ──────────────────────────────────
    fs::path relativo = "src/main.cpp";
    fmt::print("  is_absolute(): {}\n", relativo.is_absolute());
    fmt::print("  is_relative(): {}\n", relativo.is_relative());

    // ── Normalização (remove . e ..) ──────────────────────────────────
    fs::path feio = "/tmp/cpp_academy/../cpp_academy/./logs";
    fmt::print("  lexically_normal(): {}\n", feio.lexically_normal().string());
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Criar e remover arquivos e diretórios
// ─────────────────────────────────────────────────────────────────────────────
void demo_criar_remover() {
    fmt::print("\n══ 2. Criar e remover arquivos e diretórios ══\n");

    const fs::path base = "/tmp/cpp_academy_fs_demo";

    // ── create_directories (cria toda a hierarquia) ────────────────────
    fs::create_directories(base / "subdir1" / "nested");
    fs::create_directories(base / "subdir2");
    fmt::print("  criado: {}\n", (base / "subdir1/nested").string());

    // ── Criar arquivos de teste ────────────────────────────────────────
    for (auto& nome : {"alfa.txt", "beta.cpp", "gamma.hpp"}) {
        std::ofstream ofs(base / nome);
        ofs << "conteudo de " << nome << "\n";
    }
    {
        std::ofstream ofs(base / "subdir1" / "nested" / "deep.txt");
        ofs << "arquivo profundo\n";
    }

    fmt::print("  arquivos criados\n");

    // ── copy e copy_file ──────────────────────────────────────────────
    fs::copy_file(base / "alfa.txt",
                  base / "subdir2" / "alfa_backup.txt",
                  fs::copy_options::overwrite_existing);
    fmt::print("  cópia: alfa.txt → subdir2/alfa_backup.txt\n");

    // ── rename / move ─────────────────────────────────────────────────
    fs::rename(base / "gamma.hpp", base / "gamma_renamed.hpp");
    fmt::print("  renomeado: gamma.hpp → gamma_renamed.hpp\n");

    // ── exists / is_regular_file / is_directory ───────────────────────
    fmt::print("  exists(alfa.txt)    = {}\n", fs::exists(base / "alfa.txt"));
    fmt::print("  is_directory(base)  = {}\n", fs::is_directory(base));
    fmt::print("  is_regular_file()   = {}\n", fs::is_regular_file(base / "beta.cpp"));
    fmt::print("  is_symlink()        = {}\n", fs::is_symlink(base / "alfa.txt"));

    // ── file_size ─────────────────────────────────────────────────────
    fmt::print("  tamanho de alfa.txt = {} bytes\n",
        fs::file_size(base / "alfa.txt"));

    // Cleanup final será feito por demo_iteracao_recursiva
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Iteração de diretórios
// ─────────────────────────────────────────────────────────────────────────────
void demo_iteracao() {
    fmt::print("\n══ 3. Iteração de diretórios ══\n");

    const fs::path base = "/tmp/cpp_academy_fs_demo";

    // ── directory_iterator — nível único ──────────────────────────────
    fmt::print("  [directory_iterator — nível único]\n");
    for (const auto& entrada : fs::directory_iterator(base)) {
        const char* tipo = entrada.is_directory()    ? "DIR "
                         : entrada.is_regular_file() ? "FILE"
                                                     : "????";
        fmt::print("    [{}] {}\n", tipo, entrada.path().filename().string());
    }

    // ── recursive_directory_iterator — toda a árvore ──────────────────
    fmt::print("\n  [recursive_directory_iterator — árvore completa]\n");
    for (const auto& entrada : fs::recursive_directory_iterator(base)) {
        int profundidade = static_cast<int>(
            std::distance(entrada.path().begin(), entrada.path().end()) -
            std::distance(base.begin(), base.end())
        );
        std::string indentacao(static_cast<size_t>(profundidade * 2), ' ');
        const char* tipo = entrada.is_directory()    ? "📁"
                         : entrada.is_regular_file() ? "📄"
                                                     : "❓";
        fmt::print("    {}{} {}\n", indentacao, tipo,
            entrada.path().filename().string());
    }

    // ── Filtrar por extensão ───────────────────────────────────────────
    fmt::print("\n  [apenas .txt]\n");
    for (const auto& e : fs::recursive_directory_iterator(base)) {
        if (e.is_regular_file() && e.path().extension() == ".txt")
            fmt::print("    {}\n", e.path().string());
    }

    // ── Limpeza ────────────────────────────────────────────────────────
    std::error_code ec;
    uintmax_t removidos = fs::remove_all(base, ec);
    fmt::print("\n  remove_all(): {} entradas removidas\n", removidos);
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Metadados e permissões
// ─────────────────────────────────────────────────────────────────────────────
void demo_metadados() {
    fmt::print("\n══ 4. Metadados, permissões e espaço em disco ══\n");

    // Cria arquivo temporário para inspecionar
    const fs::path p = "/tmp/cpp_academy_meta.txt";
    {
        std::ofstream ofs(p);
        ofs << "conteúdo de teste para metadados\n";
    }

    // ── last_write_time ────────────────────────────────────────────────
    auto mtime = fs::last_write_time(p);
    fmt::print("  last_write_time: {}\n", fmt_time(mtime));

    // Modificar o timestamp
    // fs::last_write_time(p, fs::file_time_type::clock::now());

    // ── Permissões ─────────────────────────────────────────────────────
    auto perms = fs::status(p).permissions();
    auto tem = [&](fs::perms bit) {
        return (perms & bit) != fs::perms::none;
    };

    fmt::print("  permissões:\n");
    fmt::print("    owner: r={} w={} x={}\n",
        tem(fs::perms::owner_read),
        tem(fs::perms::owner_write),
        tem(fs::perms::owner_exec));
    fmt::print("    group: r={} w={} x={}\n",
        tem(fs::perms::group_read),
        tem(fs::perms::group_write),
        tem(fs::perms::group_exec));
    fmt::print("    other: r={} w={} x={}\n",
        tem(fs::perms::others_read),
        tem(fs::perms::others_write),
        tem(fs::perms::others_exec));

    // Adicionar permissão de execução ao dono
    fs::permissions(p,
        fs::perms::owner_exec,
        fs::perm_options::add);
    fmt::print("  owner_exec adicionado\n");

    // Remover permissão de escrita do owner
    fs::permissions(p,
        fs::perms::owner_write,
        fs::perm_options::remove);
    fmt::print("  owner_write removido\n");

    // Restaurar para poder deletar
    fs::permissions(p, fs::perms::owner_all, fs::perm_options::add);

    // ── Espaço em disco ────────────────────────────────────────────────
    auto space = fs::space("/tmp");
    fmt::print("  espaço em /tmp:\n");
    fmt::print("    capacity:  {:>12} bytes\n", space.capacity);
    fmt::print("    free:      {:>12} bytes\n", space.free);
    fmt::print("    available: {:>12} bytes\n", space.available);

    fs::remove(p);

    // ── Paths especiais ────────────────────────────────────────────────
    fmt::print("  current_path(): {}\n", fs::current_path().string());
    fmt::print("  temp_directory_path(): {}\n", fs::temp_directory_path().string());
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Tratamento de erros com std::error_code
// ─────────────────────────────────────────────────────────────────────────────
void demo_error_code() {
    fmt::print("\n══ 5. Tratamento de erros — exceção vs error_code ══\n");

    // ── Versão com exceção (padrão) ────────────────────────────────────
    try {
        fs::file_size("/tmp/nao_existe_absolutamente.xyz");
    } catch (const fs::filesystem_error& e) {
        fmt::print("  exceção: {}\n", e.what());
        fmt::print("  path1:   {}\n", e.path1().string());
        fmt::print("  code:    {}\n", e.code().message());
    }

    // ── Versão sem exceção — error_code ───────────────────────────────
    // Sobrecargas com error_code como último parâmetro nunca lançam
    std::error_code ec;
    auto tam = fs::file_size("/tmp/nao_existe.xyz", ec);
    if (ec) {
        fmt::print("  error_code: {} (valor={})\n", ec.message(), ec.value());
    } else {
        fmt::print("  tamanho: {}\n", tam);
    }

    fmt::print("  → Prefira error_code em código de alta performance\n");
    fmt::print("  → Use exceções quando falha é realmente excepcional\n");
}

inline void run() {
    demo_path();
    demo_criar_remover();
    demo_iteracao();
    demo_metadados();
    demo_error_code();
}

} // namespace demo_filesystem
