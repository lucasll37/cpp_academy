#pragma once
// =============================================================================
//  math_utils.hpp — Utilitários matemáticos com documentação Doxygen completa
// =============================================================================
//
//  Este arquivo demonstra TODOS os principais comandos Doxygen:
//
//    @file        — identifica o arquivo
//    @brief       — descrição curta (aparece em listagens)
//    @details     — descrição longa (aparece na página do símbolo)
//    @tparam      — documenta parâmetro de template
//    @param       — documenta parâmetro de função
//    @param[in]   — parâmetro de entrada
//    @param[out]  — parâmetro de saída
//    @param[in,out]— parâmetro de entrada e saída
//    @return      — documenta o valor de retorno
//    @retval      — documenta um valor específico de retorno
//    @throws      — exceção que a função pode lançar
//    @pre         — pré-condição que o chamador deve garantir
//    @post        — pós-condição garantida após a chamada
//    @note        — nota informativa
//    @warning     — aviso importante
//    @bug         — bug conhecido
//    @todo        — tarefa pendente
//    @deprecated  — marca símbolo obsoleto
//    @see         — referência cruzada para outro símbolo
//    @code/@endcode — bloco de código no comentário
//    @since       — versão desde a qual o símbolo existe
//    @author      — autor
//    @version     — versão
//    @date        — data
//
// =============================================================================

/**
 * @file math_utils.hpp
 * @brief Utilitários matemáticos de propósito geral.
 *
 * @details
 * Provê funções e classes matemáticas cobrindo:
 *  - Operações numéricas com tipos genéricos
 *  - Estatísticas descritivas básicas
 *  - Geometria 2D
 *  - Aritmética de matrizes simples
 *
 * Todos os algoritmos são implementados em termos de conceitos C++20
 * para garantir erros de compilação legíveis quando tipos inválidos
 * são usados.
 *
 * @author  Lucas
 * @version 1.0.0
 * @date    2024-01-01
 * @since   1.0.0
 *
 * @see Vector2D
 * @see Stats
 */

#include <algorithm>
#include <cmath>
#include <concepts>
#include <numeric>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

// =============================================================================
//  Namespace principal
// =============================================================================

/**
 * @namespace math
 * @brief Namespace raiz de todos os utilitários matemáticos.
 */
