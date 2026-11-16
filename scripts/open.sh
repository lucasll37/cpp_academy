#!/usr/bin/env bash
# =============================================================================
#  scripts/open.sh — abre um arquivo (ou URL) no navegador padrão, em qualquer
#  ambiente. É o que está por trás do `make open PROJ=<subprojeto>`.
#
#  Uso:
#    scripts/open.sh <arquivo.html | http(s)://…>
#    scripts/open.sh --dry-run <alvo>      # só mostra o que faria
#
#  Ambientes:
#    WSL 1/2          → navegador padrão do WINDOWS: wslview, explorer.exe,
#                       cmd.exe /c start, powershell.exe Start-Process
#    Windows          → Git Bash / MSYS2 / Cygwin: explorer.exe, cmd /c start
#    macOS            → open
#    Linux / BSD      → $BROWSER, xdg-open, gio open, sensible-browser,
#                       x-www-browser, firefox, chromium, google-chrome
#    sem tela         → não há navegador para abrir: imprime o caminho
#
#  Variáveis:
#    NAVEGADOR=<cmd>        força um comando (ex.: NAVEGADOR=firefox make open)
#    OPEN_AMBIENTE=<amb>    força o ambiente: wsl | windows | macos | linux
#                           (útil para testar a detecção com --dry-run)
#
#  ⚠ Duas armadilhas que este script evita:
#    1. `command -v open` NÃO detecta macOS: no Debian/Ubuntu, /usr/bin/open é
#       o `openvt`, que abre um terminal virtual. O macOS é detectado por uname.
#    2. No WSL, o caminho Linux não significa nada para o Windows: ele precisa
#       virar \\wsl.localhost\<distro>\… (wslpath -w) antes de ir ao explorer.
#       E o explorer.exe devolve código 1 mesmo quando abre com sucesso.
# =============================================================================
set -uo pipefail

seco=0
if [[ "${1:-}" == "--dry-run" || "${1:-}" == "-n" ]]; then seco=1; shift; fi
alvo="${1:-}"
if [[ -z "$alvo" ]]; then
  echo "uso: $0 [--dry-run] <arquivo.html | URL>" >&2
  exit 2
fi

# ── O alvo: arquivo local (vira caminho absoluto) ou URL ────────────────────
eh_url=0
if [[ "$alvo" =~ ^(https?|file):// ]]; then
  eh_url=1
  absoluto="$alvo"
else
  if [[ ! -e "$alvo" ]]; then
    echo "open.sh: não existe: $alvo" >&2
    exit 1
  fi
  if command -v realpath >/dev/null 2>&1; then
    absoluto="$(realpath "$alvo")"
  else
    absoluto="$(cd "$(dirname "$alvo")" && pwd)/$(basename "$alvo")"
  fi
fi

# ── Qual ambiente? ───────────────────────────────────────────────────────────
detectar() {
  case "$(uname -s 2>/dev/null)" in
    Darwin)               echo macos ;;
    MINGW*|MSYS*|CYGWIN*) echo windows ;;
    Linux)
      if [[ -n "${WSL_DISTRO_NAME:-}" || -n "${WSL_INTEROP:-}" ]] ||
         grep -qi microsoft /proc/sys/kernel/osrelease 2>/dev/null; then
        echo wsl
      else
        echo linux
      fi ;;
    *) echo linux ;;      # FreeBSD, OpenBSD…: o caminho do xdg-open serve
  esac
}
ambiente="${OPEN_AMBIENTE:-$(detectar)}"

# ── Execução (ou só exibição, com --dry-run) ─────────────────────────────────
mostrar() { printf '  %q' "$@"; printf '\n'; }

# Roda o comando; devolve o código dele. `ignora_codigo` = o comando devolve
# código ≠ 0 mesmo quando funciona (caso do explorer.exe).
abrir_com() {
  local rotulo="$1"; shift
  if (( seco )); then
    echo "[$ambiente] usaria $rotulo:"; mostrar "$@"
    return 0
  fi
  echo "abrindo no navegador padrão [$ambiente, via $rotulo]: $alvo"
  "$@"
}

# Para programas que podem bloquear o terminal até o navegador fechar
# (navegadores chamados diretamente, $BROWSER): solta o processo.
abrir_solto() {
  local rotulo="$1"; shift
  if (( seco )); then
    echo "[$ambiente] usaria $rotulo (em segundo plano):"; mostrar "$@"
    return 0
  fi
  echo "abrindo no navegador padrão [$ambiente, via $rotulo]: $alvo"
  if command -v setsid >/dev/null 2>&1; then
    setsid "$@" >/dev/null 2>&1 < /dev/null &
  else
    nohup "$@" >/dev/null 2>&1 < /dev/null &
  fi
  disown 2>/dev/null || true
  return 0
}

