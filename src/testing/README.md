# testing — Testes Automatizados com GoogleTest

## O que é este projeto?

Um curso completo de **teste automatizado em C++**, do primeiro `EXPECT_EQ` até *death tests*,
*mocks*, cobertura e *fuzzing* — com testes de verdade que rodam de verdade.

Diferente dos outros subprojetos, este tem **dois tipos de alvo**:

| alvo | o que é |
|---|---|
| `./dist/bin/testing` | o executável **didático**, que narra o assunto |
| `test_*.cpp` | **testes de verdade**, rodados por `meson test -C build` |

O sistema sob teste é `loja::Carrinho` (`carrinho.hpp`/`.cpp`): um carrinho de compras pequeno
o bastante para caber na cabeça e grande o bastante para ter os problemas que testes de
verdade encontram — uma regra de negócio com faixas, uma pré-condição que aborta, uma exceção,
uma dependência externa e aritmética de ponto flutuante.

```bash
make build                                      # compila e instala em dist/
./dist/bin/testing                              # a narrativa

meson test -C build                             # os testes
meson test -C build testing_parametrizado -v    # um deles, com a saída completa
```

> ⚠ `make build` é o comando certo — não `meson setup dist`. Neste repositório `build/` é o
> diretório de compilação e `dist/` é o **prefixo de instalação**; os binários instalados ficam
> em `./dist/bin/`. O `make build` também faz `source build/conanbuild.sh` antes do
> `meson compile`, sem o que o Meson nem encontra o `fmt`.

**Contagem exata**, conferida com `meson test -C build --list` e `--gtest_list_tests`:

| alvo de teste | casos |
|---|---|
| `testing_basico` | 6 |
| `testing_fixture` | 6 |
| `testing_parametrizado` | 18 (8 `Values` + 8 `Range` + 2 tipos) |
| `testing_death` | 5 |
| `testing_mock` | 6 |
| **total deste subprojeto** | **5 alvos, 41 casos** |

> ⚠ `meson test -C build` roda **6** alvos, não 5: o sexto é `academy:guia / saudacao`,
> declarado em [`src/meson/`](../meson/) e que não pertence a este subprojeto.

> ⚠ Este subprojeto **não** reativa o `tests/` da raiz, que segue desabilitado por decisão de
> projeto. Tudo aqui é auto-contido.

---

## Antes de começar

Nada abaixo pressupõe experiência com testes. Estes são os termos que o resto do texto usa.

**Teste automatizado.** Um programa que executa outro programa e **verifica** o resultado sem
ninguém olhar. "Rodei e pareceu certo" não é teste: não repete, não avisa quando quebra.

**Unidade.** O menor pedaço com comportamento próprio — normalmente uma classe ou uma função
livre. Um *teste unitário* exercita uma unidade **sem** banco de dados, rede ou disco.

**GoogleTest (gtest).** A biblioteca de testes do Google. Ela dá três coisas: as macros de
asserção (`EXPECT_EQ` e companhia), um **registro** de todos os testes do binário e um `main()`
pronto que roda o registro inteiro e imprime o relatório.

**gmock.** Uma biblioteca **irmã**, distribuída junto com o gtest. Ela gera *dublês* de teste
(`MOCK_METHOD`) e fornece os **matchers** (`Eq`, `Ge`, `ElementsAre`...). Os matchers são úteis
mesmo em testes que não usam mock nenhum — daí a palavra aparecer nas duas metades do texto.

**Macro.** Uma substituição textual feita pelo pré-processador **antes** da compilação.
`TEST(A, B) { ... }` não é sintaxe de C++: é uma macro que se expande em uma classe e um
registro. A seção "Como o gtest sabe quais testes existem" mostra exatamente no quê.

**Suíte de testes** (*test suite*). Um grupo de casos com o mesmo nome no primeiro argumento de
`TEST`. `TEST(Carrinho, ComecaVazio)` põe o caso `ComecaVazio` na suíte `Carrinho`.

**Fixture.** Uma **classe** que carrega o estado comum a vários casos (o objeto sob teste, os
dublês, arquivos temporários). O gtest constrói **uma instância nova para cada caso** — é o que
garante que um teste não contamine o outro. Demo 3 mostra a nossa, linha por linha.

**Dublê de teste** (*test double*). Qualquer objeto falso posto no lugar de uma dependência real.
Os cinco tipos (dummy, stub, spy, mock, fake) estão na tabela da Demo 4.

**Matcher.** Um objeto-predicado componível usado com `EXPECT_THAT(valor, matcher)`. `Ge(3)` é
um matcher; `AllOf(Gt(0), Lt(100))` é a composição de dois. Vêm do gmock.

**Death test.** Um teste que verifica que uma chamada **mata o processo** (por `assert`, `abort`
ou `std::terminate`). Demo 6.

**Cobertura** (*coverage*). A porcentagem do código que os testes executaram. É um detector de
buracos, não uma nota. Demo 5.

**ULP** (*unit in the last place*). A distância entre um `double` e o próximo `double`
representável. É a unidade natural de tolerância em ponto flutuante, porque **acompanha a
magnitude**: 4 ULPs perto de `1e9` valem cerca de `1e-7`, e perto de `1e-9` valem cerca de
`1e-25`. `EXPECT_DOUBLE_EQ` usa 4 ULPs; `EXPECT_NEAR` usa uma tolerância **absoluta**, que você
escolhe.

**AAA.** *Arrange, Act, Assert* — os três blocos de um teste bem escrito. Demo 2.

**F.I.R.S.T.** As cinco propriedades de um bom teste unitário. Demo 1.

## Por que estudar isto?

A pergunta útil não é "devo testar?". É **"que teste vale a pena escrever?"**. Uma suíte grande
de testes ruins é **pior** que nenhuma: custa manutenção, falha por motivos errados, e ensina a
equipe a ignorar a luz vermelha.

E porque a lição mais valiosa aqui não é a API do GoogleTest: é que **código difícil de testar
é código mal desenhado**, e o teste é o primeiro cliente a reclamar. A Demo 4 mostra a linha
exata — um construtor — que separa a classe testável em microssegundos da classe que só roda
com Postgres no ar.

## Estrutura de arquivos

