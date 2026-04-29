# `doxygen` — Documentação de Código C++ com Doxygen

Subprojeto do `cpp_academy` dedicado às práticas de documentação usando **Doxygen** — o gerador de documentação padrão para projetos C/C++.

---

## O que é Doxygen?

Doxygen analisa comentários especiais no código-fonte e gera documentação em vários formatos (HTML, PDF, man pages, XML). O HTML gerado inclui:

- Listagem de todos os namespaces, classes, funções e variáveis
- Diagramas de herança e colaboração (via Graphviz)
- Browser do código-fonte com referências cruzadas
- Barra de pesquisa

---

## Estilos de Comentário Reconhecidos

```cpp
// ── 1. Bloco JavaDoc (/// mais asteriscos) ─────────────────────────────────
/**
 * @brief Descrição curta.
 * @details Descrição longa...
 */
void minha_funcao();

// ── 2. Barra tripla (para membros inline) ─────────────────────────────────
double x; ///< Coordenada horizontal.
double y; ///< Coordenada vertical.

// ── 3. Estilo exclamação (alternativa ao JavaDoc) ─────────────────────────
/*! @brief Outra forma válida. */
```

O estilo `/** ... */` e `///` são os mais usados na prática. Escolha um e seja consistente.

---

## Comandos Essenciais

### Identificação

| Comando | Uso |
|---|---|
| `@file` | Identifica o arquivo (`@file math_utils.hpp`) |
| `@brief` | Descrição **curta** — aparece em listas e índices |
| `@details` | Descrição **longa** — aparece na página do símbolo |
| `@author` | Autor do arquivo ou classe |
| `@version` | Versão (`@version 1.0.0`) |
| `@date` | Data de criação |
| `@since` | Versão desde a qual o símbolo existe |

### Parâmetros e Retorno

| Comando | Uso |
|---|---|
| `@param[in] nome desc` | Parâmetro de **entrada** |
| `@param[out] nome desc` | Parâmetro de **saída** |
| `@param[in,out] nome desc` | Parâmetro de **entrada e saída** |
| `@tparam T desc` | Parâmetro de **template** |
| `@return desc` | Valor de retorno (descrição livre) |
| `@retval valor desc` | Valor **específico** de retorno |

```cpp
/**
 * @brief Divide dois números.
 * @param[in]  a  Dividendo.
 * @param[in]  b  Divisor.
 * @param[out] ok True se a divisão foi bem-sucedida.
 * @return Quociente, ou 0.0 se @p b == 0.
 * @retval 0.0 Quando @p b é zero.
 */
double dividir(double a, double b, bool& ok);
```

### Contratos e Avisos

| Comando | Uso |
|---|---|
| `@pre` | Pré-condição que o chamador deve garantir |
| `@post` | Pós-condição garantida após a chamada |
| `@throws ExcType desc` | Exceção que pode ser lançada |
| `@note` | Nota informativa (caixa azul no HTML) |
| `@warning` | Aviso importante (caixa amarela no HTML) |
| `@bug` | Bug conhecido no símbolo |
| `@todo` | Tarefa pendente |
| `@deprecated` | Marca símbolo como obsoleto |

```cpp
/**
 * @brief Normaliza o vetor.
 * @pre   `length() > 0`
 * @post  `length() == 1.0`
 * @throws std::domain_error Se o vetor for nulo.
 * @note  Use `length_sq()` para comparações — evita `sqrt`.
 * @warning Não use com vetores de ponto fixo.
 * @bug   Pode perder precisão para valores < 1e-300.
 * @todo  Adicionar variante para `float`.
 */
Vector2D normalized() const;
```

### Referências Cruzadas e Código

| Comando | Uso |
|---|---|
| `@see SímboloOuURL` | Referência cruzada |
| `@ref Símbolo` | Link inline para outro símbolo |
| `@code` / `@endcode` | Bloco de código |
| `@code{.cpp}` | Bloco de código com linguagem específica |
| `@code{.sh}` | Bloco de código shell |

```cpp
/**
 * @see Vector2D::dot()
 * @see https://en.cppreference.com/w/cpp/numeric/math/hypot
 *
 * @code
 * Vector2D a{3.0, 4.0};
 * double len = a.length(); // 5.0
 * @endcode
 */
double length() const noexcept;
```

### Agrupamento

