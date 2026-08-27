# chrono — Tempo, Datas e Medição

## O que é este projeto?

Um guia completo sobre `<chrono>`: o sistema de unidades de tempo do C++, os relógios e as
garantias de cada um, o calendário do C++20 e — o que mais aparece na prática — **como
cronometrar sem se enganar**.

Compile e rode:

```bash
make build           # configura, compila e instala em ./dist/
./dist/bin/chrono    # roda os 5 demos
```

> `make build` faz três coisas que um `meson compile` solto não faz: usa o diretório de build
> `build/`, carrega o ambiente do Conan (`source build/conanbuild.sh`, sem o qual o meson não
> acha a `fmt`) e instala os binários em `dist/bin/`. É por isso que o executável está em
> `./dist/bin/chrono`, e não em `build/`.

> ⚠ Compila com **C++23** (o `meson.build` deste diretório aplica o override só neste alvo). O
> calendário é impresso via `operator<<` da libstdc++ porque a fmt 10 — a versão deste
> repositório — ainda não traz `formatter` para `year_month_day`; isso chegou na fmt 11. Com
> `std::format` do C++20 ou fmt 11+, o helper da seção "Imprimir durations e datas" seria
> desnecessário.

---

## Antes de começar

Esta seção define, uma a uma, **todas** as palavras que o restante do README usa. Nada aqui
depende de conhecimento prévio de `<chrono>`.

### O menor programa de `<chrono>`

Este programa completo compila e roda. É 90% do que a maioria das pessoas vem buscar:

```cpp
#include <chrono>
#include <fmt/core.h>
#include <thread>

int main() {
    using namespace std::chrono;

    const auto t0 = steady_clock::now();          // 1 · instante inicial
    std::this_thread::sleep_for(milliseconds{150});
    const auto t1 = steady_clock::now();          // 2 · instante final

    const auto dt = t1 - t0;                      // 3 · quanto durou
    fmt::print("levou {} ms\n", duration_cast<milliseconds>(dt).count());
}
```

Saída, verificada: `levou 150 ms`.

Três peças acabaram de aparecer, e cada uma tem um demo próprio:

| Peça | O que é | Demo |
|---|---|---|
| `steady_clock` | um **relógio**: quem sabe responder `now()` | Demo 2 |
| `t0`, `t1` | **`time_point`**: um instante, medido por um relógio específico | Demo 3 |
| `dt` | **`duration`**: uma quantidade de tempo, sem posição no calendário | Demo 1 |

A regra que organiza tudo: `time_point − time_point = duration`. Instante menos instante dá
uma duração; duração mais instante dá outro instante; instante mais instante **não compila**,
porque não significa nada.

### `std::ratio`, `Rep` e `Period`

Um `duration` é declarado assim:

```cpp
template <class Rep, class Period = std::ratio<1>> class duration;
```

- **`Rep`** (de *representation*) é o **tipo numérico** que guarda a contagem — quase sempre
  `int64_t`, às vezes `double`.
- **`Period`** é **quanto vale um tique, em segundos**, escrito como uma fração.
- **`std::ratio<N, D>`** é essa fração: um número racional resolvido em **tempo de
  compilação**, com numerador `N` e denominador `D`. `std::ratio<1, 1000>` significa
  "um tique vale 1/1000 de segundo" — ou seja, um milissegundo.

Como a fração vive no **tipo**, ela custa **zero** em runtime: `sizeof(milliseconds)` é 8
bytes, exatamente o `int64_t` de dentro. A unidade é verificada pelo compilador e some do
binário.

### Monotônico

Um relógio é **monotônico** (*steady*, na linguagem do padrão) quando `now()` **nunca**
devolve um valor menor que uma chamada anterior. Ele conta desde um zero arbitrário e nada no
sistema o ajusta.

`system_clock` **não** é monotônico: ele é o relógio de parede, e o **NTP** (*Network Time
Protocol*, o serviço que sincroniza a hora da máquina pela rede) pode **saltá-lo para trás** a
qualquer momento. Se isso acontecer entre o seu `t0` e o seu `t1`, a diferença `t1 - t0` vem
**negativa**.