```
src/testing/
├── main.cpp                  ← a narrativa, cor pale_violet_red
├── demo_porque.hpp           ← 1 · pirâmide, F.I.R.S.T., os testes que dão prejuízo
├── demo_anatomia.hpp         ← 2 · AAA, nomes, EXPECT vs ASSERT, o catálogo
├── demo_organizacao.hpp      ← 3 · ciclo da fixture, as 4 variantes, as flags
├── demo_dubles.hpp           ← 4 · os 5 dublês, injeção de dependência
├── demo_cobertura.hpp        ← 5 · gcov, sanitizers, property-based, fuzzing
│
├── carrinho.hpp / .cpp       ← o SISTEMA SOB TESTE
├── test_basico.cpp           ← TEST, EXPECT/ASSERT, float, exceções
├── test_fixture.cpp          ← TEST_F, SetUp/TearDown, isolamento provado
├── test_parametrizado.cpp    ← TEST_P (fronteiras!), TYPED_TEST
├── test_mock.cpp             ← gmock: ON_CALL, EXPECT_CALL, Nice/Strict
├── test_death.cpp            ← EXPECT_DEATH, e os 4 perigos
├── meson.build
└── README.md
```

---

## Demo 0: Seu primeiro teste, do zero

Este é um arquivo **completo**. Nada foi omitido.

```cpp
// t.cpp
#include <gtest/gtest.h>

TEST(Aritmetica, DoisMaisDoisSaoQuatro) {
    EXPECT_EQ(2 + 2, 4);
}
```

Não há `main()`, e não há chamada para `DoisMaisDoisSaoQuatro` em lugar nenhum. Os dois fatos
são propositais, e a próxima subseção explica por quê.

Compilando e rodando (neste repositório, o gtest vem do Conan; `build/gtest.pc` diz onde ele
está):

```bash
GP=$(sed -n 's|^prefix=||p' build/gtest.pc)
g++ -std=c++20 t.cpp -o /tmp/t -I$GP/include -L$GP/lib -lgtest -lgtest_main -pthread
/tmp/t
```

Saída **literal**:

```
Running main() from .../googletest/src/gtest_main.cc
[==========] Running 1 test from 1 test suite.
[----------] Global test environment set-up.
[----------] 1 test from Aritmetica
[ RUN      ] Aritmetica.DoisMaisDoisSaoQuatro
[       OK ] Aritmetica.DoisMaisDoisSaoQuatro (0 ms)
[----------] 1 test from Aritmetica (0 ms total)

[----------] Global test environment tear-down
[==========] 1 test from 1 test suite ran. (0 ms total)
[  PASSED  ] 1 test.
```

Ler esse relatório é metade do valor do framework. `[ RUN      ]` abre um caso, `[       OK ]`
o fecha com sucesso, e `[  PASSED  ]` resume. **O código de saída do processo é 0.**

### E quando falha

Troque o `4` por `5` e recompile:

```cpp
TEST(Aritmetica, DoisMaisDoisSaoQuatro) {
    EXPECT_EQ(2 + 2, 5);          // errado de propósito
}
```

```
[ RUN      ] Aritmetica.DoisMaisDoisSaoQuatro
/tmp/t.cpp:4: Failure
Expected equality of these values:
  2 + 2
    Which is: 4
  5

[  FAILED  ] Aritmetica.DoisMaisDoisSaoQuatro (0 ms)
...
[  PASSED  ] 0 tests.
[  FAILED  ] 1 test, listed below:
[  FAILED  ] Aritmetica.DoisMaisDoisSaoQuatro

 1 FAILED TEST
```

E o processo sai com **código 1** — é assim que o `meson test` e o CI sabem que quebrou.

Repare no que a mensagem dá de graça: **arquivo e linha**, o **texto da expressão** (`2 + 2`),
o **valor que ela produziu** (`Which is: 4`) e o **valor esperado** (`5`). Nada disso apareceria
com `EXPECT_TRUE(2 + 2 == 5)`, que imprimiria apenas `false`. **Prefira sempre a macro que
conhece os dois lados.**

### Como o gtest sabe quais testes existem

Você nunca chama a sua função de teste. `TEST(Aritmetica, DoisMaisDoisSaoQuatro) { ... }` é uma
**macro** que se expande, aproximadamente, nisto:

```cpp
class Aritmetica_DoisMaisDoisSaoQuatro_Test : public ::testing::Test {
    void TestBody() override;                    // ← o seu { ... } vira este corpo
    static ::testing::TestInfo* const info_;     // ← um objeto ESTÁTICO
};

// A inicialização deste estático roda ANTES do main() e registra o caso
// num registro global mantido pela biblioteca:
::testing::TestInfo* const Aritmetica_DoisMaisDoisSaoQuatro_Test::info_ =
    ::testing::internal::MakeAndRegisterTestInfo("Aritmetica", "DoisMaisDoisSaoQuatro", ...);

void Aritmetica_DoisMaisDoisSaoQuatro_Test::TestBody() { EXPECT_EQ(2 + 2, 4); }
```

O `main()` de `gtest_main` faz só duas coisas: `InitGoogleTest(&argc, argv)` e
`return RUN_ALL_TESTS();` — que percorre o registro global e executa tudo o que se cadastrou.

Três consequências práticas caem daí:

1. **Você nunca declara um teste em header nenhum**, nem o menciona duas vezes. Basta o arquivo
   `.cpp` entrar no binário.
2. O par `(suíte, caso)` vira **nome de classe C++** — daí a proibição de espaço, acento, hífen,
   e a recomendação de não usar `_` (ele já é o separador que a macro usa).
3. Um teste dentro de um `namespace { ... }` anônimo continua sendo registrado: o registro
   acontece pelo **construtor do estático**, não pelo nome.

---

## Demo 1: Por que testar

### A pirâmide

```
        ▲
       ╱ ╲     E2E / sistema       poucos, lentos, frágeis, caros
      ╱───╲    integração          alguns
     ╱     ╲   UNITÁRIOS           MUITOS, rápidos, estáveis, baratos
    ╱_______╲
```

A forma é consequência do custo por teste em três eixos: **tempo** (µs vs minutos),
**estabilidade** (mais peças = mais motivos de falhar sem haver bug) e **diagnóstico** (um
unitário que falha aponta a linha; um E2E aponta "algo, em algum lugar").

> ⚠ A inversão ("casquinha de sorvete") acontece quando a equipe testa só pela interface. O
> sintoma é a suíte que demora 40 minutos e falha 1 vez em 5 sem motivo.

### F.I.R.S.T.

