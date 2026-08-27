#!/usr/bin/env python3
"""
src/meson/gerar_tabela.py — gerador de código do exemplo.

Lê `tabela.txt` e escreve um header C++ com um array constexpr.

O ponto didático não é o script: é o `custom_target()` no meson.build, que
transforma este par (entrada, script) num nó do grafo de dependências. Quem
chama isto é o Ninja, com os caminhos que o Meson substituiu em @INPUT@ e
@OUTPUT@ — nunca você, na mão.

Uso:  gerar_tabela.py <entrada.txt> <saida.hpp>
"""
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print(f"uso: {sys.argv[0]} <entrada.txt> <saida.hpp>", file=sys.stderr)
        return 1

    entrada, saida = sys.argv[1], sys.argv[2]

    itens = []
    with open(entrada, encoding="utf-8") as f:
        for linha in f:
            linha = linha.strip()
            if not linha or linha.startswith("#") or "=" not in linha:
                continue
            chave, valor = linha.split("=", 1)
            itens.append((chave.strip(), valor.strip()))

    with open(saida, "w", encoding="utf-8") as f:
        f.write("// GERADO POR gerar_tabela.py — NÃO EDITE À MÃO.\n")
        f.write(f"// Fonte: {entrada}\n")
        f.write("#pragma once\n")
        f.write("#include <array>\n")
        f.write("#include <string_view>\n\n")
        f.write("namespace tabela_gerada {\n\n")
        f.write("struct Comando { std::string_view nome; std::string_view descricao; };\n\n")
        f.write(f"inline constexpr std::array<Comando, {len(itens)}> comandos{{{{\n")
        for chave, valor in itens:
            f.write(f'    Comando{{"{chave}", "{valor}"}},\n')
        f.write("}};\n\n")
        f.write("} // namespace tabela_gerada\n")

    return 0


if __name__ == "__main__":
    sys.exit(main())
