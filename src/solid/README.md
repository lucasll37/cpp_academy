# solid — Os cinco princípios, com violação e conformidade lado a lado

## O que é este projeto?

Cinco subprojetos independentes, um por princípio. Cada um mostra **o mesmo problema duas
vezes**: uma versão que viola o princípio e uma que o respeita — e depois mede ou demonstra o
que a violação custa.

```bash
make build
./dist/bin/srp    ./dist/bin/ocp    ./dist/bin/lsp
./dist/bin/isp    ./dist/bin/dip
```

## Antes de tudo: o que SOLID **não** é

SOLID é um acrônimo cunhado por Michael Feathers para cinco princípios que Robert C. Martin
publicou entre 1996 e 2000, pensando em **hierarquias de classes**. Isso importa por dois
motivos:

1. **Não são leis.** São observações sobre o que torna software caro de mudar. Aplicá-los onde
   não há pressão de mudança produz abstração inútil — e abstração inútil é dívida, não ativo.
2. **Vieram de antes do C++ moderno.** Vários deles têm hoje soluções que não passam por
   herança: `std::variant` + `visit`, `std::function`, concepts, templates. Cada subprojeto
   aponta a alternativa moderna onde ela existe.

> ⚠ O sintoma de SOLID mal aplicado é uma interface com **um único implementador**, criada
> "porque um dia pode ter outro". Ela dobra o número de arquivos, impede o compilador de
> inlinar, e o segundo implementador nunca chega. Ver
> [`src/type_erasure/`](../type_erasure/) sobre quando `virtual` **não** se paga.

## Os cinco, em uma frase cada

| | Nome | A frase | O que dói sem ele |
|---|---|---|---|
| **S** | Single Responsibility | uma classe, **um motivo para mudar** | mexer no formato de log quebra o cálculo de imposto |
| **O** | Open/Closed | **aberto** para extensão, **fechado** para modificação | todo tipo novo exige editar um `switch` que já tem 40 casos |
| **L** | Liskov Substitution | o subtipo tem de **poder substituir** a base | `Quadrado` herda de `Retângulo` e quebra quem esperava retângulo |
| **I** | Interface Segregation | ninguém depende de método que **não usa** | implementar `Impressora` obriga a escrever `enviarFax()` |
| **D** | Dependency Inversion | dependa da **abstração**, não do concreto | a classe constrói o próprio `BancoDeDados` e não há como testá-la |

---

## S — Single Responsibility · [`srp/`](srp/)

> *"Uma classe deve ter um, e apenas um, motivo para mudar."*

O ponto que quase todo mundo lê errado: **não** é "uma classe faz uma coisa só". É **um motivo
para mudar** — e "motivo" quer dizer *uma pessoa ou um papel que pede a mudança*.

Uma classe `Relatorio` que calcula números **e** os formata em PDF tem dois motivos: o setor
financeiro muda a fórmula; o setor de design muda o layout. Duas pessoas diferentes, dois
ritmos diferentes, um arquivo só — e todo conflito de merge entre elas.

O subprojeto mostra um pipeline de processamento na forma monolítica e depois separado em
etapas componíveis.

> ⚠ Um bug latente foi encontrado aqui: `srp/demo_pipeline.hpp` não compilava — faltava
> `#include <algorithm>`. Ficou invisível porque `solid/` **não era construído**. É a razão de
> hoje não haver nenhum `subdir()` comentado em [`src/meson.build`](../meson.build).

## O — Open/Closed · [`ocp/`](ocp/)

> *"Aberto para extensão, fechado para modificação."*

Acrescentar um comportamento novo deve ser **escrever código novo**, não **editar código que
já funciona** — porque editar código testado é onde as regressões nascem.

A forma clássica é polimorfismo: uma base abstrata, e cada extensão é uma classe nova.

> ⚠ Mas em C++ moderno há três formas, e a clássica nem sempre é a melhor:
>
> | forma | tipo novo | operação nova |
> |---|---|---|
> | `virtual` | fácil | **caro** — mexe na base e em todo derivado |
> | `std::variant` + `visit` | **caro** — recompila os visitors | fácil |
> | type erasure | fácil, e **sem tocar no tipo** | caro |
>
> É o **problema da expressão**: você escolhe qual eixo fica barato. Medido em
> [`src/type_erasure/`](../type_erasure/), onde `variant` foi **4× mais rápido** que as
> alternativas com heap.