| | |
|---|---|
| **F**ast | se demora, ninguém roda antes de commitar |
| **I**solated | independe dos outros e da **ordem** |
| **R**epeatable | inimigos: relógio, rede, aleatoriedade sem semente, ordem de `unordered_map` |
| **S**elf-validating | passa ou falha, sem ninguém ler a saída |
| **T**imely | escrito junto com o código — código difícil de testar é mal desenhado |

### O que testar primeiro

1. **Fronteiras.** A regra de desconto tem faixas em 10, 50 e 100. Os casos que importam são
   9/10, 49/50, 99/100 — é ali que mora o `>` que deveria ser `>=`.
2. **O caminho de erro.** É o código menos exercitado manualmente, e por isso o mais quebrado.
3. **O bug que você acabou de consertar.** Melhor custo-benefício que existe: ele nunca volta.
4. **A regra de negócio.**
5. O caminho feliz — por último.

**Não** teste: getters triviais, a biblioteca padrão, detalhes privados, código gerado.

### ⚠ Os testes que dão prejuízo

- **Frágil** — falha em toda refatoração correta. Sintoma: cheio de `EXPECT_CALL`. Testa
  *como*, não *o quê*.
- **Instável (flaky)** — o pior de todos, porque destrói a confiança na suíte inteira. Deve ser
  **consertado ou removido**, nunca deixado piscando.
- **Tautológico** — repete a implementação. Com a API real deste subprojeto:

  ```cpp
  // ❌ passa mesmo que a fórmula do total esteja errada — ele a repete
  EXPECT_DOUBLE_EQ(c.total(), c.subtotal() * (1 - c.percentual_de_desconto()));

  // ✓ o valor esperado veio de FORA: 10 canetas a 2,50 = 25,00, menos 5%
  EXPECT_DOUBLE_EQ(c.total(), 23.75);
  ```
- **Sem asserção** — só verifica que não explodiu.
- **Gigante** — quando falha a asserção nº 4, você não descobre se as outras 26 falhariam.

---

## Demo 2: Anatomia e asserções

### Arrange / Act / Assert

Três blocos, nessa ordem, com uma linha em branco entre eles:

```cpp
TEST(Carrinho, MesmoSkuDuasVezesSomaQuantidade) {
    PrecosFixos    precos;              // Arrange — preparar o mundo
    loja::Carrinho c{precos};

    c.adicionar("caneta", 2);           // Act — UMA ação, a que está sob teste
    c.adicionar("caneta", 5);

    ASSERT_EQ(c.tamanho(), 1u);         // Assert — verificar
    EXPECT_EQ(c.itens()[0].quantidade, 7);
}
```

Se o **Act** tem mais de duas ou três linhas, ou se há dois blocos act-assert, o teste está
testando duas coisas.

> Um teste é o único código em que **a repetição é preferível à abstração**.
> `EXPECT_DOUBLE_EQ(total, 77.9)` com o número literal é melhor que uma função
> `calcula_esperado()` — que repetiria a implementação e passaria a esconder o bug.

### O nome como documentação

```
ruim : TEST(CarrinhoTest, Test1)  ·  TEST(CarrinhoTest, FuncionaCorretamente)
bom  : TEST(Carrinho, MesmoSkuDuasVezesSomaQuantidade)
bom  : TEST(Desconto, ExatamenteDezPecasDaCincoPorCento)
```

A fórmula: `<situação><ação><resultado esperado>`. Os nomes viram **identificadores C++** — nada
de espaço, acento, hífen, e o próprio GoogleTest desaconselha `_`.

### EXPECT vs ASSERT

`EXPECT_*` registra a falha e **continua**; `ASSERT_*` registra e faz **`return`**.

Use `EXPECT` por padrão (mais informação por execução). Use `ASSERT` quando o resto do teste não
faz sentido — ou **crasharia**:

```cpp
ASSERT_EQ(c.tamanho(), 1u);          // sem isto, a linha seguinte é UB
EXPECT_EQ(c.itens()[0].sku, "caneta");
```

> ⚠ `ASSERT_*` faz `return` — logo não funciona em função que devolva valor, e dentro de uma
> **função auxiliar** ele retorna da auxiliar, deixando o teste continuar. Para propagar:
> `ASSERT_NO_FATAL_FAILURE(minha_verificacao())`.

### O catálogo

```
booleanas   EXPECT_TRUE / FALSE
comparação  EXPECT_EQ NE LT LE GT GE        ← imprimem OS DOIS valores na falha
strings C   EXPECT_STREQ STRNE STRCASEEQ    ← para std::string, use EXPECT_EQ
float       EXPECT_FLOAT_EQ DOUBLE_EQ NEAR
exceções    EXPECT_THROW NO_THROW ANY_THROW
morte       EXPECT_DEATH DEBUG_DEATH        ← Demo 6
controle    FAIL() ADD_FAILURE() SUCCEED() GTEST_SKIP()
```

Todo `EXPECT_X` tem um `ASSERT_X` correspondente, com a mesma assinatura.

### Matchers e `EXPECT_THAT`

Um **matcher** é um objeto-predicado componível. `EXPECT_THAT(valor, matcher)` verifica que o
valor casa com o matcher, e imprime uma mensagem que **descreve o matcher** quando falha.

Os matchers vêm do **gmock**, mesmo quando não há mock nenhum no teste. Isso significa duas
coisas concretas:

```cpp
#include <gmock/gmock.h>     // ← precisa deste include
using ::testing::ElementsAre, ::testing::Ge, ::testing::HasSubstr;   // ← e destes using
```

e que o binário precisa linkar com `gmock` (no `meson.build`, é o que separa o alvo
`testing_mock` dos demais).

```cpp
EXPECT_THAT(v, ElementsAre(1, 2, 3));            // exatamente estes, nesta ordem
EXPECT_THAT(v, UnorderedElementsAre(3, 1, 2));   // estes, em qualquer ordem
EXPECT_THAT(v, SizeIs(Ge(3)));                   // matcher DENTRO de matcher
EXPECT_THAT(s, HasSubstr("erro"));
EXPECT_THAT(x, AllOf(Gt(0), Lt(100)));           // composição
EXPECT_THAT(o, Optional(Eq(42)));                // std::optional preenchido com 42
```

Eles valem pela **mensagem de falha**: um `ElementsAre` que falha imprime os dois vetores e
marca o índice divergente. Um `EXPECT_TRUE(v == esperado)` imprimiria apenas `false`.

### ⚠ Ponto flutuante

