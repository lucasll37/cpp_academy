# oop — Herança, Polimorfismo e o Modelo de Objeto

## O que é este projeto?

Guia sobre o **modelo de objeto do C++**: como a herança realmente funciona, o que o
compilador gera por baixo do `virtual`, e os quatro modos clássicos de o polimorfismo
sumir sem que ninguém perceba.

Herança e `virtual` aparecem em quase todo subprojeto deste repositório —
[design-of-pattern/](../design-of-pattern/), [plugin_registry/](../plugin_registry/),
[ecs/](../ecs/) — mas sempre *usados*, nunca *explicados*. Este subprojeto fecha essa lacuna.

```bash
make build && ./dist/bin/oop
```

O executável se chama `oop` (é o nome dado em [`meson.build`](meson.build)) e é instalado em
`dist/bin/`. Não existe `cd dist && ninja`: o diretório de **build** deste repositório é
`build/`, e `dist/` é apenas o **prefixo de instalação**.

---

## Antes de começar

Esta seção define **todo** termo usado adiante. Se você já sabe o que é uma vtable, pule
para "Por que estudar isto?".

### Classe, objeto, membro

Uma **classe** é uma descrição de tipo: quais dados ele guarda (**membros de dado**) e o que
ele sabe fazer (**métodos** ou **funções-membro**). Um **objeto** é uma instância concreta
dessa descrição, ocupando bytes reais na memória.

```cpp
struct Ponto { int x; int y; };   // a classe (a descrição)
Ponto p{1, 2};                    // o objeto (8 bytes na pilha)
```

Em C++, `struct` e `class` são a mesma coisa, com uma única diferença: em `struct` os
membros são `public` por padrão; em `class`, `private`.

### Os três níveis de acesso a MEMBRO

| palavra | quem pode acessar |
|---|---|
| `public` | qualquer código |
| `protected` | a própria classe **e as classes derivadas dela** |
| `private` | só a própria classe (e seus `friend`s) |

> ⚠ **`protected` como acesso de membro** e **`protected` como forma de herança** são coisas
> diferentes, e é exatamente aqui que a confusão nasce. O primeiro responde *"quem enxerga
> este membro?"*; o segundo responde *"os membros que eu herdei ficam com qual acesso visto
> de fora?"*. O Demo 1 trata dos dois, nessa ordem.

### Herança e subobjeto de base

**Herdar** é dizer "o meu tipo contém tudo o que aquele tipo tem, mais o que eu acrescentar".
A parte herdada, dentro do objeto derivado, chama-se **subobjeto de base**: um pedaço
contíguo de bytes que *é*, para todos os efeitos, um objeto da base morando dentro do
derivado.

```
objeto Cachorro na memória
┌────────────────────────┬──────────────────┐
│ subobjeto Animal       │ parte do Cachorro│
│ (vptr, nome…)          │ (truques…)       │
└────────────────────────┴──────────────────┘
 ↑
 um Animal* legítimo aponta para cá
```

### Tipo estático e tipo dinâmico

- **Tipo estático**: o tipo que está *escrito* na declaração da variável. O compilador o
  conhece; nunca muda.
- **Tipo dinâmico**: o tipo do objeto que *realmente* está ali em execução.

```cpp
Cachorro rex;
Animal& a = rex;   // tipo ESTÁTICO de `a` = Animal
                   // tipo DINÂMICO do objeto referido = Cachorro
```

Só ponteiros e referências podem ter tipo dinâmico diferente do estático. Uma variável por
valor sempre é exatamente o seu tipo estático — e é daí que vem o *object slicing* (Demo 3).

### Despacho estático e despacho dinâmico

**Despacho** é a decisão de *qual corpo de função vai rodar* nesta chamada.

| | quem decide | quando |
|---|---|---|
| **despacho estático** | o compilador, pelo **tipo estático** | compilação |
| **despacho dinâmico** | o próprio objeto, pelo **tipo dinâmico** | execução |

`virtual` é a palavra que troca o primeiro pelo segundo. É a única troca que ela faz.

### vtable e vptr

O padrão não obriga nenhum mecanismo, mas todo compilador de produção usa este:

- toda classe **polimórfica** (que tem ao menos um método `virtual`) ganha uma **vtable** —
  um array de ponteiros de função, **um array por CLASSE**, guardado uma vez no binário;
- todo **objeto** dessa classe ganha um **vptr** — um ponteiro para a vtable da sua classe
  real, **um ponteiro por OBJETO**, no começo do objeto.

```
        objeto c1 (Circulo)        objeto c2 (Circulo)
        ┌──────────┬───────┐       ┌──────────┬───────┐
        │  vptr    │ dados │       │  vptr    │ dados │
        └────┬─────┴───────┘       └────┬─────┴───────┘
             │                          │
             └──────────┬───────────────┘
                        ▼
              vtable de Circulo  (UMA no programa inteiro)
              ┌──────────────────────────┐
              │ [0] &Circulo::area       │
              │ [1] &Circulo::~Circulo   │
              └──────────────────────────┘
```

Chamar `p->area()` vira, na prática, `(*p->vptr[0])(p)`: uma leitura do vptr, uma leitura da
entrada da tabela, um `call` indireto.

### Classe abstrata e método virtual puro

`virtual double area() const = 0;` — o `= 0` declara um **método virtual puro**: existe na
interface, não tem (obrigatoriamente) corpo, e o derivado *precisa* implementá-lo. Uma classe
com pelo menos um virtual puro é uma **classe abstrata**: **não pode ser instanciada**, só
serve como base. Ainda assim ela pode ter dados, construtores e implementações — inclusive um
corpo para o próprio virtual puro, chamável explicitamente por `Base::area()`.