tem() { command -v "$1" >/dev/null 2>&1; }

sem_navegador() {
  echo "open.sh: não encontrei como abrir um navegador neste ambiente [$ambiente]." >&2
  if (( eh_url )); then
    echo "  Abra à mão: $absoluto" >&2
  else
    echo "  Abra à mão: file://$absoluto" >&2
  fi
  echo "  (ou force um comando: NAVEGADOR=firefox make open …)" >&2
  exit 1
}

# 0. Pedido explícito
if [[ -n "${NAVEGADOR:-}" ]]; then
  caminho="$absoluto"
  # um .exe do Windows chamado de dentro do WSL precisa do caminho do Windows
  if [[ "$ambiente" == wsl && "$NAVEGADOR" == *.exe && $eh_url -eq 0 ]] && tem wslpath; then
    caminho="$(wslpath -w "$absoluto")"
  fi
  # shellcheck disable=SC2086  # NAVEGADOR pode trazer argumentos
  abrir_solto "NAVEGADOR=$NAVEGADOR" $NAVEGADOR "$caminho"
  exit 0
fi

case "$ambiente" in
  wsl)
    # Caminho que o Windows entende: \\wsl.localhost\<distro>\…  (ou C:\… em /mnt/c)
    if (( eh_url )); then
      janela="$absoluto"
    elif tem wslpath; then
      janela="$(wslpath -w "$absoluto")"
    else
      janela=""
    fi
    if tem wslview; then
      abrir_com "wslview" wslview "$absoluto" && exit 0
    fi
    if [[ -n "$janela" ]] && tem explorer.exe; then
      abrir_com "explorer.exe" explorer.exe "$janela"
      exit 0                       # o explorer.exe devolve 1 mesmo quando abre
    fi
    if [[ -n "$janela" ]] && tem cmd.exe; then
      # cd /mnt/c: o cmd.exe reclama quando o diretório atual é um caminho UNC
      ( cd /mnt/c 2>/dev/null || true; abrir_com "cmd.exe /c start" cmd.exe /c start "" "$janela" ) && exit 0
    fi
    if [[ -n "$janela" ]] && tem powershell.exe; then
      abrir_com "powershell.exe Start-Process" powershell.exe -NoProfile -NonInteractive \
        -Command "Start-Process -FilePath '${janela//\'/\'\'}'" && exit 0
    fi
    # interop do Windows desligado: tenta um navegador Linux (WSLg)
    if [[ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]] && tem xdg-open; then
      abrir_solto "xdg-open (WSLg)" xdg-open "$absoluto"
      exit 0
    fi
    sem_navegador
    ;;

  windows)
    if (( eh_url )); then janela="$absoluto"; else janela="$(cygpath -w "$absoluto" 2>/dev/null || echo "$absoluto")"; fi
    if tem explorer.exe || tem explorer; then
      abrir_com "explorer.exe" explorer.exe "$janela"
      exit 0                       # código 1 mesmo com sucesso
    fi
    # MSYS converteria "/c" num caminho; "//c" chega ao cmd como "/c"
    case "$(uname -s)" in MINGW*|MSYS*) flag="//c" ;; *) flag="/c" ;; esac
    if tem cmd.exe || tem cmd; then
      abrir_com "cmd.exe start" cmd.exe "$flag" start "" "$janela" && exit 0
    fi
    sem_navegador
    ;;

  macos)
    abrir_com "open" open "$absoluto" && exit 0
    sem_navegador
    ;;

  linux|*)
    if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
      echo "open.sh: sem ambiente gráfico (DISPLAY/WAYLAND_DISPLAY vazios) — não há navegador para abrir." >&2
      sem_navegador
    fi
    if [[ -n "${BROWSER:-}" ]]; then
      # convenção $BROWSER: lista separada por ':'; '%s' é trocado pelo alvo
      IFS=':' read -r -a candidatos <<< "$BROWSER"
      for cmd in "${candidatos[@]}"; do
        [[ -z "$cmd" ]] && continue
        read -r -a partes <<< "$cmd"
        if [[ "$cmd" == *%s* ]]; then
          for k in "${!partes[@]}"; do partes[k]="${partes[k]//%s/$absoluto}"; done
        else
          partes+=("$absoluto")
        fi
        abrir_solto "\$BROWSER" "${partes[@]}"
        exit 0
      done
    fi
    if tem xdg-open; then abrir_solto "xdg-open" xdg-open "$absoluto"; exit 0; fi
    if tem gio; then abrir_solto "gio open" gio open "$absoluto"; exit 0; fi
    for c in sensible-browser x-www-browser firefox chromium chromium-browser google-chrome; do
      if tem "$c"; then abrir_solto "$c" "$c" "$absoluto"; exit 0; fi
    done
    sem_navegador
    ;;
esac
