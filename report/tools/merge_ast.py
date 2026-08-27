#!/usr/bin/env python3
# =============================================================================
#  Concatena documentos pandoc no nivel da arvore sintatica (JSON).
#
#  A parte pre-textual e escrita em Markdown (para poder usar campos do Word) e
#  o corpo vem do LaTeX. Como o pandoc so aceita um formato de entrada por
#  execucao, os dois sao lidos separadamente e unidos aqui. A juncao e no AST,
#  entao nada se perde no caminho -- diferente de converter um dos lados para
#  um formato intermediario.
# =============================================================================
"""Une varios .json do pandoc em um unico documento."""

import argparse
import json
import sys


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("inputs", nargs="+", help="arquivos .json do pandoc, na ordem final")
    ap.add_argument("-o", "--output", required=True)
    args = ap.parse_args()

    merged, blocks = None, []
    for path in args.inputs:
        with open(path, encoding="utf-8") as fh:
            doc = json.load(fh)
        if merged is None:
            merged = doc
        elif doc["pandoc-api-version"] != merged["pandoc-api-version"]:
            sys.exit("erro: %s foi gerado por outra versao do pandoc" % path)
        blocks.extend(doc["blocks"])

    merged["blocks"] = blocks
    with open(args.output, "w", encoding="utf-8") as fh:
        json.dump(merged, fh)
    print("  %d blocos de %d documentos -> %s" % (len(blocks), len(args.inputs), args.output))


if __name__ == "__main__":
    main()
