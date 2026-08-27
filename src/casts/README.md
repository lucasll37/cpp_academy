# casts — as cinco formas de converter um tipo em outro

## O que é este projeto?

Um programa que executa lado a lado os **cinco mecanismos de conversão de tipo** do C++ —
`static_cast`, `dynamic_cast`, `const_cast`, `reinterpret_cast` e o cast herdado de C `(T)x` —
mostrando o que cada um verifica, o que cada um **não** verifica, e onde cada um vira
comportamento indefinido.

```bash
make build && ./dist/bin/casts          # ou, só este alvo:
source build/conanbuild.sh && meson compile -C build casts && ./dist/bin/casts
```

Saem cinco seções numeradas (`1 · static_cast` … `5 · C-style cast`) e, ao final,
`✓ Todos os demos de cast concluídos!`.

## Antes de começar: os conceitos que você precisa

Nada aqui pressupõe mais do que C++ básico — sintaxe, funções, classes, herança.

### Objeto, tipo e representação

Um **objeto** é uma região de memória. Um **tipo** é a interpretação daquela região: quantos
bytes, o que os bits significam, que operações são legais.

```
memória:        [ 00 ][ 00 ][ 80 ][ 3F ]     ← 4 bytes, sempre os mesmos
como float:                1.0f
como uint32_t:      1065353216 (0x3F800000)
```

Converter é escolher entre duas coisas muito diferentes: **mudar o valor mantendo o
significado** (`1.0f` vira o inteiro `1`, e os bytes viram `01 00 00 00` — é o `static_cast`) ou
**manter os bytes e mudar o significado** (os mesmos `00 00 80 3F` lidos como `uint32_t` — é o
`reinterpret_cast`). Confundir as duas é a origem da maioria dos bugs de cast.

### As conversões que acontecem sozinhas

A **maioria** das conversões em C++ não precisa de cast nenhum:

| Conversão implícita | Exemplo | Perde algo? |
|---|---|---|
| promoção integral | `short → int` | não |
| conversões aritméticas usuais | `int + double → double` | não |
| ponto flutuante → inteiro | `double → int` | **sim**, trunca |
| derivado → base (*upcast*) | `Cachorro* → Animal*` | não |
| array → ponteiro (*decay*) | `int[5] → int*` | perde o tamanho |
| construtor não-`explicit` | `std::string s = "oi";` | não |
| operador de conversão | `if (ptr)` chama `operator bool()` | não |

Um cast é o que você escreve quando a conversão implícita **se recusou**, ou quando ela
aconteceria com perda e você quer registrar por escrito que a perda é intencional.

> **Regra que vale para o documento inteiro:** se precisou de cast, pergunte primeiro *por que
> a conversão implícita se recusou*. A resposta quase sempre aponta um problema de desenho — e
> o cast só o esconde.

### Narrowing

**Narrowing** é a conversão que pode não caber (`double → int`, `int → uint8_t`). É legal com
`=` e apenas avisada; a inicialização com **chaves** a proíbe — o motivo número um para
preferi-la (veja [`src/syntax/`](../syntax/)).

```cpp
double pi = 3.14;
int a = pi;      // warning: -Wfloat-conversion   → a == 3
int b{pi};       // warning: -Wnarrowing (vira erro com -pedantic-errors)
```

### Upcast, downcast, crosscast

```
                 Veiculo
                 /     \
             Carro     Moto              upcast    = derivado → base   (subindo)
              /  \                       downcast  = base → derivado   (descendo)
   CarroEletrico  \                      crosscast = ramo → ramo       (atravessando)
                   `- CarroVoador -- Voador      (herança múltipla)
```

**Upcast** é sempre seguro (todo `Carro` *é um* `Veiculo`) e implícito. **Downcast** é uma
aposta: nem todo `Veiculo` é `Carro`. **Crosscast** atravessa ramos independentes de herança
múltipla — **só `dynamic_cast` consegue**; `static_cast` nem compila, pois não há relação
direta entre `Veiculo` e `Voador`.

### vptr, vtable e RTTI, de verdade

Quando uma classe tem ao menos um método `virtual`, cada objeto ganha um ponteiro escondido, o
**vptr**, apontando para uma tabela por classe, a **vtable**:

```
objeto Carro na memória          vtable de Carro (uma por CLASSE)
┌───────────────────┐            ┌──────────────────────────────┐
│ vptr ─────────────┼───────────►│ &type_info de Carro          │ ← é aqui que o RTTI mora
│ num_portas = 4    │            │ offset-to-top = 0            │
└───────────────────┘            │ &Carro::~Carro, &Carro::nome │
                                 └──────────────────────────────┘
