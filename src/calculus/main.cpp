#include <Eigen/Dense>
#include <fmt/core.h>
#include <cmath>

using Matrix = Eigen::MatrixXd;
using Vector = Eigen::VectorXd;

// ============================================================
// UTILIDADE — imprime uma matriz com label
// ============================================================
static void print(const char* label, const Matrix& m) {
    fmt::println("{}:", label);
    for (int r = 0; r < m.rows(); ++r) {
        fmt::print("  ");
        for (int c = 0; c < m.cols(); ++c)
            fmt::print("{:8.4f} ", m(r, c));
        fmt::println("");
    }
    fmt::println("");
}

static void print_vec(const char* label, const Vector& v) {
    fmt::println("{}:", label);
    fmt::print("  ");
    for (int i = 0; i < v.size(); ++i)
        fmt::print("{:8.4f} ", v(i));
    fmt::println("\n");
}

// ============================================================
// 1. DECOMPOSIÇÃO LU (PartialPivLU)
//
// A = P⁻¹ · L · U
//   P = matriz de permutação (pivoteamento de linhas)
//   L = triangular inferior com 1s na diagonal
//   U = triangular superior
//
// PARA QUÊ SERVE:
//   Resolver sistemas Ax = b de forma eficiente.
//   Calcular determinante.
//   Inverter matrizes.
//
// COMPLEXIDADE: O(n³) para fatorar, O(n²) para cada solve.
// Uma vez fatorada, resolver múltiplos b's é barato.
// ============================================================
static void demo_lu() {
    fmt::println("══════════════════════════════════════");
    fmt::println("  1. DECOMPOSIÇÃO LU");
    fmt::println("══════════════════════════════════════\n");

    Matrix A(3, 3);
    A << 2, 1, -1,
        -3, -1,  2,
        -2,  1,  2;

    Vector b(3);
    b << 8, -11, -3;

    // PartialPivLU: pivoteamento parcial — estável para matrizes invertíveis
    Eigen::PartialPivLU<Matrix> lu(A);

    // extraindo L e U manualmente para visualização
    Matrix LU  = lu.matrixLU();
    Matrix L   = Matrix::Identity(3, 3);
    Matrix U   = Matrix::Zero(3, 3);

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            if (i > j) L(i, j) = LU(i, j);
            if (i <= j) U(i, j) = LU(i, j);
        }

    print("A", A);
    print("L (triangular inferior)", L);
    print("U (triangular superior)", U);

    // resolver Ax = b
    Vector x = lu.solve(b);
    print_vec("x (solução de Ax = b)", x);

    // verificação: A*x deve ser igual a b
    Vector residuo = A * x - b;
    fmt::println("norma do resíduo ||Ax - b||: {:.2e}\n", residuo.norm());

    // determinante via LU — produto dos elementos da diagonal de U
    fmt::println("det(A): {:.4f}\n", lu.determinant());
}

// ============================================================
// 2. DECOMPOSIÇÃO QR (HouseholderQR)
//
// A = Q · R
//   Q = matriz ortogonal (Q^T · Q = I)
//   R = triangular superior
//
// PARA QUÊ SERVE:
//   Resolver sistemas sobredeterminados (mínimos quadrados).
//   Base para algoritmos de autovalores.
//   Mais estável numericamente que LU para sistemas mal-condicionados.
//
// MÍNIMOS QUADRADOS: quando temos mais equações que incógnitas
//   (m > n), não há solução exata — QR encontra x que minimiza ||Ax - b||².
// ============================================================
static void demo_qr() {
    fmt::println("══════════════════════════════════════");
    fmt::println("  2. DECOMPOSIÇÃO QR");
    fmt::println("══════════════════════════════════════\n");

    // sistema sobredeterminado: 4 equações, 2 incógnitas
    // representa uma regressão linear y = a + b*x
    Matrix A(4, 2);
    A << 1, 1,
         1, 2,
         1, 3,
         1, 4;

    Vector b(4);
    b << 2.1, 3.9, 6.2, 7.8;  // dados com ruído

    Eigen::HouseholderQR<Matrix> qr(A);

    Matrix Q = qr.householderQ();
    Matrix R = qr.matrixQR().triangularView<Eigen::Upper>();

    print("A (sistema sobredeterminado)", A);
    print("Q (ortogonal, 4×4)", Q);
    print("R (triangular superior, 4×2)", R);

    // solução de mínimos quadrados: minimiza ||Ax - b||²
    Vector x = qr.solve(b);
    print_vec("x (mínimos quadrados: [intercepto, inclinação])", x);

    fmt::println("reta ajustada: y = {:.4f} + {:.4f} * x", x(0), x(1));
    fmt::println("resíduo ||Ax - b||: {:.4f}\n", (A * x - b).norm());
}