namespace math {

// =============================================================================
//  Conceitos C++20
// =============================================================================

/**
 * @concept Numeric
 * @brief Restringe o tipo para qualquer tipo aritmético.
 *
 * @details
 * Satisfeito por `int`, `float`, `double`, `long`, etc.
 * Não satisfeito por `std::string`, ponteiros crus, ou tipos customizados
 * sem sobrecarga dos operadores aritméticos.
 *
 * @tparam T Tipo a verificar.
 */
template <typename T>
concept Numeric = std::is_arithmetic_v<T>;

// =============================================================================
//  Funções livres
// =============================================================================

/**
 * @brief Calcula o valor absoluto de um número.
 *
 * @details
 * Versão genérica usando `std::abs`. Para tipos de ponto flutuante,
 * prefira `std::fabs` diretamente pois evita conversões implícitas.
 *
 * @tparam T Tipo aritmético (satisfaz @ref Numeric).
 * @param[in] x O valor de entrada.
 * @return O valor absoluto de @p x, no mesmo tipo @p T.
 *
 * @note Para `unsigned`, qualquer valor já é não-negativo; a função
 *       retorna @p x sem modificação.
 *
 * @code
 * int   a = math::abs(-5);    // 5
 * float b = math::abs(-3.14f);// 3.14f
 * @endcode
 */
template <Numeric T>
[[nodiscard]] T abs(T x) noexcept {
    if constexpr (std::is_unsigned_v<T>) return x;
    return x < T{0} ? -x : x;
}

/**
 * @brief Limita um valor ao intervalo [lo, hi].
 *
 * @details
 * Equivalente a `std::clamp`, porém com pré-condições explícitas
 * e mensagem de erro clara quando `lo > hi`.
 *
 * @tparam T Tipo comparável e aritmético.
 * @param[in] value O valor a ser limitado.
 * @param[in] lo    Limite inferior (inclusivo).
 * @param[in] hi    Limite superior (inclusivo).
 * @return
 *   - @p lo se `value < lo`
 *   - @p hi se `value > hi`
 *   - @p value caso contrário
 *
 * @pre  `lo <= hi`
 * @post O valor retornado está em `[lo, hi]`
 * @throws std::invalid_argument se `lo > hi`
 *
 * @see https://en.cppreference.com/w/cpp/algorithm/clamp
 *
 * @code
 * math::clamp(15, 0, 10);  // → 10
 * math::clamp(-3, 0, 10);  // → 0
 * math::clamp( 5, 0, 10);  // → 5
 * @endcode
 */
template <Numeric T>
[[nodiscard]] T clamp(T value, T lo, T hi) {
    if (lo > hi)
        throw std::invalid_argument("clamp: lo > hi");
    return value < lo ? lo : (value > hi ? hi : value);
}

/**
 * @brief Interpola linearmente entre dois valores.
 *
 * @details
 * Calcula `a + t * (b - a)`, que é matematicamente equivalente a
 * `(1 - t) * a + t * b`, mas evita perda de precisão quando `a` e
 * `b` têm magnitudes muito diferentes.
 *
 * @tparam T Tipo de ponto flutuante.
 * @param[in] a Valor inicial (t = 0.0).
 * @param[in] b Valor final   (t = 1.0).
 * @param[in] t Fator de interpolação (tipicamente em [0, 1]).
 * @return O valor interpolado.
 *
 * @note @p t pode estar fora de [0, 1] para extrapolação.
 * @warning Não verifica se @p t está no intervalo [0, 1].
 *
 * @code
 * math::lerp(0.0f, 100.0f, 0.5f);  // → 50.0f
 * math::lerp(0.0f, 100.0f, 0.0f);  // →  0.0f
 * math::lerp(0.0f, 100.0f, 1.0f);  // → 100.0f
 * @endcode
 */
template <std::floating_point T>
[[nodiscard]] T lerp(T a, T b, T t) noexcept {
    return a + t * (b - a);
}

/**
 * @brief Verifica se dois valores de ponto flutuante são aproximadamente iguais.
 *
 * @details
 * Usa tolerância relativa e absoluta para cobrir tanto valores
 * próximos de zero quanto valores muito grandes. Baseado no algoritmo
 * de Knuth (TAOCP Vol. 2).
 *
 * @tparam T Tipo de ponto flutuante.
 * @param[in] a       Primeiro valor.
 * @param[in] b       Segundo valor.
 * @param[in] rel_eps Tolerância relativa (padrão: 1e-6).
 * @param[in] abs_eps Tolerância absoluta (padrão: 1e-9).
 * @retval true  Se |a - b| ≤ max(rel_eps * max(|a|, |b|), abs_eps)
 * @retval false Caso contrário.
 *
 * @todo Adicionar variante para `std::complex<T>`.
 */
template <std::floating_point T>
[[nodiscard]] bool nearly_equal(T a, T b,
    T rel_eps = T{1e-6},
    T abs_eps = T{1e-9}) noexcept
{
    const T diff = std::abs(a - b);
    const T largest = std::max(std::abs(a), std::abs(b));
    return diff <= std::max(rel_eps * largest, abs_eps);
}

// =============================================================================
//  Classe Stats — estatísticas descritivas
// =============================================================================

/**
 * @brief Calcula estatísticas descritivas básicas sobre um conjunto de dados.
 *
 * @details
 * Recebe um `std::span<const double>` e calcula, de forma lazy (apenas quando
 * o getter é chamado pela primeira vez), as seguintes métricas:
 *
 * | Métrica     | Método         |
 * |-------------|----------------|
 * | Média       | `mean()`       |
 * | Variância   | `variance()`   |
 * | Desvio pad. | `stddev()`     |
 * | Mínimo      | `min()`        |
 * | Máximo      | `max()`        |
 * | Mediana     | `median()`     |
 *
 * @note Os dados são **não** copiados — o chamador deve garantir que o
 *       span permaneça válido durante todo o tempo de vida do objeto.
 *
 * @warning Lança `std::invalid_argument` se o span estiver vazio.
 *
 * @see lerp
 *
 * @code
 * std::vector<double> v = {1, 2, 3, 4, 5};
 * math::Stats s(v);
 * fmt::print("mean={} stddev={}", s.mean(), s.stddev());
 * @endcode
 */
class Stats {
public:
    /**
     * @brief Constrói a calculadora de estatísticas.
     *
     * @param[in] data Vista não-proprietária dos dados.
     * @throws std::invalid_argument se `data` estiver vazio.
     */
    explicit Stats(std::span<const double> data)
        : data_(data)
    {
        if (data_.empty())
            throw std::invalid_argument("Stats: data não pode ser vazio");
    }

    /**
     * @brief Calcula a média aritmética.
     * @return Σ(xᵢ) / n
     */
    [[nodiscard]] double mean() const noexcept {
        return std::accumulate(data_.begin(), data_.end(), 0.0) /
               static_cast<double>(data_.size());
    }