```

**RTTI** (*Run-Time Type Information*) não é magia nem tabela global: para cada classe
**polimórfica** o compilador emite um `std::type_info` estático e põe o endereço dele na
vtable. Como todo objeto polimórfico carrega um vptr, sempre dá para descobrir o tipo real em
runtime — e o vptr custa espaço: `struct Simples { int x; }` tem `sizeof` 4, e
`struct Poli { virtual ~Poli() = default; int x; }` tem **16** (8 do vptr + 4 + 4 de padding).
`typeid(*p).name()` devolve o tipo REAL (`"1D"`), não o estático.

Três consequências que você vai encontrar:

1. **`dynamic_cast` exige um virtual na base.** Sem virtual não há vptr, nem vtable, nem
   `type_info` — o compilador rejeita: `cannot 'dynamic_cast' … (source type is not polymorphic)`.
2. **`-fno-rtti` desliga tudo** para economizar binário:
   `error: 'dynamic_cast' not permitted with '-fno-rtti'`.
3. **RTTI custa.** O número medido está no Demo 2.

Layout completo em [`src/layout/`](../layout/); despacho virtual em [`src/oop/`](../oop/).

### Comportamento indefinido (UB)

**UB** não significa "resultado imprevisível": significa que o padrão não impõe requisito algum
ao programa inteiro, e o compilador pode **assumir que aquilo nunca acontece** e otimizar em
cima disso. Na prática, pode "funcionar" em `-O0` e quebrar em `-O2`; o sintoma pode aparecer
**longe** da linha culpada; e não existe "UB que funciona" — existe UB cujo efeito você ainda
não observou. Assunto de [`src/undefined_behavior/`](../undefined_behavior/).

### Strict aliasing, em seis linhas

*Aliasing* é dois ponteiros para o mesmo endereço. A regra de **strict aliasing** permite ao
compilador **assumir que ponteiros de tipos diferentes nunca apontam ao mesmo objeto** (exceções:
`char*`, `unsigned char*`, `std::byte*`, que podem apontar para qualquer coisa). Não é
burocracia — é o que habilita otimizações reais:

```cpp
int zera_e_le(float* f, int* i) {
    *i = 1;
    *f = 0.0f;      // por strict aliasing, o compilador assume que NÃO toca em *i
    return *i;      // logo, pode devolver 1 sem reler a memória
}
```

```asm
; g++ -O2 (padrão)                    ; g++ -O2 -fno-strict-aliasing
mov DWORD PTR [rsi], 1                mov DWORD PTR [rsi], 1
mov eax, 1   ← constante!             mov DWORD PTR [rdi], 0x0
mov DWORD PTR [rdi], 0x0              mov eax, DWORD PTR [rsi]  ← relê a memória
```

Se você violar a regra, o compilador já otimizou com base numa premissa falsa. As três saídas
legais para reinterpretar bytes: **`std::memcpy`**, **`std::bit_cast`** e **ler byte a byte via
`char*`/`unsigned char*`/`std::byte*`**. Ao ato de ler a representação de um objeto como se fosse
de outro tipo dá-se o nome de **type punning**.

### Alinhamento

Todo tipo tem um `alignof(T)`: o endereço precisa ser múltiplo dele — `uint32_t` exige 4,
`uint16_t` exige 2. Acessar um `T` desalinhado é UB. Importa aqui porque `reinterpret_cast`
faz o compilador **acreditar** no alinhamento sem verificar nada — e é o que o demo 4.4 faz.

### `std::bit_cast` (C++20)

```cpp
constexpr float    pi   = 3.14159f;
constexpr uint32_t bits = std::bit_cast<uint32_t>(pi);   // 0x40490FD0
static_assert(bits == 0x40490FD0u);                      // funciona: é constexpr!
```

Copia os bytes e devolve como `To`. Requisitos: `sizeof` idêntico e ambos os tipos
**trivialmente copiáveis**. É a substituição segura do `reinterpret_cast` para type punning —
e a única que funciona em compile-time.

## Por que estudar isto?

Porque o cast errado **não gera erro nem aviso**: compila, roda e produz o resultado errado
mais tarde.

| Escrito assim | O que realmente acontece | O compilador avisa? |
|---|---|---|
| `7 / 2` | divisão **inteira** → `3`, não `3.5` | não |
| `(int*)&limite`, com `const int limite` | `const_cast` silencioso; escrever ali é UB | **não** |
| `(Cor*)&ponto`, tipos sem relação | `reinterpret_cast` silencioso; lixo | **não** |
| `static_cast<Carro*>(v)` com `v` sendo `Moto*` | ponteiro inválido, UB no primeiro acesso | não |
| `*reinterpret_cast<uint32_t*>(&pi)` | viola strict aliasing | sim, `-Wstrict-aliasing` |
| `dynamic_cast<D*>(&b)` sem virtual em `B` | **erro de compilação** — não há RTTI | sim |

As duas linhas em negrito são o coração do subprojeto: o cast estilo C esconde as duas operações
mais perigosas da linguagem atrás de dois parênteses. Ligue `-Wold-style-cast` — neste
subprojeto ele acusa exatamente **3** ocorrências, as três intencionais do demo 5.

### Hierarquia de segurança

```
  ▲ mais seguro
  │   nenhum cast          ← reprojete até não precisar
  │   static_cast          ← verificado em compile-time
  │   dynamic_cast         ← verificado em runtime; falha com nullptr / bad_cast
  │   const_cast           ← só mexe em const/volatile; UB se o objeto for mesmo const
  │   reinterpret_cast     ← nenhuma verificação; você assume tudo
  │   (T)x   /   T(x)      ← nenhuma verificação E esconde qual das opções escolheu
  ▼ sem nenhuma verificação