A propriedade `Clock::is_steady` é exatamente essa garantia, exposta como um `bool` de
compilação.

### Epoch

A **epoch** de um relógio é o seu instante zero — o ponto a partir do qual ele conta.

- Para `system_clock`, o C++20 fixou a epoch em **1970-01-01 00:00:00 UTC** (o *Unix time*).
- Para `steady_clock`, a epoch é **arbitrária** — tipicamente o boot da máquina. É por isso
  que imprimir um `steady_clock::time_point` como se fosse data não significa nada; só a
  **diferença** entre dois deles é útil.

### Percentil (p95, p99)

O **p95** de um conjunto de medidas é o valor abaixo do qual estão 95% delas. Ordene as
medidas e pegue a que está na posição 95%. É a estatística de SLA: "95% das requisições
respondem em até X ms". O **p99** é o mesmo com 99%.

### tzdata

O **tzdata** é o banco de dados IANA de fusos horários — o arquivo que sabe que o Brasil
aboliu o horário de verão em 2019 e que o Chile não. `std::chrono::current_zone()` consulta
esse banco; se ele não estiver instalado, a função lança.

### `.count()`

`.count()` extrai o **número cru** de dentro de um `duration`, sem unidade nenhuma:
`500ms.count()` é `500`. Ele é a **saída** do sistema de tipos — a fronteira em que você
abandona a verificação do compilador. Use só ao imprimir, gravar ou passar para uma API C
(`sleep`, `poll`, `epoll_wait`), nunca no meio de um cálculo.

---

## Por que estudar isto?

Porque `<chrono>` não é "pegar a hora": é um **sistema de unidades com verificação em
compilação**, e ignorar isso gera bugs que só aparecem em produção.

Uma linha medida por este subprojeto resume o problema:

```
high_resolution_clock::is_steady = false      ← no libstdc++, ele É system_clock
```

Todo código que mede duração com `high_resolution_clock` **neste compilador** está sujeito a
resultado negativo se o NTP ajustar o relógio. O mesmo código, compilado com Clang ou MSVC,
não tem o problema — e é exatamente esse tipo de diferença que não aparece em teste.

---

## Estrutura de arquivos

```
src/chrono/
├── main.cpp                ← ponto de entrada, executa os 5 demos, cor sky_blue
├── demo_duration.hpp       ← 1 · duration<Rep,Period>, conversões, aritmética
├── demo_clocks.hpp         ← 2 · as garantias, high_resolution_clock, resolução medida
├── demo_time_point.hpp     ← 3 · instantes, a álgebra, texto, deadline vs timeout
├── demo_calendario.hpp     ← 4 · year/month/day, validação, sys_days, fusos
├── demo_medicao.hpp        ← 5 · as 5 armadilhas de benchmark, e o método
├── meson.build
└── README.md
```

---

## Demo 1: `duration`

```cpp
template <class Rep, class Period = std::ratio<1>> class duration;
```

`milliseconds` é `duration<int64_t, ratio<1,1000>>`. O "milli" está no **tipo**, não num campo:

```
nanoseconds    Rep=int64_t  Period=1/1000000000
milliseconds   Rep=int64_t  Period=1/1000
seconds        Rep=int64_t  Period=1/1
hours          Rep=int64_t  Period=3600/1
days (C++20)   Rep=int64_t  Period=86400/1
sizeof(milliseconds) = 8 bytes      ← é só o int64_t; a unidade não custa nada
```

E `seconds` e `milliseconds` são tipos **diferentes**: passar um onde se espera o outro é erro
de compilação, não bug de runtime.

### A regra das conversões implícitas

A conversão é implícita quando **não pode perder informação** — ou seja, quando o `Period` de
destino divide exatamente o de origem:

```
2h → milliseconds     ✓ implícito (3600000/1 é exato)
1500ms → seconds      ✗ NÃO COMPILA (1,5 s truncaria)
duration_cast<seconds>(1500ms) = 1 s     ← trunca em direção a ZERO
```

Para outras políticas, use `floor` (para baixo), `ceil` (para cima) ou `round` (para o mais
próximo):

