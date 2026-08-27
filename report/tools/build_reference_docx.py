#!/usr/bin/env python3
# =============================================================================
#  Gera o build/doc/reference.docx usado pelo pandoc.
#
#  Por que nao usar o proprio templates/*.doc como reference-doc: o pandoc casa
#  estilos pelo NOME em ingles ("Heading 1", "Source Code", "Image Caption").
#  Um arquivo salvo pelo Word em portugues traz "Titulo 1", "Corpo de texto" --
#  o pandoc nao os encontraria e cairia no visual padrao. Entao partimos do
#  reference.docx padrao do pandoc, que ja tem os nomes certos, e reescrevemos
#  a formatacao de cada estilo com os valores extraidos do modelo do IEAv.
#
#  Valores lidos de templates/PTC-IEAv-Modelo_estrutural_de_publicao.doc e da
#  secao FORMATACAO desse mesmo modelo:
#    * A4, margens 3 cm (esquerda/superior) e 2 cm (direita/inferior);
#    * fonte Arial, corpo 12, preto; corpo menor nas legendas;
#    * 6 pontos entre paragrafos, entrelinha simples;
#    * numeracao progressiva arabica das secoes (ABNT NBR 6024);
#    * numero de pagina no canto superior direito, so na parte textual.
# =============================================================================
"""Constroi o reference.docx com a formatacao do modelo PTC do IEAv."""

import argparse
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

CM = 567  # twips por centimetro

PAGE = dict(w=11906, h=16838, top=3 * CM, bottom=2 * CM, left=3 * CM, right=2 * CM,
            header=int(1.5 * CM), footer=int(1.5 * CM))
TEXT_WIDTH = PAGE["w"] - PAGE["left"] - PAGE["right"]  # tabulacao da margem direita

NUM_ID = 900  # numeracao multinivel das secoes; alta para nao colidir com a do pandoc

W = 'xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"'


def font(name, size_pt, *, bold=False, italic=False, caps=False, color="000000"):
    half = int(size_pt * 2)
    parts = ['<w:rFonts w:ascii="%s" w:hAnsi="%s" w:cs="%s"/>' % (name, name, name)]
    if bold:
        parts.append("<w:b/>")
    if italic:
        parts.append("<w:i/>")
    if caps:
        parts.append("<w:caps/>")
    parts.append('<w:color w:val="%s"/>' % color)
    parts.append('<w:sz w:val="%d"/><w:szCs w:val="%d"/>' % (half, half))
    parts.append('<w:lang w:val="pt-BR"/>')
    return "".join(parts)


def spacing(before=0, after=120, line=240):
    return '<w:spacing w:before="%d" w:after="%d" w:line="%d" w:lineRule="auto"/>' % (
        before, after, line)


def style(style_id, name, ppr, rpr, *, based="Normal", kind="paragraph", nxt=None):
    based_xml = '<w:basedOn w:val="%s"/>' % based if based else ""
    next_xml = '<w:next w:val="%s"/>' % (nxt or "Normal") if kind == "paragraph" else ""
    return (
        '<w:style w:type="%s" w:customStyle="0" w:styleId="%s">'
        '<w:name w:val="%s"/>%s%s<w:qFormat/>'
        "<w:pPr>%s</w:pPr><w:rPr>%s</w:rPr></w:style>"
        % (kind, style_id, name, based_xml, next_xml, ppr, rpr)
    )