```

`T(x)` (*cast funcional*, ex.: `int(pi)`) é **idêntico** a `(T)x` quando `T` é um único
identificador. Vale a mesma proibição.

## Estrutura de arquivos

```
src/casts/
├── main.cpp                    ← entrada, cor dodger_blue; imprime as 5 seções
├── demo_static_cast.hpp        ← 1 · numérico, upcast, downcast, void*, enum class  (1.1–1.5)
├── demo_dynamic_cast.hpp       ← 2 · ponteiro, referência, despacho, crosscast      (2.1–2.5)
├── demo_const_cast.hpp         ← 3 · API C legada, overload const, UB, volatile     (3.1–3.5)
├── demo_reinterpret_cast.hpp   ← 4 · ptr↔int, bytes, punning, serialização, void*   (4.1–4.6)
├── demo_c_style.hpp            ← 5 · o que (T)x esconde, e a tabela final           (5.1–5.5)
├── meson.build                 ← executable('casts', ['main.cpp'], dependencies: [fmt_dep])
└── README.md
```

## Demo 1 · `static_cast` — a conversão verificada em compile-time

**O problema:** você quer uma conversão que a linguagem não faz sozinha (ou faz, mas com perda),
e quer que o compilador confirme que ela ao menos *faz sentido*.

`static_cast<T>(x)` pergunta: "existe uma sequência de conversões que leva de `x` a `T`?". Se
não existe, **erro de compilação**. Se existe, o código é gerado e **não há verificação alguma em
runtime** — custo zero.

```cpp
int truncado = static_cast<int>(3.14159);   // 3
```

Saída real de `1.1`:

```
  double 3.14159 → int 3
  7 / 2 sem cast    = 3          ← divisão inteira: o resultado é int
  7 / 2 com cast    = 3.5        ← static_cast<double>(7) / 2 → aritmética de ponto flutuante
  'A' como int      = 65
  66 como char      = B
  int 300 → uint8_t = 44         ← 300 mod 256; permitido, mas a perda é sua
```

Hierarquias (`1.2`, `1.3`):

```cpp
Animal* animal = static_cast<Animal*>(&dog);   // upcast: seria implícito; explícito documenta

