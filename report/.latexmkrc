# =============================================================================
#  Configuracao do latexmk para este projeto.
#
#  Serve para que `latexmk` invocado diretamente -- ou pela extensao LaTeX
#  Workshop do VS Code -- use exatamente os mesmos ajustes do Makefile.
#  Se voce usa o `make`, nao precisa mexer aqui.
# =============================================================================

use File::Path qw(make_path);

$pdf_mode = 1;                       # gerar PDF via pdflatex
$out_dir  = 'build';                 # tudo (PDF e intermediarios) em build/

# -interaction=nonstopmode : nao interrompe pedindo entrada quando ha erro
# -file-line-error         : mensagens no formato arquivo:linha (clicaveis)
# -synctex=1               : permite saltar entre o PDF e o .tex
$pdflatex = 'pdflatex -interaction=nonstopmode -file-line-error -synctex=1 %O %S';

# Cada \include{chapters/NN-...} escreve um .aux com o mesmo caminho relativo
# dentro do diretorio de saida. Sem estes subdiretorios o pdflatex aborta com
# "I can't write on file `chapters/01-introducao.aux'".
make_path("$out_dir/chapters");

# O documento tem sumario, lista de figuras, lista de tabelas, glossario e
# indice remissivo. Cada um desses desloca paginas e obriga uma passagem extra
# ate tudo estabilizar; a partir de uma reconstrucao do zero (`make distclean`)
# sao precisas cerca de sete. O padrao do latexmk e cinco, e ele abortaria com
# "Maximum runs of pdflatex reached without getting stable files".
$max_repeat = 10;

# Extensoes extras a remover no `latexmk -c`.
# .idx/.ind/.ilg vem do makeindex; .glo/.gls/.glg do glossaries.
$clean_ext = 'synctex.gz synctex.gz(busy) run.xml bbl fls fdb_latexmk ' .
             'idx ind ilg glo gls glg';

# Nao abrir visualizador automaticamente: quem cuida disso e o `make view`,
# que sabe converter o caminho para o formato do Windows no WSL.
$view = 'none';