### RTTI — *Run-Time Type Information*

Os metadados de tipo que o compilador emite junto da vtable para toda classe polimórfica: o
nome do tipo e o mapa da hierarquia. É o que permite, **em execução**, perguntar "que tipo é
este objeto de verdade?" (`typeid`) e "posso convertê-lo com segurança para aquele tipo?"
(`dynamic_cast`). Sem `virtual` não há RTTI, porque não há vptr por onde alcançá-lo.

### ABI, *name mangling* e *demangle*

**ABI** (*Application Binary Interface*) é o contrato binário entre o compilador e o
linker/sistema: como os tipos são dispostos em memória, como os argumentos são passados, e
como os nomes de função viram símbolos no arquivo objeto. GCC e Clang no Linux seguem o
**Itanium C++ ABI**.

**Name mangling** é a codificação do nome: como o C++ tem namespaces, sobrecarga e templates,
o nome fonte não basta como símbolo único. `demo_rtti::Circulo` vira `N9demo_rtti7CirculoE`
(`N` … `E` delimitam um nome com namespace; os números são o comprimento de cada pedaço).
**Demangle** é o caminho de volta — `abi::__cxa_demangle` no GCC/Clang.

### CRTP — *Curiously Recurring Template Pattern*

O idioma em que **a classe base é um template parametrizado pelo próprio derivado**:

```cpp
template <typename D>
struct Contavel { void bump() { static_cast<D&>(*this).ao_contar(); } };

struct Evento : Contavel<Evento> { void ao_contar() { /* ... */ } };
```

Como a base já conhece `D` em **compilação**, ela chama o derivado com um `static_cast` — sem
vtable, sem vptr, sem indireção, e com inline possível. O preço é que nada disso é decidido
em execução: `Contavel<Evento>` e `Contavel<Outro>` são tipos **diferentes** e não cabem no
mesmo container. Ver [templates/demo_crtp.hpp](../templates/demo_crtp.hpp).

### EBO — *Empty Base Optimization*

Um objeto em C++ nunca pode ter `sizeof` zero — dois objetos distintos precisam de endereços
distintos. Então um **membro** de tipo vazio ocupa 1 byte (mais o padding que ele arrastar).
Uma **base** vazia, porém, é dispensada dessa regra: o compilador pode dar a ela o mesmo
endereço do derivado, e ela passa a custar **0 byte**. É por isso que mixins são bases, não
membros. Ver [layout/](../layout/) para a medição isolada e para `[[no_unique_address]]`, que
traz o mesmo efeito para membros no C++20.

### Devirtualizar

Quando o compilador consegue **provar** o tipo dinâmico num ponto do código, ele substitui a
chamada indireta pela chamada direta — e aí pode até inliná-la. `final` ajuda nessa prova:
se `Imutavel` é `final`, nada pode derivar dela, logo um `Imutavel*` só pode apontar para um
`Imutavel`.

### Substituibilidade e o Princípio de Liskov (LSP)

Herança pública é uma promessa: **todo lugar que funciona com uma `Base` continua funcionando
se você entregar um `Derivado`**. Isso é o *Princípio de Substituição de Liskov*. O
compilador verifica a parte sintática (as assinaturas existem); a parte semântica (o
comportamento não surpreende) é responsabilidade sua, e é onde a herança pública erra sem
que nada avise. Ver [solid/lsp/](../solid/lsp/).

### NVI — *Non-Virtual Interface*

Idioma em que a interface pública é **não-virtual** e os pontos de variação são `virtual`
**privados**. O método público faz o que é invariável (validar, aplicar argumentos padrão,
registrar log) e delega ao virtual privado:

```cpp
class Relatorio {
public:
    void gerar(int detalhe = 3) const { fazer_gerar(detalhe); }  // não-virtual, público
private:
    virtual void fazer_gerar(int detalhe) const = 0;             // virtual, privado
};
```

Sim, um derivado **pode** sobrescrever um virtual privado: sobrescrever e chamar são
permissões diferentes.

---

## Por que estudar isto?

### Primeiro, o problema que o polimorfismo resolve

Sem despacho dinâmico, "fazer coisa diferente para cada tipo" vira um `switch` sobre uma
etiqueta que **você edita toda vez que acrescenta um tipo**:

```cpp
enum class Tipo { Circulo, Quadrado };            // ← editar aqui
struct Forma { Tipo tipo; double r, l; };

double area(const Forma& f) {
    switch (f.tipo) {                             // ← e aqui
        case Tipo::Circulo:  return 3.14159265 * f.r * f.r;
        case Tipo::Quadrado: return f.l * f.l;
    }
    return 0;
}
```

Acrescentar `Triangulo` obriga a mexer no `enum`, na struct (que engorda para caber os campos
de todo mundo) e em **cada** `switch` espalhado pelo programa — e o compilador só reclama de
alguns deles.

Com `virtual`, o tipo novo é uma classe nova e **nenhuma linha do código que consome muda**.
É esse o negócio que você está fechando. O resto deste README é sobre o preço.

### Segundo, os erros da área são silenciosos

O compilador só pega alguns:

| Erro | O compilador avisa? |
|---|---|
| Assinatura errada ao "sobrescrever" | Só se o nome colidir (`-Woverloaded-virtual`) |
| Destrutor não-virtual na base | Só no `delete` (`-Wdelete-non-virtual-dtor`) |
| Object slicing numa cópia | **Não** |
| Passar hierarquia por valor | **Não** |
| Argumento padrão em virtual | **Não** |
| Chamar virtual dentro do construtor | **Não** |
| Herança pública que viola o LSP | **Não** |

