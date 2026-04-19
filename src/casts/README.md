# casts — Guia Completo de Type Casting em C++

## O que é este projeto?

Demonstra os **cinco mecanismos de conversão de tipos** em C++, desde o mais seguro até o mais perigoso. Entender cada um e saber quando (não) usá-los é essencial para escrever código C++ correto e seguro.

---

## Hierarquia de segurança

```
static_cast          ← verificado em compile-time, seguro para conversões conhecidas
  ↕
dynamic_cast         ← verificado em runtime via RTTI, falha graciosamente
  ↕
const_cast           ← remove/adiciona const, perigoso se objeto é genuinamente const
  ↕
reinterpret_cast     ← reinterpretação de bytes, muito perigoso
  ↕
C-style cast (Type)  ← faz qualquer coisa, EVITAR sempre
```

---

## Estrutura de arquivos

```
casts/
├── main.cpp                ← ponto de entrada, executa todos os demos
├── demo_static_cast.hpp    ← conversões verificadas em compile-time
├── demo_dynamic_cast.hpp   ← downcast seguro com RTTI
├── demo_const_cast.hpp     ← manipulação de qualificador const
├── demo_reinterpret_cast.hpp ← reinterpretação de bits
└── demo_c_style.hpp        ← por que evitar C-style casts
```

---

## 1. `static_cast` — O Cast Padrão

`static_cast` pede ao compilador que verifique se a conversão faz sentido **em tempo de compilação**. Se não fizer, erro de compilação (não crash em runtime).

### Quando usar:

```cpp
// ✓ Conversões numéricas explícitas
double d = 3.99;
int i = static_cast<int>(d);        // trunca para 3

// ✓ Upcast em hierarquia (sempre seguro)
Derived* der = new Derived();
Base* base = static_cast<Base*>(der);   // OK

// ✓ Downcast em hierarquia (quando você SABE o tipo)
Base* b = new Derived();
Derived* d = static_cast<Derived*>(b);  // OK se b for realmente Derived

// ✗ NÃO funciona entre tipos não relacionados
int* p = static_cast<int*>(3.14);  // ERRO de compilação
```

### Diferença de C-style:
```cpp
double d = 3.99;
int i1 = (int)d;             // C-style — compila mas é ambíguo
int i2 = static_cast<int>(d); // C++ — intenção clara, pesquisável no código
```

---

## 2. `dynamic_cast` — Downcast Seguro com RTTI

`dynamic_cast` usa **RTTI (Run-Time Type Information)** para verificar em tempo de execução se o downcast é válido. É a única opção segura quando você não tem certeza do tipo real.

### Pré-requisitos:
- A classe base deve ter **pelo menos uma função virtual** (para ter RTTI)
- Compilado sem `-fno-rtti`

```cpp
struct Animal {
    virtual ~Animal() = default; // virtual necessário para RTTI
    virtual void falar() const = 0;
};

struct Cachorro : Animal {
    void falar() const override { fmt::print("Au!\n"); }
    void buscar() const { fmt::print("buscando...\n"); }
};

struct Gato : Animal {
    void falar() const override { fmt::print("Miau!\n"); }
};

Animal* animal = new Cachorro();

// Com ponteiro: retorna nullptr se falhar (não lança exceção)
Cachorro* c = dynamic_cast<Cachorro*>(animal);
if (c) {
    c->buscar();   // seguro! é realmente um Cachorro
}

Gato* g = dynamic_cast<Gato*>(animal);
// g == nullptr  ← falha graciosamente, sem crash

// Com referência: lança std::bad_cast se falhar
try {
    Cachorro& cc = dynamic_cast<Cachorro&>(*animal);
    cc.buscar();
} catch (const std::bad_cast& e) {
    fmt::print("não é um Cachorro: {}\n", e.what());
}
```

### Custo do `dynamic_cast`:
É mais lento que `static_cast` pois precisa percorrer a hierarquia de tipos em runtime. Em código de alta performance, use padrões alternativos (visitor pattern, type tags).

