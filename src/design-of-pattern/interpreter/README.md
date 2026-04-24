# interpreter — Padrão Interpreter GoF em C++

## O que é este projeto?

Implementa o padrão **Interpreter** do GoF: cada regra da gramática de uma linguagem vira uma classe C++. A "linguagem" aqui são expressões aritméticas com variáveis — a mesma do `src/parser/`, para facilitar a comparação.

---

## Estrutura de arquivos

```
interpreter/
├── expression.hpp   ← nós da AST (NumberExpr, AddExpr, VarExpr, ...)
├── parser.hpp       ← parser recursivo descendente: string → AST
├── program.hpp      ← statements (assign, print) e mini-linguagem
└── main.cpp         ← 5 demos
```

---

## O padrão

```
           ┌─────────────────┐
           │   Expression    │  ← interface base
           │─────────────────│
           │ +interpret(ctx) │
           │ +to_string()    │
           └────────┬────────┘
                    │
        ┌───────────┼───────────┐
        │           │           │
  ┌─────┴───┐  ┌────┴────┐  ┌──┴──────┐
  │ Number  │  │Variable │  │ AddExpr │  ← nós internos têm filhos
  │  Expr   │  │  Expr   │  │─────────│
  └─────────┘  └─────────┘  │ left_   │
   (terminal)   (terminal)  │ right_  │
                             └─────────┘
```

Cada classe implementa `interpret(ctx)` — o nome do padrão vem daí.

---

## A gramática implementada

```
expr    := term   { ('+' | '-') term   }
term    := factor { ('*' | '/') factor }
factor  := NUMBER | VARIABLE | '(' expr ')' | '-' factor
```

A precedência sai naturalmente da estrutura:
- `expr` chama `term` → multiplicação/divisão é avaliada antes de soma/subtração
- `term` chama `factor` → parênteses e literais têm prioridade máxima

---

## Uso básico

### Construção manual da AST

```cpp
interpreter::Context ctx;
ctx.set("x", 5.0);

// (x + 1) * 2
auto expr = mul(
    add(var("x"), num(1)),
    num(2)
);

fmt::print("{}\n", expr->to_string());    // ((x + 1) * 2)
fmt::print("{}\n", expr->interpret(ctx)); // 12
```

### Via parser (string → AST)

```cpp
auto ast = interpreter::parse("pi * r * r");

interpreter::Context ctx;
ctx.set("pi", 3.14159);
ctx.set("r",  10.0);

fmt::print("{}\n", ast->interpret(ctx)); // 314.159
```

### Reutilizar a mesma AST com contextos diferentes

```cpp
auto formula = interpreter::parse("b * b - 4 * a * c");

for (auto& equacao : equacoes) {
    interpreter::Context ctx;
    ctx.set("a", equacao.a);
    ctx.set("b", equacao.b);
    ctx.set("c", equacao.c);
    double delta = formula->interpret(ctx); // mesma AST, novo contexto
}
```

### Mini-linguagem com statements

```cpp
const std::string programa = R"(
    P = 1000
    r = 0.1
    resultado = P * 1.1 * 1.1 * 1.1
    print resultado
)";

auto prog = interpreter::ProgramParser::parse(programa);
interpreter::Context ctx;
prog.run(ctx);
```

---

## Extensão do padrão

Para adicionar um novo operador (ex: `%` modulo):

```cpp
// 1. Nova classe (expression.hpp)
struct ModExpr : Expression {
    ExprPtr left_, right_;
    double interpret(Context& ctx) const override {
        return std::fmod(left_->interpret(ctx), right_->interpret(ctx));
    }
    std::string to_string() const override {
        return fmt::format("({} % {})", left_->to_string(), right_->to_string());
    }
};

// 2. Helper
inline ExprPtr mod(ExprPtr l, ExprPtr r) {
    return std::make_unique<ModExpr>(std::move(l), std::move(r));
}

// 3. Adicionar '%' no parse_term() do Parser
```

Três passos, nenhum arquivo existente quebrado — Open/Closed Principle em ação.

---

## Interpreter GoF vs Flex/Bison

| | Interpreter GoF | Flex/Bison |
|---|---|---|
| **Dependências** | Zero — C++ puro | flex + bison instalados |
| **AST** | Objeto reutilizável | Precisa implementar separado |
| **Testabilidade** | Cada nó é testável isoladamente | Testa o parser inteiro |
| **Gramáticas grandes** | Trabalhoso | Ideal |
| **Recuperação de erros** | Manual | Robusto |
| **Performance** | Adequada para DSLs | Máxima (LALR(1)) |
| **Quando usar** | DSL embutida, < 20 regras | Linguagem completa |

---

## Como compilar e executar

```bash
make build
./dist/bin/interpreter
```

Não requer dependências além de `fmt`.