---

## Estrutura de arquivos

```
src/oop/
├── main.cpp                        ← ponto de entrada, cor medium_purple
├── demo_inheritance.hpp            ← 1 · public/protected/private, ordem, ctor herdado, name hiding
├── demo_virtual.hpp                ← 2 · vtable, vptr, override, final, dtor virtual, abstrata
├── demo_slicing.hpp                ← 3 · object slicing e como torná-lo incompilável
├── demo_multiple_inheritance.hpp   ← 4 · diamante, herança virtual, mixins CRTP
├── demo_rtti.hpp                   ← 5 · typeid, type_index, dynamic_cast e seu custo
├── meson.build
└── README.md
```

---

## O polimorfismo em 20 linhas

Antes de qualquer armadilha, o mecanismo funcionando. Este é um programa completo:

```cpp
#include <memory>
#include <vector>
#include <cstdio>

struct Forma {                                   // a INTERFACE
    virtual ~Forma() = default;                  // obrigatório: veja o Demo 2
    virtual double area() const = 0;             // `= 0` → cada derivado implementa
};

struct Circulo : Forma {
    double r;
    explicit Circulo(double raio) : r{raio} {}
    double area() const override { return 3.14159265 * r * r; }
};

struct Quadrado : Forma {
    double l;
    explicit Quadrado(double lado) : l{lado} {}
    double area() const override { return l * l; }
};

int main() {
    std::vector<std::unique_ptr<Forma>> formas;  // dono dos objetos, sem vazar
    formas.push_back(std::make_unique<Circulo>(1.0));
    formas.push_back(std::make_unique<Quadrado>(2.0));

    double total = 0;
    for (const auto& f : formas) total += f->area();   // ← o despacho dinâmico
    std::printf("area total = %.4f\n", total);
}
```

```
area total = 7.1416
```

Cinco decisões, todas deliberadas:

1. **`= 0`** torna `Forma` abstrata: ninguém instancia uma "forma genérica" por engano.
2. **`virtual ~Forma()`** existe porque o `unique_ptr<Forma>` vai destruir pela base.
3. **`override`** faz o compilador conferir que a assinatura bate com a da base.
4. **`unique_ptr`** é o dono: nenhum `new` cru, nenhum `delete`, nenhum vazamento.
5. **`vector<unique_ptr<Forma>>`**, não `vector<Forma>` — a razão está no Demo 3.

E a frase que resume o valor: **acrescentar `Triangulo` não exige tocar em uma linha do
`main`.** Basta uma classe nova com `area()`.

---

## Demo 1: Herança

### As três formas de HERDAR

A palavra-chave antes do nome da base controla o acesso **máximo** que os membros herdados
terão vistos de fora do derivado:

| Forma | Significado | `public` da base vira | Conversão implícita para a base? |
|---|---|---|---|
| `public` | "é um" | `public` | sim |
| `protected` | "é implementado em termos de", visível aos netos | `protected` | não (fora da hierarquia) |
| `private` | "é implementado em termos de" | `private` | não |

Em todos os casos, membros `private` **da base** continuam inacessíveis ao derivado — herdar
não é uma chave-mestra.

Só a herança pública cria **substituibilidade** (a promessa do LSP descrita acima). As outras
duas são composição com sintaxe diferente — e composição costuma ser a escolha melhor:

```cpp
class Carro : private Motor {          // herança privada
public:
    void dar_partida() { ligar(); }    // acessível AQUI DENTRO
    using Motor::desligar;             // reexpõe UM membro, seletivamente
};

class CarroComposto {                  // composição — o default recomendado
public:
    void dar_partida() { motor_.ligar(); }
private:
    Motor motor_;                      // TEM UM motor, não É UM motor
};
```

```
Conversão implícita para a base:
  MotorEletrico (pública) : sim
  Carro         (privada) : não
```

Prefira composição, exceto quando você precisa (a) sobrescrever um método virtual da base ou
(b) aproveitar a EBO numa base vazia.

### Ordem de construção e destruição

```
Construção: base → membros (na ordem de DECLARAÇÃO) → corpo do ctor
Destruição: exatamente o inverso
```

Medido no demo:

```
construindo Derivada:
  + Peca('peça-da-base')      ← membro da BASE, antes de tudo
+ Base::Base()                ← corpo do ctor da base
  + Peca('primeira')          ← membros do derivado, na ordem de DECLARAÇÃO
  + Peca('segunda')
+ Derivada::Derivada()        ← corpo do ctor do derivado
(objeto pronto)
destruindo:
- Derivada::~Derivada()
  - Peca('segunda')
  - Peca('primeira')
- Base::~Base()
  - Peca('peça-da-base')
```

A ordem da **lista de inicialização é ignorada**. Escrevê-la fora de ordem compila, roda na
ordem de declaração mesmo assim, e rende um `-Wreorder`. Esse aviso é sério: se um membro
inicializa a partir de outro, você lê algo ainda não construído.

### Herdando construtores com `using Base::Base;`

Sem isso, cada construtor da base precisa ser reescrito à mão no derivado — puro boilerplate
que envelhece mal:

```cpp
struct Conexao {
    std::string host;
    int         porta;
    Conexao(std::string h, int p) : host(std::move(h)), porta(p) {}
    explicit Conexao(std::string h) : host(std::move(h)), porta(80) {}
};

struct ConexaoSegura : Conexao {
    using Conexao::Conexao;    // herda os DOIS construtores acima
    bool tls = true;           // ⚠ precisa de inicializador default
};
```

