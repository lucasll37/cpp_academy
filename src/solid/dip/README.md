# dip — Dependency Inversion Principle

> a) "High-level modules should not depend on low-level modules. Both should depend on
> abstractions."
> b) "Abstractions should not depend on details. Details should depend on abstractions."
> — Robert C. Martin

## O que é este projeto?

O quinto princípio SOLID — e o que dá origem à Clean Architecture, à Hexagonal e à Onion.

O nome engana. **"Inversão" não é sobre injetar dependência** — é sobre a direção da
**seta** no diagrama de módulos:

```
ANTES:   [Política de negócio]  ──depende──▶  [MySQL]

DEPOIS:  [Política de negócio] ──▶ [IRepositorio] ◀──implementa── [MySQL]
```

A seta do MySQL foi invertida: agora ele aponta **para cima**, para a abstração que a
política definiu.

---

## Estrutura de arquivos

```
dip/
├── main.cpp                 ← executa os 3 demos
├── demo_violation.hpp       ← a seta de dependência, antes e depois
├── demo_injection.hpp       ← construtor, parâmetro, template, composition root
└── demo_architecture.hpp    ← a regra da dependência e Clean Architecture
```

---

## Demo 1: A seta invertida

```cpp
class ServicoDePedidos {          // ❌ regra de negócio conhecendo o banco
    BancoMySql banco_;            // ← acoplado ao TIPO concreto
};
```

Consequências, todas caras: testar a **regra** exige um MySQL rodando; trocar de banco edita
a regra; a regra recompila quando o header do MySQL muda.

A correção declara a abstração **ao lado da política**:

```cpp
// Camada de política — repare no nome
class RepositorioDePedidos { ... };      // não "IBancoDeDados"

class ServicoDePedidos {
    explicit ServicoDePedidos(RepositorioDePedidos& repo);
};
```

O mesmo `ServicoDePedidos` roda com MySQL, com arquivo e em memória. Ele nunca soube que
MySQL existe.

> **O teste decisivo:** em que direção o código-fonte aponta em tempo de compilação? Se o
> módulo de negócio dá `#include "mysql.hpp"`, não houve inversão nenhuma — por mais
> interfaces que existam no meio.

---

## Demo 2: Formas de injeção

| Forma | Quando |
|---|---|
| **construtor** | dependência obrigatória — estado inválido fica inexprimível |
| **parâmetro** | varia a cada chamada; não guarda estado |
| **template** | escolha conhecida em compilação; custo zero |

### Composition root

Se o serviço não pode construir o repositório, **quem constrói**? Um único ponto que pode
conhecer tudo, porque não tem regra de negócio: o `main`, ou um módulo de bootstrap.

```cpp
static Aplicacao montar_producao();   // RelogioDoSistema + LogNoConsole
static Aplicacao montar_teste();      // RelogioFixo     + LogEmMemoria
```

`Auditoria` é o mesmo objeto nos dois. Só a fiação mudou — e **todo `new` do programa está
nessas duas funções**.

### Injeção estática — custo zero

```cpp
template <RelogioEstatico R>
class AuditoriaEstatica {
    R relogio_;                       // por VALOR: sem indireção, sem heap
};
// sizeof(AuditoriaEstatica<RelogioRapido>) = 1 byte
```

A dependência continua invertida — o **concept é a abstração**. Em troca, os tipos são
diferentes: não dá para trocar em runtime nem guardar num `vector`.

### ⚠ Service locator não é injeção

```cpp
void fazer() {
    auto* log = Locator::obter<Log>();   // ← o objeto vai BUSCAR
}
```

Pior que o acoplamento direto: as dependências ficam **invisíveis na assinatura**, o teste
vira global mutável, e a falha é em runtime (a injeção por construtor falha em compilação).

> **A regra que separa os dois:** injeção → a dependência é **empurrada** para o objeto.
> Service locator → o objeto vai **buscar**. Se a classe busca, ela conhece o buscador —
> não houve inversão.

---

## Demo 3: DIP em escala

### A regra da dependência

O código-fonte só pode apontar **para dentro**, na direção das políticas de mais alto nível.

| Círculo | Pode incluir | Volatilidade |
|---|---|---|
| entidades | **nada** | mínima |
| casos de uso | entidades | baixa |
| adaptadores | casos de uso + entidades | média |
| frameworks | tudo | máxima |

O teste é mecânico: `grep -r '#include' src/entidades/`. Se aparecer um header de SQL, a
arquitetura foi violada.

### Controle vs. código

```
fluxo de CONTROLE : caso de uso ──▶ banco     (de dentro p/ fora)
fluxo de CÓDIGO   : banco ──▶ caso de uso     (de fora p/ dentro)
```

São **opostos** — e é essa oposição que dá nome ao princípio.

### ⚠ Quando NÃO inverter

DIP paga-se com uma interface, uma indireção e um arquivo por dependência. Não vale quando:

- a dependência é **estável** — `std::string`, `std::vector`, `fmt`. Ninguém abstrai a
  biblioteca padrão.
- o programa é pequeno e não tem testes automatizados
- existe um implementador e não haverá outro

> A pergunta que decide: *"esta dependência é volátil?"* Inverta o que muda (banco, rede,
> relógio, sistema de arquivos, aleatoriedade). Não inverta o que é estável há 30 anos.

---

## Regras de ouro

| Situação | Faça |
|---|---|
| Regra de negócio tocando I/O | declare a porta **na** camada de negócio |
| Nomeando a porta | pelo que a política precisa, não pelo que a infra é |
| Dependência obrigatória | injeção por **construtor** |
| Escolha conhecida em compilação | template + concept |
| `new` espalhado pelo código | recolha tudo no composition root |
| Tentado a usar um singleton global | é service locator — não faça |
| Dependência estável (`std::`, `fmt`) | **não** inverta |
| Verificando a arquitetura | `grep '#include'` na camada interna |

---

## Como compilar e executar

```bash
make build && ./dist/bin/dip
```

Saída esperada ao final: `✓ dip — todos os demos concluídos`
