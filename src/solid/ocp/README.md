# ocp — Open/Closed Principle

> "Software entities should be **open for extension**, but **closed for modification**."
> — Bertrand Meyer (1988) / Robert C. Martin

## O que é este projeto?

O segundo princípio SOLID: como acrescentar comportamento novo **sem editar** o código que
já funciona e já foi testado.

O sintoma de violação é sempre o mesmo: um `switch` ou uma cascata de `if/else if` sobre um
**tipo** ou um **enum**, repetida em várias funções.

---

## Estrutura de arquivos

```
ocp/
├── main.cpp                ← executa os 4 demos
├── demo_violation.hpp      ← a cascata de if/else vs. polimorfismo
├── demo_strategy.hpp       ← std::function, template policy, concepts
├── demo_registry.hpp       ← registro auto-populado — extensão sem tocar no main
└── demo_testability.hpp    ← o custo contado, e a armadilha da abstração especulativa
```

---

## Demo 1: A cascata

```cpp
double area(const Forma& f) {
    if      (f.tipo == Circulo)   return 3.14159 * f.raio * f.raio;
    else if (f.tipo == Retangulo) return f.largura * f.altura;
    // ← e um `else if` por forma nova. Para sempre.
}
double perimetro(const Forma& f) { /* a MESMA cascata, repetida */ }
```

Acrescentar um triângulo exige editar **quatro** lugares já testados — e esquecer um deles
não gera erro de compilação, gera resultado errado.

Com polimorfismo, o `Triangulo` é um **arquivo novo**, `area_total` nunca é tocada, e o
`= 0` obriga a implementar tudo.

### ⚠ Quando a cascata é a resposta certa

OCP paga-se com vtable, alocação e um arquivo por caso. Se o conjunto é **fechado** (4
naipes, 3 estados de semáforo), o switch é mais simples e mais rápido.

A pergunta que decide: *"casos novos vão chegar depois do deploy, vindos de quem não
controla este arquivo?"*

---

## Demo 2: OCP por injeção

Quando o que varia é um **algoritmo** (não uma família de entidades), injete-o:

| Mecanismo | Extensão | Custo | Homogêneo? |
|---|---|---|---|
| herança | runtime | vtable | sim |
| `std::function` | runtime | indireção | sim |
| template (policy) | compilação | **zero** | não |
| concept | compilação | **zero** | não |

*"Homogêneo"* = dá para guardar variantes num mesmo `vector`.

O concept resolve o pior defeito da policy por template — a mensagem de erro:

```cpp
template <typename P>
concept PoliticaDeDesconto = requires(double total) {
    { P::aplicar(total) } -> std::convertible_to<double>;
    { P::nome() }         -> std::convertible_to<std::string>;
};

template <PoliticaDeDesconto P> class Carrinho { /* ... */ };
```

`Carrinho<Quebrada>` falha **na linha da chamada**, dizendo qual expressão faltou — não com
300 linhas de instanciação.

---

## Demo 3: Registro auto-populado

Nos demos anteriores, o consumidor ficou fechado — mas alguém ainda editava o ponto de
composição. O registro elimina até isso:

```cpp
REGISTRAR_EXPORTADOR(ExportadorMarkdown, "markdown");
```

O loop que consome **nunca foi editado**; ele nem conhece os nomes das classes. O
`ExportadorMarkdown` foi acrescentado depois e apareceu sozinho.

### O preço

- a **ordem de inicialização estática** entre TUs não é definida → o registro precisa ser um
  Meyers singleton (função com `static` local), nunca uma variável global
- o linker pode **descartar** o objeto estático se nada na TU for referenciado
  (`--gc-sections`, bibliotecas estáticas) → exige `--whole-archive` ou um símbolo âncora
- o fluxo fica indireto: não dá para achar quem registrou lendo o `main`

---

## Demo 4: O custo, contado

| O que muda ao acrescentar um caso | Com switch | Com OCP |
|---|---|---|
| arquivos criados | 0 | 1 |
| arquivos **editados** | 3-5 | **0** |
| testes a reescrever | 3-5 | **0** |
| risco de regressão | alto | nenhum |

A linha que decide é *arquivos editados*: **código que não é tocado não pode regredir**.

### ⚠ A armadilha: abstração especulativa

OCP aplicado **antes** de haver um segundo caso é custo puro — e a abstração escolhida com
uma amostra de tamanho 1 quase sempre é a errada.

**Rule of Three:** 1ª vez escreva direto · 2ª vez duplique e anote o incômodo · 3ª vez
abstraia, porque agora você já viu o eixo da variação.

> OCP é resposta a uma mudança que **já aconteceu duas vezes** — não profecia sobre a que
> talvez aconteça.

---

## Regras de ouro

| Situação | Faça |
|---|---|
| `switch` sobre tipo, repetido em N funções | polimorfismo |
| O que varia é um algoritmo | injete: `std::function` ou policy |
| Escolha conhecida em compilação | template + concept (custo zero) |
| Precisa de extensão sem tocar no `main` | registro auto-populado |
| Conjunto de casos **fechado** | `switch`, ou `std::variant` + `visit` |
| Ainda só existe um caso | **não abstraia** — Rule of Three |

---

## Como compilar e executar

```bash
make build && ./dist/bin/ocp
```

Saída esperada ao final: `✓ ocp — todos os demos concluídos`