| De 1500 ms para segundos | Resultado |
|---|---|
| `duration_cast` | 1 |
| `floor` | 1 |
| `ceil` | 2 |
| `round` | 2 |

> ⚠ Para negativos os dois se separam: `duration_cast<seconds>(-1500ms)` é **−1** (trunca em
> direção a zero) e `floor<seconds>(-1500ms)` é **−2** (arredonda para baixo, para −∞).

### Aritmética

```
1h + 30min + 45s = 5445 s      ← tipo comum: seconds (a unidade mais fina que representa
                                  hora, minuto e segundo exatamente)
1s + 500ms       = 1500 ms     ← tipo comum: milliseconds
2h * 3           = 6 h
1h / 15min       = 4           ← escalar puro, sem unidade
100min % 1h      = 40 min
1h * 1h          ✗ tempo² não é um duration
1h + 5           ✗ 5 do quê? sem unidade, sem soma
```

É a mesma segurança dimensional que [`../operators/`](../operators/) constrói à mão — só que
aqui ela vem pronta na biblioteca padrão.

### Imprimir durations e datas

O README inteiro mostra durations como texto (`5445 s`, `1500 ms`). É assim que isso é feito:

**(a) O idioma portátil** — converta e extraia o número na fronteira:

```cpp
fmt::print("{} ms\n", duration_cast<milliseconds>(dt).count());
fmt::print("{} ms\n", dt / 1ms);   // divisão por 1ms: o mesmo número, sem o cast
```

**(b) Com `std::format` (C++20) ou fmt 11+**, o `duration` se formata sozinho, com a unidade
junto:

```cpp
fmt::print("{}\n", 1500ms);          // 1500ms
fmt::print("{:%F %T}\n", agora);     // 2026-08-27 14:33:44
```

O `demo_time_point.hpp` já usa essa forma para `system_clock::time_point`, e ela funciona
nesta versão da fmt.

**(c) Para os tipos de calendário** (`year_month_day`, `month`, `weekday`) a fmt 10 ainda não
tem `formatter`. O `demo_calendario.hpp` faz a ponte com um helper de 5 linhas, que usa o
`operator<<` que a libstdc++ já fornece:

```cpp
template <typename T>
std::string str(const T& valor) {
    std::ostringstream oss;
    oss << valor;
    return oss.str();
}
```

Com fmt 11 ou `std::format`, isso vira `fmt::print("{:%F}", data)` e o helper some.

### Unidades customizadas — uma linha

```cpp
using Frame60  = duration<int64_t, ratio<1, 60>>;      // 1 tique = 1/60 s
using Quinzena = duration<int64_t, ratio<1209600>>;    // 1 tique = 14 dias
```

E elas participam de toda a aritmética e de todas as verificações automaticamente — inclusive
das que impedem truncamento silencioso. O demo mostra `90 frames a 60fps = 1500 ms` e
`2 quinzenas = 672 horas`, sem nenhuma conta escrita à mão.

---

## Demo 2: Os Relógios

| Relógio | Monotônico | É hora real | Usar para |
|---|---|---|---|
| `system_clock` | **não** | sim | **datar** |
| `steady_clock` | **sim** | não | **medir** |
| `high_resolution_clock` | depende! | depende! | **nada** |

O bug que a coluna "Monotônico" previne:

```cpp
auto t0 = system_clock::now();
fazer_trabalho();                      // ← o NTP ajusta o relógio aqui
auto dt = system_clock::now() - t0;    // dt pode ser NEGATIVO
```

Com `steady_clock` isso é impossível **por construção**, não por disciplina.

### ⚠ `high_resolution_clock`

O padrão permite que ele seja apenas um apelido para outro relógio. Medido neste ambiente:

```
é o mesmo que system_clock? true         ← libstdc++
high_resolution_clock::is_steady = false
```

| Implementação | Aponta para |
|---|---|
| libstdc++ (GCC) | **`system_clock`** |
| libc++ (Clang) | `steady_clock` |
| MSVC | `steady_clock` |

O mesmo código muda de comportamento ao trocar de compilador — e o bug não aparece em teste,
só quando o relógio do sistema é ajustado. **Não use.** Escolha explicitamente: `steady_clock`
para medir, `system_clock` para datar.

