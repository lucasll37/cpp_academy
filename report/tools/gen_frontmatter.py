#!/usr/bin/env python3
# =============================================================================
#  Gera os elementos pre e pos-textuais da PTC a partir de ieav-metadata.yaml.
#
#  A saida e Markdown do pandoc porque so ele permite as duas coisas de que
#  esta parte precisa e que o LaTeX do relatorio nao daria: divs com
#  `custom-style`, que viram estilos nomeados do Word, e blocos `{=openxml}`,
#  que permitem inserir campos de verdade (SUMARIO, LISTA DE ILUSTRACOES,
#  numero de pagina) -- campos que o Word recalcula, em vez de texto congelado
#  que envelheceria a cada recompilacao.
#
#  As listas de ilustracoes, tabelas e listagens sao montadas por ESTILO
#  (TOC \t "Image Caption,1"), e nao por campos SEQ. E o que permite que as
#  legendas numeradas pelo filtro Lua alimentem as listas sem nenhum campo
#  espalhado pelo corpo do texto.
# =============================================================================
"""Escreve build/doc/frontmatter.md e build/doc/backmatter.md."""

import argparse
import re
import sys
from pathlib import Path

CM = 567
PAGE_XML = ('<w:pgSz w:w="11906" w:h="16838" w:code="9"/>'
            '<w:pgMar w:top="%d" w:right="%d" w:bottom="%d" w:left="%d" '
            'w:header="850" w:footer="850" w:gutter="0"/>'
            % (3 * CM, 2 * CM, 2 * CM, 3 * CM))

PENDING = "[A PREENCHER: %s]"


def md_escape(text):
    """Neutraliza a pontuacao que o leitor de Markdown interpretaria."""
    return re.sub(r"([\\`*_\[\]#<>])", r"\\\1", str(text))


def value(meta, key, description, pending, upper=False):
    raw = meta.get(key)
    if raw is None or (isinstance(raw, str) and not raw.strip()):
        pending.append(description)
        return PENDING % description  # o marcador nunca vai para caixa alta
    text = str(raw).strip()
    return text.upper() if upper else text


def block(text):
    return "```{=openxml}\n%s\n```\n" % text


def page_break():
    return block('<w:p><w:r><w:br w:type="page"/></w:r></w:p>')


def empty_paragraphs(n):
    return block("<w:p/>" * n)


def section_break():
    """Fecha a secao pre-textual: paginas contadas, porem sem numeracao.

    O numero de pagina vem do cabecalho declarado no reference.docx, que so
    vale para a secao seguinte -- a parte textual. E o que o modelo do IEAv
    pede: "as folhas pre-textuais devem ser contadas, mas nao numeradas".
    """
    return block("<w:p><w:pPr><w:sectPr>%s</w:sectPr></w:pPr></w:p>" % PAGE_XML)


def field(instruction, placeholder):
    """Campo do Word (TOC). `dirty` faz o Word recalcular ao abrir o arquivo."""
    return block(
        '<w:p><w:r><w:fldChar w:fldCharType="begin" w:dirty="true"/></w:r>'
        '<w:r><w:instrText xml:space="preserve"> %s </w:instrText></w:r>'
        '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
        "<w:r><w:t>%s</w:t></w:r>"
        '<w:r><w:fldChar w:fldCharType="end"/></w:r></w:p>' % (instruction, placeholder))


def div(style_name, paragraphs):
    body = "\n\n".join(p for p in paragraphs if p is not None)
    return '::: {custom-style="%s"}\n%s\n:::\n' % (style_name, body)


def heading(title):
    return div("Titulo PreTextual", [md_escape(title)])


TAB = "`<w:r><w:tab/></w:r>`{=openxml}"

UPDATE_HINT = "Selecione tudo e tecle F9 para montar esta lista."


def entry_list(items):
    """Par termo/significado alinhado por tabulacao, como no modelo."""
    return [f"{md_escape(k)}{TAB}{md_escape(v)}" for k, v in items.items()]


