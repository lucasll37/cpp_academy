# tools/vendor/

Binarios baixados, nao versionados junto com o texto do relatorio.

## pandoc

Build estatico oficial (pandoc 3.10.2, linux-amd64), obtido em
<https://github.com/jgm/pandoc/releases>. Esta aqui porque a conversao para
`.doc` precisa do pandoc e a maquina em que ela foi montada nao tinha permissao
para instalar pacotes do sistema.

E apenas um plano B: `tools/doc.mk` usa o pandoc do sistema sempre que existir
um no `PATH`. Depois de rodar `make setup-doc`, que instala o pandoc via apt,
este diretorio pode ser apagado sem afetar nada.

Para reobter:

    curl -sL -o pandoc.tar.gz \
      https://github.com/jgm/pandoc/releases/download/3.10.2/pandoc-3.10.2-linux-amd64.tar.gz
    tar xzf pandoc.tar.gz && mv pandoc-3.10.2 pandoc && rm pandoc.tar.gz