### Resolução: do tipo vs a real

```
system_clock   tipo: 1/1000000000 s  |  resolução medida: ~10 ns
custo de steady_clock::now(): ~20 ns por chamada
```

A coluna "tipo" é o que o `Period` **promete** representar. A "medida" é o menor intervalo que
duas chamadas consecutivas a `now()` conseguem distinguir. Elas raramente coincidem: o tipo é
nanossegundos, mas chamar `now()` já custa dezenas deles.

> Medir algo que dura menos que isso é medir o relógio, não o seu código.
> Os números acima variam de máquina para máquina e entre execuções — rode e veja o seu.

### Os relógios do C++20

| Relógio | O que é |
|---|---|
| `system_clock` | Unix time — **ignora** segundos bissextos |
| `utc_clock` | UTC de verdade — **conta** os bissextos |
| `tai_clock` | Tempo Atômico Internacional, sem bissextos |
| `gps_clock` | O do sistema GPS |
| `file_clock` | O usado por `std::filesystem::last_write_time` |

A distinção `system` vs `utc` importa num caso real: um **segundo bissexto** faz aquele minuto
do UTC ter 61 segundos. O `system_clock` finge que não aconteceu (repete um segundo); o
`utc_clock` representa de verdade. Para 99% dos programas, `system_clock` é o certo.

Converter entre eles é `std::chrono::clock_cast<utc_clock>(t)` — e note que `steady_clock`
**não participa**: a epoch dele é arbitrária e não mapeia para data nenhuma.

---

## Demo 3: `time_point`

```
time_point - time_point = duration      ("quanto durou")
time_point + duration   = time_point    ("daqui a")
time_point - duration   = time_point    ("atrás")
time_point + time_point = ✗ ERRO        (não faz sentido)
```

Um `time_point` é internamente uma `duration` contada desde a **epoch** do seu relógio — só
isso. `sizeof(system_clock::time_point)` é 8 bytes, o mesmo `int64_t`. É por isso que ele
carrega o tipo do relógio no seu próprio tipo, e por isso `system - steady` **não compila**: as
epochs são diferentes e a subtração não teria significado.

A consequência prática é boa: você não consegue, por acidente, medir duração com o relógio de
parede e datar um evento com o monotônico.

### Convertendo para texto

```cpp
fmt::print("{}\n", agora);              // 2026-08-27 17:33:44.381843149
fmt::print("{:%Y-%m-%d}\n", agora);     // 2026-08-27
fmt::print("{:%H:%M:%S}\n", agora);     // 17:33:44.381843149

// truncar a precisão, para comparar timestamps sem os nanossegundos:
auto segundo = time_point_cast<seconds>(agora);
```

> ⚠ O `time_point` de `system_clock` é **UTC**. Para hora local seria preciso `zoned_time` —
> ver a seção 4.5.

> ⚠ Se você recorrer à via C: `localtime` (sem o `_r`) **não é thread-safe** — devolve um
> ponteiro para um buffer estático compartilhado. Use `localtime_r`.

### Deadline vs timeout

```
conectar(timeout=100ms) → autenticar(timeout=100ms) → consultar(timeout=100ms)
Total possível: 300ms, embora o chamador tenha pedido 100.

auto limite = steady_clock::now() + 100ms;
conectar(limite) → autenticar(limite) → consultar(limite)
Total garantido: 100ms.
```

Um **timeout** é "espere no máximo 100 ms"; um **deadline** é "desista às 14:30:05.100". O
timeout se renova a cada nível da cadeia; o deadline não.

É por isso que a biblioteca oferece os dois: `wait_for` (timeout) e `wait_until` (deadline).
Use `_until` em qualquer coisa que possa aninhar, e `_for` só na folha. Ver
[`../concurrency/demo_condition.hpp`](../concurrency/demo_condition.hpp).

---

## Demo 4: O Calendário do C++20

```cpp
2026y / August / 25d        // as três ordens funcionam, conforme a convenção local
August / 25d / 2026y
25d / August / 2026y
```

O `operator/` foi escolhido porque já lembra data em quase toda notação do mundo — e porque
não colidia com nenhum uso existente nesses tipos. Os tipos são minúsculos (`sizeof(year)` é
2, `month` e `day` são 1): a segurança não custa memória.