```
a = api.exemplo.com:443 (tls=true)
b = exemplo.com:80 (tls=true)
```

⚠ **A armadilha:** os construtores herdados sabem inicializar a parte-base, mas **não sabem
nada dos membros novos do derivado**. Por isso `tls` precisa de um inicializador default na
declaração (`= true`); sem ele, `ConexaoSegura a{"host", 443}` deixaria `tls` indeterminado.
O construtor padrão, o de cópia e o de move **nunca** são herdados — esses o compilador
continua gerando por conta própria.

### Ocultação de nome (*name hiding*)

Declarar qualquer membro com um nome no derivado esconde **todos** os overloads daquele
nome na base:

```cpp
struct Impressora     { void imprimir(int); void imprimir(const std::string&); };
struct ImpressoraRuim : Impressora { void imprimir(double); };   // escondeu os dois

ImpressoraRuim r;
r.imprimir(42);          // ⚠ chama imprimir(double) — 42 vira 42.0
r.imprimir("texto");     // ✗ nem compila
```

```
ImpressoraRuim::imprimir(double) = 3.14
ImpressoraRuim::imprimir(double) = 42     ← 42 virou double
```

A busca de nome para assim que encontra o nome, no escopo mais interno, **antes** da
resolução de overload. A correção é uma linha:

```cpp
struct ImpressoraBoa : Impressora {
    using Impressora::imprimir;      // traz de volta todos os overloads da base
    void imprimir(double d);
};
```

```
ImpressoraBoa::imprimir(double) = 3.14
Impressora::imprimir(int) = 42            ← agora acha o overload certo
Impressora::imprimir(str) = texto
```

### ⚠ Quando a herança pública compila e mesmo assim está errada (LSP)

`Quadrado : Retangulo` é o exemplo clássico — e este subprojeto o tem de verdade, em
`demo_rtti.hpp`:

```cpp
struct Retangulo : Forma {
    double largura = 1.0, altura = 1.0;
    double area() const override { return largura * altura; }
};
struct Quadrado : Retangulo {
    explicit Quadrado(double lado) : Retangulo(lado, lado) {}
};
```

Compila, e o `dynamic_cast<Retangulo*>` do Demo 5 confirma que um `Quadrado` **é um**
`Retangulo` para o sistema de tipos. Mas a promessa da herança pública é *substituibilidade*,
e ela quebra assim:

```cpp
void alarga(Retangulo& r) { r.largura = 10; }   // função escrita para Retangulo

Quadrado q{3.0};
alarga(q);            // compila
// q.area() == 30, e q não é mais um quadrado — o invariante do tipo foi violado
```

Nenhum aviso, nenhum sanitizer, nenhuma exceção: só um objeto que mente sobre o próprio nome.
O teste prático é sempre este — **"existe algum código escrito para a base que passa a estar
errado se eu lhe entregar o derivado?"**. Se existe, a herança pública é a ferramenta errada:
use composição, ou dois tipos irmãos sem relação. Ver [solid/lsp/](../solid/lsp/).

---

## Demo 2: Funções Virtuais

### O mecanismo, e o que ele custa

`virtual` troca despacho estático por despacho dinâmico:

```
via Forma& (o objeto REAL é um Círculo):
  Forma (estático)      ← não-virtual: o compilador resolveu pelo tipo da REFERÊNCIA
  Círculo (dinâmico)    ← virtual: o objeto resolveu pelo próprio vptr
```

O custo em memória é **um ponteiro por objeto**, medido no demo:

```
sizeof(int)        = 4 bytes
sizeof(SemVirtual) = 8 bytes (2 ints)
sizeof(ComVirtual) = 16 bytes (vptr 8 + int 4 + int 4 — sem padding)
diferença          = 8 bytes = 1 ponteiro
```

> Repare: **não há padding nenhum** aqui. O vptr ocupa 8 bytes, os dois `int` ocupam 4 + 4, e
> 8 + 4 + 4 = 16, que já é múltiplo do alinhamento de 8. Uma versão anterior deste README
> dizia "2 ints + vptr + padding" — estava errado. Ver [layout/](../layout/) para os casos em
> que o padding de fato aparece.

E a prova de que a vtable é **uma por classe** enquanto o vptr é **um por objeto**:

```
vptr de dois Círculos são iguais? true   ← uma vtable POR CLASSE
vptr de Círculo == vptr de Forma?  false  ← o vptr de cada objeto
                                            aponta para a vtable da classe REAL
```

Cuidado com a leitura da segunda linha: ela **não** demonstra que o vptr é por objeto (dois
`Circulo` distintos têm o mesmo valor de vptr, como mostra a primeira linha). O que ela
demonstra é que o **valor** do vptr depende da classe real do objeto — é assim que o mesmo
`Forma*` acaba chamando corpos diferentes.

> ⚠ **O truque usado para imprimir isso não é portável.** O demo lê o vptr com
> `*reinterpret_cast<void* const*>(&c1)`, que acessa a representação do objeto por um tipo
> incompatível: é **undefined behavior** formal (violação de *strict aliasing*) e só funciona
> porque o Itanium ABI coloca o vptr no offset 0. É uma espiada deliberada no layout, feita
> aqui para tornar o mecanismo visível — nunca escreva isso em código de produção. Ver
> [undefined_behavior/](../undefined_behavior/) e [layout/](../layout/).

O custo maior costuma não ser a memória nem a indireção: é que a chamada virtual **não pode
ser inlined**, e com ela morrem as otimizações que dependiam de ver o corpo da função.