```
0.1 + 0.2 = 0.30000000000000004441
0.3       = 0.29999999999999998890
diferença = 5.551e-17
```

`EXPECT_EQ` sobre `double` é quase sempre um bug de teste. As duas alternativas:

| macro | tolerância | quando |
|---|---|---|
| `EXPECT_DOUBLE_EQ(a, b)` | **4 ULPs** — relativa, acompanha a magnitude | comparar resultados de contas |
| `EXPECT_NEAR(a, b, eps)` | **absoluta**, você escolhe o `eps` | quando você sabe o erro tolerável |

Lembrando a definição de ULP dada em *Antes de começar*: 4 ULPs perto de `1e9` valem cerca de
`1e-7`; perto de `1e-9`, cerca de `1e-25`. É por isso que `DOUBLE_EQ` funciona igualmente bem
em qualquer escala — e por isso que ele **não** funciona em torno de zero.

> ⚠ Nenhuma funciona bem **em torno de zero**: 4 ULPs de zero é absurdamente pequeno; use
> `EXPECT_NEAR(x, 0.0, 1e-12)`. E `NaN` não é igual a nada, nem a si mesmo — verifique com
> `EXPECT_TRUE(std::isnan(x))`.

---

## Demo 3: Organização — fixtures, parâmetros e tipos

### O que é uma fixture

Os seis testes de `test_basico.cpp` repetem as mesmas duas linhas de preparo. Uma **fixture** é
uma classe que carrega esse estado; o gtest constrói **uma instância nova por caso**. Este é o
código real de `test_fixture.cpp`, encurtado só nos comentários:

```cpp
class PrecosFixos : public loja::RepositorioDePrecos {          // o dublê
public:
    std::optional<double> preco_de(const std::string& sku) const override {
        if (sku == "caneta")  return 2.50;
        if (sku == "caderno") return 12.00;
        return std::nullopt;
    }
};

class CarrinhoTest : public ::testing::Test {   // ← toda fixture herda de ::testing::Test
protected:                                      // ← protected: os casos são classes DERIVADAS
    CarrinhoTest() : carrinho{precos} {}        // construtor: roda antes de cada caso

    void SetUp()    override { ++setups_executados;    }
    void TearDown() override { ++teardowns_executados; }

    static void SetUpTestSuite()    { ++suites_iniciadas; }   // UMA vez, para a suíte inteira
    static void TearDownTestSuite() {}

    static inline int suites_iniciadas  = 0;
    static inline int setups_executados = 0;

    PrecosFixos    precos;
    loja::Carrinho carrinho;                    // ← novo a cada caso
};

// TEST_F (de Fixture): o 1º argumento é o NOME DA CLASSE, e vira o nome da suíte.
// O corpo do teste é membro da classe — por isso `carrinho` é visível sem qualificação.
TEST_F(CarrinhoTest, ComecaVazio) {
    EXPECT_TRUE(carrinho.vazio());
}

TEST_F(CarrinhoTest, PrimeiroTesteAdiciona) {
    carrinho.adicionar("caneta", 5);
    EXPECT_EQ(carrinho.tamanho(), 1u);
}

// Este caso PROVA o isolamento: se a fixture fosse compartilhada, o carrinho
// já teria a caneta do caso anterior.
TEST_F(CarrinhoTest, SegundoTesteVeCarrinhoLimpo) {
    EXPECT_TRUE(carrinho.vazio())
        << "se este teste falhar, a fixture está sendo compartilhada";
}
```

`TEST_F` expande para uma classe que **herda de `CarrinhoTest`**. É por isso que os membros
precisam ser `protected` (ou `public`) e por isso que o corpo do teste os enxerga sem `this->`.

### O ciclo de vida da fixture

```
SetUpTestSuite()             ← UMA vez
  ┌ construtor ┐
  │ SetUp()    │
  │  caso 1    │  ← instância NOVA
  │ TearDown() │
  └ destrutor  ┘
  ┌ ... caso 2 ┘  ← outra instância
TearDownTestSuite()          ← UMA vez
```

| | quando |
|---|---|
| **construtor** | padrão — permite membros `const` e sem construtor padrão |
| **`SetUp()`** | quando a preparação pode falhar com `ASSERT_*`, ou quando a fixture é herdada |
| **destrutor** | padrão, e à prova de exceções |
| **`TearDown()`** | quando a limpeza usa `ASSERT_*` ou pode lançar |

> ⚠ `SetUpTestSuite` compartilha estado **entre** casos. É a porta de entrada para o teste que
> só falha numa ordem específica. Use apenas para recursos caros e **imutáveis**.

### As quatro variantes

| macro | varia | quando |
|---|---|---|
| `TEST` | nada | caso isolado, sem preparação |
| `TEST_F` | fixture | preparação repetida |
| `TEST_P` | **dados** | mesma lógica, muitas entradas |
| `TYPED_TEST` | **tipos** | template, ou N implementações |

### `TEST_P` — um corpo, N casos

`TEST_P` é o que mais rende, e é onde as **fronteiras** entram na suíte. Código real de
`test_parametrizado.cpp`:

```cpp
// 1. O caso: entrada e resultado esperado, num struct com NOMES.
struct CasoDesconto {
    int         pecas;
    double      desconto_esperado;
    const char* rotulo;              // vira parte do NOME do teste
};

// 2. A fixture herda de TestWithParam<T> em vez de Test.
class DescontoTest : public ::testing::TestWithParam<CasoDesconto> {
protected:
    PrecosFixos precos;
};

// 3. O corpo, UMA vez. GetParam() devolve o caso atual.
TEST_P(DescontoTest, AplicaAFaixaCorreta) {
    const auto& caso = GetParam();

    loja::Carrinho c{precos};
    c.adicionar("caneta", caso.pecas);

    EXPECT_DOUBLE_EQ(c.percentual_de_desconto(), caso.desconto_esperado)
        << "com " << caso.pecas << " peças";
}

// 4. A instanciação. Os cinco argumentos:
INSTANTIATE_TEST_SUITE_P(
    Faixas,                       // (a) prefixo do nome gerado
    DescontoTest,                 // (b) a fixture
    ::testing::Values(            // (c) o GERADOR de parâmetros
        CasoDesconto{  1, 0.00, "uma_peca"},
        CasoDesconto{  9, 0.00, "logo_abaixo_de_10"},
        CasoDesconto{ 10, 0.05, "exatamente_10"},
        CasoDesconto{ 49, 0.05, "logo_abaixo_de_50"},
        CasoDesconto{ 50, 0.10, "exatamente_50"},
        CasoDesconto{ 99, 0.10, "logo_abaixo_de_100"},
        CasoDesconto{100, 0.15, "exatamente_100"},
        CasoDesconto{999, 0.15, "muito_acima"}
    ),
    [](const ::testing::TestParamInfo<CasoDesconto>& info) {   // (d) o gerador de NOMES
        return std::string{info.param.rotulo};
    });
```

