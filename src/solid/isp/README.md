# isp — Interface Segregation Principle

> "Clients should not be forced to depend upon interfaces that they do not use."
> — Robert C. Martin

## O que é este projeto?

O quarto princípio SOLID: interfaces devem ser pequenas e definidas pelo **cliente**.

ISP e LSP são o mesmo problema visto de dois lados:

- **LSP:** *o derivado consegue cumprir tudo que a base promete?*
- **ISP:** *a base está prometendo coisas demais?*

---

## Estrutura de arquivos

```
isp/
├── main.cpp                ← executa os 2 demos
├── demo_violation.hpp      ← a "fat interface" e a segregação
└── demo_client_side.hpp    ← role interfaces e o que elas compram
```

---

## Demo 1: A fat interface

```cpp
class Multifuncional {
    virtual void imprimir(const std::string&) = 0;
    virtual void digitalizar(const std::string&) = 0;
    virtual void enviar_fax(const std::string&) = 0;
    virtual void grampear(int) = 0;
};
```

Uma impressora simples é **forçada** a declarar três métodos que não sabe fazer. As três
saídas são todas ruins:

| Saída | Problema |
|---|---|
| lançar exceção | viola o LSP — o cliente quebra |
| não fazer nada | pior: quebra em **silêncio** |
| devolver valor inventado | pior ainda: corrompe dados |

E acrescentar `virtual void encadernar()` na interface **recompila e quebra todos** os
implementadores.

### A correção

Cada capacidade vira uma interface própria; o implementador declara só o que sabe fazer, e
o **cliente pede o mínimo de que precisa**:

```cpp
void fluxo_de_impressao(Imprimivel& d, const std::string& doc);
void fluxo_de_arquivamento(Digitalizavel& d, const std::string& destino);

fluxo_de_impressao(scanner, "x");    // ✗ ERRO DE COMPILAÇÃO
```

O ganho decisivo: o que era exceção em runtime (ou silêncio) virou **erro de compilação**.

A herança múltipla de interfaces puras é o caso seguro — ver
[oop/demo_multiple_inheritance.hpp](../../oop/demo_multiple_inheritance.hpp).

---

## Demo 2: A interface pertence ao cliente

A leitura ingênua do ISP é *"interfaces pequenas"*. A leitura correta é mais forte: a
interface é definida pelo que o **cliente** precisa, não pelo que o fornecedor oferece.

Consequência que muda o desenho: **a interface deve ser declarada junto do cliente**. Um
serviço com 12 métodos pode ser consumido por dez clientes através de dez interfaces
minúsculas, cada uma nomeada segundo o **papel** que cumpre.

```cpp
// declarada AO LADO do EnviadorDeBoasVindas, com 2 métodos
class FonteDeContato {
    virtual std::string nome(int id) const = 0;
    virtual std::string email(int id) const = 0;
};

// declarada AO LADO do ProcessadorDePagamento, com 2 métodos
class Carteira {
    virtual double saldo(int id) const = 0;
    virtual void   debitar(int id, double valor) = 0;
};
```

Adaptadores fininhos ligam o serviço legado a cada papel.

O que isso compra:

- o enviador **não consegue** tocar em dinheiro; o processador **não consegue** mandar
  e-mail — garantido pelo tipo
- os dublês de teste têm **2 métodos** cada, em vez de 12
- nenhum framework de mock, nenhum banco, nenhuma rede

### Nomear pelo papel, não pela classe

| Nome ruim (do fornecedor) | Nome bom (do papel) |
|---|---|
| `IServicoDeUsuarios` | `FonteDeContato` |
| `IRepositorioMySql` | `LeitorDePedidos` |
| `IGerenciadorDeArquivos` | `Persistidor` |
| `IHelper` / `IManager` / `IUtil` | qualquer coisa menos |

---

## Regras de ouro

| Situação | Faça |
|---|---|
| `override` que lança ou não faz nada | segregue a interface |
| Dublê de teste com 12 métodos p/ testar 2 | segregue a interface |
| Escrevendo uma interface | pergunte "quem é o cliente, e do que ele precisa?" |
| Nomeando uma interface | use o **papel**, não a classe concreta |
| Precisa de várias capacidades | herde múltiplas interfaces puras |
| Já tem um serviço legado gordo | adaptadores fininhos, um por papel |
| Segregando | pare quando cada interface tiver um cliente real |

> ⚠ Segregar demais vira sopa de interfaces. O limite: cada interface deve ter pelo menos
> um cliente que use **todos** os seus métodos.

---

## Como compilar e executar

```bash
make build && ./dist/bin/isp
```

Saída esperada ao final: `✓ isp — todos os demos concluídos`