Animal* a = new Cachorro();
Cachorro* d = static_cast<Cachorro*>(a);       // downcast: NÃO verifica nada
d->latir();                                    // correto AQUI, porque sabemos que é Cachorro
```

> ⚠ **O downcast com `static_cast` é uma promessa não verificada.** Se o objeto real não for do
> tipo alvo, o ponteiro é inválido e o primeiro acesso é UB — sem nullptr, sem exceção, sem
> aviso. Sem **certeza**, use `dynamic_cast`.

> ⚠ **Downcast a partir de uma base virtual é proibido**, e o compilador explica: `cannot convert
> from pointer to base class 'VB' to pointer to derived class 'VD' because the base is virtual`.
> Aí `dynamic_cast` é a única saída.

`void*` (`1.4`) é o "ponteiro sem tipo" de C: guarda um endereço e esquece o que há nele.
`T* → void*` é implícito; **`void* → T*` exige `static_cast`** — e `static_cast` é o cast
**correto** para isso, não `reinterpret_cast`. Já `enum class` (`1.5`) **não** converte
implicitamente para inteiro, ao contrário do `enum` simples — é justamente o ponto dele; e
`static_cast` é a porta de saída, nos dois sentidos.

## Demo 2 · `dynamic_cast` — o downcast que verifica

**O problema:** você tem um `Veiculo*` e precisa saber, *em runtime*, se ele é um `Carro`.

```cpp
Carro* c = dynamic_cast<Carro*>(v);        // ponteiro:   nullptr se não for
if (c) { ... }
try { Carro& r = dynamic_cast<Carro&>(ref); }   // referência: lança std::bad_cast se não for
catch (const std::bad_cast&) { ... }
```

A diferença é deliberada: referências não podem ser nulas, então a única forma de sinalizar
falha é lançar. Saída real de `2.1`–`2.4`:

```
  v1 é Carro ✓ — portas: 4
  v2 NÃO é Carro ✓ — dynamic_cast retornou nullptr
  v1 é CarroEletrico? false (é só Carro)          ← não basta ser "algum" Carro
  bad_cast capturado ✓ — Moto não pode ser Carro
  Crosscast Veiculo* → Voador* bem-sucedido ✓     ← só dynamic_cast consegue isto
```

### O custo, medido

1000 objetos de 3 classes numa hierarquia de 3 níveis, 20 000 passadas (20 milhões de
operações), GCC 13.3 `-O2`, x86-64:

| operação | tempo total | por operação | relativo |
|---|---|---|---|
| `static_cast<D1*>` | 8,1 ms | ~0,4 ns | 1× |
| chamada `virtual` | 138 ms | ~6,9 ns | 17× |
| `dynamic_cast<D1*>` | 198 ms | ~9,9 ns | **~24×** |

Leitura honesta: o `static_cast` aqui é um *no-op* (`D1` está no deslocamento 0), então os
8 ms medem quase só o laço. O que importa é a ordem de grandeza: **um `dynamic_cast` custa
dezenas de vezes o que custa não ter cast**, e mais que uma chamada virtual.

### `dynamic_cast<void*>`

Forma especial: devolve o endereço do **objeto mais derivado** — útil para comparar identidade
em herança múltipla, onde ponteiros para bases diferentes têm endereços diferentes. Medido:

```
&c = 0x7ffe45865b20   |   (B*)&c = 0x7ffe45865b30 (deslocado 16 B!)
dynamic_cast<void*>((B*)&c) = 0x7ffe45865b20   ← volta ao início do objeto
```

> ⚠ **Um `if / else if` de `dynamic_cast` é cheiro de projeto.** O demo `2.3` mostra o padrão
> para você reconhecê-lo. Saídas melhores, em ordem: um método `virtual` na base
> ([`src/oop/`](../oop/)); `std::variant` + `std::visit`
> ([`src/vocabulary_types/`](../vocabulary_types/)); ou o padrão *Visitor*
> ([`src/design-of-pattern/`](../design-of-pattern/)), em que a base declara
> `virtual void aceitar(Visitante&)` — despacho por vtable, sem consulta a RTTI.

## Demo 3 · `const_cast` — o único que mexe em `const`

**O problema:** uma API C antiga pede `char*` onde deveria pedir `const char*`, e você só tem
uma string constante.

```cpp
void api_legada_c(char* buffer, int tamanho);        // deveria ser const char*, mas não é
const char* mensagem = "hello world";
api_legada_c(const_cast<char*>(mensagem), std::strlen(mensagem));
```

Seguro **porque sabemos que `api_legada_c` não escreve no buffer**. O `const_cast` remove a
promessa do tipo; não remove a proteção real da memória.

### O truque do overload const / não-const (`3.2`)

Duas versões de `operator[]`, idênticas exceto pelo `const`. Para não duplicar a lógica, a versão
não-`const` delega para a `const` (Scott Meyers, *Effective C++*, item 3):

```cpp
const char& operator[](int i) const { /* a lógica de verdade */ return dados[i]; }