### `override` — o bug silencioso

```cpp
struct Sensor           { virtual double ler(int) const; };
struct Quebrado : Sensor { double ler(double) const; };       // ✗ não sobrescreve nada
struct Certo    : Sensor { double ler(int) const override; }; // ✓ verificado
```

```
chamando via Sensor& em SensorQuebrado:
  Sensor::ler(int)          ← foi para a BASE: a assinatura não bateu
chamando via Sensor& em SensorCerto:
  SensorCerto::ler(int)     ← sobrescreveu de verdade
```

`double` em vez de `int` cria uma função **nova**, não uma sobrescrita. Compila, roda, e toda
chamada polimórfica continua indo para a base. Marque **sempre** `override`: custo zero, e o
erro vira erro de compilação.

`final` vai além — impede herdar a classe ou sobrescrever o método, documenta a intenção e
permite ao compilador **devirtualizar**.

### Destrutor virtual: o vazamento que ele evita, medido

```cpp
struct BaseComDtorVirtual { virtual ~BaseComDtorVirtual(); };
struct DerivadaSegura : BaseComDtorVirtual {
    std::vector<int> recurso = std::vector<int>(1000);
    ~DerivadaSegura() override;
};
```

Com destrutor virtual, deletando pela base, os **dois** destrutores rodam:

```
COM destrutor virtual, deletando pela base:
  - DerivadaSegura::~ (libera 1000 ints)     ← rodou
  - BaseComDtorVirtual::~
```

Sem destrutor virtual, `delete ptr_base` é **undefined behavior**: o destrutor do derivado
nunca roda e os 1000 `int` do `std::vector` vazam. A linha está comentada no código de
propósito — o GCC recusa com `-Wdelete-non-virtual-dtor` (que vem em `-Wall`) e o
AddressSanitizer aborta o programa antes de você ver a saída.

> Se a classe tem qualquer método virtual, o destrutor também precisa ser virtual.
> Se ela não é feita para herança, marque-a `final` — aí o destrutor pode continuar
> não-virtual e você não paga o vptr.

### Virtual dentro do construtor

```
BaseCtor::BaseCtor() chamando quem_sou():
  BaseCtor                                     ← não desceu para o derivado
objeto pronto, chamando quem_sou() de novo:
  DerivadaCtor (nome='derivada-inicializada')
```

Durante o construtor da base, o vptr ainda é o da base — o objeto **ainda é** uma base. Faz
sentido: se despachasse para o derivado, `nome` seria lido antes de existir. A linguagem
escolheu a opção segura. O mesmo vale no destrutor, na direção inversa.

### ✗ Argumento padrão em virtual

O corpo é escolhido **dinamicamente**, o argumento padrão **estaticamente**. Os dois
discordam:

```cpp
struct BaseArg          { virtual void f(int x = 10) const; };
struct DerivadaArg : BaseArg { void f(int x = 20) const override; };

DerivadaArg d;  BaseArg& b = d;
d.f();   // DerivadaArg::f(x=20)
b.f();   // DerivadaArg::f(x=10)   ← corpo do derivado, default da base
```

```
d.f();   // tipo estático = DerivadaArg
  DerivadaArg::f(x=20)
b.f();   // tipo estático = BaseArg, objeto = DerivadaArg
  DerivadaArg::f(x=10)
```

Solução: **NVI** (definido em "Antes de começar") — o método público não-virtual aplica o
default e delega a um virtual privado, que não tem argumento padrão nenhum.

### Classe abstrata e virtual puro

```cpp
struct Serializavel {
    virtual std::string serializar() const = 0;   // = 0 → virtual PURO
    virtual ~Serializavel() = default;
};

struct Ponto : Serializavel {
    int x = 1, y = 2;
    std::string serializar() const override { return fmt::format("{{x:{},y:{}}}", x, y); }
};

// Serializavel s;                                // ✗ erro: classe abstrata
std::vector<std::unique_ptr<Serializavel>> itens;
itens.push_back(std::make_unique<Ponto>());
```

```
{x:1,y:2}
```

Quatro fatos que o `= 0` costuma esconder:

1. A classe **não pode ser instanciada** — nem por valor, nem em `vector<Serializavel>`. É a
   forma mais forte de proibir o *object slicing* (Demo 3).
2. Ela **ainda pode ter dados, construtores e métodos implementados**. "Abstrata" não é
   sinônimo de "interface pura sem estado".
3. Um virtual **puro pode ter corpo**. O corpo não conta como implementação (o derivado
   continua obrigado a fornecer a sua), mas é chamável explicitamente por
   `Base::serializar()` — útil para uma implementação default compartilhada.
4. O **destrutor virtual puro** é o único caso em que o corpo é *obrigatório*: escreva
   `virtual ~Base() = 0;` e, fora da classe, `Base::~Base() {}`. Serve para tornar uma classe
   abstrata quando não há nenhum outro método a marcar como puro.

---

## Demo 3: Object Slicing

Copiar um derivado para uma variável do tipo base não é conversão, é **amputação** — a
variável destino tem exatamente `sizeof(Base)` bytes, e o vptr dela é o da base:

```cpp
Cachorro rex("Rex", 5);
Animal copia = rex;      // ← o corte
```

```
original : Cachorro 'Rex' com 5 truques → 'Au!'
Animal copia = rex;
cortado  : Animal 'Rex' → '...'          ← perdeu os truques E o comportamento
Animal& ref = rex;
via ref  : Cachorro 'Rex' com 5 truques → 'Au!'   ← referências preservam o objeto real
```

O caso comum não é a atribuição — é o **parâmetro**:

