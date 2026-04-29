# `srp` — Single Responsibility Principle

Subprojeto do `cpp_academy` dedicado ao estudo profundo do **SRP** (Single Responsibility Principle) — o primeiro dos cinco princípios SOLID.

---

## A Definição Precisa

> **"A module should be responsible to one, and only one, actor."**
> — Robert C. Martin, *Clean Architecture* (2017)

A definição popular "uma classe = uma responsabilidade" é incompleta. A palavra-chave é **ator** — um grupo de stakeholders que pode exigir a mesma mudança. Se duas equipes diferentes podem pedir alterações na mesma classe, ela tem duas responsabilidades.

---

## Demos

### `demo_violation.hpp` — God Class vs. Decomposição

O anti-padrão mais comum: uma classe que mistura **lógica de negócio + apresentação + persistência**.

```
ReportManager (❌)          EmployeeRepository (✅)
  add_employee()     →        add()
  average_salary()            average_salary()
  format_report()   →      ReportFormatter
  save_to_file()    →      ReportExporter
```

| Classe | Responsabilidade | Ator que pode pedir mudança |
|---|---|---|
| `EmployeeRepository` | dados e regras de negócio | RH / Gestores |
| `ReportFormatter` | apresentação / formato | UX / Design |
| `ReportExporter` | destino / persistência | DevOps / Infra |

---

### `demo_pipeline.hpp` — Pipeline de Processamento de Pedidos

Cenário realista: processar um CSV de pedidos de e-commerce com 6 etapas independentes.

```
CsvParser → OrderValidator → PricingEngine → InvoiceBuilder → NotificationService
                                   ↑
                           OrderProcessor (orquestra, não implementa)
```

Cada etapa tem exatamente uma razão para mudar:

```cpp
// Trocar formato CSV → só CsvParser muda
// Trocar alíquota de imposto → só PricingEngine muda
// Trocar e-mail por SMS → só NotificationService muda
```

---

### `demo_functions.hpp` — SRP em Funções + CQS

SRP não é só para classes. Uma função que `valida E calcula E imprime` viola o princípio.

**Heurística:** se você precisa de `"e"` ou `"ou"` para descrever o que uma função faz, ela tem mais de uma responsabilidade.

```cpp
// ❌ Uma função faz tudo
void process_and_print_student(const Student& s);

// ✅ Cada função tem uma responsabilidade
double      average(const std::vector<double>& grades);
bool        is_passing(double avg, double threshold = 6.0);
std::optional<std::string> validate(const Student& s);
std::string format_row(const Student& s, double avg, bool passed);
void        print_report(const std::vector<Student>& students);
```

**Command-Query Separation (CQS):** complemento natural ao SRP para funções.

```cpp
// ❌ Muda estado E retorna valor
int increment_and_get();

// ✅ Separados
void increment();        // Command: muda estado, retorna void
int  value() const;      // Query: lê estado, sem efeito colateral
```

---

### `demo_actors.hpp` — O Exemplo Clássico de Uncle Bob

O exemplo do livro *Clean Architecture* (Cap. 7): a classe `Employee` que serve três atores diferentes.

```
❌ Employee serve a 3 atores:
   calculatePay()  → COO / RH
   reportHours()   → CFO (auditoria)
   save()          → CTO / DBA

   Risco: total_hours() compartilhado entre calculatePay() e reportHours()
   Uma mudança pedida pelo COO afeta silenciosamente o relatório do CFO!
```

```
✅ Separação por ator:
   PayCalculator      → ator: COO/RH
   HourReporter       → ator: CFO
   EmployeeRepository → ator: CTO/DBA

   Cada um tem seu próprio método privado total_hours / auditable_hours.
   Não há acoplamento oculto.
```

---

### `demo_testability.hpp` — SRP + Interfaces + Injeção de Dependência

A combinação mais poderosa: SRP + interfaces abstratas + DI = código 100% testável.

```cpp
// Interfaces — cada uma representa uma responsabilidade isolada
struct IUserRepository  { virtual find_by_email(...) = 0; };
struct IPasswordHasher  { virtual verify(...)         = 0; };
struct ITokenGenerator  { virtual generate(...)       = 0; };
struct IAuditLogger     { virtual log_login(...)       = 0; };

// AuthService orquestra — não implementa nada
class AuthService {
    AuthService(IUserRepository, IPasswordHasher, ITokenGenerator, IAuditLogger);
    LoginResult login(email, password);
};
```

Nos testes, substituímos por stubs/mocks:

```cpp
auto repo   = std::make_shared<InMemoryUserRepository>(); // sem banco
auto hasher = std::make_shared<StubHasher>();             // sem bcrypt
auto tokens = std::make_shared<StubTokenGenerator>();     // token fixo
auto audit  = std::make_shared<SpyAuditLogger>();         // grava chamadas
```

Em produção, substituímos por implementações reais sem tocar em `AuthService`.

---

## Quando SRP não se aplica

O SRP pode ser **sobre-aplicado**. Sinais de que você foi longe demais:

- Classes de uma linha (`class EmailValidator { bool validate(email); }`)
- Cada getter/setter em uma classe separada
- Módulos tão pequenos que o custo de coordenação supera o benefício

**Regra de ouro:** coesão > granularidade. Agrupe o que muda junto. Separe o que muda por razões diferentes.

---

## Sinais de Violação do SRP

| Sinal | Possível problema |
|---|---|
| Classe com mais de ~200-300 linhas | Múltiplas responsabilidades |
| Nome genérico: `Manager`, `Helper`, `Utils`, `Service` | God class |
| Método com `"e"` ou `"ou"` na descrição | Duas responsabilidades |
| Testes que precisam mockar 5+ dependências | Classe faz demais |
| Mudança em formato de e-mail quebra cálculo de salário | Acoplamento oculto |
| Classe importa tanto `<filesystem>` quanto `<regex>` quanto `<curl.h>` | Mistura de camadas |

---

## Relação com os Outros Princípios SOLID

```
SRP — define fronteiras: o quê cada módulo faz
OCP — fronteiras fechadas para modificação, abertas para extensão
LSP — contratos que as implementações devem respeitar
ISP — interfaces pequenas (complemento ao SRP para interfaces)
DIP — dependa de abstrações, não de implementações (viabiliza SRP)
```

---

## Compilar e Executar

```bash
# Ativar o subprojeto em src/meson.build:
# subdir('srp')

make configure
make build
./dist/bin/srp

# Ou diretamente:
source build/conanbuild.sh
meson compile -C build srp
./build/src/srp/srp
```

---

## Dependências

Apenas `fmt` (já presente no projeto) e a STL padrão C++20. Nenhum Conan extra necessário.

---

## Arquivos

```
src/srp/
├── main.cpp               ← ponto de entrada
├── meson.build            ← build do executável
├── demo_violation.hpp     ← God Class vs. decomposição
├── demo_pipeline.hpp      ← pipeline de processamento de pedidos
├── demo_functions.hpp     ← SRP em funções + CQS
├── demo_actors.hpp        ← Employee, COO, CFO, CTO (Clean Architecture)
├── demo_testability.hpp   ← interfaces + injeção + mocks
└── README.md
```
