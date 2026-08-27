# errors — Exceções, Garantias e Erros como Valores

## O que é este projeto?

Guia sobre **tratamento de erro em C++**: o mecanismo das exceções, os quatro níveis de
garantia que toda função oferece (mesmo sem você escolher qual), e as duas alternativas
baseadas em valor — `std::error_code` e `std::expected<T,E>`.

> ⚠ Este subprojeto compila com **C++23** por causa de `std::expected`. O padrão da raiz
> continua `c++20`; a exceção é local, via `override_options` no
> [meson.build](meson.build).

```bash
make build          # configura + compila + instala em dist/
./dist/bin/errors
```

Saída final esperada: `✓ Todos os demos de tratamento de erro concluídos!`

---

## Antes de começar

Esta seção define, do zero, cada termo que o resto do documento usa. Se você já escreve
`try`/`catch` no dia a dia, pule para "Por que estudar isto?".

### O vocabulário mínimo: `throw`, `try`, `catch`

Programa completo e compilável — três elementos, três papéis:

```cpp
#include <stdexcept>
#include <cstdio>

int dividir(int a, int b) {
    if (b == 0) throw std::invalid_argument("divisor zero");   // quem LANÇA
    return a / b;
}

int main() {
    try {                                                      // quem se OFERECE p/ tratar
        std::printf("10/2 = %d\n", dividir(10, 2));
        std::printf("10/0 = %d\n", dividir(10, 0));
        std::printf("esta linha nunca roda\n");
    } catch (const std::exception& e) {                         // quem TRATA
        std::printf("capturado: %s\n", e.what());
    }
    std::printf("o programa continua\n");
}
```

```
10/2 = 5
capturado: divisor zero
o programa continua
```

Três fatos que o exemplo já mostra:

- o `throw` **abandona** a função na hora — `return a / b` nunca executa, e o `printf` da
  linha seguinte no `main` também não;
- o controle salta para o `catch` compatível mais próximo **pilha acima**, atravessando
  quantas funções forem necessárias;
- depois do bloco `catch`, a execução segue normalmente. Uma exceção tratada não encerra o
  programa.