### Validação — o que `std::tm` nunca teve

```
2024-02-29 → ok() = true
2026-02-30 is not a valid date → ok() = false
```

> ⚠ `ok()` **não** é chamado automaticamente. Construir uma data inválida compila e não lança —
> você é responsável por verificar na fronteira de entrada.

Anos bissextos com a regra completa (não só "divisível por 4"):

```
1900 → false    ← divisível por 100, não por 400
2000 → true
2024 → true
2100 → false
```

### `sys_days` — a ponte

| Tipo | Para |
|---|---|
| `year_month_day` | calendário (mês, ano, validação) |
| `sys_days` | aritmética (somar dias, diferença) |

`sys_days` é só um contador de dias desde a epoch, então converter entre os dois é barato.
Converta conforme a operação.

### ⚠ Aritmética de calendário é traiçoeira

```
2026-01-31 + 1 mês   = 2026-02-31 is not a valid date  → ok() = false
2026-01-31 + 31 dias = 2026-03-03                       → ok() = true
2024-02-29 + 1 ano   = 2025-02-29 is not a valid date  → ok() = false
```

A biblioteca **não conserta** — ela devolve inválido e deixa você escolher a política:

| Política | Resultado | Como |
|---|---|---|
| último dia do mês | 2026-02-28 | `y/m/last` |
| transbordar | 2026-03-03 | `sys_days{data_invalida}` normaliza |
| rejeitar | erro | porque `ok() == false` |

| Operação | Pode ser inválida |
|---|---|
| `+ days` | não |
| `+ months` | **sim** |
| `+ years` | **sim** (29/02) |

### ⚠ Fusos horários

A API do C++20 é esta:

```cpp
auto tz = std::chrono::current_zone();
auto zt = std::chrono::zoned_time{tz, std::chrono::system_clock::now()};
fmt::print("{}\n", zt);           // hora LOCAL, com o fuso
// ou, para um fuso específico:
auto sp = std::chrono::zoned_time{"America/Sao_Paulo", agora};
```

Ela depende do **tzdata** e do suporte compilado na libstdc++ — pode lançar
`std::runtime_error` se o banco não estiver instalado, ou falhar no link em libstdc++ mais
antigas. **O demo não a chama de propósito**, para o programa não quebrar em máquina sem
tzdata. Teste se o seu ambiente suporta:

```bash
printf '#include <chrono>\nint main(){ auto z = std::chrono::current_zone(); (void)z; }\n' \
  | g++ -std=c++23 -x c++ - -o /tmp/tz && /tmp/tz && echo "fusos OK"
```

Enquanto isso, a conversão UTC → local segue possível pela via C, que é o que a seção 3.4 usa:

```cpp
const std::time_t t = std::chrono::system_clock::to_time_t(agora);
std::tm local{};
localtime_r(&t, &local);                    // ← _r: a variante thread-safe
char buf[64];
std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &local);
```

> A regra que não muda, com ou sem tzdata: **armazene em UTC, converta para local só na
> apresentação.** Guardar hora local em banco é a origem de metade dos bugs de horário de
> verão que existem.

---

## Demo 5: Como Cronometrar

As cinco armadilhas, em ordem de frequência.

### 1. ⚠ O compilador elimina o que você quer medir

Em `-O2`, um laço cujo resultado é descartado **não existe** no binário. Você cronometra zero e
conclui que o seu código é infinitamente rápido.

| Defesa | Qualidade |
|---|---|
| barreira de otimização (`asm volatile`) | **a correta** |
| `volatile` na variável | simples, mas impede otimizações legítimas em volta |
| imprimir o resultado | polui a medida |

A barreira é isto, e é o `demo_medicao.hpp` inteiro nessa parte:

```cpp
template <typename T>
inline void nao_otimize(T&& valor) {
    asm volatile("" : : "r,m"(valor) : "memory");
}
```

Lendo cláusula por cláusula:

- `asm volatile("")` — um bloco de assembly **vazio**. Nenhuma instrução é emitida; o custo em
  runtime é zero. O `volatile` proíbe o compilador de removê-lo por ser inútil.