char& operator[](int i) {
    return const_cast<char&>(                // passo 2: tira o const do RESULTADO
        static_cast<const Buffer&>(*this)[i] // passo 1: vira const para chamar a outra versão
    );
}
```

Seguro porque `*this` **não era** const — nós é que o tornamos const por um instante. A saída
prova a delegação: `operator[] não-const chamado (delega para const)`, e logo abaixo
`operator[] const chamado`.

> ⚠ **Nunca faça o contrário** (a versão `const` chamando a não-`const`): seria prometer não
> modificar e chamar código que pode modificar.

### O UB (`3.4`)

```cpp
const int CONSTANTE = 42;
int* ptr = const_cast<int*>(&CONSTANTE);
// *ptr = 100;   ← UB. A linha está comentada no fonte, intencionalmente.
```

Um objeto declarado `const` pode ir para memória somente-leitura (`.rodata`), e o compilador
pode substituir cada leitura pelo literal `42` sem tocar na memória. Escrever ali é UB
**sempre**. **A regra:** `const_cast` só é seguro quando o objeto **original** não é `const` e
você apenas atravessa uma interface que perdeu o `const` pelo caminho. Ele também remove
`volatile` (`3.5`) — o qualificador que diz "não otimize as leituras, isto muda por fora":
registradores de hardware, memória mapeada; raro fora de embarcado.

## Demo 4 · `reinterpret_cast` — os mesmos bytes, outro nome

**O problema:** você precisa tratar a memória como memória — serializar uma struct, passar um
contexto por um callback C, guardar um endereço num inteiro. `reinterpret_cast` **não converte
nada**: só muda o tipo estático da expressão. Nenhum bit se move, nenhuma verificação acontece.

**Ponteiro ↔ inteiro (`4.1`) — legítimo.** `uintptr_t` (de `<cstdint>`) é o inteiro sem sinal
garantidamente grande o bastante para conter um ponteiro; usar `int` seria um bug em qualquer
plataforma 64-bit. **Inspecionar bytes (`4.2`) — legítimo**, porque `unsigned char`/`std::byte`
são a **exceção explícita** da regra de aliasing.

```cpp
uintptr_t endereco   = reinterpret_cast<uintptr_t>(ptr);   // e de volta com <int*>
const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&f);
// float 1.0f  →  00 00 80 3F     (little-endian de 0x3F800000, o IEEE-754 de 1.0)
// int 0xDEADBEEF  →  EF BE AD DE
```

### ⚠ Type punning (`4.3`) — o erro clássico

```cpp
float pi = 3.14159f;
// ✗ ERRADO — viola strict aliasing (UB, mesmo que "funcione")
// uint32_t bits = *reinterpret_cast<uint32_t*>(&pi);
// uint32_t bits =  reinterpret_cast<uint32_t&>(pi);   ← a forma por referência é o MESMO erro

uint32_t bits; std::memcpy(&bits, &pi, sizeof(float));       // ✓ 0x40490FD0
constexpr uint32_t b2 = std::bit_cast<uint32_t>(pi);         // ✓ melhor: compile-time
```

E não é erro teórico — o próprio GCC denuncia:

```
$ g++ -O2 -Wstrict-aliasing=2 -c sa.cpp
warning: dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]
```

> ⚠ Versões anteriores deste README listavam `reinterpret_cast<uint32_t&>(f)` entre os **usos
> legítimos**. Estava errado, e contradizia o próprio fonte (`demo_reinterpret_cast.hpp:82-83`,
> que marca a linha equivalente com `✗ ERRADO`). Use `memcpy` ou `std::bit_cast`.

### Serialização (`4.4`) — legítimo de ida, armadilha na volta

```cpp
#pragma pack(push, 1)
struct PacoteRede { uint8_t versao; uint16_t tamanho; uint32_t checksum; };
#pragma pack(pop)                                              // sizeof == 7, sem padding