// ============================================================
// 3. DECOMPOSIÇÃO SVD (JacobiSVD)
//
// A = U · Σ · V^T
//   U = matriz ortogonal (vetores singulares esquerdos)
//   Σ = diagonal com valores singulares σ₁ ≥ σ₂ ≥ ... ≥ 0
//   V = matriz ortogonal (vetores singulares direitos)
//
// PARA QUÊ SERVE:
//   Compressão de dados (rank reduzido).
//   PCA (análise de componentes principais).
//   Pseudo-inversa de Moore-Penrose.
//   Mede o "quão invertível" é uma matriz via número de condição.
//
// NÚMERO DE CONDIÇÃO: σ_max / σ_min
//   Próximo de 1 → bem condicionada
//   Muito grande → mal condicionada (soluções instáveis)
// ============================================================
static void demo_svd() {
    fmt::println("══════════════════════════════════════");
    fmt::println("  3. DECOMPOSIÇÃO SVD");
    fmt::println("══════════════════════════════════════\n");

    Matrix A(3, 3);
    A << 1, 2, 3,
         4, 5, 6,
         7, 8, 9;

    // ComputeThinU/V: só calcula as colunas necessárias (mais eficiente)
    Eigen::JacobiSVD<Matrix> svd(A, Eigen::ComputeThinU | Eigen::ComputeThinV);

    print("A", A);
    print("U (vetores singulares esquerdos)", svd.matrixU());
    print_vec("Σ (valores singulares)", svd.singularValues());
    print("V (vetores singulares direitos)", svd.matrixV());

    // número de condição
    Vector sv = svd.singularValues();
    double cond = sv(0) / sv(sv.size() - 1);
    fmt::println("número de condição: {:.2e}", cond);
    fmt::println("(valores singulares próximos de zero → rank deficiente)\n");

    // rank numérico (valores singulares acima de threshold)
    fmt::println("rank numérico: {}\n", (int)svd.rank());

    // reconstrução de rank reduzido (compressão)
    // usando apenas o maior valor singular
    Matrix A_rank1 = sv(0) * svd.matrixU().col(0) * svd.matrixV().col(0).transpose();
    print("A reconstruída com rank 1 (aproximação)", A_rank1);
    fmt::println("erro de aproximação rank-1: {:.4f}\n", (A - A_rank1).norm());
}

// ============================================================
// 4. AUTOVALORES E AUTOVETORES (SelfAdjointEigenSolver)
//
// A · v = λ · v
//   λ = autovalor (scalar)
//   v = autovetor (direção que A só escala, não rotaciona)
//
// PARA QUÊ SERVE:
//   PCA (autovalores da matriz de covariância).
//   Análise de estabilidade de sistemas dinâmicos.
//   Resolver equações diferenciais.
//   Vibração de estruturas (autovalores = frequências naturais).
//
// SelfAdjointEigenSolver: para matrizes simétricas (A = A^T).
//   Mais rápido e numericamente estável que o caso geral.
//   Autovalores são sempre reais.
// ============================================================
static void demo_eigen() {
    fmt::println("══════════════════════════════════════");
    fmt::println("  4. AUTOVALORES E AUTOVETORES");
    fmt::println("══════════════════════════════════════\n");

    // matriz de covariância (sempre simétrica)
    Matrix C(3, 3);
    C << 4, 2, 0.6,
         2, 2, 0.5,
         0.6, 0.5, 1;

    Eigen::SelfAdjointEigenSolver<Matrix> solver(C);

    print("C (matriz de covariância)", C);
    print_vec("autovalores (variância em cada direção)", solver.eigenvalues());
    print("autovetores (colunas = direções principais)", solver.eigenvectors());

    // verificação: C * v = λ * v para cada par
    fmt::println("verificação C·v = λ·v (resíduo por autovetor):");
    for (int i = 0; i < 3; ++i) {
        double lambda = solver.eigenvalues()(i);
        Vector v      = solver.eigenvectors().col(i);
        double res    = (C * v - lambda * v).norm();
        fmt::println("  λ{} = {:7.4f}  ||C·v - λ·v|| = {:.2e}", i, lambda, res);
    }

    // variância explicada (como em PCA)
    fmt::println("\nvariância explicada por componente:");
    double total = solver.eigenvalues().sum();
    for (int i = 2; i >= 0; --i) {  // eigenvalues vêm em ordem crescente
        double pct = 100.0 * solver.eigenvalues()(i) / total;
        fmt::println("  PC{}: {:.1f}%", 3 - i, pct);
    }
    fmt::println("");
}

int main() {
    demo_lu();
    demo_qr();
    demo_svd();
    demo_eigen();
    return 0;
}