- `: :` — o campo de saídas está vazio; o bloco não produz nada.
- `"r,m"(valor)` — a **entrada**: "este valor precisa estar num registrador ou na memória
  quando este bloco rodar". Como o compilador não sabe o que o assembly faz com ele, ele é
  obrigado a **materializar** o valor de verdade — ou seja, a calcular o que você queria medir.
- `: "memory"` — o *clobber*: "este bloco pode ler e escrever qualquer posição de memória".
  Isso impede o compilador de mover acessos à memória para dentro ou para fora da região.

É exatamente o que o `DoNotOptimize` do Google Benchmark faz.

### 2. O trecho é curto demais

```
uma execução medida direto : ~20-30 ns    ← mediu o RELÓGIO
1000000 execuções / N      :   ~1-2 ns    ← mediu a operação
```

> Regra: o trecho medido precisa durar pelo menos 100× a resolução do relógio. Com ~20 ns de
> overhead por `now()`, isso significa alvos de 2 µs ou mais — ou repetir e dividir.

### 3. Aquecimento

A primeira execução mede coisas que as seguintes não medem: *page faults*, cache frio,
preditor de saltos sem histórico, CPU ainda em frequência baixa.

O demo mede dois casos e é **honesto quando o efeito não aparece**: no caso (A), um vetor já
preenchido por `iota`, as cinco execuções vêm parecidas — porque o `iota` já pagou os page
faults antes do cronômetro. No caso (B), um buffer de 3 MB recém-alocado e nunca tocado, a
primeira paga a materialização das páginas.

> Em qualquer caso, descarte as primeiras. O custo de descartar é zero; o de não descartar é
> uma conclusão errada.

### 4. Mediana, não média

Uma execução típica de 21 repetições do mesmo trabalho, neste ambiente:

```
  mínimo  : 173 µs   ← o mais próximo do custo REAL
  mediana : 197 µs   ← robusta a outliers
  média   : 192 µs   ← puxada por qualquer pico
  p95     : 205 µs
  máximo  : 206 µs   ← o escalonador do SO interferiu
```

Os valores mudam a cada execução — é essa a questão. Uma única troca de contexto contamina a
média inteira; a mediana não se move.

| Você quer | Use |
|---|---|
| custo típico | mediana |
| melhor caso real | mínimo |
| latência de SLA | p95 / p99 |
| — | média (quase nunca) |

É a mesma escolha que [`../yolo/`](../yolo/) faz ao medir inferência.

### 5. Um cronômetro reutilizável

O `Cronometro` RAII e a função `medir()` deste demo fazem o básico certo:

```
✓ usa steady_clock (monotônico)      ✗ estatística (mediana, desvio, IC)
✓ descarta a primeira execução       ✗ ajustar N até estabilizar
✓ divide por repetições              ✗ fixar a thread numa CPU, desligar turbo
✓ aplica barreira de otimização      ✗ detectar interferência do escalonador
```

A coluna da direita é o que separa um cronômetro didático de um benchmark sério. Para isso:
Google Benchmark, nanobench ou Catch2. E ver [`../performance/`](../performance/), que ataca o
mesmo problema do outro lado (cache, `-O2`, o que realmente custa).

> **A regra que resume todas:** um número sem as flags de compilação ao lado não é um
> resultado — é um boato.

---

## Regras de ouro

| Situação | Use |
|---|---|
| Medir duração | `steady_clock` |
| Registrar quando algo aconteceu | `system_clock` |
| Qualquer coisa | **nunca** `high_resolution_clock` |
| API que espera e pode aninhar | deadline (`wait_until`) |
| API folha | timeout (`wait_for`) |
| Converter para unidade menor | implícito |
| Converter para unidade maior | `duration_cast` / `floor` / `ceil` / `round` |
| Extrair o número | `.count()` — só na fronteira |
| Somar dias | `sys_days` |
| Somar meses/anos | `year_month_day` + verificar `ok()` |
| Armazenar data/hora | UTC, sempre |
| Cronometrar | descarte a 1ª, repita, use mediana, barreira de otimização |
| Benchmark sério | Google Benchmark, nanobench |