const uint8_t* raw = reinterpret_cast<const uint8_t*>(&pkt);   // ✓ struct → bytes
// saída real:  02 80 00 BE BA FE CA

uint8_t buffer[] = { 0x02, 0x80, 0x00, 0xBE, 0xBA, 0xFE, 0xCA };
const PacoteRede* recebido = reinterpret_cast<const PacoteRede*>(buffer);   // ⚠ bytes → struct
// saída real:  deserializado: versao=2, tamanho=128, checksum=0xCAFEBABE
```

> ⚠ A volta funciona nesta máquina e ainda assim é UB por **duas** razões: não existe um objeto
> `PacoteRede` naquele endereço (só bytes), e `buffer` não tem o alinhamento de `uint16_t` nem
> de `uint32_t` nos deslocamentos 1 e 3 — o UBSan flagra exatamente isso (veja "Como validar").
> A forma correta é `std::memcpy(&pkt, buffer, sizeof(pkt))`, que também resolve o alinhamento.
> Num protocolo real ainda falta converter a **ordem de bytes** (`ntohs`/`ntohl`, ou
> `std::byteswap` em C++23): o remetente pode ser big-endian.

**Callbacks C com `void*` (`4.5`).**

> ⚠ O fonte usa `reinterpret_cast` aqui, mas para `T* ↔ void*` o cast **certo** é `static_cast` —
> coerente com o que `demo_static_cast.hpp:99-101` ensina. Prefira `static_cast<void*>(&ctx)` e
> `static_cast<Contexto*>(user_data)`, e reserve `reinterpret_cast` para o que só ele faz:
> ponteiro↔inteiro e leitura de bytes.

## Demo 5 · o cast estilo C — e por que ele é proibido aqui

`(T)x` tenta, **nesta ordem** (`[expr.cast]/4` do padrão), a primeira alternativa que compilar:

1. `const_cast<T>(x)`
2. `static_cast<T>(x)`
3. `static_cast<T>` seguido de `const_cast<T>`
4. `reinterpret_cast<T>(x)`
5. `reinterpret_cast<T>` seguido de `const_cast<T>`

Note que **`const_cast` vem primeiro** e que **`reinterpret_cast` está na lista**: as duas
operações mais perigosas da linguagem ficam disponíveis por acidente, sem aparecer no código.
(Versões anteriores deste README traziam a ordem trocada; a acima é a do padrão, e a mesma do
cabeçalho de `demo_c_style.hpp`.)

O demo prova as duas em código que compila **sem um único aviso**:

```cpp
const int limite = 100;
int* ptr_c = (int*)&limite;    // 5.2 — const_cast silencioso

struct Vec2 { float x, y; };   struct Cor { uint8_t r, g, b, a; };
Vec2 ponto { 1.0f, 2.0f };
Cor* cor_c = (Cor*)&ponto;     // 5.3 — reinterpret_cast silencioso
// saída real: r=0 g=0 b=128 a=63   ← são os bytes de 1.0f lidos como quatro uint8_t
```

Há ainda um argumento de manutenção (`5.4`): `grep reinterpret_cast` encontra **todos** os pontos
de risco de um milhão de linhas em um segundo; `grep '(int)'` encontra ruído. Casts nomeados são
**buscáveis**; `(T)x` não é. Para impor, ligue `-Wold-style-cast`.

## Regras de ouro

| Situação | Faça |
|---|---|
| a conversão implícita já resolve | **não escreva cast nenhum** |
| numérico, `enum class`↔int, `void*`→`T*`, upcast | `static_cast` |
| downcast e você tem **certeza** do tipo | `static_cast` + um comentário dizendo por quê |
| downcast e o tipo **pode variar** | `dynamic_cast`, testando `nullptr` |
| crosscast em herança múltipla, ou base virtual | `dynamic_cast` (é o único que consegue) |
| encadeando `dynamic_cast` em `if / else if` | pare — use `virtual`, `std::variant` ou Visitor |
| API C legada sem `const` na assinatura | `const_cast`, e só se o original não for `const` |
| ler os bits de um float como inteiro | `std::bit_cast` (C++20) ou `std::memcpy` |
| struct → bytes, ponteiro ↔ inteiro | `reinterpret_cast`, isolado numa função com nome |
| bytes → struct | `std::memcpy`, nunca `reinterpret_cast<T*>(buffer)` |
| qualquer coisa | **nunca** `(T)x` nem `T(x)` — ligue `-Wold-style-cast` |

## Como validar

```bash
make build && ./dist/bin/casts

