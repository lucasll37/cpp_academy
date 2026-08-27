# lsp — Liskov Substitution Principle

> "Se S é subtipo de T, objetos de T podem ser substituídos por objetos de S sem alterar
> nenhuma propriedade desejável do programa." — Barbara Liskov (1987)

## O que é este projeto?

O terceiro princípio SOLID — e o único que o compilador **não pode verificar por você**.

LSP é sobre **comportamento**, não sobre assinatura. O compilador checa a assinatura;
ninguém checa o comportamento. Por isso violações de LSP compilam perfeitamente e só
aparecem em produção.

---

## Estrutura de arquivos

```
lsp/
├── main.cpp                ← executa os 3 demos
├── demo_violation.hpp      ← Quadrado/Retângulo e por que ele falha
├── demo_contracts.hpp      ← as 4 regras: pré, pós, invariante, history rule
└── demo_testability.hpp    ← testes de contrato, NVI, e as alternativas
```

---

## Demo 1: Quadrado/Retângulo

O exemplo é canônico porque *"quadrado é um retângulo"* é verdade na **matemática** e
mentira no **código**:

```
com Retangulo: largura=5, altura=4 → área esperada 20, obtida 20  ✓
com Quadrado:  largura=5, altura=4 → área esperada 20, obtida 16  ✗ LSP VIOLADO
```

O erro não foi implementar mal o `Quadrado`. Foi afirmar que ele **é um** retângulo
**mutável** — e essa relação não sobrevive à mutabilidade.

Duas saídas, ambas boas:

1. **imutabilidade** — sem setters, o conflito desaparece (`com_largura()` devolve objeto novo)
2. **irmãos** sob uma interface que os dois **podem** cumprir (`escala()` em vez de setters
   independentes)

> A pergunta certa não é *"X é um Y?"*, é *"X consegue cumprir **todas** as promessas que Y
> faz aos seus clientes?"*

---

## Demo 2: As quatro regras

| O subtipo pode... | Pré-condição | Pós-condição | Invariante |
|---|---|---|---|
| exigir **mais** | ✗ | — | — |
| exigir **menos** | ✓ | — | — |
| entregar **mais** | — | ✓ | ✓ |
| entregar **menos** | — | ✗ | ✗ |

**Mnemônico:** o derivado pode ser mais **generoso**, nunca mais **exigente**. É a mesma
contravariância dos tipos de função.

### Regra 1 — pré-condições não podem ser fortalecidas

```
Deposito             depositar(50) → saldo 50.00  ✓
DepositoComEstorno   depositar(50) → saldo 50.00  ✓
DepositoVip          depositar(50) → EXCEÇÃO: VIP exige mínimo de 100  ✗ LSP
```

### Regra 2 — pós-condições não podem ser enfraquecidas

```
Ordenador                5 elementos → 1 1 2 3 3  ✓
OrdenadorQueDeduplica    5 elementos → 1 2 3      ✗ perdeu elementos
```

Quem contava com `resultado.size() == entrada.size()` quebra em **silêncio**.

### Regra 3 — invariantes da base têm que valer

```
Conta                    saldo= 100.00  invariante(saldo>=0): ✓
ContaComChequeEspecial   saldo= -50.00  invariante(saldo>=0): ✗ LSP VIOLADO
```

Cheque especial é um conceito legítimo — mas modelá-lo quebrando `saldo >= 0` sabota todo
código que confia nisso. A `ContaComLimite` do demo mostra como fazer certo.

### Regra 4 — History Rule

Um derivado não pode permitir mudanças de estado que a base proibia:

```
PontoImutavel  → cache válido ✓
PontoMutavel   → cache INVÁLIDO ✗ LSP
```

Nenhum método foi sobrescrito. Nenhuma assinatura mudou. O que mudou foi o conjunto de
**histórias possíveis** do objeto. Por isso `final` e a imutabilidade são ferramentas de LSP.

---

## Demo 3: Como detectar

### Testes de contrato — o LSP virando código executável

Uma suíte escrita contra a **base**, rodada contra **cada** derivado:

```
PilhaVetor         5 passou, 0 falhou  ✓
PilhaLimitada(3)   3 passou, 2 falhou  ✗ LSP VIOLADO
  ✗ empilhar 10 → tamanho 10
  ✗ round-trip: 5 entraram, 5 devem sair
```

Em GoogleTest, `TYPED_TEST_SUITE` faz exatamente isto.

### NVI — o contrato mora na base

```cpp
std::string buscar(int id) {              // público, NÃO virtual
    if (id <= 0) throw std::invalid_argument("id deve ser positivo");  // pré
    std::string r = buscar_impl(id);                                    // miolo
    if (r.empty()) throw std::runtime_error("devolveu vazio");          // pós
    return r;
}
private:
    virtual std::string buscar_impl(int id) = 0;   // privado, virtual
```

O derivado não consegue afrouxar o contrato — ele não controla a fronteira pública.

### Quando a herança não cabe

Pinguim é uma Ave, mas não voa. Cinco saídas, da melhor para a pior:

| | Saída |
|---|---|
| ✓ | **segregar** a interface — `Ave` e `AveQueVoa` (→ ISP) |
| ✓ | **compor** — Pinguim *tem* um modo de locomoção |
| ✓ | `std::variant` — hierarquia fechada, sem herança |
| ~ | marcar `final` — não resolve, mas contém o estrago |
| ✗ | `voar()` lançando exceção — o cliente quebra |
| ✗ | `voar()` sem fazer nada — pior: quebra em silêncio |

A saída boa é o princípio seguinte. LSP e ISP são o mesmo problema visto de dois lados:

- **LSP:** *o derivado consegue cumprir tudo que a base promete?*
- **ISP:** *a base está prometendo coisas demais?*

---

## Regras de ouro

| Situação | Faça |
|---|---|
| Vai herdar | pergunte se o derivado cumpre **todas** as promessas |
| `override` que lança ou não faz nada | a herança está errada — segregue ou componha |
| Contrato importante na base | NVI: público não-virtual + virtual privado |
| Classe não foi feita para herança | `final` |
| Tem uma hierarquia | escreva a suíte de contrato **uma vez**, rode em todos |
| A relação "é um" é só do domínio | imutabilidade, ou irmãos sob interface comum |

---

## Como compilar e executar

```bash
make build && ./dist/bin/lsp
```

Saída esperada ao final: `✓ lsp — todos os demos concluídos`