---

## Como validar

### 1. Rodar os demos

```bash
make build
./dist/bin/chrono
```

A última linha tem de ser:

```
✓ Todos os demos de chrono concluídos!
```

Quatro linhas confirmam as lições centrais — procure por elas:

| Linha esperada | O que ela prova |
|---|---|
| `high_resolution_clock::is_steady = false` | neste compilador ele é `system_clock`, e não serve para medir |
| `1500ms → seconds    : NÃO COMPILA` | a conversão que perderia informação é barrada pelo tipo |
| `2026-02-30 is not a valid date → ok() = false` | o calendário valida, mas só quando você pergunta |
| `mínimo` < `mediana` ≤ `média` (às vezes) | a distribuição das medidas é assimétrica, com cauda à direita |

### 2. Ver a eliminação por *dead code* acontecer

Este é o experimento mais importante do subprojeto. Compile o mesmo código em `-O2` e compare:

```bash
g++ -O2 -std=c++23 src/chrono/main.cpp -o /tmp/chrono_o2 -lfmt \
  && /tmp/chrono_o2 | grep -A2 descartado
```

Saída esperada, com os números variando mas a **relação** fixa:

```
        resultado descartado     :      0 µs
        com `volatile`           :    925 µs
        com barreira de otimiz.  :    943 µs
```

A primeira linha é **zero**: em `-O2` o laço que soma dois milhões de números e joga o
resultado fora simplesmente não foi emitido. As outras duas continuam custando o que sempre
custaram. Compare com a saída do binário de `dist/bin` (compilado em `-O0`, o padrão do
`make build`), onde as três linhas são parecidas — sem otimização não há o que eliminar.

> O comando emite avisos da `fmt` do sistema durante a compilação; eles não afetam o
> experimento. Note também que ele usa a `fmt` de `/usr/include`, não a do Conan.

### 3. Verificar o suporte a fusos horários

```bash
printf '#include <chrono>\nint main(){ auto z = std::chrono::current_zone(); (void)z; }\n' \
  | g++ -std=c++23 -x c++ - -o /tmp/tz && /tmp/tz && echo "fusos OK"
```

Se imprimir `fusos OK`, a sua libstdc++ tem tzdata e você pode usar `zoned_time`. Se falhar no
link ou lançar, a seção 4.5 explica a alternativa via `localtime_r`.

---

## Referências

### Neste repositório

- [`../performance/`](../performance/) — medição, cache e o efeito do `-O2`: o mesmo problema
  do Demo 5, atacado pelo lado do hardware.
- [`../undefined_behavior/`](../undefined_behavior/) — por que o compilador tem o direito de
  apagar o seu laço, e o que mais ele apaga.
- [`../concurrency/demo_condition.hpp`](../concurrency/demo_condition.hpp) — `wait_for` e
  `wait_until` em uso real, que é onde a distinção deadline/timeout do Demo 3 se paga.
- [`../operators/`](../operators/) — segurança dimensional construída à mão, com os mesmos
  princípios que o `duration` usa.
- [`../yolo/`](../yolo/) — mediana vs média aplicada a medições de inferência.

### Fora

- [cppreference: `<chrono>`](https://en.cppreference.com/w/cpp/chrono) — a referência completa
  de relógios, durations, `time_point` e do calendário.
- [cppreference: `std::ratio`](https://en.cppreference.com/w/cpp/numeric/ratio/ratio)
- [Howard Hinnant, biblioteca `date`](https://github.com/HowardHinnant/date) — o protótipo do
  calendário e dos fusos do C++20, com a documentação mais didática que existe sobre o assunto,
  incluindo o algoritmo `days_from_civil`.
- [P0355 — extensões de calendário e fuso para `<chrono>`](https://wg21.link/p0355) — a
  proposta que virou o Demo 4.
- [Google Benchmark](https://github.com/google/benchmark) e
  [nanobench](https://github.com/martinus/nanobench) — as ferramentas que fazem, de verdade, o
  que o `medir()` do Demo 5 faz de brincadeira.
- [IANA Time Zone Database](https://www.iana.org/time-zones) — o tzdata.