def build_front(meta, pending):
    out = []

    # ---------------------------------------------------------------- capa --
    instituicao = meta.get("instituicao") or []
    out.append(div("Capa Instituicao", [md_escape(line) for line in instituicao]))
    out.append(empty_paragraphs(5))

    titulo = value(meta, "titulo", "titulo do trabalho", pending)
    subtitulo = meta.get("subtitulo")
    capa = [
        md_escape(meta.get("publicacao") or "PUBLICAÇÃO TÉCNICO-CIENTÍFICA"),
        md_escape(value(meta, "divisao", "nome da divisao do IEAv", pending, upper=True)),
        md_escape(value(meta, "tipo_publicacao", "tipo de publicacao", pending, upper=True)),
        md_escape(titulo if not subtitulo else "%s: %s" % (titulo, subtitulo)),
        md_escape(value(meta, "ano", "ano da publicacao", pending)),
    ]
    out.append(div("Capa Bloco", capa))

    autores = [a for a in (meta.get("autores") or []) if a]
    if not autores:
        pending.append("autores (nomes completos e por extenso)")
        autores = [PENDING % "autores (nomes completos e por extenso)"]
    out.append(div("Capa Autores", ["Elaborado por:"] + [md_escape(a) for a in autores]))
    out.append(page_break())

    # -------------------------------------------------------------- resumo --
    out.append(heading("RESUMO"))
    out.append(div("Resumo Texto", [
        md_escape(value(meta, "resumo", "resumo, ate 500 palavras (ABNT NBR 6028)", pending)),
        "**Palavras-chave:** " + md_escape(
            value(meta, "palavras_chave", "palavras-chave separadas por ponto e virgula", pending)),
    ]))
    out.append(page_break())

    # ------------------------------------------------------------ abstract --
    out.append(heading("ABSTRACT"))
    out.append(div("Resumo Texto", [
        md_escape(value(meta, "abstract", "abstract (versao do resumo em ingles)", pending)),
        "**Keywords:** " + md_escape(value(meta, "keywords", "keywords em ingles", pending)),
    ]))
    out.append(page_break())

    # --------------------------------------------------- listas opcionais ---
    simbolos = meta.get("simbolos") or {}
    if simbolos:
        out.append(heading("LISTA DE SÍMBOLOS"))
        out.append(div("Lista Simbolos", entry_list(simbolos)))
        out.append(page_break())

    abreviaturas = meta.get("abreviaturas") or {}
    if abreviaturas:
        out.append(heading("LISTA DE ABREVIATURAS E SIGLAS"))
        out.append(div("Lista Simbolos", entry_list(dict(sorted(abreviaturas.items())))))
        out.append(page_break())

    # As tres listas abaixo sao montadas pelo Word a partir dos estilos de
    # legenda aplicados no corpo pelo filtro tools/abnt.lua.
    out.append(heading("LISTA DE ILUSTRAÇÕES"))
    out.append(field(r'TOC \h \z \t "Image Caption,1"', UPDATE_HINT))
    out.append(page_break())

    out.append(heading("LISTA DE TABELAS"))
    out.append(field(r'TOC \h \z \t "Table Caption,1"', UPDATE_HINT))
    out.append(page_break())

    out.append(heading("LISTA DE LISTAGENS"))
    out.append(field(r'TOC \h \z \t "Legenda Codigo,1"', UPDATE_HINT))
    out.append(page_break())

    # -------------------------------------------------------------- sumario --
    niveis = meta.get("sumario_niveis") or 3
    out.append(heading("SUMÁRIO"))
    out.append(field(r'TOC \o "1-%d" \h \z \u' % niveis, UPDATE_HINT))
    out.append(section_break())

    return "\n".join(out)


def build_back(meta, bib_path, master_path, pending):
    out = []

    # "Conclusoes ou comentarios finais" e elemento obrigatorio do modelo. Se
    # nao houver um capitulo correspondente entre os \include de report.tex,
    # entra aqui um marcador -- como secao primaria numerada, que e o lugar
    # dela na estrutura, para que a falta apareca tambem no sumario.
    if master_path.exists():
        master = master_path.read_text(encoding="utf-8")
        chapters = re.findall(r"^\s*\\include\{chapters/([^}]+)\}", master, re.M)
        if not any(re.search(r"conclus|coment", name, re.I) for name in chapters):
            pending.append("capitulo de conclusoes (nenhum \\include correspondente "
                           "em report.tex)")
            out.append("# CONCLUSÕES\n")
            out.append(div("Resumo Texto", [md_escape(
                PENDING % "conclusoes ou comentarios finais (elemento obrigatorio)")]))

    out.append(div("Titulo PosTextual", ["REFERÊNCIAS"]))
    entries = 0
    if bib_path.exists():
        entries = len(re.findall(r"^\s*@\w+\{", bib_path.read_text(encoding="utf-8"), re.M))
    if entries == 0:
        pending.append("referencias (nenhuma entrada em references.bib)")
        out.append(div("Resumo Texto", [md_escape(
            PENDING % "referencias conforme a ABNT NBR 6023")]))
    else:
        pending.append(
            "referencias: %d entradas em references.bib, nenhuma citada no texto" % entries)
        out.append(div("Resumo Texto", [md_escape(
            PENDING % ("referencias: %d entradas em references.bib ainda sem \\cite no texto"
                       % entries))]))
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--metadata", default="tools/ieav-metadata.yaml")
    ap.add_argument("--bib", default="references.bib")
    ap.add_argument("--master", default="report.tex",
                    help="arquivo mestre, consultado para saber quais capitulos entram")
    ap.add_argument("--out", default="build/doc")
    args = ap.parse_args()

    try:
        import yaml
    except ImportError:
        sys.exit("erro: falta o modulo pyyaml -> sudo apt-get install python3-yaml")

    meta = yaml.safe_load(Path(args.metadata).read_text(encoding="utf-8")) or {}
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    pending = []
    (out / "frontmatter.md").write_text(build_front(meta, pending), encoding="utf-8")
    (out / "backmatter.md").write_text(
        build_back(meta, Path(args.bib), Path(args.master), pending), encoding="utf-8")
    (out / "pendencias.txt").write_text("\n".join(pending) + "\n", encoding="utf-8")

    if pending:
        print("  %d pendencia(s) marcadas como [A PREENCHER]:" % len(pending))
        for item in pending:
            print("    - %s" % item)
    else:
        print("  nenhuma pendencia nos metadados")


if __name__ == "__main__":
    main()