    /**
     * @brief Calcula a variância amostral (divisor n−1).
     *
     * @details
     * Usa a fórmula de Welford para estabilidade numérica em uma única
     * passagem sobre os dados.
     *
     * @return s² = Σ(xᵢ − x̄)² / (n−1)
     *
     * @note Para variância populacional, divida por n em vez de n−1.
     * @bug  Pode acumular erro para datasets > 10⁸ elementos.
     */
    [[nodiscard]] double variance() const noexcept {
        if (data_.size() == 1) return 0.0;
        const double m = mean();
        double acc = 0.0;
        for (double x : data_) {
            double d = x - m;
            acc += d * d;
        }
        return acc / static_cast<double>(data_.size() - 1);
    }

    /**
     * @brief Calcula o desvio padrão amostral.
     * @return √variance()
     * @see variance()
     */
    [[nodiscard]] double stddev() const noexcept {
        return std::sqrt(variance());
    }

    /**
     * @brief Retorna o menor elemento.
     * @return min(data)
     */
    [[nodiscard]] double min() const noexcept {
        return *std::min_element(data_.begin(), data_.end());
    }

    /**
     * @brief Retorna o maior elemento.
     * @return max(data)
     */
    [[nodiscard]] double max() const noexcept {
        return *std::max_element(data_.begin(), data_.end());
    }

    /**
     * @brief Calcula a mediana.
     *
     * @details
     * Para n par, retorna a média dos dois valores centrais.
     * **Copia** o span internamente para não modificar os dados originais.
     *
     * @return Valor central (ou média dos dois centrais para n par).
     */
    [[nodiscard]] double median() const {
        std::vector<double> sorted(data_.begin(), data_.end());
        std::sort(sorted.begin(), sorted.end());
        const size_t n = sorted.size();
        if (n % 2 == 1)
            return sorted[n / 2];
        return (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
    }

    /**
     * @brief Número de elementos no dataset.
     * @return data.size()
     */
    [[nodiscard]] size_t count() const noexcept { return data_.size(); }

private:
    std::span<const double> data_; ///< Vista não-proprietária dos dados.
};

// =============================================================================
//  Classe Vector2D — geometria 2D
// =============================================================================

/**
 * @brief Vetor 2D de ponto flutuante com operações geométricas.
 *
 * @details
 * Implementa as operações mais comuns de álgebra vetorial 2D:
 *  - Adição, subtração e escalonamento
 *  - Produto escalar (dot) e produto vetorial (cross, componente z)
 *  - Comprimento (`length`) e normalização (`normalized`)
 *  - Rotação e reflexão
 *
 * ### Convenção de coordenadas
 * O eixo y aponta para cima (convenção matemática padrão).
 * Para coordenadas de tela (y para baixo), inverta o sinal de y.
 *
 * @note Todos os ângulos estão em **radianos**.
 *
 * @code
 * math::Vector2D a{3.0, 0.0};
 * math::Vector2D b{0.0, 4.0};
 * double d = a.dot(b);           // 0.0
 * double h = (a + b).length();   // 5.0
 * @endcode
 */
struct Vector2D {
    double x; ///< Componente horizontal.
    double y; ///< Componente vertical.

    // ── Construtores ──────────────────────────────────────────────────────

    /**
     * @brief Constrói o vetor nulo.
     */
    constexpr Vector2D() noexcept : x{0.0}, y{0.0} {}

    /**
     * @brief Constrói com coordenadas explícitas.
     * @param[in] x Componente x.
     * @param[in] y Componente y.
     */
    constexpr Vector2D(double x, double y) noexcept : x{x}, y{y} {}

    // ── Operadores aritméticos ────────────────────────────────────────────

    /** @brief Soma vetorial: `(a.x + b.x, a.y + b.y)`. */
    [[nodiscard]] Vector2D operator+(const Vector2D& o) const noexcept {
        return {x + o.x, y + o.y};
    }

    /** @brief Subtração vetorial: `(a.x - b.x, a.y - b.y)`. */
    [[nodiscard]] Vector2D operator-(const Vector2D& o) const noexcept {
        return {x - o.x, y - o.y};
    }

    /**
     * @brief Escalonamento por escalar: `(x*s, y*s)`.
     * @param[in] s Fator de escala.
     */
    [[nodiscard]] Vector2D operator*(double s) const noexcept {
        return {x * s, y * s};
    }

    /** @brief Negação: `(-x, -y)`. */
    [[nodiscard]] Vector2D operator-() const noexcept {
        return {-x, -y};
    }