`e.what()` devolve um `const char*` com a mensagem. `std::invalid_argument` é um dos tipos
prontos de [`<stdexcept>`](#demo-3-a-hierarquia-padrão) — mas você pode lançar qualquer tipo.

### RAII, em duas frases

**RAII** (*Resource Acquisition Is Initialization*) é a técnica que faz o C++ dispensar
`finally`: todo recurso — memória, arquivo, lock, socket — vira **membro de um objeto**, e o
**destrutor** desse objeto devolve o recurso. Como o destrutor de todo objeto local roda
durante o desenrolamento da pilha, o recurso volta **mesmo quando a função sai por uma
exceção**, sem uma linha de limpeza escrita à mão.

É por isso que exceções e RAII são o mesmo assunto: sem RAII, cada `throw` seria um
vazamento. Detalhes em [`src/smart-pointers/`](../smart-pointers/) e
[`src/memory/`](../memory/).

### Desenrolamento da pilha (*stack unwinding*)

O nome do passo em que o runtime volta pilha acima procurando um handler, destruindo pelo
caminho todos os objetos locais das funções abandonadas, **na ordem inversa da construção**.
É o Demo 1.2, e o único motivo de RAII funcionar.

### `noexcept` — a sintaxe

Aparece **depois** da lista de parâmetros, antes do corpo:

```cpp
void f() noexcept;                                   // promete não lançar
void g() noexcept(true);                             // idêntico ao de cima
void h() noexcept(false);                            // pode lançar (o padrão)

template <typename T>
void trocar(T& a, T& b) noexcept(std::is_nothrow_move_constructible_v<T>);  // condicional

static_assert(noexcept(f()));    // `noexcept(expr)` também é um OPERADOR que devolve bool
```

A forma condicional é a que a biblioteca padrão usa: "sou `noexcept` **se** o meu `T` for".

### `std::terminate`, sem eufemismo

`std::terminate()` é a função que o runtime chama **quando desiste**. Por padrão ela chama
`std::abort()`: o processo morre com `SIGABRT`.

É chamada quando:

- uma exceção escapa sem encontrar nenhum handler;
- um destrutor lança **durante** o unwinding de outra exceção (duas exceções em voo);
- uma função marcada `noexcept` lança;
- uma exceção escapa da função de uma `std::thread`;
- uma exceção escapa do construtor ou do destrutor de um objeto de duração estática.

O detalhe que dá peso à ameaça: **a pilha pode não ser desenrolada**. O padrão permite ao
runtime chamar `terminate` no ponto do `throw`, sem rodar destrutor nenhum. Na prática:
arquivos não são fechados, locks não são soltos, buffers não são gravados. Não é "o programa
sai com erro", é "o programa some".

Dá para trocar o comportamento com `std::set_terminate(minha_funcao)` — útil para logar algo
antes de morrer. O que **não** dá é continuar: a função instalada não pode retornar; se
retornar, `abort()` acontece do mesmo jeito.

### `[[nodiscard]]`

Atributo que faz o compilador avisar quando o valor de retorno de uma função é **ignorado**.
Aparece no fim do documento porque `std::expected` e `std::error_code` **não** o trazem de
fábrica.

---

## Por que estudar isto?

Porque "usar exceção ou valor de retorno" não é questão de gosto — é uma decisão de
engenharia com números por trás. Medido pelo próprio demo (`./dist/bin/errors`, seção 1.6;
build **debug**, GCC 13, x86-64):

```
20000 iterações:
  sem exceção        :      12 µs
  lançando+capturando:   12525 µs      ← ~1000×
```

Três execuções seguidas na mesma máquina deram 12525, 12386 e 12400 µs contra 11–12 µs do
caminho feliz: o número exato varia, a ordem de grandeza não. **~1000× por lançamento.**

### O que "zero-cost" quer dizer, e o que não quer

A implementação moderna é *table-driven*: o compilador emite tabelas que dizem, para cada
faixa de endereços, quais objetos destruir e qual handler existe. Nada disso é consultado
enquanto tudo dá certo — não há teste, não há flag, não há registro de handler em runtime.
Daí "zero-cost".

> ⚠ **Zero-cost refere-se ao TEMPO no caminho feliz — não ao espaço.** As tabelas ficam no
> binário e não são pagináveis para fora enquanto o processo desenrola. Medido neste próprio
> executável:
>
> ```console
> $ size -A dist/bin/errors | grep -E 'eh_frame|except_table|^\.text'
> .text                 268642    27424
> .eh_frame_hdr          13196   321256
> .eh_frame              55124   334456
> .gcc_except_table       3960   389580
> ```
>
> (a primeira coluna é o tamanho em bytes; a segunda é o endereço de carga, que muda a cada
> build.)
>
> 72.280 bytes de tabelas para 268.642 bytes de código — cerca de **27% do tamanho do
> `.text`**. (`.eh_frame` também serve a *backtraces* e *profilers*, e existe mesmo sem
> exceções no x86-64; `.gcc_except_table` é a parte que só existe por causa dos handlers.)

É esse custo em espaço, e não o tempo, que faz projetos embarcados compilarem com
`-fno-exceptions`. E aí o mundo muda: `new` passa a devolver `nullptr` em vez de lançar
`bad_alloc`, `std::vector::at` aborta em vez de lançar `out_of_range`, construtores perdem a
única forma de sinalizar falha — e `std::expected` deixa de ser alternativa e vira a **única**
opção. Ver [Como validar](#como-validar) para ver isso na prática.

---

## Estrutura de arquivos

```
src/errors/
├── main.cpp                     ← ponto de entrada; catch(...) na fronteira do programa
├── demo_exceptions.hpp          ← 1 · throw/catch, unwinding, rethrow, exception_ptr, custo
├── demo_exception_safety.hpp    ← 2 · as 4 garantias, copy-and-swap, noexcept, push_back
├── demo_hierarchy.hpp           ← 3 · std::exception, exceção customizada, nested
├── demo_error_code.hpp          ← 4 · error_code, categoria própria, system_error
├── demo_expected.hpp            ← 5 · std::expected<T,E> (C++23) e encadeamento monádico
├── meson.build                  ← declara o alvo `errors` com override_options cpp_std=c++23
└── README.md
```

---

## Demo 1: O Mecanismo

### O que acontece num `throw`

1. o objeto lançado é copiado/movido para uma área controlada pelo runtime
2. a pilha é **desenrolada** até o primeiro handler compatível
3. durante o desenrolamento, **todo destrutor de objeto local roda**
4. o handler executa; ao terminar, o objeto da exceção é destruído

O passo 3 é o motivo de RAII e exceções serem o mesmo assunto:

```
+ nível-1 construído
+ nível-2 construído
+ nível-3 construído
nível-3 vai lançar...
- nível-3 DESTRUÍDO (durante o unwinding)
- nível-2 DESTRUÍDO (durante o unwinding)
- nível-1 DESTRUÍDO (durante o unwinding)
handler alcançado: erro originado no nível 3
```

Três destrutores rodaram sem uma linha de limpeza escrita à mão.

### Ordem dos handlers

Os handlers são testados **em ordem**, de cima para baixo — o primeiro compatível vence.
Não há "melhor match" como na resolução de overload. Por isso o handler mais derivado vem
primeiro.

### Capture por referência

```cpp
catch (std::runtime_error e)   // ✗ cópia + object slicing
catch (const ErroDeRede& e)    // ✓
```

Capturar por valor **corta** a exceção (*object slicing*): a cópia guarda só a subparte
base — o tipo dinâmico e os campos do derivado são descartados. Um `catch (std::runtime_error e)`
que recebe um `ErroDeRede` perde o campo `codigo` e o `what()` sobrescrito, e você trata um
erro genérico sem saber que ele era específico. É o mesmo mecanismo de
[oop/demo_slicing.hpp](../oop/demo_slicing.hpp). Medido no demo 1.3:

```
[por valor]      serviço indisponível — e o código HTTP se perdeu
[por referência] [HTTP 503] serviço indisponível
```

O GCC pega o caso com `-Wcatch-value`, que já vem dentro de `-Wall`.

Lançar, por outro lado, é sempre por valor: `throw T(...)`, nunca `throw new T(...)`.

### `throw;` vs `throw e;`

```cpp
catch (const std::exception&) {
    throw;      // ✓ relança O MESMO objeto, tipo dinâmico intacto
    throw e;    // ✗ lança uma CÓPIA cortada para o tipo estático
}
```

```
com `throw;`     → chegou como ErroDeRede: [HTTP 404] não encontrado
com `throw e;`   → chegou degradado como std::exception: 'std::exception'
```

### Atravessando fronteiras com `std::exception_ptr`

Uma exceção não atravessa sozinha a fronteira de uma thread: se ela escapa da função passada
a uma `std::thread`, o resultado é `std::terminate` imediato. A solução do padrão é
**empacotar** a exceção num valor copiável e relançá-la do outro lado:

```cpp
std::exception_ptr guardada;
try {
    throw ErroDeRede(500, "erro interno");
} catch (...) {
    guardada = std::current_exception();     // empacota a exceção ATIVA
}

// ...tempo depois, possivelmente em outra thread...
try {
    if (guardada) std::rethrow_exception(guardada);
} catch (const ErroDeRede& e) {
    fmt::print("tipo intacto: {}\n", e.detalhe());
}
```

```
catch(...) pegou algo que não é std::exception
exceção guardada num std::exception_ptr
...tempo depois, possivelmente em outra thread...
relançada e capturada com o tipo intacto: [HTTP 500] erro interno
```

Repare no que sobrevive: `rethrow_exception` relança o **mesmo objeto**, com o tipo dinâmico
intacto — o `catch (const ErroDeRede&)` do outro lado pega, e não um `std::exception` cortado.

- `std::current_exception()` só faz sentido **dentro** de um handler; fora, devolve um
  `exception_ptr` nulo.
- `std::exception_ptr` é copiável, contado por referência, e pode ser guardado em contêiner.
- `std::make_exception_ptr(e)` cria um sem precisar lançar.

> ⚠ Você raramente precisa escrever isso à mão. `std::async`, `std::packaged_task` e
> `std::promise` já fazem esse empacotamento: a exceção lançada na tarefa é guardada num
> `exception_ptr` dentro do `future` e **relançada no `.get()`** de quem espera. Ver
> [`src/concurrency/`](../concurrency/). Escreva o `exception_ptr` você mesmo apenas quando
> for uma `std::thread` crua ou uma fronteira com C.

---

## Demo 2: As Quatro Garantias

| Garantia | Promessa |
|---|---|
| **nenhuma** | nada — pode vazar ou corromper |
| **básica** | não vaza, invariantes valem, valores não especificados |
| **forte** | ou completou, ou nada mudou (commit-or-rollback) |
| **nothrow** | não lança, ponto |

Mire na **básica** sempre; na **forte** quando o custo permitir.

### A ordem das operações já muda a garantia

```cpp
void ruim(const std::string& k) {
    ++contador_;                                    // ✗ estado mudou
    itens_.push_back(std::make_unique<...>(k));     //   e isto pode lançar
}

void bom(const std::string& k) {
    auto novo = std::make_unique<...>(k);           // ✓ o arriscado primeiro
    itens_.push_back(std::move(novo));
    ++contador_;                                    //   o nothrow por último
}
```

### Garantia forte via copy-and-swap

Faça o trabalho arriscado numa **cópia**; só quando tudo der certo, `swap` (que é nothrow):

```
forte após 2 operações: saldo=200 contador=2
falhou no meio: operação inválida: INVÁLIDA
forte APÓS a falha:     saldo=200 contador=2   ← inalterado
```

### Destrutores não podem lançar

Se um destrutor lançar durante o unwinding de outra exceção, há **duas exceções em voo** —
o runtime não tem como escolher qual propagar, e o padrão responde com `std::terminate`.

Por isso, desde o C++11, **todo destrutor é implicitamente `noexcept`**. A precisão que falta
na frase curta: a dedução é `noexcept(true)` *desde que* nenhuma base e nenhum membro tenha
destrutor potencialmente lançante — e você pode sobrescrever a dedução escrevendo
`~T() noexcept(false)`. Só que fazer isso não resolve o problema: continua sendo `terminate`
se a exceção sair durante o unwinding de outra. O `noexcept(false)` apenas move a explosão
para o segundo caso.

Padrão correto: **engolir (e logar) no destrutor**, e oferecer um `fechar()` público para
quem quiser tratar o erro de verdade — o mesmo desenho de `std::fstream::close()`.

```
[dtor] 'relatorio.txt' fechado normalmente
[dtor] erro engolido ao fechar 'corrompido': falha ao gravar o buffer final
```

### O contrato do `noexcept`

`noexcept` **não impede** a função de lançar — ele **promete** que ela não lança. Quebrar a
promessa não gera unwinding nem handler: `std::terminate` direto.

Em troca, a biblioteca otimiza. `std::vector` só usa o move ctor ao realocar se ele for
`noexcept` — ver [value_semantics/demo_rule_of_five.hpp](../value_semantics/demo_rule_of_five.hpp).

Marque `noexcept` em: construtores/atribuições de **move**, `swap`, destrutores, getters
triviais. (A sintaxe está em [Antes de começar](#noexcept--a-sintaxe).)

E por que `swap` é dito *nothrow*? Porque um `swap` bem escrito **só troca ponteiros e
escalares** — ele não aloca, não copia elemento nenhum, não chama nada que possa falhar. É
essa propriedade, e não uma promessa vazia, que torna o *copy-and-swap* acima uma garantia
forte: o único passo que muda o estado é justamente o passo que não pode falhar.

### Estudo de caso: `std::vector::push_back`

Este é o ponto em que `noexcept` deixa de ser detalhe de estilo e vira desempenho medido.

`push_back` oferece **garantia forte**: se ele lançar, o vector fica exatamente como estava.
Quando a capacidade acaba, ele precisa alocar um buffer novo e transferir os elementos — e aí
enfrenta um dilema:

| Como transferir | Velocidade | O que acontece se falhar no meio |
|---|---|---|
| **mover** cada elemento | rápido | o buffer antigo já foi esvaziado; não há como voltar → só garantia **básica** |
| **copiar** cada elemento | lento | o original continua intacto → dá para desfazer → garantia **forte** |

A biblioteca resolve com `std::move_if_noexcept`: **move se o move ctor do seu tipo for
`noexcept`; copia caso contrário**. Ou seja, o vector só abre mão da cópia quando o move
promete não falhar.

```
antes da realocação: size=2 capacity=2
após  a realocação: size=3 capacity=4
std::string tem move noexcept? true
→ o vector MOVEU os elementos. Se não fosse noexcept,
  ele teria COPIADO, para poder desfazer em caso de falha.
```

A frase que amarra o subprojeto inteiro: **um move constructor sem `noexcept` torna o seu
tipo mensuravelmente mais lento dentro de um `std::vector`** — o compilador não reclama, o
teste passa, e você paga uma cópia por elemento a cada realocação. Ver
[value_semantics/demo_rule_of_five.hpp](../value_semantics/demo_rule_of_five.hpp).

---

## Demo 3: A Hierarquia Padrão

```
std::exception
├── std::logic_error         ← erro de PROGRAMAÇÃO (o chamador errou)
│   ├── invalid_argument, domain_error, length_error, out_of_range
├── std::runtime_error       ← erro de EXECUÇÃO (o mundo errou)
│   ├── range_error, overflow_error, underflow_error, system_error
├── std::bad_alloc
└── std::bad_cast, bad_optional_access, bad_variant_access, ...
```

A pergunta que separa os dois ramos: *"isto poderia ter sido evitado por quem chamou,
verificando antes?"*

- **sim** → `logic_error`. É bug. Não capture em produção; corrija o código.
- **não** → `runtime_error`. É o mundo. Trate, tente de novo, faça fallback.

### Exceção customizada com contexto

```cpp
class ErroDeConfiguracao : public std::runtime_error {
public:
    ErroDeConfiguracao(std::string arq, int linha, const std::string& detalhe)
        : std::runtime_error(fmt::format("{}:{}: {}", arq, linha, detalhe)),
          arquivo_(std::move(arq)), linha_(linha) {}

    const std::string& arquivo() const noexcept;
    int                linha()   const noexcept;
};
```

Derive de `std::runtime_error`: ele já gerencia a string do `what()`, que precisa
sobreviver ao objeto lançado.

> ⚠ Não guarde uma `std::string` e devolva `.c_str()` de um `what()` que monta a mensagem
> na hora — o buffer morreria com o temporário.

### Uma raiz por domínio

```cpp
struct ErroDaAcademia : std::runtime_error { using std::runtime_error::runtime_error; };
struct ErroDeParse    : ErroDaAcademia     { using ErroDaAcademia::ErroDaAcademia; };
struct ErroDeRede     : ErroDaAcademia     { int codigo; /* ... */ };
```

Permite `catch (const ErroDaAcademia&)` — pega tudo que é seu sem pegar um `bad_alloc` do
sistema.

### `std::nested_exception`

Cada camada acrescenta contexto sem apagar a causa:

```
falha ao iniciar o servidor na porta 443
  não foi possível carregar o certificado TLS
    arquivo '/etc/ssl/cert.pem' não encontrado
```

Feito com `std::throw_with_nested` na subida e `std::rethrow_if_nested` na leitura.

### Anti-padrões

| ✗ | Em vez disso |
|---|---|
| `throw 42;` | lance tipos, não valores nus |
| `throw new MinhaExcecao();` | vaza — lance por valor |
| `catch (MinhaExcecao e)` | `catch (const MinhaExcecao&)` |
| `catch (...) { }` vazio | não engula silenciosamente |
| `throw e;` para repassar | `throw;` |
| exceção escapando do destrutor | engula e logue |
| exceção escapando de uma thread | `std::terminate` — use `exception_ptr` |

`catch(...)` é legítimo em **exatamente três lugares**: no `main`, na função de uma thread,
e numa fronteira com C. Em qualquer outro, você está escondendo um bug.

---

## Demo 4: `std::error_code`

Um par (valor inteiro, categoria) que identifica um erro de forma extensível, sem alocar,
sem lançar, e comparável entre bibliotecas diferentes.

### Um `error_code` funcionando, antes de qualquer teoria

Programa completo — nenhuma categoria própria, nenhum `try`:

```cpp
#include <filesystem>
#include <system_error>
#include <cstdio>

int main() {
    std::error_code ec;                                          // começa "sem erro"
    auto n = std::filesystem::file_size("/nao/existe", ec);      // preenche ec em vez de lançar
    if (ec) std::printf("erro: %s (categoria=%s, valor=%d)\n",
                        ec.message().c_str(), ec.category().name(), ec.value());
    else    std::printf("tamanho = %zu\n", static_cast<size_t>(n));
}
```

```
erro: No such file or directory (categoria=generic, valor=2)
```

Três membros e é isso: `value()` é o número (aqui, o `ENOENT` do POSIX), `category()` diz
**de quem** é esse número, e `message()` traduz para texto. O par (valor, categoria) é o que
torna o `2` da sua biblioteca distinguível do `2` da minha.

| Conceito | Significa |
|---|---|
| `error_code` | o que **aconteceu** — específico da plataforma/biblioteca |
| `error_condition` | o que **significa** — portável |

> ⚠ A polaridade do `bool` é invertida em relação ao `expected`:
> `if (ec)` significa **houve erro**. Um `std::error_code` recém-construído é
> `false` — "deu certo".

> ⚠ Quando a função devolve um valor **e** preenche o `ec`, o valor devolvido em caso de erro
> não tem significado. No demo 4.5, `fs::file_size` falho devolve
> `18446744073709551615` (`static_cast<uintmax_t>(-1)`). Teste o `ec` **antes** de olhar para
> o retorno.

### Categoria customizada em 3 passos

```cpp
// (a) o enum
enum class ErroDeParse { ok = 0, token_inesperado, fim_prematuro, /* ... */ };

// (b) a categoria singleton
class CategoriaDeParse : public std::error_category {
    const char* name() const noexcept override { return "parse"; }
    std::string message(int ev) const override { /* switch */ }
    std::error_condition default_error_condition(int ev) const noexcept override;
};

// (c) registra o enum
template <> struct std::is_error_code_enum<ErroDeParse> : std::true_type {};
```

O `default_error_condition` é o que faz a mágica da interoperabilidade:

```
nosso.category()  = parse
nosso.message()   = número mal formado
nosso == std::errc::invalid_argument ? true    ← código genérico entende
```

É assim que Asio, `<filesystem>` e o seu código coexistem.

### A API dupla do `<filesystem>`

Toda função tem duas assinaturas — uma que lança, outra que recebe `std::error_code&`:

```cpp
auto t = fs::file_size(p, ec);   // não lança; teste `ec`
auto t = fs::file_size(p);       // lança fs::filesystem_error
```

Escolha pela **frequência esperada** do erro.

---

## Demo 5: `std::expected<T, E>` (C++23)

`optional<T>` diz "não tem valor" mas não diz por quê. `error_code` diz o porquê mas não
carrega o valor. `expected<T,E>` faz as duas coisas.

```cpp
std::expected<int, Falha> para_inteiro(std::string_view s) noexcept {
    if (s.empty()) return std::unexpected{Falha::vazio};
    // ...
    return valor;                      // sucesso: conversão implícita
}
```

> A polaridade aqui é a natural: `if (r)` significa **sucesso**.

### Encadeamento monádico — o ganho real

*Monádico*, aqui, quer dizer só uma coisa: **cada etapa recebe o SUCESSO da anterior, e uma
falha em qualquer ponto pula todas as etapas seguintes sem que você escreva um único `if`.**
O `expected` que carrega o erro simplesmente atravessa o resto do pipeline intocado.

```cpp
para_inteiro(entrada)
    .and_then(exigir_positivo)                  // T → expected<U,E>
    .transform([](int v) { return v * 2; })      // T → U  (só o sucesso)
    .transform([](int v) { return fmt::format("resultado={}", v); });
```

```
'21   ' → resultado=42
'-5   ' → falhou: negativo não permitido aqui
'abc  ' → falhou: caractere não numérico
```

Nenhum `if` no meio do pipeline: a primeira falha curto-circuita todas as etapas seguintes.

| Operação | Recebe | Devolve | Papel |
|---|---|---|---|
| `and_then(f)` | `T` | `expected<U,E>` | encadeia |
| `transform(f)` | `T` | `U` | mapeia o sucesso |
| `or_else(f)` | `E` | `expected<T,G>` | recupera |
| `transform_error(f)` | `E` | `G` | traduz o erro |

### As formas lado a lado

| Forma | Motivo do erro | Custo | Ignorável? |
|---|---|---|---|
| `optional<T>` | não | zero | sim |
| exceção | sim, rico | alto | não |
| out-param + `bool` | via bool/enum | zero | sim, e silenciosamente |
| `error_code` | sim, extensível | zero | sim |
| `expected<T,E>` | sim, **tipado** | zero | sim* |

> \* **`std::expected` não é `[[nodiscard]]` no padrão** — ignorar o retorno compila sem um
> pio. Marque você as suas funções:
> `[[nodiscard]] std::expected<int,Falha> para_inteiro(...);`

---

## Regras de ouro

| O erro é... | Use |
|---|---|
| esperado e frequente | `expected` ou `error_code` |
| excepcional e raro | exceção |
| bug do chamador | `assert` / `logic_error` |
| irrecuperável | `std::terminate` / `abort` |

| Situação | Faça |
|---|---|
| Capturando | `catch (const T&)` — sempre |
| Repassando | `throw;` — nunca `throw e;` |
| Lançando | por valor — nunca `new` |
| Escrevendo um move ctor | `noexcept` |
| Escrevendo um destrutor | engula os erros |
| Erro atravessa muitas camadas | exceção (propagar `expected` muda toda assinatura) |
| Falha em construtor ou operador | exceção (não há retorno disponível) |
| Função devolve `expected`/`error_code` | marque-a `[[nodiscard]]` |

---

## Como validar

### Build do repositório

```bash
make build            # conan install + meson setup em build/ + compile + install em dist/
./dist/bin/errors
```

O diretório de **build** é `build/`; `dist/` é o **prefixo de instalação**. O binário
instalado fica em `./dist/bin/errors`.

Recompilando só este alvo — o `source` não é opcional, é ele que põe o `fmt` do Conan no
`PKG_CONFIG_PATH`:

```bash
source build/conanbuild.sh
meson compile -C build errors
./build/src/errors/errors        # binário do build dir, antes do install
```

### Compilação isolada, sem Meson

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/errors/main.cpp -o /tmp/err -L$FMTP/lib -lfmtd && /tmp/err
```

Estado atual: **sem nenhum warning** com `-Wall -Wextra -Wpedantic`. Note o `-std=c++23`:
com `c++20` o `<expected>` do demo 5 não existe.

### Sob ASan + UBSan

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/errors/main.cpp -o /tmp/errsan -L$FMTP/lib -lfmtd && /tmp/errsan
```

Estado atual: **0 achados**. Isto importa aqui mais do que em outros subprojetos — o teste de
verdade das garantias de exceção é "nada vazou quando a pilha desenrolou", e é exatamente
isso que o ASan mede.

> Se preferir um build dir do Meson com sanitizers, o `source` também é obrigatório, senão o
> `meson setup` falha em `dependency('fmt')`:
> ```bash
> source build/conanbuild.sh
> meson setup build-asan -Db_sanitize=address,undefined .
> meson compile -C build-asan errors
> ./build-asan/src/errors/errors
> ```

### Ver o `-Wcatch-value` disparar

O demo 1.3 **suprime** o aviso de propósito, com um `#pragma GCC diagnostic ignored`, porque
capturar por valor ali é a própria lição. Para ver o aviso ao vivo, use um arquivo à parte:

```bash
cat > /tmp/cv.cpp <<'EOF'
#include <stdexcept>
void f() {
    try { throw std::runtime_error("x"); }
    catch (std::runtime_error e) { (void)e; }
}
EOF
g++ -std=c++23 -Wall -Wextra -fsyntax-only /tmp/cv.cpp
```

Esperado:

```
warning: catching polymorphic type 'class std::runtime_error' by value [-Wcatch-value=]
```

### Ver o que `-fno-exceptions` derruba

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -fno-exceptions -I$FMTP/include -fsyntax-only src/errors/main.cpp
```

Esperado: **falha**, e a primeira mensagem já explica o mundo do embarcado —

```
error: exception handling disabled, use '-fexceptions' to enable
   45 |         throw std::domain_error(fmt::format("divisão por zero: {}/{}", a, b));
```

Todo `throw`, todo `try` e todo `catch` deixam de existir. O que continua compilando são
exatamente os demos 4 e 5: `std::error_code` e `std::expected`.

### Medir o custo em espaço das tabelas de unwinding

```bash
size -A dist/bin/errors | grep -E 'eh_frame|except_table|^\.text'
```

Os números desta máquina estão em [Por que estudar isto?](#por-que-estudar-isto).

Saída esperada ao final da execução: `✓ Todos os demos de tratamento de erro concluídos!`

---

## Referências

Dentro deste repositório:

- [`src/smart-pointers/`](../smart-pointers/) — `unique_ptr`/`shared_ptr`: o RAII que torna
  o unwinding seguro
- [`src/value_semantics/`](../value_semantics/) — a regra dos cinco e por que o move ctor
  precisa ser `noexcept`
- [`src/vocabulary_types/`](../vocabulary_types/) — `optional`, `variant`, `expected` lado a
  lado
- [`src/concurrency/`](../concurrency/) — como `future`/`promise` transportam a exceção entre
  threads
- [`src/oop/demo_slicing.hpp`](../oop/demo_slicing.hpp) — o *object slicing* que estraga um
  `catch` por valor
- [`src/testing/`](../testing/) — como escrever testes que verificam a garantia forte
- [`src/undefined_behavior/`](../undefined_behavior/) — o que acontece quando ninguém
  sinaliza erro nenhum

Fontes canônicas:

- **David Abrahams**, *Exception-Safety in Generic Components* (1998) — o artigo que criou os
  termos "básica", "forte" e "nothrow" usados no Demo 2
- **Herb Sutter**, *GotW #47: Uncaught Exceptions* e *Guru of the Week* sobre destrutores —
  por que um destrutor que lança é indefensável
- **Herb Sutter**, **P0709**, *Zero-overhead deterministic exceptions: throwing values* — a
  análise mais completa do custo real das exceções e do porquê de tanto código as desligar
- *C++ Core Guidelines*, seção **E (Error handling)** — em especial E.2, E.6 (RAII), E.14
  (lance tipos derivados de `std::exception`), E.15 (capture por referência), E.16
  (destrutores nunca falham) e E.30 (não use especificações de exceção)
- cppreference — `<exception>`, `<stdexcept>`, `<system_error>` e `<expected>`
