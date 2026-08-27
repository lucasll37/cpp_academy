#!/usr/bin/env python3
# =============================================================================
#  Converte entre formatos do Word usando o que estiver disponivel na maquina.
#
#  Dois motores, nesta ordem:
#    1. LibreOffice headless (soffice), quando instalado no Linux -- nao depende
#       do Windows e funciona em CI;
#    2. o proprio Microsoft Word, via automacao COM, quando se roda em WSL e o
#       Word esta instalado no lado Windows. E o motor de maior fidelidade,
#       justamente por ser o programa que vai abrir o arquivo final.
#
#  Uso:
#    office_convert.py entrada.docx --to doc --outdir build/doc
# =============================================================================
"""Conversao .docx <-> .doc via LibreOffice ou Microsoft Word."""

import argparse
import glob
import shutil
import subprocess
import sys
from pathlib import Path

# wdSaveFormat: o formato binario 97-2003 e 0; o .docx e 16.
WORD_FORMATS = {"doc": 0, "docx": 16, "pdf": 17}


def to_windows_path(path):
    out = subprocess.run(["wslpath", "-w", str(path)], capture_output=True, text=True)
    return out.stdout.strip() if out.returncode == 0 else None


def convert_with_soffice(src, target, outdir):
    subprocess.run(
        ["soffice", "--headless", "--norestore", "--convert-to", target,
         "--outdir", str(outdir), str(src)],
        check=True, capture_output=True, text=True, timeout=1800)
    return outdir / (src.stem + "." + target)


def convert_with_word(src, target, outdir):
    """Automatiza o Word do Windows a partir do WSL.

    O arquivo de saida precisa ter o caminho montado a partir do diretorio (que
    existe) e nao do arquivo (que ainda nao existe): `wslpath -w` falha em
    caminhos inexistentes e devolveria um caminho relativo invalido para o Word.
    """
    win_src = to_windows_path(src.resolve())
    win_dir = to_windows_path(outdir.resolve())
    if not win_src or not win_dir:
        raise RuntimeError("nao foi possivel converter os caminhos com wslpath")
    dst = outdir / (src.stem + "." + target)
    win_dst = "%s\\%s" % (win_dir.rstrip("\\"), dst.name)

    script = """
$ErrorActionPreference = 'Stop'
$word = New-Object -ComObject Word.Application
$word.Visible = $false
$word.DisplayAlerts = 0
try {
    $doc = $word.Documents.Open('%s', $false, $true)
    # Campos (SUMARIO, LISTA DE ILUSTRACOES, numero de pagina) so ganham
    # conteudo depois de calculados; sem isto o .doc sairia com as listas vazias.
    foreach ($range in $doc.StoryRanges) { $null = $range.Fields.Update() }
    if ($doc.TablesOfContents.Count -gt 0) {
        foreach ($toc in $doc.TablesOfContents) { $toc.Update() }
    }
    $doc.SaveAs([ref]'%s', [ref]%d)
    $doc.Close($false)
} finally {
    $word.Quit()
}
Write-Output 'OK'
""" % (win_src, win_dst, WORD_FORMATS[target])

    try:
        result = subprocess.run(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
            capture_output=True, text=True, timeout=1800)
    except OSError as exc:
        raise RuntimeError(
            "o WSL nao consegue executar binarios do Windows (%s).\n"
            "    reative a ponte com:  sudo sh -c 'echo \":WSLInterop:M::MZ::/init:PF\""
            " > /proc/sys/fs/binfmt_misc/register'\n"
            "    ou use o motor portatil:  make setup-doc" % exc)
    if "OK" not in result.stdout:
        raise RuntimeError("Word falhou:\n%s\n%s" % (result.stdout[-800:], result.stderr[-800:]))
    return dst


def wsl_interop_ok():
    """Diz se o WSL ainda sabe executar binarios do Windows.

    Achar powershell.exe no PATH nao basta: /mnt/c continua montado mesmo
    quando o handler binfmt_misc que faz a ponte com o Windows sumiu (o
    systemd costuma zerar o binfmt_misc no boot). Sem esse handler qualquer
    .exe morre com "Exec format error", entao o motor `word` nao existe de
    fato e anuncia-lo so faz o erro aparecer cinco minutos depois.
    """
    return bool(glob.glob("/proc/sys/fs/binfmt_misc/WSLInterop*"))


def available_engines():
    engines = []
    if shutil.which("soffice"):
        engines.append("soffice")
    if shutil.which("powershell.exe") and shutil.which("wslpath") and wsl_interop_ok():
        engines.append("word")
    return engines


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source")
    ap.add_argument("--to", default="doc", choices=sorted(WORD_FORMATS))
    ap.add_argument("--outdir", default=None)
    ap.add_argument("--engine", default="auto", choices=("auto", "soffice", "word"))
    ap.add_argument("--check", action="store_true", help="so informa os motores disponiveis")
    args = ap.parse_args()

    engines = available_engines()
    if args.check:
        print(", ".join(engines) if engines else "nenhum")
        return 0 if engines else 1

    src = Path(args.source)
    if not src.exists():
        sys.exit("erro: %s nao existe" % src)
    outdir = Path(args.outdir) if args.outdir else src.parent
    outdir.mkdir(parents=True, exist_ok=True)

    order = engines if args.engine == "auto" else [args.engine]
    if not order:
        msg = ["erro: nem LibreOffice nem Word disponiveis -> rode `make setup-doc`"]
        if shutil.which("powershell.exe") and not wsl_interop_ok():
            msg.append("  (o Word ate esta instalado, mas o WSL perdeu o handler"
                       " binfmt_misc que executa .exe -- veja `make doc-check`)")
        sys.exit("\n".join(msg))

    problems = []
    for engine in order:
        try:
            fn = convert_with_soffice if engine == "soffice" else convert_with_word
            dst = fn(src, args.to, outdir)
            if not dst.exists():
                raise RuntimeError("saida nao foi criada")
            print("  %s -> %s (via %s)" % (src.name, dst, engine))
            return 0
        except Exception as exc:  # tenta o proximo motor antes de desistir
            problems.append("%s: %s" % (engine, exc))
    sys.exit("erro: conversao falhou.\n  " + "\n  ".join(problems))


if __name__ == "__main__":
    sys.exit(main())