Os nomes que isso produz — saída real de `--gtest_list_tests`:

```
Faixas/DescontoTest.
  AplicaAFaixaCorreta/uma_peca
  AplicaAFaixaCorreta/logo_abaixo_de_10
  AplicaAFaixaCorreta/exatamente_10
  AplicaAFaixaCorreta/logo_abaixo_de_50
  AplicaAFaixaCorreta/exatamente_50
  ...
```

> ⚠ O gerador de nomes (argumento **d**) é opcional e quase obrigatório na prática. Sem ele os
> casos se chamam `AplicaAFaixaCorreta/0`, `/1`, ... — e "`/3` falhou" não diz nada. O nome
> gerado vira identificador: só letras, dígitos e `_`.

Um laço `for` dentro de um `TEST_F` dá **1 teste**, para na primeira falha e diz "algo em algum
caso". O `TEST_P` acima dá **8 testes independentes**, e a falha diz
`Faixas/DescontoTest.AplicaAFaixaCorreta/exatamente_50`.

**Os geradores prontos:**

```
Values(a, b, c)      lista explícita
Range(1, 9)          1,2,...,8      (e Range(0, 10, 2) → 0,2,4,6,8)
Bool()               false, true
ValuesIn(container)  a partir de um vetor/array montado em runtime
Combine(a, b)        PRODUTO CARTESIANO — cuidado, explode rápido
```

### `TYPED_TEST` — um corpo, N tipos

Enquanto `TEST_P` varia os **dados**, `TYPED_TEST` varia o **tipo**. É como se testa um template
ou várias implementações da mesma interface:

```cpp
template <typename T>
class ContainerTest : public ::testing::Test {
protected:
    T container;
};

using TiposTestados = ::testing::Types<std::vector<int>, std::deque<int>>;
TYPED_TEST_SUITE(ContainerTest, TiposTestados);      // ← ; obrigatório

TYPED_TEST(ContainerTest, ComecaVazioEDepoisNao) {
    EXPECT_TRUE(this->container.empty());            // ⚠ this-> obrigatório
    this->container.push_back(42);
    EXPECT_EQ(this->container.size(), 1u);
}
```

> ⚠ Dentro de `TYPED_TEST`, todo membro da fixture precisa de **`this->`**. A fixture é um
> *template*, o nome `container` é **dependente do parâmetro de tipo**, e a regra do C++ manda o
> compilador **não** procurar nomes dependentes na classe base durante a primeira passagem. Sem
> `this->`, o erro é `'container' was not declared in this scope`.

Os nomes gerados trazem o tipo:

```
ContainerTest/0.  # TypeParam = std::vector<int, std::allocator<int> >
  ComecaVazioEDepoisNao
ContainerTest/1.  # TypeParam = std::deque<int, std::allocator<int> >
  ComecaVazioEDepoisNao
```

### As flags que salvam o dia

```bash
--gtest_filter=*Desconto*:-*Lento*   # incluir e excluir
--gtest_repeat=200 --gtest_shuffle   # a combinação que caça flakiness
--gtest_break_on_failure             # parar no debugger
--gtest_list_tests                   # listar sem rodar
--gtest_output=xml:res.xml           # para o CI

meson test -C build testing_mock -v
meson test -C build --repeat 50
meson test -C build --gdb
meson test -C build --wrapper=valgrind
```

Um teste temporariamente quebrado se marca renomeando para `DISABLED_...`: o GoogleTest o pula
e **avisa** quantos estão desabilitados — melhor que comentar e esquecer.

### `gtest_main` ou `main()` próprio

O Conan expõe quatro pacotes pkg-config separados:

| pacote | conteúdo |
|---|---|
| `gtest` | asserções e o runner — **sem `main()`** |
| `gtest_main` | só o `main()` padrão |
| `gmock` | matchers e `MOCK_METHOD` — **sem `main()`** |
| `gmock_main` | `main()` que chama `InitGoogleMock` |

> ⚠ Linkar só com `gtest` produz `undefined reference to 'main'` — foi o que aconteceu na
> primeira versão deste `meson.build`. A separação existe para quem escreve o próprio `main()`
> (é o caso de [`src/meson/`](../meson/)).
>
> ⚠ `RUN_ALL_TESTS()` precisa ter o retorno **retornado**. Ignorá-lo faz o binário sair com 0
> mesmo com testes falhando — e o CI fica verde com a suíte quebrada. O GoogleTest o marca
> `[[nodiscard]]` (o atributo do C++17 que faz o compilador avisar quando o retorno é
> descartado) justamente por isso.

---

## Demo 4: Dublês e testabilidade

| dublê | o que faz | verifica |
|---|---|---|
| dummy | só preenche um parâmetro | nada |
| **stub** | devolve respostas prontas | **estado** |
| spy | um stub que anota o que recebeu | os dois |
| **mock** | tem expectativas programadas | **interação** |
| **fake** | implementação real, simplificada | **estado** |

A escolha certa quase sempre é **fake ou stub**. O mock existe para uma pergunta específica —
"o código chamou X?" — e essa pergunta só importa quando a chamada **é** o efeito que o código
produz (enviou o e-mail, gravou o log).

### A dependência, como interface

Tudo neste subprojeto depende de uma única declaração, em `carrinho.hpp`:

```cpp
class RepositorioDePrecos {
public:
    virtual ~RepositorioDePrecos() = default;
    // devolve nullopt quando o produto não existe
    virtual std::optional<double> preco_de(const std::string& sku) const = 0;
};
```

É uma classe **abstrata**: `= 0` diz que o método não tem corpo aqui e que ninguém instancia
`RepositorioDePrecos` diretamente. Quem quiser ser um repositório de preços herda dela e
implementa `preco_de`. Em produção seria uma consulta ao banco; nos testes, o `PrecosFixos` de
seis linhas que você já viu.

### Injeção de dependência