```cpp
void f(Animal a);         // ✗ corta
void f(const Animal& a);  // ✓ preserva
```

```
[por valor]      Animal 'Rex' → '...'
[por referência] Cachorro 'Rex' com 5 truques → 'Au!'
```

A diferença entre as duas assinaturas é **um caractere**. Em hierarquia polimórfica, nunca
passe por valor.

### `std::vector<Base>` é sempre errado

O container reserva `sizeof(Base)` por slot. Não há espaço físico para o derivado:

```
sizeof(Animal)   = 40 bytes
sizeof(Cachorro) = 48 bytes
Um vector<Animal> reserva 40 bytes por slot — não cabe.
vector<Animal>:
  Animal 'Rex' → '...'
  Animal 'Bob' → '...'
↑ dois Cachorros entraram, dois Animals saíram
```

A correção é guardar ponteiros — `std::vector<std::unique_ptr<Animal>>` — que têm tamanho
fixo e apontam para o objeto real na *heap*:

```
Cachorro 'Rex' com 5 truques → 'Au!'
Cachorro 'Bob' com 3 truques → 'Au!'
Animal 'genérico' → '...'
↑ cada objeto manteve o tipo real e o comportamento
```

O `unique_ptr` ainda garante a destruição correta — desde que `Animal` tenha destrutor
virtual (e tem).

### Tornando o corte incompilável

| Receita | Efeito |
|---|---|
| Base abstrata (um `= 0`) | Nem instancia — a mais forte |
| Cópia `= delete` na base | Derivados também perdem a cópia |
| Cópia `protected` na base | Derivados copiam entre si; o mundo externo não corta |

---

## Demo 4: Herança Múltipla e o Diamante

### O caso seguro: várias interfaces

Interfaces puras (só virtuais puros, sem estado) não têm o problema do diamante — não há
dados a duplicar. É 90% dos usos legítimos de herança múltipla.

Mas atenção ao layout: os **subobjetos de base** ficam em **offsets diferentes** dentro do
objeto, e converter o ponteiro **ajusta o endereço**:

```
&b            = 0x7ffed95bd410
(Desenhavel*) = 0x7ffed95bd410
(Salvavel*)   = 0x7ffed95bd418   ← mesmo objeto, endereço diferente
```

Por isso comparar ponteiros de bases diferentes não diz se são o mesmo objeto — use
`dynamic_cast<void*>`, que devolve o endereço do objeto **mais derivado**.

### O diamante

```
    A       Sem `virtual` : Multifuncional contém DOIS Dispositivos
   / \      Com `virtual` : contém UM
  B   C
   \ /
    D
```

Sem herança virtual:

```
+ Dispositivo()
+ Dispositivo()                     ← o construtor rodou DUAS vezes
mf.Scanner::id   = 'via-scanner'
mf.Impressor::id = 'via-impressor'  ← dois estados independentes
consumo total contado em dobro: 10 + 10 = 20
`Dispositivo* p = &mf;` nem compila — conversão ambígua
sizeof(Multifuncional) = 96 bytes
```

Com `virtual Dispositivo`:

```
+ Dispositivo('mfp-central')        ← uma vez só
+ Scanner()
+ Impressor()
+ Multifuncional()
mf.id = mf.Scanner::id = mf.Impressor::id = 'mfp-01'
consumo (único): 10
sizeof com virtual = 64 bytes (vs 96 sem)
```

**A regra que surpreende:** quem inicializa a base virtual é a classe **mais derivada**. As
chamadas a `Dispositivo(...)` dentro de `Scanner` e `Impressor` são simplesmente ignoradas —
por isso o `id` inicial impresso é `'mfp-central'`, escolhido por `Multifuncional`.

**Por que é caro:** com base virtual, o subobjeto compartilhado não pode ficar num offset fixo
conhecido em compilação — ele depende de qual é a classe mais derivada. Cada subobjeto passa a
carregar um ponteiro extra (**vbptr**, *virtual base pointer*) ou um offset na própria vtable,
e todo acesso a um membro da base virtual vira uma indireção a mais em execução. Foi o que
comprou a queda de 96 → 64 bytes: menos duplicação de dados, mais metadados de navegação.

### Mixins com CRTP

Se a variação é conhecida em compilação, não é preciso vtable nenhuma:

```cpp
struct Evento : Contavel<Evento>, Imprimivel<Evento> { /* ... */ };
```

```
clique (contador=2)
sizeof(Evento) = 40 bytes — sem vptr
```

Os mixins são bases **vazias**: o compilador aplica **EBO** e elas ocupam 0 byte. Um membro do
mesmo tipo vazio ocuparia pelo menos 1 byte. Ver [templates/demo_crtp.hpp](../templates/demo_crtp.hpp)
e [layout/](../layout/).

---

## Demo 5: RTTI — *Run-Time Type Information*

RTTI são os metadados de tipo que o compilador emite para toda classe polimórfica, alcançáveis
pelo vptr. Duas operações os consomem: `typeid` e `dynamic_cast`.

### `typeid`

Em objeto **polimórfico**, é avaliado em runtime e devolve o tipo **dinâmico**. Em tipo
não-polimórfico, é resolvido em compilação e devolve o tipo **estático**:

```
typeid(c).name()  cru      = N9demo_rtti7CirculoE
demanglado                 = demo_rtti::Circulo
typeid(f) via Forma&       = demo_rtti::Circulo          ← DINÂMICO
typeid(Forma)              = demo_rtti::Forma
não-polimórfica via base&  = demo_rtti::NaoPolimorfica   ← ESTÁTICO
```

