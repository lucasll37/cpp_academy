# doxygen — transformar comentário em documentação verificável

## O que é este projeto?

Um subprojeto que ensina a documentar código C++ com **Doxygen**: um programa que lê os seus
`.hpp`/`.cpp`, extrai as assinaturas reais dos símbolos, casa cada assinatura com o comentário
que está acima dela e escreve um **site HTML navegável** com tudo isso.

Duas coisas para rodar, e elas são independentes:

```bash
# 1. o executável de demonstração (só exercita a API documentada)
make build
./dist/bin/doxygen_demo

# 2. a documentação em si — o produto de verdade deste subprojeto
ninja -C build docs
xdg-open build/src/doxygen/docs/html/index.html
```

> ⚠ O nome do binário é `doxygen_demo`, não `doxygen` — este último é a ferramenta.
> Confira com `grep -n "executable(" src/doxygen/meson.build`.

---

## Antes de começar: as sete palavras

Nada aqui pressupõe conhecimento de ferramentas de documentação. Se alguma destas sete
palavras for nova, leia esta seção inteira antes de qualquer outra.

### 1. Gerador de documentação

Não é um formatador de texto nem um editor. É um **compilador de comentários**. O ciclo é:

```
   soma.hpp                      doxygen                   html/
┌──────────────────┐          ┌───────────┐          ┌──────────────────┐
│ /** @brief ... */│  ──────▶ │  faz o    │ ──────▶  │ index.html       │
│ int soma(int a,  │  lê o    │  parse do │  escreve │ soma_8hpp.html   │
│          int b); │  código  │  C++ E do │          │ globals.html     │
└──────────────────┘          │ comentário│          │ ...              │
                              └───────────┘          └──────────────────┘
```