Por que o `Carrinho` é testável em microssegundos, sem banco de dados? Por causa de uma linha:

```cpp
explicit Carrinho(const RepositorioDePrecos& precos);
```

Compare com o que quase todo mundo escreve primeiro:

```cpp
class Carrinho {
    BancoDeDados db_{"localhost:5432"};   // ❌ só testável com Postgres rodando
};
```

Não é sobre "usar interfaces em tudo". É sobre inverter **quem decide** a dependência concreta —
não a classe, e sim quem a constrói. É o D de SOLID ([`src/solid/dip/`](../solid/dip/)).

Em ordem de preferência: parâmetro de **função** > parâmetro de **construtor** > parâmetro de
**template** (despacho estático, sem vtable — [`src/type_erasure/`](../type_erasure/)).

### ⚠ O que impede o teste

- **Singleton** — amarra o código a uma configuração global compartilhada entre testes.
- **`new` dentro da classe** — não há como substituir a dependência.
- **O relógio** — `system_clock::now()` na lógica torna impossível testar a virada do mês.
  Injete o relógio ([`src/chrono/`](../chrono/)).
- **Métodos privados que você "precisa" testar** — é sintoma: ou o comportamento é alcançável
  pela interface pública, ou aquilo é outra classe querendo nascer.
  ⚠ `#define private public` "funciona" e é UB — muda o layout e viola a ODR
  ([`src/linkage/`](../linkage/)).

---

## Demo 5: gmock na prática

Até aqui todos os dublês foram escritos à mão. O gmock gera o dublê a partir da interface, e
acrescenta a capacidade de **verificar a interação**.

### Declarar o mock

Uma linha por método virtual:

```cpp
#include <gmock/gmock.h>

class MockPrecos : public loja::RepositorioDePrecos {
public:
    // MOCK_METHOD(retorno, nome, (parâmetros), (qualificadores));
    MOCK_METHOD(std::optional<double>, preco_de, (const std::string& sku),
                (const, override));
};

using ::testing::_;        // o coringa: casa com QUALQUER argumento
using ::testing::Eq;       // matcher de igualdade
using ::testing::Return;   // a ação "devolva este valor"
```

> ⚠ Os parênteses em volta da lista de parâmetros e em volta dos qualificadores são
> **obrigatórios** — sem eles a macro não compila. E um tipo com vírgula
> (`std::map<int, int>`) precisa dos seus próprios parênteses ou de um `using` como alias: a
> macro é do pré-processador e se confunde com a vírgula.
>
> ⚠ O `override` na lista de qualificadores é o que garante que você está mesmo substituindo o
> método virtual da interface, e não criando um novo por engano de assinatura.

### `ON_CALL` vs `EXPECT_CALL` — a distinção central

| | o que faz | falha o teste? |
|---|---|---|
| `ON_CALL(m, f(...)).WillByDefault(acao)` | define o **comportamento padrão** | **não** — é só um stub |
| `EXPECT_CALL(m, f(...)).Times(n).WillOnce(acao)` | declara uma **expectativa** | **sim**, se não for cumprida |

```cpp
// ON_CALL — um stub. O teste verifica o ESTADO resultante.
TEST(MockPrecos, StubBasico) {
    NiceMock<MockPrecos> precos;
    ON_CALL(precos, preco_de(_)).WillByDefault(Return(std::nullopt));
    ON_CALL(precos, preco_de(Eq("caneta")))
        .WillByDefault(Return(std::optional<double>{2.50}));

    loja::Carrinho c{precos};
    c.adicionar("caneta", 2);

    EXPECT_DOUBLE_EQ(c.subtotal(), 5.0);          // ← a asserção está aqui
}

// EXPECT_CALL — uma expectativa. Não há EXPECT_ nenhum no corpo,
// e ainda assim o teste pode falhar: a verificação acontece no
// DESTRUTOR do mock, no fim do escopo.
TEST(MockPrecos, ConsultaUmaVezPorAdicionar) {
    MockPrecos precos;
    EXPECT_CALL(precos, preco_de(Eq("caneta")))
        .Times(2)
        .WillRepeatedly(Return(std::optional<double>{2.50}));

    loja::Carrinho c{precos};
    c.adicionar("caneta", 1);
    c.adicionar("caneta", 1);
}
```

Se a expectativa não for cumprida, a mensagem é esta (saída real do gmock):

```
t3.cpp:19: Failure
Actual function call count doesn't match EXPECT_CALL(precos, preco_de(::testing::Eq("caneta")))...
         Expected: to be called once
           Actual: never called - unsatisfied and active
```

**Ações** mais usadas depois de `WillOnce` / `WillRepeatedly` / `WillByDefault`:
`Return(v)`, `ReturnRef(x)`, `Throw(e)`, `Invoke(f)` (chama uma função sua),
`DoAll(a1, a2)`, `SetArgPointee<0>(v)`.

### ⚠ A armadilha da ordem das regras

Descoberta na primeira execução de `test_mock.cpp`:

```cpp
ON_CALL(precos, preco_de(Eq("caneta"))).WillByDefault(Return(2.50));
ON_CALL(precos, preco_de(_)).WillByDefault(Return(std::nullopt));   // ⚠ genérica por último
```

Isto falha com `C++ exception with description "produto desconhecido: caneta"`. O gmock tenta as
regras da **mais recente para a mais antiga** — o `_`, declarado por último, é testado primeiro
e captura também a caneta. **A regra genérica precisa vir primeiro**, como no código do
repositório.

### Nice, Naggy e Strict

| envoltório | chamada não esperada |
|---|---|
| `NiceMock<M>` | ignorada em silêncio |
| `M` (o padrão, "naggy") | imprime um **aviso** no log; o teste **passa** |
| `StrictMock<M>` | **falha** o teste |

Recomendação prática: **`NiceMock` por padrão.** Um mock naggy enche o log de ruído; um strict
quebra o teste toda vez que a implementação ganha uma chamada legítima a mais. Use `StrictMock`
só quando "não chamar mais nada" **é** o requisito.

### Sequência, e "não chame mais"

```cpp
{
    ::testing::InSequence seq;      // dentro deste escopo, a ORDEM importa
    EXPECT_CALL(precos, preco_de(Eq("primeiro"))).WillOnce(Return(...));
    EXPECT_CALL(precos, preco_de(Eq("segundo"))).WillOnce(Return(...));
}

// Fecha as contas até aqui e recomeça com uma expectativa nova:
::testing::Mock::VerifyAndClearExpectations(&precos);
EXPECT_CALL(precos, preco_de(_)).Times(0);      // a partir daqui, NENHUMA consulta
```