---

## 3. `const_cast` — Adicionando/Removendo `const`

`const_cast` é o único cast que pode alterar qualificadores `const` e `volatile`.

### Único uso legítimo: interface legada que não aceita `const`

```cpp
// Função de biblioteca C antiga — não pode ser modificada
void legada_print(char* str);  // deveria ser const char*, mas não é

void imprimir(const std::string& s) {
    // Sabemos que legada_print não modifica str
    legada_print(const_cast<char*>(s.c_str())); // ✓ aceitável
}
```

### Comportamento indefinido (UB) — NUNCA faça:

```cpp
const int x = 42;          // genuinamente constante — pode estar em ROM
int* p = const_cast<int*>(&x);
*p = 100;                  // ✗ UNDEFINED BEHAVIOR! O compilador pode crash, ignorar,
                           //   ou "funcionar" — comportamento imprevisível
```

### Regra de ouro:
Se o objeto foi originalmente declarado `const`, modificá-lo via `const_cast` é **sempre UB**. Só é seguro quando o objeto original é não-const e você está navegando por uma interface que removeu o `const` desnecessariamente.

---

## 4. `reinterpret_cast` — Reinterpretação de Bits

Instrui o compilador a tratar os bytes de um tipo como se fossem outro tipo, **sem nenhuma conversão**. É o cast mais poderoso e mais perigoso.

### Usos legítimos:

```cpp
// ✓ Serialização: ver bytes de um float
float f = 3.14f;
uint32_t bits = reinterpret_cast<uint32_t&>(f); // IEEE 754 bits
// Para código portável, prefira std::bit_cast<uint32_t>(f) (C++20)

// ✓ Ponteiro → inteiro (para aritmética de endereço)
int x = 42;
uintptr_t addr = reinterpret_cast<uintptr_t>(&x);
fmt::print("endereço: 0x{:x}\n", addr);

// ✓ Interop com C APIs que usam void*
void* buf = malloc(sizeof(int));
int* ip = reinterpret_cast<int*>(buf);  // OK, alinhamento garantido por malloc
```

### Perigos:

```cpp
// ✗ Viola strict aliasing — UB!
float f = 1.0f;
int* i = reinterpret_cast<int*>(&f);
*i = 42;  // UB: compilador assume que int* e float* não se sobrepõem

// ✓ Alternativa correta: std::memcpy ou std::bit_cast
int bits;
std::memcpy(&bits, &f, sizeof(bits)); // seguro
```

---

## 5. C-style Cast — Por que Evitar

O cast no estilo C `(Type)value` é um atalho que pode fazer qualquer uma destas operações:
1. `static_cast`
2. `const_cast`
3. `reinterpret_cast`
4. `static_cast` + `const_cast`
5. `reinterpret_cast` + `const_cast`

O problema: **o compilador decide qual usar**, e você pode não perceber que está fazendo algo perigoso.

```cpp
const int x = 42;
int* p = (int*)&x;   // faz const_cast implicitamente — parece inofensivo!
                     // mas é o mesmo que const_cast<int*>(&x) → UB se x é const

// Com C++ cast, a intenção fica clara e o risco é visível:
int* p = const_cast<int*>(&x);  // você sabe o que está fazendo
```

### Regra prática:
```
Proibir C-style casts via: -Wold-style-cast (GCC/Clang)
```

---

## Resumo de decisão

```
Preciso converter tipos?
│
├── Conversão numérica (double→int, int→float)?
│     → static_cast
│
├── Downcast em hierarquia de herança?
│   ├── Sei com certeza o tipo real?  → static_cast
│   └── Não sei / pode variar?        → dynamic_cast
│
├── Passar const para API legada sem const?
│     → const_cast (com cuidado)
│
├── Reinterpretar bytes / serialização?
│     → reinterpret_cast ou std::bit_cast (C++20)
│
└── C-style cast?
      → NUNCA. Substitua por um dos acima.
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja casts
./bin/casts
```