O ponto que muda tudo: ele lê **as duas coisas**. Um formatador de Markdown só conhece o seu
texto. O Doxygen conhece o texto *e* a assinatura — por isso pode dizer que o seu `@param b`
não corresponde a nenhum parâmetro chamado `b`. Ver a seção
[Documentação que não mente](#documentação-que-não-mente).

### 2. Bloco de documentação

Um comentário **marcado**. O Doxygen ignora comentários comuns: `// isto` e `/* isto */` não
existem para ele. Reconhecidos são quatro:

| Estilo | Abre com | Uso típico |
|---|---|---|
| JavaDoc | `/**` (barra + **dois** asteriscos) | funções, classes, arquivos |
| Qt | `/*!` (barra, asterisco, exclamação) | idem, estilo alternativo |
| Linha | `///` (**três** barras) | membros, enumeradores |
| Linha para trás | `///<` | documenta o item que está **à esquerda** |

```cpp
/// Coordenada horizontal.        ← documenta o x abaixo
double x;
double y;   ///< Coordenada vertical.   ← documenta o y à esquerda
```

Escolha um estilo e seja consistente. Este projeto usa `/** ... */` e `///<`.

### 3. Tag (ou comando)

Uma palavra dentro do bloco, prefixada, que diz ao Doxygen **que papel** aquele texto exerce.
`@brief Soma dois inteiros.` não é uma frase qualquer: é a frase que vai para a coluna de
resumo do índice. `@param a primeira parcela` vira **uma linha na tabela Parameters**.

> ⚠ **Os dois prefixos são equivalentes.** `@brief` e `\brief` fazem exatamente a mesma coisa;
> `@param` e `\param` também. Metade da documentação do mundo usa `\`, e você vai encontrá-la.
> Este projeto usa `@` em todo lugar, por ser mais legível dentro de strings e macros.

### 4. Referência cruzada

Um **link automático de um símbolo para outro**. Com `AUTOLINK_SUPPORT = YES` (ligado no
`Doxyfile.in`), quando você escreve o nome de uma classe ou função no texto do comentário,
o Doxygen o transforma em link clicável para a página daquele símbolo. Quando o nome sozinho
é ambíguo, você força com `@ref`:

```cpp
/// @tparam T Tipo aritmético (satisfaz @ref math::Numeric).
```

Aqui `math::Numeric` **é um símbolo deste projeto** — um *concept* definido na linha 94 de
[math_utils.hpp](math_utils.hpp) — e não uma palavra mágica do Doxygen. O `@ref` gera um link
para a página `conceptmath_1_1Numeric.html`. Referência cruzada é o que separa documentação
gerada de um PDF: você **navega**.

### 5. Concept (C++20) — o que é `template <Numeric T>`

Aparece em [math_utils.hpp](math_utils.hpp) e você vai tropeçar nele antes de chegar ao
Doxygen. Um *concept* é um **predicado sobre tipos**, avaliado em tempo de compilação:

```cpp
template <typename T>
concept Numeric = std::is_arithmetic_v<T>;   // "T é int, float, char, double, ..."

template <Numeric T>          // lê-se: "para todo T que satisfaz Numeric"
T abs(T x) { ... }            // abs(std::string{}) nem compila, com erro legível
```

Sem concept, `abs(std::string{})` compilaria a chamada e explodiria trinta linhas depois,
dentro do corpo do template. Com concept, o erro é na chamada e diz o motivo.
Detalhes em [../concepts/README.md](../concepts/README.md). Aqui basta saber que
`template <Numeric T>` é "template com uma restrição", e que ela se documenta com `@tparam`.

### 6. Graphviz e o programa `dot`

**Graphviz** é um desenhista de grafos: recebe uma descrição textual de nós e setas e devolve
uma imagem. O executável dele chama-se `dot`. O Doxygen não desenha nada sozinho — quando
`HAVE_DOT = YES`, ele **escreve arquivos `.dot` e chama o `dot`** para virarem SVG:

```
Doxygen ──escreve──▶ grafo.dot ──dot──▶ grafo.svg ──embutido na──▶ página HTML
```

É de onde vêm os diagramas de herança (quem herda de quem), de colaboração (quais tipos a
classe contém) e de inclusão (`INCLUDE_GRAPH`: quais headers este arquivo puxa).

> Você **não** precisa instalar nada: o [`conanfile.py`](../../conanfile.py) da raiz já faz
> `apt.install(["graphviz"])` no `system_requirements`. Se `HAVE_DOT = YES` e o `dot` não
> existir, o Doxygen só avisa e gera as páginas sem os diagramas.

### 7. Os formatos de saída

O `Doxyfile.in` liga só o HTML, mas os outros existem e valem saber para que servem:

| Formato | Chave | Para que serve |
|---|---|---|
| HTML | `GENERATE_HTML` | o site navegável — é o que você quer 95% das vezes |
| LaTeX | `GENERATE_LATEX` | vira PDF rodando `make` dentro de `docs/latex/` |
| XML | `GENERATE_XML` | **não é para humanos**: é o que Sphinx/Breathe consomem para publicar docs Python-style de uma API C++ |
| man | `GENERATE_MAN` | páginas de `man`, para bibliotecas de sistema instaladas via pacote |
| RTF / DocBook | `GENERATE_RTF`, `GENERATE_DOCBOOK` | interoperar com Word e com toolchains editoriais |

---

## Por que estudar isto?

O problema não é "meu código não tem comentário". É que **o comentário comum morre onde está
escrito** e ninguém verifica se ele ainda é verdade.

| Sem Doxygen | Com Doxygen |
|---|---|
| o comentário só existe para quem abre o `.hpp` | vira uma página HTML com índice e busca |
| ler a API exige ler a implementação | a listagem de símbolos é gerada e está sempre completa |
| descobrir quem usa `Vector2D` é `grep` | a página do símbolo lista as referências cruzadas |
| o comentário mente há dois anos e ninguém percebeu | `WARN_IF_DOC_ERROR` **falha** quando `@param` não bate com a assinatura |
| a hierarquia de classes está na cabeça de alguém | o `dot` desenha o diagrama a partir do código real |
| o IDE mostra um tooltip vazio | clangd/VSCode/CLion leem o `@brief` e mostram no tooltip |

A última linha da tabela é a mais importante e a que mais gente ignora: com
`WARN_IF_UNDOCUMENTED` e `WARN_AS_ERROR`, a documentação deixa de ser uma promessa e passa a
ser **um artefato verificado pelo build**, exatamente como um teste.

---

## Estrutura de arquivos

```
src/doxygen/
├── README.md         ← este arquivo
├── main.cpp          ← 3 demos que exercitam a API + o bloco @mainpage (linha 17)
├── math_utils.hpp    ← 567 linhas: TODO comando de documentação em uso real
├── Doxyfile.in       ← TEMPLATE de configuração (ver ⚠ abaixo)
└── meson.build       ← executable('doxygen_demo') + custom_target('docs')
```

> ⚠ **Não existe `Doxyfile` nesta pasta, e é de propósito.** Existe `Doxyfile.in`, que contém
> os marcadores `@SOURCE_ROOT@` e `@BUILD_ROOT@`. Esses dois caminhos são absolutos e só são
> conhecidos quando você roda `meson setup` — dependem de onde o repositório está no disco e
> de qual diretório de build você escolheu. O `configure_file()` do
> [meson.build](meson.build) substitui os dois e escreve o **Doxyfile real** em
> `build/src/doxygen/Doxyfile`. Rodar `doxygen Doxyfile.in` não funciona: o Doxygen tentaria
> ler um diretório literalmente chamado `@SOURCE_ROOT@/src/doxygen`.

---

## Doxygen em 60 segundos

Faça este exercício antes de olhar qualquer tabela de comandos. São seis linhas de terminal e
termina com uma página aberta no navegador. **Fora do repositório**, para não confundir nada:

```bash
mkdir -p /tmp/dox && cd /tmp/dox
cat > soma.hpp <<'EOF'
#pragma once
/** @file soma.hpp
 *  @brief Aritmética mínima, só para ver o Doxygen funcionando. */

/**
 * @brief Soma dois inteiros.
 * @param a primeira parcela
 * @param b segunda parcela
 * @return a soma de @p a e @p b
 * @warning Não trata overflow com sinal — que é comportamento indefinido.
 */
int soma(int a, int b) { return a + b; }
EOF

doxygen -g Doxyfile          # escreve um Doxyfile com TODAS as opções e os padrões
```

Agora edite duas linhas do `Doxyfile` recém-criado:

```diff
- INPUT                  =
+ INPUT                  = .
- GENERATE_LATEX         = YES
+ GENERATE_LATEX         = NO
```

`INPUT` diz o que ler (vazio = nada), e `GENERATE_LATEX = NO` evita gerar centenas de arquivos
`.tex` que você não vai usar. Só falta gerar e abrir:

```bash
doxygen Doxyfile
xdg-open html/index.html
```

### O `@file` não é decoração

Se você omitir o bloco `@file`, o Doxygen gera o site **sem a função** — só com
`soma_8hpp_source.html` (o código-fonte colorido) e nenhuma `soma_8hpp.html`. Medido: com
`@file`, saem 6 páginas; sem ele, 4, e a documentação da `soma` não aparece em lugar nenhum.

A regra é essa: **símbolos globais (funções livres, macros, variáveis) só são documentados se
o arquivo que os contém estiver documentado.** Classes e membros de classe não sofrem disso.
É a pegadinha nº 1 de quem começa.

---

## O que o HTML mostra

Essa é a parte que nenhuma tabela de comandos ensina: **qual tag vira qual coisa na tela**.
Abaixo, o texto real da `soma_8hpp.html` gerada acima (Doxygen 1.9.4), com as tags anotadas:

```
soma.hpp File Reference
Aritmética mínima, só para ver o Doxygen funcionando.      ← @file + @brief do arquivo
More...

Functions
    int  soma (int a, int b)
         Soma dois inteiros.   More...                     ← @brief da função, na LISTAGEM

Detailed Description
Aritmética mínima, só para ver o Doxygen funcionando.

Function Documentation
◆ soma()
    int soma ( int a, int b )                              ← assinatura extraída do CÓDIGO
    Soma dois inteiros.                                    ← @brief de novo, agora no corpo

    Parameters                                             ← tabela gerada pelos @param
        a    primeira parcela
        b    segunda parcela

    Returns                                                ← seção gerada pelo @return
        a soma de a e b                                    ← @p a e @p b viraram monoespaçado

    Warning                                                ← caixa destacada, gerada pelo @warning
        Não trata overflow com sinal — que é comportamento indefinido.
```

Repare no que aconteceu:

- `@brief` aparece **duas vezes**: uma na tabela de listagem (com "More..." linkando para
  baixo) e outra no início do bloco detalhado.
- `@param` não é texto corrido: as duas linhas colapsaram numa **tabela de duas colunas**.
- `@p a` no texto do `@return` virou `a` em fonte monoespaçada — `@p` marca "a próxima
  palavra é um parâmetro desta função".
- `@warning` virou uma caixa própria, com título. `@note`, `@bug`, `@todo` e `@deprecated`
  produzem caixas equivalentes; `@todo` e `@bug` ainda entram em páginas-índice globais
  (`todo.html`, `bug.html`) que juntam todas as ocorrências do projeto inteiro.
- A **assinatura** `int soma ( int a, int b )` não veio do comentário. Veio do parser de C++.

---

## Os comandos, agora que você já viu o resultado

### Identificação

| Comando | O que faz |
|---|---|
| `@file` | declara que este arquivo está documentado — **obrigatório** para documentar funções livres |
| `@brief` | a frase curta; vai para índices e tooltips do IDE |
| `@details` | a explicação longa; só aparece na página do símbolo |
| `@namespace` | documenta um namespace (usado na linha 73 de [math_utils.hpp](math_utils.hpp)) |
| `@concept` | documenta um *concept* C++20 — gera a página `concepts.html` |
| `@author` `@version` `@date` `@since` | metadados; `@since` é o mais útil dos quatro |

### Parâmetros e retorno

| Comando | O que faz |
|---|---|
| `@param nome desc` | uma linha na tabela **Parameters** |
| `@param[in] nome` | idem, marcando o parâmetro como só de entrada |
| `@param[out] nome` | valor produzido pela função através daquele parâmetro |
| `@param[in,out] nome` | lido e modificado |
| `@tparam T desc` | o mesmo, mas para parâmetro de **template** |
| `@return desc` | a seção **Returns** |
| `@retval v desc` | uma linha por valor concreto de retorno — bom para `bool` e códigos de erro |
| `@p nome` | dentro de um texto, marca a palavra como **nome de parâmetro** (vira monoespaçado) |

> `@p` é fácil de esquecer porque é o único que aparece **no meio de uma frase** e não no
> começo de uma linha. Exemplo real, linha 109 de [math_utils.hpp](math_utils.hpp):
> `@return O valor absoluto de @p x, no mesmo tipo @p T.`

### Contrato

| Comando | O que faz |
|---|---|
| `@pre` | o que **o chamador** tem de garantir antes de chamar |
| `@post` | o que a função garante depois de retornar |
| `@throws` / `@throw` | uma exceção que pode sair da função, e quando |
| `@invariant` | condição sempre verdadeira para a classe |

Um contrato bem escrito responde a "de quem é a culpa quando quebra". `@pre lo <= hi` mais
`@throws std::invalid_argument se lo > hi` (linhas 141-143) dizem: violar é erro do chamador,
e a função vai reclamar em vez de devolver lixo.

### Anotação

| Comando | Aparece como |
|---|---|
| `@note` | caixa neutra |
| `@warning` | caixa de atenção |
| `@bug` | caixa **+ entrada em `bug.html`** |
| `@todo` | caixa **+ entrada em `todo.html`** |
| `@deprecated` | caixa **+ entrada em `deprecated.html`** |
| `@see` | seção "See also" com links (aceita símbolo ou URL) |
| `@code` … `@endcode` | bloco de código com realce de sintaxe |
| `@ref alvo` | link explícito para um símbolo ou seção |

### Agrupamento — `@defgroup`, `@ingroup`, `@addtogroup`

Por padrão a documentação é organizada por **arquivo** e por **classe**. Grupos ("módulos")
criam um segundo eixo, temático: "tudo que é conversão de unidades", mesmo que esteja
espalhado em cinco headers.

```cpp
/**
 * @defgroup conversion Conversão de Unidades   ← cria o grupo: id + título
 * @brief Funções para conversão entre sistemas de unidades.
 * @{                                           ← ABRE o grupo
 */

double deg_to_rad(double deg);   // tudo aqui dentro pertence ao grupo
double rad_to_deg(double rad);

/** @} */                                       ← FECHA o grupo
```

`@{` e `@}` são chaves, como as de bloco em C++: **tudo entre elas entra no grupo**, sem
precisar repetir `@ingroup` em cada símbolo. As três tags se dividem assim:

| Tag | Papel |
|---|---|
| `@defgroup id Título` | **cria** o grupo. Só pode aparecer uma vez por id |
| `@ingroup id` | põe **este símbolo** no grupo (útil solto, fora de `@{ @}`) |
| `@addtogroup id` | **acrescenta** a um grupo já criado, em outro arquivo, sem redefinir o título |

O código em [math_utils.hpp](math_utils.hpp) linhas 540-565 usa `@defgroup` + `@{ @}` **e**
`@ingroup` em cada função — os dois juntos, o que é redundante mas inofensivo.

### Página principal — `@mainpage`

O `index.html` do site precisa de conteúdo, e por padrão vem quase vazio. `@mainpage` marca um
bloco como sendo essa página. Ele pode ficar em qualquer arquivo; aqui está na linha 17 de
[main.cpp](main.cpp). Dentro dele, `@section id Título` cria as seções e o índice lateral.

> ⚠ **Comentários C não aninham.** Escrever `/**` literal dentro de um bloco `/* ... */`
> quebra os dois lados: o GCC avisa `'/*' within comment` e o Doxygen avisa
> `Reached end of file while still inside a (nested) comment`. Foi exatamente o que aconteceu
> aqui — o `@mainpage` de `main.cpp` mostrava exemplos de sintaxe e o alvo precisou de
> `-Wno-comment` para compilar. A correção **não** foi manter a supressão: foi reescrever os
> exemplos como tabela e remover o `-Wno-comment`. A nota histórica está no topo do
> [meson.build](meson.build). Regra geral: desligar um aviso é a segunda melhor opção; a
> primeira é remover a causa.

---

## Os demos, um a um

Todos vivem em [math_utils.hpp](math_utils.hpp), e cada um existe para exercitar um conjunto
de tags. Leia o header **junto com** a página HTML correspondente — é aí que a moeda cai.

### Demo 1 · Cabeçalho de arquivo — linhas 34-57

`@file`, `@brief`, `@details`, `@author`, `@version`, `@date`, `@since` e dois `@see`.
Produz a página `math__utils_8hpp.html`, com a descrição do arquivo no topo e o grafo de
inclusão gerado pelo `dot`.

### Demo 2 · Concept documentado — linhas 82-94

```cpp
/**
 * @concept Numeric
 * @brief Restringe o tipo para qualquer tipo aritmético.
 * @tparam T Tipo a verificar.
 */
template <typename T>
concept Numeric = std::is_arithmetic_v<T>;
```

Gera `conceptmath_1_1Numeric.html`, e é o alvo de todos os `@ref math::Numeric` do arquivo.
Confirmado na saída: o Doxygen imprime `Generating docs for concept math::Numeric...`.

### Demo 3 · Parâmetros, retorno e `@p` — `abs`, linhas 99-123

O caso mínimo completo: `@tparam` com `@ref`, `@param[in]`, `@return` usando `@p`, um `@note`
e um `@code`/`@endcode`. É o que virou a tabela **Parameters** que você viu acima.

### Demo 4 · Contrato — `clamp`, linhas 125-158

`@pre lo <= hi`, `@post O valor retornado está em [lo, hi]`,
`@throws std::invalid_argument se lo > hi`, mais um `@see` para uma URL externa
(cppreference) — provando que `@see` aceita link, não só símbolo. O `@return` aqui é uma
**lista de três casos**, não uma frase: Markdown com `-` funciona dentro dos blocos.

### Demo 5 · `@retval` e `@todo` — `nearly_equal`, linhas 188-214

Dois `@retval` (um para `true`, outro para `false`) em vez de um `@return` genérico — a forma
certa de documentar predicado. E um `@todo` que, além da caixa amarela na página da função,
cria uma entrada em `todo.html` juntando todos os `@todo` do projeto.

### Demo 6 · Classe — `Stats`, linhas 218-347

`@class` implícito com `@brief`/`@details`, um `@note` sobre `std::span` **não copiar** os
dados, um `@warning` sobre o construtor lançar, um `@bug` real (acúmulo de erro numérico para
datasets grandes, linha 283) e `@see` entre membros (`stddev()` → `variance()`).

### Demo 7 · Struct com operadores e `@deprecated` — `Vector2D`, linhas 350-533

Documentação membro a membro, inclusive de `operator+`, `operator*` e `operator==`. O
`operator==` está marcado `@deprecated` (linha 526) porque igualdade exata de `double`
raramente é o que se quer — a caixa aparece na página **e** em `deprecated.html`.

### Demo 8 · Grupo — linhas 536-565

`@defgroup conversion` + `@{ @}` + `@ingroup`, cobrindo `deg_to_rad` e `rad_to_deg`. Gera uma
entrada em **Modules**, ao lado de Files e Classes.

### Demo 9 · `@mainpage` — [main.cpp](main.cpp), linhas 17-63

A página inicial do site, com `@section intro`, `@section estrutura` (uma tabela Markdown),
`@section gerar` (um `@code{.sh}` com os comandos reais) e `@section convencoes`.

---

## Documentação que não mente

Este é o argumento mais forte a favor do Doxygen, e o que quase todo tutorial deixa de fora.

Três chaves do `Doxyfile.in` transformam documentação em algo **checado**:

| Chave | Efeito |
|---|---|
| `WARN_IF_UNDOCUMENTED = YES` | avisa quando um símbolo público não tem bloco nenhum |
| `WARN_IF_DOC_ERROR = YES` | avisa quando o bloco **contradiz** a assinatura |
| `WARN_AS_ERROR = FAIL_ON_WARNINGS` | faz o `doxygen` sair com código ≠ 0 — o build quebra |

Prova, medida agora. Pegue o `soma.hpp` do exercício de 60 segundos e troque `@param b` por
`@param c` (um rename de parâmetro esquecido — o erro mais comum que existe):

```bash
doxygen Doxyfile
```

```
/tmp/dox/soma.hpp:7: error: argument 'c' of command @param is not found in the
                            argument list of soma(int a, int b)
/tmp/dox/soma.hpp:7: error: The following parameter of soma(int a, int b) is not
                            documented:
  parameter 'b'
```

Com `WARN_AS_ERROR = FAIL_ON_WARNINGS`, `echo $?` responde `1`. Nenhum linter de texto
consegue isso, porque nenhum linter de texto sabe a assinatura da função.

> ⚠ O `Doxyfile.in` deste projeto mantém `WARN_AS_ERROR = NO`. É deliberado: falhar a build
> de um repositório didático por um `@param` faltando atrapalha quem está aprendendo. **Em
> projeto real, ligue.** É a diferença entre documentação e ficção.

Estado atual, medido com `rm -rf build/src/doxygen/docs && ninja -C build docs`: **zero
avisos** para as 764 linhas de `math_utils.hpp` + `main.cpp`.

### E o `.cpp` que não deve virar página?

Duas saídas, dependendo do que você quer:

- **`EXTRACT_ALL = NO`** (o padrão aqui): só entra no site o que tiver bloco de documentação.
  Um `.cpp` sem `@file` fica de fora automaticamente. É o comportamento que você quer quase
  sempre — e é por isso que o `@file` importa tanto.
- **`EXCLUDE_PATTERNS`**: exclui por caminho, independentemente de estar documentado. O
  `Doxyfile.in` (linha 62) já exclui `*/build/*`, `*/.git/*`, `*/node_modules/*` e `*/README.md`.

Ligar `EXTRACT_ALL = YES` faz o Doxygen documentar **tudo**, com ou sem comentário — bom para
um primeiro retrato de código legado, ruim como estado permanente, porque destrói o valor de
`WARN_IF_UNDOCUMENTED` (nada mais fica "não documentado").

---

## As chaves do `Doxyfile.in`, explicadas

Só as que este projeto liga e que mudam algo visível. Todas em [Doxyfile.in](Doxyfile.in).

| Chave | Valor aqui | O que muda na saída |
|---|---|---|
| `OUTPUT_DIRECTORY` | `@BUILD_ROOT@/docs` | raiz de tudo; com `HTML_OUTPUT = html`, o site fica em `docs/html/` |
| `INPUT` | `@SOURCE_ROOT@/src/doxygen @SOURCE_ROOT@/include` | o que ler; aceita vários caminhos |
| `RECURSIVE` | `YES` | desce nos subdiretórios de `INPUT` |
| `OUTPUT_LANGUAGE` | `Portuguese` | traduz os rótulos gerados: "Parameters" vira "Parâmetros" |
| `EXTRACT_ALL` | `NO` | só documenta o que tem bloco (ver acima) |
| `EXTRACT_STATIC` | `YES` | inclui funções `static` de arquivo — as `demo_*` de `main.cpp` |
| `GENERATE_TREEVIEW` | `YES` | painel de navegação em árvore à esquerda, com `TREEVIEW_WIDTH = 250` |
| `SEARCHENGINE` | `YES` | caixa de busca client-side no canto superior direito |
| `SOURCE_BROWSER` | `YES` | gera as páginas `*_source.html` com o código colorido |
| `REFERENCES_LINK_SOURCE` | `YES` | os nomes na doc linkam **para a linha do código** |
| `HAVE_DOT` | `YES` | habilita todos os grafos abaixo (exige o `dot`) |
| `CLASS_GRAPH` | `YES` | diagrama de herança por classe |
| `COLLABORATION_GRAPH` | `YES` | diagrama de "quais tipos esta classe usa" |
| `INCLUDE_GRAPH` / `INCLUDED_BY_GRAPH` | `YES` | quem este header inclui / quem o inclui |
| `CALL_GRAPH` / `CALLER_GRAPH` | `NO` | grafos de chamada — desligados: explodem em projeto grande |
| `DOT_IMAGE_FORMAT` | `svg` | vetorial, dá zoom sem borrar |
| `MAX_DOT_GRAPH_DEPTH` | `3` | corta grafos gigantes |
| `HTML_TIMESTAMP` | `NO` | sem data no rodapé — assim regerar não muda o diff |

Medido nesta build: `Running dot for graph 1/5` — cinco grafos, e a fase leva a maior parte do
tempo. Desligar `HAVE_DOT` é o primeiro ajuste quando a geração fica lenta.

> ⚠ **Versão importa.** O `ninja -C build docs` **não** usa o `doxygen` do seu sistema: usa o
> que o Conan instala, fixado em `doxygen/1.9.4` no [conanfile.py](../../conanfile.py) linha 48.
> Consequência prática: `HTML_COLORSTYLE = LIGHT|DARK|AUTO_LIGHT` **não existe** no 1.9.4 —
> lá as chaves ainda são `HTML_COLORSTYLE_HUE`, `_SAT` e `_GAMMA`. `HTML_COLORSTYLE` só chegou
> no 1.9.5. Confira sempre com `doxygen --version` antes de copiar opção de tutorial da
> internet; opções desconhecidas viram aviso de configuração e são ignoradas.

> ⚠ `CLASS_DIAGRAMS` é **obsoleta**. Em versões recentes ela foi absorvida por `CLASS_GRAPH`,
> e mantê-la produz um aviso de chave desconhecida. Este `Doxyfile.in` usa `CLASS_GRAPH`.

---

## A integração com o Meson

O conteúdo real de [meson.build](meson.build) — três construções, todas explicadas em
[../meson/README.md](../meson/README.md):

```meson
# 1. configure_file: substitui @SOURCE_ROOT@/@BUILD_ROOT@ e escreve o Doxyfile real
doxyfile = configure_file(
    input:         'Doxyfile.in',
    output:        'Doxyfile',
    configuration: {
        'SOURCE_ROOT': meson.project_source_root(),
        'BUILD_ROOT':  meson.current_build_dir(),
    }
)

# 2. find_program(required: false): degradação graciosa — sem doxygen, o resto compila
doxygen_prog = find_program('doxygen', required: false)

if doxygen_prog.found()
    # 3. custom_target: um comando arbitrário que produz um artefato
    docs_target = custom_target('docs',
        input:            doxyfile,
        output:           'docs',        # ver ⚠ abaixo
        command:          [doxygen_prog, '@INPUT@'],
        build_by_default: false,         # `make build` NÃO gera docs
        install:          false,
    )
    alias_target('docs', docs_target)    # permite `ninja docs` em vez do caminho completo
else
    warning('doxygen não encontrado — target `docs` desabilitado.')
endif
```

> ⚠ **`output:` tem de casar com o que o Doxygen realmente cria.** O `Doxyfile.in` tem
> `OUTPUT_DIRECTORY = @BUILD_ROOT@/docs`, ou seja, o Doxygen cria o diretório
> `build/src/doxygen/docs/`. Se o `output:` declarasse `'html'`, o Ninja procuraria um
> artefato que nunca aparece, concluiria que o alvo falhou em produzi-lo e **re-executaria o
> Doxygen em toda build**. É um erro silencioso e caro: nada quebra, só fica lento para sempre.

`build_by_default: false` é o que faz `make build` compilar 90 binários sem gastar segundos
gerando documentação. Você pede explicitamente com `ninja -C build docs`.

---

## Como validar

```bash
# 1. o executável
make build
./dist/bin/doxygen_demo
```

Deve imprimir três seções emolduradas e terminar com `✓ doxygen demo concluído`. Alguns
valores para conferir que a API documentada faz o que a documentação promete:

```
  clamp(15, 0, 10) = 10          ← @post: resultado está em [lo, hi]
  0.1+0.2 == 0.3         → false ← igualdade exata de double
  nearly_equal(0.1+0.2)  → true  ← @retval true do Demo 5
  mean:     18.0000              ← (4+8+15+16+23+42)/6
  variance: 182.0000             ← divisor n−1, como diz o @return
  (a+b).length() = 5.0           ← 3² + 4² = 25, √25 = 5
  a rotacionado 90°  = (0.0, 3.0)
```

```bash
# 2. a documentação — do zero, para ver a saída inteira
rm -rf build/src/doxygen/docs
ninja -C build docs
```

Deve terminar com `Running dot for graph 5/5`, `Patching output file 8/8` e `finished...`, e
**sem nenhuma linha de `warning:` ou `error:`**. O Doxygen formata seus avisos como
`arquivo:linha: warning: texto` (é o `WARN_FORMAT` do Doxyfile), então filtre pelos dois-pontos
— assim a linha `warning_level : 3` que o Meson imprime ao reconfigurar não polui o resultado:

```bash
ninja -C build docs 2>&1 | grep -E ": (warning|error):" || echo "sem avisos"
```

```bash
# 3. os arquivos gerados
ls build/src/doxygen/docs/html/*.html | wc -l     # 32 páginas
ls build/src/doxygen/docs/html/ | grep -E "Stats|Numeric|Vector2D"
```

Devem existir, entre outros, `classmath_1_1Stats.html`, `conceptmath_1_1Numeric.html`,
`structmath_1_1Vector2D.html`, `todo.html`, `bug.html`, `deprecated.html` e `modules.html`.

```bash
# 4. abrir
xdg-open build/src/doxygen/docs/html/index.html
```

Roteiro de 1 minuto dentro do site, para ver cada tag no lugar:

1. A página inicial é o `@mainpage` de `main.cpp`, com as quatro `@section` no índice.
2. **Classes → math::Stats** → o `@bug` da `variance()` numa caixa própria.
3. **Concepts → math::Numeric** → e volte por qualquer `@ref math::Numeric`.
4. **Modules → Conversão de Unidades** → o grupo criado pelo `@defgroup`.
5. **Related Pages → Todo List / Bug List / Deprecated List** → os índices agregados.
6. Em qualquer função, clique no nome do arquivo no topo → o `SOURCE_BROWSER` leva à linha.

---

## Referências

### Neste repositório

- [../meson/README.md](../meson/README.md) — `custom_target`, `configure_file`, `find_program`,
  `alias_target` explicados do zero
- [../concepts/README.md](../concepts/README.md) — o `template <Numeric T>` do Demo 3
- [../testing/README.md](../testing/README.md) — a mesma ideia deste README aplicada a testes:
  transformar promessa em verificação automática
- [main.cpp](main.cpp) · [math_utils.hpp](math_utils.hpp) · [Doxyfile.in](Doxyfile.in) ·
  [meson.build](meson.build)

### Fontes canônicas

- Manual do Doxygen — <https://www.doxygen.nl/manual/index.html>
- Lista completa de comandos especiais — <https://www.doxygen.nl/manual/commands.html>
- Todas as opções de configuração — <https://www.doxygen.nl/manual/config.html>
- Documenting the code (o tutorial oficial dos estilos de bloco) —
  <https://www.doxygen.nl/manual/docblocks.html>
- Graphviz — <https://graphviz.org/>
- `doxygen-awesome-css`, o tema que faz a saída parecer deste século —
  <https://github.com/jothepro/doxygen-awesome-css>
- Breathe, a ponte `GENERATE_XML` → Sphinx — <https://breathe.readthedocs.io/>