## L — Liskov Substitution · [`lsp/`](lsp/)

> *"Se S é subtipo de T, objetos de T podem ser substituídos por objetos de S sem alterar as
> propriedades desejáveis do programa."*

É o único dos cinco que descreve uma condição **verificável**, e por isso o mais concreto:

- o subtipo **não pode fortalecer** as pré-condições (exigir mais do chamador);
- o subtipo **não pode enfraquecer** as pós-condições (entregar menos);
- as **invariantes** da base continuam valendo.

O exemplo canônico é `Quadrado : Retângulo`. Matematicamente um quadrado *é* um retângulo; em
código, não: `setLargura(5)` num retângulo deixa a altura em paz, e num quadrado não. Quem
escreveu código contra `Retângulo` quebra.

O subprojeto usa **testes de contrato**: um mesmo conjunto de testes rodado contra a base e
contra cada derivado. Se o derivado falha num teste que a base passa, o LSP foi violado — e
isso é detectável, não opinião.

> ⚠ A ferramenta de C++ para isso é o **NVI** (Non-Virtual Interface): o método público é
> **não-virtual** e checa o contrato; ele chama um `virtual` privado que faz o trabalho. Assim
> o derivado não consegue burlar a checagem.

## I — Interface Segregation · [`isp/`](isp/)

> *"Clientes não devem ser forçados a depender de interfaces que não usam."*

Uma interface gorda obriga todo implementador a escrever métodos que não fazem sentido para
ele — e o que ele escreve é `throw std::logic_error("não suportado")`, que é uma violação de
LSP acontecendo.

A correção é quebrar em interfaces pequenas e coesas, e compor. Em C++, `Escaneavel`,
`Imprimivel` e `Faxeavel` separados; uma multifuncional herda das três, uma impressora simples
herda de uma.

> ⚠ Em C++ há uma alternativa que o Java não tem: **templates e concepts**. Um `concept` exige
> exatamente as operações que a função usa, sem obrigar ninguém a herdar de nada — segregação
> **por uso**, verificada em compilação e com custo zero em runtime. Ver
> [`src/concepts/`](../concepts/).

## D — Dependency Inversion · [`dip/`](dip/)

> *"Módulos de alto nível não devem depender de módulos de baixo nível. Ambos devem depender de
> abstrações."*
> *"Abstrações não devem depender de detalhes. Detalhes devem depender de abstrações."*

É o princípio de maior retorno prático dos cinco, e o motivo é direto: **é ele que torna o
código testável**.

```cpp
class Carrinho {
    BancoDeDados db_{"localhost:5432"};   // ❌ só testável com Postgres no ar
};

class Carrinho {
    const RepositorioDePrecos& precos_;   // ✓ recebe a abstração
public:
    explicit Carrinho(const RepositorioDePrecos& p) : precos_{p} {}
};
```

A segunda versão roda em microssegundos com um dublê em memória. A primeira exige um servidor,
falha quando alguém mexe no banco, e não roda no CI.

É exatamente essa a estrutura de [`src/testing/`](../testing/), onde `loja::Carrinho` recebe um
`RepositorioDePrecos` por construtor — e é por isso que os 41 testes de lá rodam em
milissegundos.

> ⚠ "Inversão" não quer dizer "use interfaces em tudo". Quer dizer inverter **quem decide** a
> dependência concreta: não a classe, e sim quem a constrói. Em ordem de preferência:
> parâmetro de **função** > parâmetro de **construtor** > parâmetro de **template** (despacho
> estático, sem vtable — [`src/templates/`](../templates/)).

---

## Como validar

```bash
make build
for p in srp ocp lsp isp dip; do ./dist/bin/$p >/dev/null || echo "FALHOU: $p"; done
```

Estado atual: **5/5 compilam sem warnings e executam com código 0.**

## Referências

- [`src/design-of-pattern/`](../design-of-pattern/) — os 23 padrões GoF, com "versão moderna"
- [`src/type_erasure/`](../type_erasure/) — o problema da expressão, medido
- [`src/concepts/`](../concepts/) — segregação de interface em compilação
- [`src/testing/`](../testing/) — o DIP como pré-requisito de testabilidade
- Robert C. Martin, *Agile Software Development: Principles, Patterns, and Practices*
- Barbara Liskov & Jeannette Wing, *A Behavioral Notion of Subtyping* (1994) — o L
- Herb Sutter, *Virtuality* (2001) — o argumento do NVI