O nome cru vem *manglado* no formato do **Itanium ABI** (`N` … `E` delimitam um nome
qualificado; os dígitos são o comprimento de cada componente) e precisa passar por
`abi::__cxa_demangle` para ficar legível. **Nunca serialize esse nome**: ele muda entre
compiladores e entre versões.

### `typeid` vs `dynamic_cast`

```
typeid(fq) == typeid(Quadrado)  ? true
typeid(fq) == typeid(Retangulo) ? false   ← teste EXATO, não aceita base
dynamic_cast<Retangulo*>(&quadrado)       → válido, aceita derivados
```

`dynamic_cast` tem duas formas, com tratamentos de falha diferentes:

| Forma | Falha | Quando usar |
|---|---|---|
| ponteiro | devolve `nullptr` | é um **teste**: falhar é esperado |
| referência | lança `std::bad_cast` | falhar é **excepcional** |

```
demo_rtti::Circulo           não é Retangulo
demo_rtti::Retangulo         é Retangulo → diagonal = 5.00
demo_rtti::Quadrado          é Retangulo → diagonal = 7.07
dynamic_cast<Retangulo&> num Circulo → std::bad_cast: std::bad_cast
```

Upcast (derivado → base) é implícito e não precisa de `dynamic_cast`. Downcast e *sidecast*
(entre bases irmãs) exigem.

### O custo, medido

200 000 iterações de cada operação, `./dist/bin/oop` nesta máquina (GCC 13.3, build de debug
`-O0`, x86-64). Os números absolutos são da sua máquina; **a razão entre eles é a lição**:

| operação | total | por operação | × sobre a chamada virtual |
|---|---|---|---|
| chamada virtual | ~520 µs | ~2,6 ns | **1,0×** |
| `typeid ==` | ~551 µs | ~2,8 ns | **1,1×** |
| `dynamic_cast` | ~2 980 µs | ~14,9 ns | **5,7×** |

```
200000 iterações de cada:
  chamada virtual   :    520 µs
  dynamic_cast      :   2980 µs   ← a mais cara, por boa margem
  typeid ==         :    551 µs
```

Por que a diferença: `typeid ==` compara dois `type_info` — na prática, uma comparação de
ponteiros. `dynamic_cast` é uma **chamada de biblioteca** (`__dynamic_cast` da libstdc++) que
**percorre a árvore de herança em runtime**: não é O(1), e fica mais caro quanto mais profunda
e ramificada for a hierarquia. Recompilar com `-O2` quase não muda o quadro — o acumulador é
`volatile` de propósito, e `__dynamic_cast` não inlina.

Reproduza você mesmo: rode `./dist/bin/oop` duas ou três vezes e olhe a seção 5.4.

### Alternativas

Quase toda cadeia de `if (dynamic_cast<X>)` é um método virtual que não foi escrito.

1. **Método virtual** — o objeto responde por si. É a primeira coisa a tentar.
2. **Visitor** — despacho duplo sem RTTI. Ver
   [design-of-pattern/visitor/](../design-of-pattern/visitor/).
3. **`std::variant` + `std::visit`** — hierarquia **fechada**, sem herança nenhuma, contígua na
   memória, e o compilador garante a exaustividade dos casos. Ver
   [vocabulary_types/](../vocabulary_types/).
4. **Type erasure** — polimorfismo **sem** obrigar o tipo a herdar de nada, com semântica de
   valor. Ver [type_erasure/](../type_erasure/), que mede as quatro alternativas lado a lado.

Use RTTI quando você **não** controla a hierarquia (biblioteca de terceiros), quando está
escrevendo serialização/reflexão genérica, ou quando precisa de um registro dirigido por tipo
(`std::type_index` como chave — ver [plugin_registry/](../plugin_registry/) e [ecs/](../ecs/)).

---

## Regras de ouro

| Situação | Faça |
|---|---|
| Classe tem método virtual | Destrutor virtual também |
| Classe não é feita para herança | Marque `final` |
| Sobrescrevendo um virtual | Sempre `override` |
| Método derivado com nome da base | `using Base::nome;` para não esconder overloads |
| Derivado sem construtores próprios | `using Base::Base;` — e dê default aos membros novos |
| Passando objeto polimórfico | Por referência ou ponteiro — nunca por valor |
| Guardando hierarquia em container | `vector<unique_ptr<Base>>` |
| Precisa de argumento padrão em virtual | Use NVI |
| Reutilizar implementação sem "é um" | Composição |
| Antes de herdar publicamente | Aplique o teste do LSP |
| Encontrou `if (dynamic_cast<X>)` | Provavelmente falta um método virtual |

### Flags de diagnóstico, num só lugar

```bash
-Wall -Wextra -Wpedantic \
-Wnon-virtual-dtor      \  # classe com virtual e destrutor público não-virtual
-Wdelete-non-virtual-dtor \# o `delete` que é UB (já vem em -Wall)
-Woverloaded-virtual    \  # o derivado escondeu um virtual sobrecarregado (já vem em -Wall)
-Wsuggest-override      \  # sobrescreve sem escrever `override`
-Wreorder                  # lista de inicialização fora da ordem de declaração (já vem em -Wall)
```

Nenhuma dessas flags pega slicing, passagem por valor, argumento padrão em virtual, chamada
virtual no construtor nem violação de LSP. Esses continuam por sua conta — é por isso que este
subprojeto existe.

---

## Como validar

O caminho normal, com o build do repositório:

```bash
make build            # compila em build/ (o conanbuild.sh é carregado pelo Makefile)
./dist/bin/oop        # o binário instalado
```