### Quando mockar

**Mocke** quando a dependência é lenta, não determinística, tem efeito colateral irreversível,
é difícil de levar a um estado de erro, ou ainda não existe.

**Não mocke** tipos de valor, a biblioteca padrão, código seu que é rápido e determinístico, nem
tipos que você não possui — mocke a **sua** camada de adaptação em volta deles
(*"don't mock what you don't own"*).

> Regra de bolso: um teste com mais de dois ou três `EXPECT_CALL` está testando implementação.
> `test_mock.cpp` fecha com um caso que mostra o **fake** ganhando do mock exatamente por isso.

---

## Demo 6: Death tests — testar que o programa morre

### Por que existe

Nem todo erro deve virar exceção. `Carrinho::adicionar` tem uma **pré-condição**:

```cpp
void Carrinho::adicionar(const std::string& sku, int quantidade) {
    assert(quantidade > 0 && "quantidade precisa ser positiva");
    ...
    if (!preco) throw ProdutoDesconhecido{sku};      // ← erro de ENTRADA: exceção
```

Quantidade zero não é entrada inválida do usuário — é **bug do chamador**. A resposta certa a um
bug é **abortar**: continuar rodando com uma invariante quebrada é pior que parar. Produto
inexistente, sim, é entrada inválida, e vira exceção. A distinção é o assunto de
[`src/errors/`](../errors/).

### O mecanismo: `fork()`

Como testar que uma chamada aborta, se abortar mataria o próprio processo de teste? O gtest
resolve com **`fork()`** — a chamada do sistema que duplica o processo:

```
processo de teste ──fork()──┬── FILHO: executa a expressão, e morre
                            │
                            └── PAI: espera, e verifica
                                     • o código de saída (morreu?)
                                     • o texto que o filho escreveu em STDERR
```

```cpp
class CarrinhoDeathTest : public ::testing::Test {     // ⚠ o sufixo importa: veja (1) abaixo
protected:
    PrecosFixos precos;
};

#ifndef NDEBUG      // o assert some em release; o teste também

TEST_F(CarrinhoDeathTest, QuantidadeZeroAborta) {
    loja::Carrinho c{precos};

    //           EXPECT_DEATH(instrução, regex_aplicado_ao_stderr)
    EXPECT_DEATH(c.adicionar("caneta", 0), "quantidade precisa ser positiva");
}

// EXPECT_DEBUG_DEATH espera a morte em debug e apenas EXECUTA a instrução em
// release — é o par natural do `assert`, e dispensa o #ifndef manual.
TEST_F(CarrinhoDeathTest, FormaPortavelComDebugDeath) {
    loja::Carrinho c{precos};
    EXPECT_DEBUG_DEATH(c.adicionar("caneta", 0), "quantidade");
}

#else

TEST_F(CarrinhoDeathTest, SemAssercoesEmRelease) {
    GTEST_SKIP() << "compilado com NDEBUG: a pré-condição não está ativa";
}

#endif
```

O **segundo argumento é um regex** aplicado ao que o filho escreveu em `stderr`. É ele que
distingue "morreu pelo motivo certo" de "morreu por qualquer motivo".

### ⚠ Os quatro perigos

**(1) Threads.** `fork()` num processo com threads clona **só** a thread que chamou. Se outra
segurava um mutex, o filho trava para sempre. O gtest avisa: *"Death tests use fork(), which is
unsafe particularly in a threaded context"*. Daí a convenção: uma suíte cujo nome **termina em
`DeathTest`** é executada **antes** de todas as outras, reduzindo a chance de já haver threads.
Se ainda assim houver, troque o estilo:

```cpp
::testing::FLAGS_gtest_death_test_style = "threadsafe";   // reexecuta o BINÁRIO em vez de forkar
```

**(2) Efeitos colaterais não voltam.** O que a instrução faz acontece **no filho** e é
descartado:

```cpp
int x = 0;
EXPECT_DEATH({ x = 1; morre(); }, "");
EXPECT_EQ(x, 1);      // ❌ NUNCA passa: x ainda é 0 no pai
```

**(3) O regex.** É POSIX estendido no Linux e um subconjunto simples no Windows. `.` casa
qualquer caractere e parênteses agrupam — escreva `\\(` para casar um parêntese literal. Um
regex vazio `""` casa qualquer coisa, e aí o teste só verifica **que** morreu, não por quê.

**(4) Velocidade.** Cada death test é um `fork` mais uma execução. Num teste unitário que roda
em microssegundos, isso é caro. Use com parcimônia.

### O que NÃO é death test

Uma exceção é comportamento **normal e previsto**: o programa continua. Use `EXPECT_THROW`.

```cpp
EXPECT_THROW(c.adicionar("inexistente", 1), loja::ProdutoDesconhecido);
EXPECT_TRUE(c.vazio()) << "o carrinho sobreviveu à exceção";
```

---

## Demo 7: Cobertura, sanitizers e além

### ⚠ O que 100% de cobertura NÃO garante

- **Que as asserções existem.** `TEST(X, Cobre) { minha_funcao(1, 2); }` dá 100% e não verifica
  nada.
- **Que os valores certos foram testados.** Cobrir `if (n >= 10)` com n=5 e n=50 é 100% de ramo
  — e não descobre que o certo era `> 10`. As **fronteiras** encontram esse bug, e cobertura não
  pede por elas.
- **Que o código que falta existe.** Um `else` esquecido não aparece em relatório nenhum.
- **Ausência de UB.**

Use cobertura como **detector de buracos** ("este `catch` nunca rodou"), não como meta numérica
— que produz testes sem asserção escritos para satisfazer o gate do CI.

A métrica melhor: **teste de mutação**. A ferramenta altera o código (`>=` vira `>`) e verifica
se algum teste falha. Um mutante que sobrevive é um buraco real. Em C++: `mull`, `Dextool`.

### Os quatro tipos de cobertura

Para `if (a && b) faz_algo();`:

| tipo | o que mede | testes necessários |
|---|---|---|
| linha | a linha executou? | 1 |
| ramo | os dois lados do `if`? | 2 |
| condição | cada subcondição foi true **e** false? | `a` e `b` variando |
| caminho | todas as combinações | exponencial |

