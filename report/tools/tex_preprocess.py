#!/usr/bin/env python3
# =============================================================================
#  Prepara o corpo LaTeX para o pandoc.
#
#  Nada em chapters/ e alterado: os capitulos sao lidos, concatenados na ordem
#  dos \include de report.tex e gravados em build/doc/body.tex.
#
#  A unica transformacao com consequencia editorial e a resolucao das
#  referencias cruzadas. Em vez de renumerar por conta propria, este script le
#  os numeros que o proprio LaTeX ja calculou nos arquivos .aux -- por isso
#  `make doc` depende de `make pdf`. Assim o .doc e o PDF citam sempre os
#  mesmos numeros.
#
#  Uso: tex_preprocess.py --root . --build build --out build/doc
# =============================================================================
"""Concatena os capitulos e resolve \\ref a partir dos .aux do LaTeX."""

import argparse
import json
import re
import sys
from pathlib import Path

# Ancoras que o hyperref grava no .aux para secoes. O numero impresso no PDF e
# romano (\thesection foi redefinido no preambulo), mas a ancora carrega a
# numeracao arabica -- que e a exigida pela NBR 6024 e a que o Word vai gerar.
# Ex.: subsubsection.4.2.1 -> "4.2.1"
SECTION_ANCHORS = ("section", "subsection", "subsubsection", "paragraph")

# Linguagens sem definicao de sintaxe no realce do pandoc (skylighting). Manter
# a classe so produziria um aviso por bloco e nenhum realce; o bloco continua
# saindo monoespacado e com moldura.
UNSUPPORTED_LANGUAGES = ("EDL", "protobuf")


def split_groups(text):
    """Divide "{a}{b}{c}" nos grupos de chaves de primeiro nivel."""
    groups, depth, cur = [], 0, []
    for ch in text:
        if ch == "{":
            depth += 1
            if depth == 1:
                cur = []
                continue
        elif ch == "}":
            depth -= 1
            if depth == 0:
                groups.append("".join(cur))
                continue
            if depth < 0:
                break
        if depth >= 1:
            cur.append(ch)
    return groups


def read_labels(build_dir):
    """Mapa rotulo -> numero, lido de todos os .aux da compilacao."""
    labels = {}
    aux_files = sorted(build_dir.rglob("*.aux"))
    for aux in aux_files:
        for line in aux.read_text(encoding="utf-8", errors="replace").splitlines():
            m = re.match(r"\\newlabel\{([^}]*)\}\{", line)
            if not m:
                continue
            key = m.group(1)
            if key.startswith("sub@"):
                continue  # entrada auxiliar do subcaption; o rotulo real ja veio
            # m.end() cai logo apos a chave que abre o grupo externo, entao
            # split_groups ja devolve os campos internos do \newlabel.
            fields = split_groups(line[m.end():])
            if len(fields) < 4:
                continue
            printed, anchor = fields[0], fields[3]
            kind = anchor.split(".")[0]
            if kind in SECTION_ANCHORS:
                # ancora arabica: subsubsection.4.2.1 -> 4.2.1
                labels[key] = anchor.split(".", 1)[1]
            else:
                labels[key] = printed
    return labels, aux_files


def split_verbatim(text):
    """Fatia o texto em trechos (is_code, conteudo).

    As substituicoes so podem tocar o texto normal: dentro de um lstlisting
    tudo e literal, e um \\ref ali dentro seria codigo-fonte de verdade.
    """
    parts, pos = [], 0
    pattern = re.compile(r"\\begin\{lstlisting\}.*?\\end\{lstlisting\}", re.S)
    for m in pattern.finditer(text):
        if m.start() > pos:
            parts.append((False, text[pos:m.start()]))
        parts.append((True, m.group()))
        pos = m.end()
    parts.append((False, text[pos:]))
    return parts


def resolve_refs(text, labels, missing):
    def repl(m):
        key = m.group(1)
        if key in labels:
            return labels[key]
        missing.add(key)
        return "??"

    return re.sub(r"\\ref\{([^}]*)\}", repl, text)


def strip_equation_labels(text, labels, eqnums, missing):
    """Tira o \\label de dentro dos ambientes equation.

    O pandoc entrega o ambiente inteiro ao texmath, e um \\label solto vira
    lixo na formula. O numero correspondente vai para eqnums.json, na ordem do
    documento, e o filtro Lua o reinsere alinhado a direita, como pede o
    modelo do IEAv.
    """
    def repl(m):
        body = m.group(1)
        found = re.search(r"\\label\{([^}]*)\}", body)
        if found:
            key = found.group(1)
            eqnums.append(labels.get(key, "?"))
            if key not in labels:
                missing.add(key)
            body = body.replace(found.group(0), "")
        else:
            eqnums.append("")
        return "\\begin{equation}%s\\end{equation}" % body

    return re.sub(r"\\begin\{equation\}(.*?)\\end\{equation\}", repl, text, flags=re.S)


def clean(text):
    """Remove o que o pandoc nao aproveita e normaliza as listagens."""
    # \captionsetup e do pacote caption e so afeta a diagramacao do PDF.
    text = re.sub(r"\\captionsetup\{[^}]*\}\s*", "", text)
    for lang in UNSUPPORTED_LANGUAGES:
        text = re.sub(r"(,\s*)?language=%s\b" % re.escape(lang), "", text)
    return text


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".", help="raiz do projeto")
    ap.add_argument("--build", default="build", help="diretorio de compilacao do LaTeX")
    ap.add_argument("--out", default="build/doc", help="diretorio de saida")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    build = Path(args.build)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    labels, aux_files = read_labels(build)
    if not aux_files:
        sys.exit("erro: nenhum .aux em %s -- rode `make` antes de `make doc`" % build)
    print("  %d rotulos lidos de %d arquivos .aux" % (len(labels), len(aux_files)))

    master = (root / "report.tex").read_text(encoding="utf-8")
    chapters = re.findall(r"^\s*\\include\{chapters/([^}]+)\}", master, re.M)
    if not chapters:
        sys.exit("erro: nenhum \\include{chapters/...} em report.tex")

    missing, eqnums, pieces = set(), [], []
    for name in chapters:
        src = (root / "chapters" / (name + ".tex")).read_text(encoding="utf-8")
        rebuilt = []
        for is_code, chunk in split_verbatim(src):
            if is_code:
                rebuilt.append(clean(chunk))
            else:
                chunk = resolve_refs(chunk, labels, missing)
                chunk = strip_equation_labels(chunk, labels, eqnums, missing)
                rebuilt.append(clean(chunk))
        pieces.append("".join(rebuilt))

    (out / "body.tex").write_text("\n\n".join(pieces), encoding="utf-8")
    (out / "eqnums.json").write_text(json.dumps(eqnums), encoding="utf-8")

    print("  %d capitulos, %d equacoes numeradas" % (len(chapters), len(eqnums)))
    if missing:
        print("  AVISO: %d rotulos sem numero no .aux (viraram '??'): %s"
              % (len(missing), ", ".join(sorted(missing)[:5])))
    print("  -> %s" % (out / "body.tex"))


if __name__ == "__main__":
    main()