# isolado, com todos os avisos
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/casts/main.cpp -o /tmp/casts -L$FMTP/lib -lfmtd && /tmp/casts
```

Estado atual: **compila sem nenhum warning** com `-Wall -Wextra -Wpedantic` (GCC 13.3, C++20).

Sob sanitizers o resultado é mais interessante:

```bash
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/casts/main.cpp -o /tmp/casts_san -L$FMTP/lib -lfmtd && /tmp/casts_san
```

O UBSan reporta **8 achados**, todos no demo 4.4 e todos legítimos:

```
demo_reinterpret_cast.hpp:119:45: runtime error: reference binding to misaligned address …
                                  for type 'unsigned int', which requires 4 byte alignment
demo_reinterpret_cast.hpp:134:44: runtime error: reference binding to misaligned address …
                                  for type 'short unsigned int', which requires 2 byte alignment
```

Não é falso positivo nem defeito a esconder: é **a lição do demo**. `#pragma pack(1)` remove o
padding, e com isso `tamanho` e `checksum` caem nos deslocamentos 1 e 3 — desalinhados por
construção. Ler `pkt.tamanho` liga uma referência a um `uint16_t` desalinhado, e isso é UB:
é o argumento final a favor do `memcpy` campo a campo.

Trechos da execução normal, para conferência:

```
── 1.1 Conversões numéricas ──          7 / 2 sem cast = 3   |   com cast = 3.5
── 2.1 dynamic_cast com ponteiro ──     v2 NÃO é Carro ✓ — dynamic_cast retornou nullptr
── 3.2 Overload const/não-const ──      operator[] não-const chamado (delega para const)
── 4.2 Inspecionar bytes de um tipo ──  float 1.0f em bytes (little-endian): 00 00 80 3F
── 4.4 Serialização de struct ──        deserializado: versao=2, tamanho=128, checksum=0xCAFEBABE
── 5.3 C-style faz reinterpret ──       Vec2{1.0,2.0} como Cor: r=0 g=0 b=128 a=63

✓ Todos os demos de cast concluídos!
```

## Referências

- [`src/syntax/`](../syntax/) — inicialização com `{}` e a proibição de narrowing
- [`src/oop/`](../oop/) — herança, `virtual`, vtable, slicing e RTTI aplicado
- [`src/layout/`](../layout/) — alinhamento, padding, o custo em bytes do vptr
- [`src/undefined_behavior/`](../undefined_behavior/) — strict aliasing e os sanitizers
- [`src/bits/`](../bits/) — `std::bit_cast`, endianness, serialização binária correta
- [`src/vocabulary_types/`](../vocabulary_types/) — `std::variant` no lugar do `dynamic_cast` encadeado
- [`src/smart-pointers/`](../smart-pointers/) — `unique_ptr`/`shared_ptr` no lugar dos `new` crus dos demos
- [`src/type_erasure/`](../type_erasure/) — polimorfismo sem herança nem RTTI
- cppreference — [static_cast](https://en.cppreference.com/w/cpp/language/static_cast),
  [dynamic_cast](https://en.cppreference.com/w/cpp/language/dynamic_cast),
  [const_cast](https://en.cppreference.com/w/cpp/language/const_cast),
  [reinterpret_cast](https://en.cppreference.com/w/cpp/language/reinterpret_cast),
  [explicit cast](https://en.cppreference.com/w/cpp/language/explicit_cast),
  [std::bit_cast](https://en.cppreference.com/w/cpp/numeric/bit_cast)
- C++ Core Guidelines — **ES.48** ("Avoid casts"), **ES.49** ("If you must use a cast, use a
  named cast"), **Type.1** ("Avoid casts")
- Scott Meyers, *Effective C++*, 3ª ed., item 3 — o truque do overload const/não-const
- *ISO C++ standard*, `[expr.cast]/4` — a ordem exata que o cast estilo C tenta