A maioria das ferramentas reporta **linha** por padrão — a métrica mais fácil e menos
informativa. **Peça ramo.**

### O pipeline de cobertura, copiável

Pelo Meson (o caminho curto neste repositório):

```bash
source build/conanbuild.sh
meson setup build-cov --buildtype=debug -Db_coverage=true \
      -Dpkg_config_path="$PWD/build" --prefix="$PWD/dist" --libdir="$PWD/dist/lib"
meson test -C build-cov
ninja -C build-cov coverage-html      # exige lcov+genhtml ou gcovr instalados
# relatório em: build-cov/meson-logs/coveragereport/index.html
```

À mão, para um único arquivo — útil para entender o que o Meson faz por baixo:

```bash
g++ --coverage -O0 -std=c++20 meu_teste.cpp meu_codigo.cpp -o prog   # gera .gcno
./prog                                                              # gera .gcda
gcov -b meu_codigo.cpp                       # -b = cobertura de RAMO, não só de linha
lcov --capture --directory . --output-file cov.info --rc branch_coverage=1
genhtml cov.info --output-directory html/ --branch-coverage
xdg-open html/index.html
```

Os três arquivos: `.gcno` sai da **compilação** (o mapa do código), `.gcda` sai da **execução**
(os contadores), e o `gcov`/`lcov` junta os dois.

### Testes sob sanitizer — o melhor retorno em C++

```bash
source build/conanbuild.sh
meson setup build-asan -Db_sanitize=address,undefined -Db_lundef=false \
      -Dpkg_config_path="$PWD/build" --prefix="$PWD/dist" --libdir="$PWD/dist/lib"
meson test -C build-asan
```

Os testes já exercitam o código. Rodá-los sob sanitizer transforma cada um num detector de
use-after-free, overflow, leak e UB — **sem escrever um teste a mais**.

| sanitizer | pega | custo |
|---|---|---|
| `address` | UAF, overflow, leak | ~2× |
| `undefined` | overflow, shift, alinhamento | ~1.2× |
| `thread` | data races | ~5–15× |

> ⚠ `address` e `thread` são **incompatíveis** entre si — dois jobs separados no CI.
>
> ⚠ `-Db_lundef=false` é necessário porque um binário instrumentado deixa símbolos do runtime do
> sanitizer indefinidos até o link final; sem desligar a checagem, o Meson recusa o link.

O detalhe do que cada um pega, medido: [`src/undefined_behavior/`](../undefined_behavior/).

### Além dos testes de exemplo

**Property-based:** em vez de "f(2,3) == 5", afirme uma **propriedade** que vale sempre e deixe a
ferramenta gerar centenas de entradas — idempotência, ida e volta
(`desserializa(serializa(x)) == x`), invariantes, oráculo. No carrinho: *"o total nunca é maior
que o subtotal"*. Biblioteca: RapidCheck.

**Fuzzing:** entradas geradas automaticamente e **guiadas por cobertura** — o fuzzer mede quais
ramos cada entrada alcançou e muta as que descobriram caminhos novos. Vale para tudo que recebe
entrada não confiável: parsers, decodificadores, protocolos.

```cpp
// Você NÃO escreve main(): o libFuzzer fornece o dele e chama esta função
// milhões de vezes com buffers de bytes que ele mesmo inventa.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    meu_parser(std::string_view{reinterpret_cast<const char*>(d), n});
    return 0;                       // 0 = "esta entrada não quebrou nada"
}
// clang++ -g -fsanitize=fuzzer,address fuzz.cpp -o fuzz && ./fuzz
```

---

## Como validar

```bash
make build                              # compila tudo e instala em dist/
./dist/bin/testing                      # a narrativa; termina em "✓ Todos os demos..."

meson test -C build                     # 6 alvos (5 daqui + saudacao de src/meson)
meson test -C build --list              # confere de onde vem cada alvo
meson test -C build testing_death -v    # confirma que os death tests rodam mesmo
```

O que se espera ver ao fim do `meson test -C build`:

```
1/6 academy / testing_basico        OK              0.01s
2/6 academy / testing_fixture       OK              0.01s
3/6 academy / testing_parametrizado OK              0.01s
4/6 academy / testing_mock          OK              0.01s
5/6 academy:guia / saudacao         OK              0.01s
6/6 academy / testing_death         OK              0.02s

Ok:                 6
Fail:               0
```

Contar os casos de um alvo, sem rodá-los:

```bash
./build/src/testing/testing_parametrizado --gtest_list_tests
```

Estado atual: **6/6 alvos passando, 41 casos neste subprojeto**, sem warnings com
`-Wall -Wextra -Wpedantic`.

Os death tests só existem quando `NDEBUG` **não** está definido (buildtype `debug`, que é o
padrão do `make`). Em release eles se auto-pulam com `GTEST_SKIP()` — testar um `assert`
desativado seria testar o compilador. Verificado nos dois modos.

## Referências

Neste repositório:

- [`src/meson/`](../meson/) — `test()`, `protocol: 'gtest'`, e um `main()` próprio
- [`src/undefined_behavior/`](../undefined_behavior/) — o que cada sanitizer pega, medido
- [`src/solid/dip/`](../solid/dip/) — a inversão de dependência que torna o teste possível
- [`src/errors/`](../errors/) — erro vs bug: quando exceção, quando `assert`
- [`src/linkage/`](../linkage/) — por que `#define private public` é UB
- [`src/chrono/`](../chrono/) — injetar o relógio, e como cronometrar

Fora dele:

- [GoogleTest Primer](https://google.github.io/googletest/primer.html) — o começo oficial
- [GoogleTest Advanced Guide](https://google.github.io/googletest/advanced.html) — `TEST_P`,
  `TYPED_TEST`, death tests, flags
- [gMock for Dummies](https://google.github.io/googletest/gmock_for_dummies.html) e o
  [gMock Cookbook](https://google.github.io/googletest/gmock_cook_book.html)
- [Assertions Reference](https://google.github.io/googletest/reference/assertions.html) — o
  catálogo completo
- [RapidCheck](https://github.com/emil-e/rapidcheck) — property-based testing em C++
- [libFuzzer](https://llvm.org/docs/LibFuzzer.html) — o fuzzer guiado por cobertura do LLVM
- Gerard Meszaros, *xUnit Test Patterns* — a taxonomia dos dublês
- Michael Feathers, *Working Effectively with Legacy Code* — como testar o que não foi feito
  para isso
