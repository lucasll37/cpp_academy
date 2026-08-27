# C++ Academy — Um Curso Completo de C++ Moderno

Relatório em LaTeX que reúne, numa ordem única, a teoria que os subprojetos de
[`../src/`](../src/) desenvolvem em separado. O conteúdo é derivado dos
`README.md` de cada subprojeto e conferido automaticamente contra o código-fonte
que eles descrevem.

Onde o repositório oferece quarenta portas de entrada independentes, o relatório
oferece uma trilha — a mesma declarada no
[`README.md` da raiz](../README.md#ordem-de-estudo-sugerida).

## Começando num WSL zerado

```bash
make setup     # instala TeX Live (subconjunto), latexmk e o abridor de PDF
make check     # confere ferramenta por ferramenta, pacote por pacote,
               # e roda o verificador contra o código-fonte de ../src/
make           # gera build/report.pdf
make view      # abre o PDF no visualizador do Windows
```

O `make setup` pede a senha do sudo e baixa cerca de 1,5 GB. Se preferir a
distribuição inteira (~5 GB), use `make setup-full`.

## Fluxo de edição

O modo recomendado é deixar o `latexmk` observando os arquivos e o PDF aberto
ao lado:

```bash
make live                                    # documento inteiro
make live CHAPTER=02-fundamentos-da-linguagem  # só um capítulo, bem mais rápido
```

A cada gravação de um `.tex` o PDF é regerado. O Edge e o SumatraPDF recarregam
sozinhos; o Adobe Acrobat trava o arquivo e precisa ser fechado antes do próximo
build.

Trabalhando dentro do VS Code, o `.vscode/settings.json` já configura a
extensão **LaTeX Workshop** para compilar pelo mesmo Makefile, mostrar o PDF
numa aba e sincronizar posição entre texto e PDF (`Ctrl+Alt+J` do `.tex` para o
PDF, `Ctrl`+clique no PDF de volta para o `.tex`).

### Alvos disponíveis

| Comando | O que faz |
|---|---|
| `make` | compila o documento completo em `build/report.pdf` |
| `make watch` | recompila a cada gravação (`Ctrl-C` para sair) |
| `make view` | compila se necessário e abre o PDF |
| `make open` | abre o PDF já existente, sem compilar |
| `make live` | `make` + `open` + `watch` |
| `make draft CHAPTER=nome` | compila só um capítulo, mantendo numeração e referências |
| `make quick` | uma única passagem do pdflatex (sumário fica desatualizado) |
| `make new-chapter NAME=17-nome` | cria o `.tex` do capítulo |
| `make check` | verifica a instalação **e confere o texto contra o fonte** |
| `make overleaf` | empacota `build/report-overleaf.zip` para importar no Overleaf |
| `make clean` | apaga os intermediários e preserva o PDF |
| `make distclean` | apaga o `build/` inteiro |

`draft` e `watch` aceitam `CHAPTER=<nome do arquivo sem .tex>`. A compilação
parcial usa `\includeonly`, então a numeração das seções e as referências
cruzadas continuam corretas — elas vêm dos `.aux` do último build completo.
Depois de um draft, o `make` seguinte refaz o documento inteiro
automaticamente.

## Conferência automática contra o código-fonte

Esta é a parte do projeto que mais vale conhecer. O script lê `../src/` como
fonte de verdade e é executado pelo `make check`:

```bash
python3 scripts/check-fontes.py --listar   # bloco a bloco, com o percentual
```

**`check-fontes.py`** confere a convenção declarada na introdução do relatório:
*quando a legenda de um bloco traz o caminho de um arquivo, o conteúdo é
transcrição do código-fonte*. Ele reduz bloco e arquivo a sequências de tokens
(descartando comentários, espaços e qualificadores de namespace) e mede que
fração dos n-gramas do bloco ocorre no arquivo citado. Reprovar significa uma de
duas coisas, e as duas pedem ação: ou a transcrição está infiel, ou o bloco não
é transcrição e a legenda não deveria sugerir que fosse.

Três convenções de legenda, e o que cada uma diz ao verificador:

| Legenda | Significa | É conferida? |
|---|---|---|
| `\texttt{src/oop/demo_virtual.hpp}` | transcrição fiel daquele arquivo | **sim** |
| `resumo de \texttt{src/...}` | condensação com reordenação | não |
| `Saída de \texttt{src/errors/}` (termina em `/`) | saída do programa daquele subprojeto | não |
| sem caminho | código ilustrativo, escrito para o texto | não |

Estado atual: **136 blocos de transcrição**, todos aprovados (`make check`).

> ⚠ O verificador reconhece a palavra exata **`resumo de`** antes do caminho. Escrever
> "condensado de" ou "resumo do comentário de" NÃO desliga a checagem — e foi assim que
> seis blocos legítimos reprovaram na primeira rodada dos capítulos novos.

## Importando no Overleaf

```bash
make overleaf     # gera build/report-overleaf.zip
```

No Overleaf: **New Project → Upload Project**, selecione o `.zip` e faça **um
ajuste, obrigatório**:

> **Menu → Settings → Main document → `report.tex`**

Sem isso a compilação falha. O Overleaf escolhe o arquivo principal procurando
o `\documentclass`, que neste projeto mora em `assets/MASAfrontmatter.tex` e não
no `report.tex`. O compilador é o pdfLaTeX (padrão) e não é preciso
*shell-escape*.

O `.zip` leva apenas o que o Overleaf precisa — `report.tex`, `assets/`,
`chapters/`, `figures/`, `references.bib` e `new-aiaa.bst` — mais um
`LEIA-ME-OVERLEAF.txt` na raiz repetindo essas instruções (o texto vive em
`tools/LEIA-ME-OVERLEAF.txt`). Ficam de fora o `build/`, o `tools/`, o
`scripts/` e o `templates/`.

Fica de fora também o **`.latexmkrc`**, de propósito: ele fixa
`$out_dir='build'` e cria `build/chapters/` antes da compilação, coisas que
fazem sentido no fluxo local e quebram no Overleaf, que procuraria o PDF onde
ele não está.

A primeira compilação é lenta — sumário, lista de figuras, lista de tabelas,
glossário e índice remissivo obrigam várias passagens até a paginação
estabilizar.

O repositório continua sendo a fonte de verdade — inclusive porque só nele os
capítulos ficam ao lado de `../src/`, que é o código que eles descrevem. O
Overleaf serve para revisão e edição colaborativa, não como cópia principal.

## Organização dos arquivos

```
report.tex                  arquivo mestre: metadados + \include de cada capítulo
assets/
  preamble.tex              pacotes, estilos, cores, linguagens do listings, TikZ
  glossario.tex             verbetes do glossário
  MASAfrontmatter.tex       template MASA (capa, folha de rosto, fontes)
  tocstyle.sty
chapters/
  00-frontmatter.tex           capa, sumário, lista de figuras e de tabelas
  01-introducao.tex            o repositório, as convenções, o ambiente
  02-fundamentos-da-linguagem.tex   syntax, casts, value_semantics, operators,
                                    smart-pointers, oop, errors
  03-biblioteca-padrao.tex     stl, ranges, file_io, vocabulary_types, memory
  04-programacao-generica.tex  templates, concepts, compile_time, lambdas
  05-design-e-arquitetura.tex  solid, design-of-pattern, ecs, plugin_registry
  06-concorrencia.tex          concurrency, coroutines, mandelbrot(+openmp)
  07-rede-e-dados.tex          network, http-client, database, rabbitmq, aws
  08-graficos-e-simulacao.tex  collision, sfml_bullet(2), qt_sfml(2), calculus,
                               flight-simulator
  09-baixo-nivel.tex           inline_asm, outline_asm, asm_course
  10-interop-e-visao.tex       python, embed-python, parser, yolo
  11-ferramentas.tex           meson, cmake, doxygen, parser, sandbox
  13-vocabulario-e-compile-time.tex  vocabulary_types, compile_time, cpp23,
                                     modules
  14-texto-tempo-e-bits.tex    strings, chrono, bits, file_io
  15-modelo-de-objeto-por-dentro.tex  type_erasure, layout, linkage
  16-corretude-e-desempenho.tex  testing, undefined_behavior, performance
  12-apendice-trilhas.tex      trilhas por objetivo e estado de cobertura
scripts/
  check-fontes.py           confere as transcrições contra ../src/
references.bib              bibliografia
build/                      PDF e intermediários (descartável)
```

> ⚠ Repare que `12-apendice-trilhas` vem por ÚLTIMO no `report.tex`, mesmo com
> número menor que 13–16. O número é histórico: os capítulos 13 a 16 foram
> escritos depois, quando o repositório ganhou os subprojetos de modelo de
> objeto e de corretude, e renumerar quebraria as referências cruzadas que já
> existiam. A ordem que vale é a do `\include` no `report.tex`.

A ordem dos capítulos 02 a 10 é a das etapas 1 a 9 da trilha do `README.md` da
raiz. Quatro subprojetos do Tier 2 do [`../src/ROADMAP.md`](../src/ROADMAP.md)
— `operators`, `lambdas`, `vocabulary_types` e `compile_time` — não constam
daquela trilha e foram inseridos onde o pré-requisito manda; a
[§ I.3](chapters/01-introducao.tex) explica a decisão.

### Figuras

**Não há imagens neste relatório.** Todas as figuras são desenhos TikZ escritos
dentro do próprio capítulo, com os estilos comuns (`caixa`, `folha`, `seta`,
`rotulo`, …) definidos em `assets/preamble.tex`. Editar uma figura é editar o
`.tex` ao lado do texto que ela ilustra, e o `make` não depende de nenhuma
ferramenta externa para isso.

O diretório `figures/` existe vazio porque o `\graphicspath` e alguns alvos do
Makefile o referenciam; se um dia entrar uma imagem de verdade, é ali.

⚠️ **Uma armadilha do TikZ que este projeto encontrou:** `\\` dentro de um grupo
aninhado num nó (`{título\\{\scriptsize a\\b}}`) quebra a composição com
*Undefined control sequence* em `\tikzscope@linewidth`. Escreva
`{título\\\scriptsize a\\b}` — sem o grupo — e o `\scriptsize` vale até o fim
do nó.

### Acrescentando um capítulo

```bash
make new-chapter NAME=17-meu-capitulo
```

Falta apenas acrescentar a linha `\include{chapters/17-meu-capitulo}` no
`report.tex`.

## Convenções do preâmbulo

Quatro blocos de `assets/preamble.tex` merecem atenção na hora de escrever:

- **Caracteres UTF-8** — o pdflatex não conhece nativamente símbolos como `→`,
  `≈`, `µ`, `§`, `✓`, `①` ou `─`. Cada um tem uma linha `\newunicodechar` no
  preâmbulo **e** uma entrada na tabela `literate` (que é o que vale dentro de
  um `lstlisting`). Ao usar um símbolo novo que não esteja lá, a compilação para
  com *"Invalid UTF-8 byte sequence"* ou *"Unicode character not set up"*: basta
  acrescentar as duas linhas.
  ⚠️ `µ` (U+00B5, micro) e `μ` (U+03BC, mu grego) são caracteres **diferentes**
  e visualmente idênticos; os programas do repositório imprimem o primeiro.
- **Linguagens do `listings`** — `Meson` e `GAS` são definidas no preâmbulo com
  `\lstdefinelanguage`; `C++`, `Python` e `bash` vêm do próprio pacote.
- **Estilo `bt`** — usado em todo bloco de código
  (`\begin{lstlisting}[style=bt, language=C++]`), junto com as cores `codekw` e
  `codecomment`.
- **Caixas de destaque** — `regra`, `armadilha` e `porque`, cada uma com um
  papel fixo descrito no comentário do preâmbulo e na seção de convenções do
  relatório. Fora delas há apenas o `aviso`, de uso único, que abre a página 1.

### Glossário e índice

O glossário usa `\printnoidxglossary`, que ordena os verbetes **em TeX puro**.
Por isso todo `\newglossaryentry` precisa de uma chave `sort=` em texto simples:
um `name={\texttt{std::pmr}}` sem `sort` faz a compilação abortar com *Improper
alphabetic constant*.

O índice remissivo tem três atalhos, definidos no preâmbulo:

| Comando | Imprime | Indexa sob |
|---|---|---|
| `\classe{std::vector}` | `std::vector` | o próprio nome |
| `\cabecalho{memory}` | `<memory>` | `cabeçalhos` |
| `\projeto{value\_semantics}` | `src/value_semantics/` | `subprojetos` |

Nada impede escrever `\texttt{}` direto quando a ocorrência não merece entrada
no índice — a convenção é marcar apenas a **primeira** aparição significativa de
cada nome.