def build_styles(cfg):
    """Devolve (substituicoes, novos_estilos) para o styles.xml."""
    body_font, body, small, code = cfg["fonte"], cfg["corpo_pt"], cfg["corpo_menor_pt"], cfg["corpo_codigo_pt"]
    mono = "Consolas"

    just = '<w:jc w:val="both"/>'
    center = '<w:jc w:val="center"/>'
    left = '<w:jc w:val="left"/>'

    def numpr(level):
        return '<w:numPr><w:ilvl w:val="%d"/><w:numId w:val="%d"/></w:numPr>' % (level, NUM_ID)

    replace = {
        "Normal": style("Normal", "Normal", spacing() + just, font(body_font, body), based=None),
        "BodyText": style("BodyText", "Body Text", spacing() + just, font(body_font, body)),
        "FirstParagraph": style("FirstParagraph", "First Paragraph", spacing() + just,
                                font(body_font, body), based="BodyText"),
        "Compact": style("Compact", "Compact", spacing(after=60) + just, font(body_font, body)),
        # Secoes primarias abrem pagina, como pede o modelo para os capitulos.
        "Heading1": style("Heading1", "heading 1",
                          '<w:keepNext/><w:pageBreakBefore/>' + numpr(0)
                          + spacing(before=0, after=240) + left + '<w:outlineLvl w:val="0"/>',
                          font(body_font, body, bold=True, caps=True)),
        "Heading2": style("Heading2", "heading 2",
                          "<w:keepNext/>" + numpr(1) + spacing(before=240, after=120) + left
                          + '<w:outlineLvl w:val="1"/>',
                          font(body_font, body, bold=True)),
        "Heading3": style("Heading3", "heading 3",
                          "<w:keepNext/>" + numpr(2) + spacing(before=240, after=120) + left
                          + '<w:outlineLvl w:val="2"/>',
                          font(body_font, body, bold=True, italic=True)),
        "Heading4": style("Heading4", "heading 4",
                          "<w:keepNext/>" + numpr(3) + spacing(before=180, after=120) + left
                          + '<w:outlineLvl w:val="3"/>',
                          font(body_font, body, italic=True)),
        # Legendas: corpo menor, conforme a secao FORMATACAO do modelo.
        "ImageCaption": style("ImageCaption", "Image Caption",
                              spacing(before=60, after=240) + center, font(body_font, small)),
        "TableCaption": style("TableCaption", "Table Caption",
                              "<w:keepNext/>" + spacing(before=240, after=60) + left,
                              font(body_font, small)),
        "Caption": style("Caption", "Caption", spacing(before=60, after=240) + center,
                         font(body_font, small)),
        "Figure": style("Figure", "Figure", spacing(before=120, after=0) + center,
                        font(body_font, body)),
        "CaptionedFigure": style("CaptionedFigure", "Captioned Figure",
                                 spacing(before=120, after=0) + center, font(body_font, body)),
        # Listagens: equivalente Word do frame=single do lstlisting.
        "SourceCode": style(
            "SourceCode", "Source Code",
            '<w:pBdr><w:top w:val="single" w:sz="4" w:space="4" w:color="999999"/>'
            '<w:left w:val="single" w:sz="4" w:space="4" w:color="999999"/>'
            '<w:bottom w:val="single" w:sz="4" w:space="4" w:color="999999"/>'
            '<w:right w:val="single" w:sz="4" w:space="4" w:color="999999"/></w:pBdr>'
            '<w:shd w:val="clear" w:fill="F7F7F7"/>'
            '<w:ind w:left="170" w:right="170"/>'
            + spacing(before=0, after=0) + left,
            font(mono, code)),
        "VerbatimChar": ('<w:style w:type="character" w:styleId="VerbatimChar">'
                         '<w:name w:val="Verbatim Char"/><w:rPr>%s</w:rPr></w:style>'
                         % font(mono, body - 1)),
        "Bibliography": style("Bibliography", "Bibliography",
                              spacing(after=120) + left, font(body_font, body)),
        "TOCHeading": style("TOCHeading", "TOC Heading",
                            spacing(after=240) + center, font(body_font, body, bold=True, caps=True)),
    }

    extra = [
        # --- capa -----------------------------------------------------------
        style("CapaInstituicao", "Capa Instituicao", spacing(after=0) + center,
              font(body_font, body, bold=True)),
        style("CapaBloco", "Capa Bloco", spacing(after=240) + center, font(body_font, body)),
        style("CapaAutores", "Capa Autores", spacing(after=0) + center,
              font(body_font, 9)),
        # --- elementos pre-textuais (fora do sumario: sem outlineLvl) --------
        style("TituloPreTextual", "Titulo PreTextual",
              "<w:keepNext/>" + spacing(before=0, after=240) + center,
              font(body_font, body, bold=True, caps=True)),
        # --- elementos pos-textuais (no sumario, mas sem numeracao) ----------
        style("TituloPosTextual", "Titulo PosTextual",
              '<w:keepNext/><w:pageBreakBefore/>' + spacing(before=0, after=240) + left
              + '<w:outlineLvl w:val="0"/>',
              font(body_font, body, bold=True, caps=True)),
        style("ResumoTexto", "Resumo Texto", spacing(after=120) + just, font(body_font, body)),
        # Simbolo/sigla a esquerda, significado alinhado em coluna.
        style("ListaSimbolos", "Lista Simbolos",
              '<w:tabs><w:tab w:val="left" w:pos="2268"/></w:tabs>'
              '<w:ind w:left="2268" w:hanging="2268"/>' + spacing(after=60) + left,
              font(body_font, body)),
        style("LegendaCodigo", "Legenda Codigo",
              spacing(before=60, after=240) + left, font(body_font, small)),
        # Equacao destacada: tabulacao central para a formula, tabulacao a
        # direita para o numero entre parenteses.
        style("Equacao", "Equacao",
              '<w:tabs><w:tab w:val="center" w:pos="%d"/>'
              '<w:tab w:val="right" w:pos="%d"/></w:tabs>' % (TEXT_WIDTH // 2, TEXT_WIDTH)
              + spacing(before=120, after=120) + left,
              font(body_font, body)),
    ]
    return replace, extra


def patch_styles(xml, cfg):
    replace, extra = build_styles(cfg)
    for style_id, new in replace.items():
        pattern = re.compile(
            r'<w:style [^>]*w:styleId="%s"[^>]*>.*?</w:style>' % re.escape(style_id), re.S)
        xml, n = pattern.subn(lambda _m, v=new: v, xml, count=1)
        if n == 0:
            extra.append(new)

    defaults = (
        "<w:docDefaults><w:rPrDefault><w:rPr>%s</w:rPr></w:rPrDefault>"
        "<w:pPrDefault><w:pPr>%s</w:pPr></w:pPrDefault></w:docDefaults>"
        % (font(cfg["fonte"], cfg["corpo_pt"]), spacing())
    )
    xml = re.sub(r"<w:docDefaults>.*?</w:docDefaults>", lambda _m: defaults, xml, count=1, flags=re.S)
    return xml.replace("</w:styles>", "".join(extra) + "</w:styles>")


def patch_numbering(xml):
    """Acrescenta a numeracao progressiva 1 / 1.1 / 1.1.1 ligada aos Titulos."""
    levels = []
    for i in range(4):
        text = ".".join("%%%d" % (j + 1) for j in range(i + 1))
        levels.append(
            '<w:lvl w:ilvl="%d"><w:start w:val="1"/><w:numFmt w:val="decimal"/>'
            '<w:pStyle w:val="Heading%d"/><w:suff w:val="space"/>'
            '<w:lvlText w:val="%s"/><w:lvlJc w:val="left"/>'
            '<w:pPr><w:ind w:left="0" w:firstLine="0"/></w:pPr></w:lvl>'
            % (i, i + 1, text))
    block = ('<w:abstractNum w:abstractNumId="%d"><w:multiLevelType w:val="multilevel"/>%s'
             "</w:abstractNum>" % (NUM_ID, "".join(levels)))
    num = '<w:num w:numId="%d"><w:abstractNumId w:val="%d"/></w:num>' % (NUM_ID, NUM_ID)
    # <w:num> deve vir depois de todos os <w:abstractNum>, conforme o schema.
    xml = xml.replace("</w:numbering>", block + num + "</w:numbering>")
    return xml


def sect_pr(header_rel):
    return (
        "<w:sectPr>"
        '<w:headerReference w:type="default" r:id="%s"/>'
        '<w:pgSz w:w="%d" w:h="%d" w:code="9"/>'
        '<w:pgMar w:top="%d" w:right="%d" w:bottom="%d" w:left="%d" '
        'w:header="%d" w:footer="%d" w:gutter="0"/>'
        '<w:cols w:space="708"/><w:docGrid w:linePitch="360"/>'
        "</w:sectPr>" % (header_rel, PAGE["w"], PAGE["h"], PAGE["top"], PAGE["right"],
                         PAGE["bottom"], PAGE["left"], PAGE["header"], PAGE["footer"])
    )


def header_xml(cfg):
    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<w:hdr %s xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'
        '<w:p><w:pPr><w:jc w:val="right"/><w:rPr>%s</w:rPr></w:pPr>'
        '<w:r><w:rPr>%s</w:rPr><w:fldChar w:fldCharType="begin"/></w:r>'
        '<w:r><w:rPr>%s</w:rPr><w:instrText xml:space="preserve"> PAGE </w:instrText></w:r>'
        '<w:r><w:rPr>%s</w:rPr><w:fldChar w:fldCharType="separate"/></w:r>'
        '<w:r><w:rPr>%s</w:rPr><w:t>1</w:t></w:r>'
        '<w:r><w:rPr>%s</w:rPr><w:fldChar w:fldCharType="end"/></w:r>'
        "</w:p></w:hdr>" % ((W,) + (font(cfg["fonte"], cfg["corpo_menor_pt"]),) * 6)
    )


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--pandoc", default="pandoc")
    ap.add_argument("--metadata", default="tools/ieav-metadata.yaml")
    ap.add_argument("--out", default="build/doc/reference.docx")
    args = ap.parse_args()

    try:
        import yaml
    except ImportError:
        sys.exit("erro: falta o modulo pyyaml -> sudo apt-get install python3-yaml")

    meta = yaml.safe_load(Path(args.metadata).read_text(encoding="utf-8")) or {}
    cfg = {
        "fonte": meta.get("fonte") or "Arial",
        "corpo_pt": meta.get("corpo_pt") or 12,
        "corpo_menor_pt": meta.get("corpo_menor_pt") or 10,
        "corpo_codigo_pt": meta.get("corpo_codigo_pt") or 9,
    }

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    base = out.parent / "_pandoc-default.docx"
    with base.open("wb") as fh:
        subprocess.run([args.pandoc, "--print-default-data-file", "reference.docx"],
                       stdout=fh, check=True)

    header_rel = "rId900"
    with zipfile.ZipFile(base) as src, zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as dst:
        for item in src.infolist():
            data = src.read(item.filename)
            if item.filename == "word/styles.xml":
                data = patch_styles(data.decode("utf-8"), cfg).encode("utf-8")
            elif item.filename == "word/numbering.xml":
                data = patch_numbering(data.decode("utf-8")).encode("utf-8")
            elif item.filename == "word/document.xml":
                xml = data.decode("utf-8")
                xml = re.sub(r"<w:sectPr>.*?</w:sectPr>", lambda _m: sect_pr(header_rel),
                             xml, count=1, flags=re.S)
                data = xml.encode("utf-8")
            elif item.filename == "word/_rels/document.xml.rels":
                xml = data.decode("utf-8")
                rel = ('<Relationship Type="http://schemas.openxmlformats.org/'
                       'officeDocument/2006/relationships/header" Id="%s" '
                       'Target="header1.xml"/>' % header_rel)
                data = xml.replace("</Relationships>", rel + "</Relationships>").encode("utf-8")
            elif item.filename == "[Content_Types].xml":
                xml = data.decode("utf-8")
                override = ('<Override PartName="/word/header1.xml" ContentType='
                            '"application/vnd.openxmlformats-officedocument.'
                            'wordprocessingml.header+xml"/>')
                data = xml.replace("</Types>", override + "</Types>").encode("utf-8")
            dst.writestr(item, data)
        dst.writestr("word/header1.xml", header_xml(cfg))

    base.unlink()
    print("  -> %s (%s %dpt, A4, margens 3-2-3-2)" % (out, cfg["fonte"], cfg["corpo_pt"]))


if __name__ == "__main__":
    main()