Sem o `source build/conanbuild.sh` que o `Makefile` executa, o `meson` nem encontra o `fmt` —
por isso não tente `meson compile` na mão sem carregá-lo antes.

Compilação isolada, útil para experimentar sem tocar no build do repositório:

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)

g++ -std=c++20 -Wall -Wextra -Wpedantic -Wnon-virtual-dtor -Wsuggest-override \
    -I$FMTP/include src/oop/main.cpp -o /tmp/oop -L$FMTP/lib -lfmtd && /tmp/oop

g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/oop/main.cpp -o /tmp/oopsan -L$FMTP/lib -lfmtd && /tmp/oopsan
```

**Estado atual:** compila sem nenhum warning, inclusive com `-Wnon-virtual-dtor` e
`-Wsuggest-override`, e roda com **0 achados sob ASan+UBSan**.

Um `#pragma GCC diagnostic` local em `demo_virtual.hpp` silencia `-Woverloaded-virtual` em
volta de `SensorQuebrado` — o aviso está **certo**, e é justamente o objeto da demonstração;
o comentário no fonte explica.

### O que se espera ver

Cinco cabeçalhos em roxo, na ordem:

```
1 · Herança — public, protected, private
2 · Funções virtuais — vtable, vptr, override
3 · Object slicing — o polimorfismo que some
4 · Herança múltipla e o diamante
5 · RTTI — typeid, type_index, dynamic_cast
```

E, dentro deles, os pontos verificáveis:

| Seção | O que conferir |
|---|---|
| 1.1 | `MotorEletrico (pública) : sim` / `Carro (privada) : não` |
| 1.2 | a base constrói primeiro e destrói por último |
| 1.3 | `b = exemplo.com:80` — o construtor de 1 argumento foi herdado |
| 1.4 | `ImpressoraRuim::imprimir(double) = 42` — o `42` virou `double` |
| 2.2 | `sizeof(ComVirtual) = 16` e `diferença = 8 bytes = 1 ponteiro` |
| 2.3 | `Sensor::ler(int)` na primeira chamada — foi para a base |
| 2.4 | os **dois** destrutores rodam quando a base tem `virtual ~` |
| 2.5 | `BaseCtor` durante o construtor, `DerivadaCtor` depois |
| 2.6 | `DerivadaArg::f(x=10)` — corpo do derivado, default da base |
| 3.3 | `sizeof(Animal) = 40` vs `sizeof(Cachorro) = 48` |
| 4.1 | `(Salvavel*)` 8 bytes adiante de `&b` |
| 4.2 / 4.3 | `Dispositivo()` duas vezes / uma vez; 96 → 64 bytes |
| 5.1 | `N9demo_rtti7CirculoE` cru, `demo_rtti::Circulo` demanglado |
| 5.4 | `dynamic_cast` várias vezes mais caro que a chamada virtual |

Última linha, em verde:

```
✓ Todos os demos de orientação a objetos concluídos!
```

---

## Referências

### Neste repositório

- [`src/layout/`](../layout/) — o custo do vptr e da herança virtual, medido byte a byte;
  padding, EBO e `[[no_unique_address]]`
- [`src/solid/lsp/`](../solid/lsp/) — o Princípio de Substituição de Liskov, o teste que a
  herança pública precisa passar
- [`src/type_erasure/`](../type_erasure/) — polimorfismo sem obrigar ninguém a herdar, com as
  quatro alternativas medidas lado a lado
- [`src/vocabulary_types/`](../vocabulary_types/) — `std::variant` + `std::visit`: hierarquia
  fechada, contígua e com exaustividade checada
- [`src/casts/`](../casts/) — `dynamic_cast` no meio dos outros quatro casts
- [`src/undefined_behavior/`](../undefined_behavior/) — *strict aliasing* e os sanitizers
- [`src/templates/demo_crtp.hpp`](../templates/demo_crtp.hpp) — CRTP e polimorfismo estático
- [`src/design-of-pattern/visitor/`](../design-of-pattern/visitor/) — despacho duplo sem RTTI
- [`src/plugin_registry/`](../plugin_registry/) e [`src/ecs/`](../ecs/) — `std::type_index`
  como chave de registro

### Fora

- cppreference: [Virtual functions](https://en.cppreference.com/w/cpp/language/virtual),
  [Object slicing](https://en.cppreference.com/w/cpp/language/derived_class),
  [Derived classes / multiple inheritance](https://en.cppreference.com/w/cpp/language/derived_class),
  [`typeid`](https://en.cppreference.com/w/cpp/language/typeid),
  [`dynamic_cast`](https://en.cppreference.com/w/cpp/language/dynamic_cast),
  [`using` declaration](https://en.cppreference.com/w/cpp/language/using_declaration)
- *C++ Core Guidelines*: **C.35** (base polimórfica precisa de destrutor público virtual ou
  protegido não-virtual), **C.67** (base polimórfica deve suprimir a cópia pública),
  **C.128** (virtual, override e final: escolha exatamente um), **C.129** (herança de
  implementação vs herança de interface), **C.146** (`dynamic_cast` só quando não há
  alternativa)
- *Itanium C++ ABI* — a especificação de layout, vtable e *name mangling* usada por GCC e
  Clang: <https://itanium-cxx-abi.github.io/cxx-abi/abi.html>
- Scott Meyers, *Effective C++*, 3ª ed. — itens 32 a 40 são exatamente este subprojeto
- `nm -C` e `c++filt` — desmanglar símbolos na linha de comando; `pahole(1)` (pacote
  `dwarves`) — ver o layout real com o vptr