```cpp
/**
 * @defgroup conversion Conversão de Unidades
 * @brief Funções de conversão entre sistemas de unidades.
 * @{   ← abre o grupo
 */

/** @brief Graus para radianos. @ingroup conversion */
double deg_to_rad(double deg);

/** @} ← fecha o grupo */
```

### Página Principal

```cpp
/**
 * @mainpage Título do Projeto
 *
 * @section intro Introdução
 * Texto em Markdown...
 *
 * @section uso Como Usar
 * @code{.sh}
 * doxygen Doxyfile
 * @endcode
 */
```

---

## Documentando Templates

```cpp
/**
 * @brief Soma dois valores genéricos.
 * @tparam T Tipo aritmético (satisfaz @ref Numeric).
 * @param[in] a Primeiro operando.
 * @param[in] b Segundo operando.
 * @return Soma a + b no tipo T.
 */
template <Numeric T>
T soma(T a, T b);
```

---

## Membros Inline com `///<`

A barra tripla seguida de `<` documenta o **membro anterior** na mesma linha:

```cpp
struct Config {
    std::string host;  ///< Endereço do servidor (padrão: "localhost").
    uint16_t    port;  ///< Porta TCP (padrão: 5432).
    bool        ssl;   ///< Habilita conexão segura.
};
```

---

## O Doxyfile

O `Doxyfile` é o arquivo de configuração. As opções mais importantes:

```ini
PROJECT_NAME      = "Meu Projeto"
OUTPUT_DIRECTORY  = docs
INPUT             = src include        # diretórios varridos
RECURSIVE         = YES
FILE_PATTERNS     = *.cpp *.hpp *.h

# Qualidade
EXTRACT_ALL       = NO    # documenta apenas o que tem comentário Doxygen
WARN_IF_UNDOCUMENTED = YES

# HTML
GENERATE_HTML     = YES
SEARCHENGINE      = YES
GENERATE_TREEVIEW = YES

# Diagramas (requer graphviz)
HAVE_DOT          = YES
CLASS_DIAGRAMS    = YES
INCLUDE_GRAPH     = YES

# Desabilitado por padrão
GENERATE_LATEX    = NO
GENERATE_XML      = NO
```

Gere um Doxyfile padrão com todos os campos comentados:

```bash
doxygen -g Doxyfile
```

---

## Integração com Meson

```meson
doxygen_prog = find_program('doxygen', required: false)

if doxygen_prog.found()
    custom_target('docs',
        input:  files('Doxyfile'),
        output: 'html',
        command: [doxygen_prog, '@INPUT@'],
        build_by_default: false,
    )
endif
```

```bash
# Gera a documentação:
cd build && ninja docs

# Abre no browser:
xdg-open docs/html/index.html
```

---

## Diagramas com Graphviz

Com `HAVE_DOT = YES` e o pacote `graphviz` instalado:

```bash
sudo apt install graphviz
```

Doxygen gera automaticamente:

| Diagrama | Opção no Doxyfile |
|---|---|
| Herança de classes | `CLASS_DIAGRAMS = YES` |
| Dependências de `#include` | `INCLUDE_GRAPH = YES` |
| Grafo de chamadas | `CALL_GRAPH = YES` |
| Grafo de colaboração | `COLLABORATION_GRAPH = YES` |

---

## Temas Modernos

O visual padrão do Doxygen é funcional mas datado. Para um visual moderno:

```bash
# Instalar doxygen-awesome-css
git clone https://github.com/jothepro/doxygen-awesome-css.git docs/awesome

# Adicionar ao Doxyfile:
HTML_EXTRA_STYLESHEET  = docs/awesome/doxygen-awesome.css
HTML_COLORSTYLE        = LIGHT
```

---

## Compilar e Executar

```bash
# Ativar em src/meson.build: subdir('doxygen')

make configure
make build

# Rodar demo
./dist/bin/doxygen_demo

# Gerar documentação
cd build && ninja docs
xdg-open build/src/doxygen/html/index.html
```

---

## Dependências

- `fmt` — formatação (já no projeto)
- `doxygen` — gerador de documentação (via Conan: `tool_requires("doxygen/1.9.4")`)
- `graphviz` — diagramas (sistema: `sudo apt install graphviz`)

---

## Arquivos

```
src/doxygen/
├── main.cpp         ← ponto de entrada + @mainpage
├── math_utils.hpp   ← API documentada com todos os comandos Doxygen
├── Doxyfile         ← configuração do gerador (anotada)
├── meson.build      ← target `docs` via custom_target
└── README.md        ← este arquivo
```