    // ── Produto escalar e vetorial ────────────────────────────────────────

    /**
     * @brief Produto escalar (dot product).
     *
     * @details
     * `dot(a, b) = |a| * |b| * cos(θ)`, onde θ é o ângulo entre os vetores.
     *
     * @param[in] o Outro vetor.
     * @return `x * o.x + y * o.y`
     *
     * @see cross()
     */
    [[nodiscard]] constexpr double dot(const Vector2D& o) const noexcept {
        return x * o.x + y * o.y;
    }

    /**
     * @brief Componente z do produto vetorial (cross product).
     *
     * @details
     * Em 2D, o resultado é um escalar (a componente z do vetor 3D resultante).
     * Positivo ⇒ `o` está à esquerda de `this`; negativo ⇒ à direita.
     *
     * @param[in] o Outro vetor.
     * @return `x * o.y - y * o.x`
     *
     * @see dot()
     */
    [[nodiscard]] constexpr double cross(const Vector2D& o) const noexcept {
        return x * o.y - y * o.x;
    }

    // ── Métricas ──────────────────────────────────────────────────────────

    /**
     * @brief Comprimento (norma L2) do vetor.
     * @return √(x² + y²)
     * @see length_sq()
     */
    [[nodiscard]] double length() const noexcept {
        return std::hypot(x, y);
    }

    /**
     * @brief Comprimento ao quadrado (evita raiz quadrada).
     *
     * @details
     * Use este método em vez de `length()` quando apenas a comparação
     * de comprimentos é necessária — é mais eficiente pois evita `sqrt`.
     *
     * @return x² + y²
     */
    [[nodiscard]] constexpr double length_sq() const noexcept {
        return x * x + y * y;
    }

    /**
     * @brief Retorna o vetor normalizado (comprimento = 1).
     *
     * @pre `length() > 0`
     * @throws std::domain_error se o vetor for nulo.
     * @return `(*this) * (1 / length())`
     */
    [[nodiscard]] Vector2D normalized() const {
        const double len = length();
        if (len < 1e-12)
            throw std::domain_error("normalized(): vetor nulo");
        return {x / len, y / len};
    }

    /**
     * @brief Rotaciona o vetor por um ângulo.
     *
     * @param[in] angle_rad Ângulo em radianos (sentido anti-horário).
     * @return Vetor rotacionado.
     *
     * @note Use `angle_rad = -θ` para rotação no sentido horário.
     */
    [[nodiscard]] Vector2D rotated(double angle_rad) const noexcept {
        const double c = std::cos(angle_rad);
        const double s = std::sin(angle_rad);
        return {x * c - y * s, x * s + y * c};
    }

    /**
     * @brief Ângulo do vetor em relação ao eixo x positivo.
     * @return Ângulo em radianos, em [-π, π].
     */
    [[nodiscard]] double angle() const noexcept {
        return std::atan2(y, x);
    }

    /**
     * @brief Distância euclidiana até outro ponto.
     * @param[in] o O outro ponto.
     * @return `(o - *this).length()`
     */
    [[nodiscard]] double distance_to(const Vector2D& o) const noexcept {
        return (*this - o).length();
    }

    // ── Comparação ────────────────────────────────────────────────────────

    /**
     * @brief Igualdade exata (raramente útil para doubles).
     *
     * @deprecated Use `nearly_equal` por componente para comparações
     *             práticas de ponto flutuante.
     */
    [[deprecated("Use nearly_equal() para doubles")]]
    bool operator==(const Vector2D& o) const noexcept {
        return x == o.x && y == o.y;
    }
};

// =============================================================================
//  Grupo de funções utilitárias — agrupadas via @defgroup
// =============================================================================

/**
 * @defgroup conversion Conversão de Unidades
 * @brief Funções para conversão entre sistemas de unidades.
 * @{
 */

/**
 * @brief Converte graus para radianos.
 * @param[in] deg Ângulo em graus.
 * @return Ângulo em radianos.
 * @ingroup conversion
 */
[[nodiscard]] constexpr double deg_to_rad(double deg) noexcept {
    return deg * (3.14159265358979323846 / 180.0);
}

/**
 * @brief Converte radianos para graus.
 * @param[in] rad Ângulo em radianos.
 * @return Ângulo em graus.
 * @ingroup conversion
 */
[[nodiscard]] constexpr double rad_to_deg(double rad) noexcept {
    return rad * (180.0 / 3.14159265358979323846);
}

/** @} */ // fim do grupo conversion

} // namespace math
