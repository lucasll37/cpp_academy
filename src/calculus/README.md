# calculus — Álgebra Linear Numérica com Eigen3

## O que é este projeto?

Demonstra as quatro **decomposições matriciais** fundamentais da álgebra linear numérica usando a biblioteca **Eigen3**. Cada decomposição tem casos de uso distintos e serve de base para algoritmos em machine learning, computação científica, engenharia de controle e processamento de sinais.

---

## Conceitos ensinados

| Decomposição | Quando usar | Complexidade |
|---|---|---|
| **LU** (`PartialPivLU`) | Resolver Ax = b, calcular determinante | O(n³) |
| **QR** (`HouseholderQR`) | Mínimos quadrados, sistemas sobredeterminados | O(mn²) |
| **SVD** (`JacobiSVD`) | Compressão, PCA, pseudo-inversa, rank | O(mn²) ou O(m²n) |
| **Autovalores** (`SelfAdjointEigenSolver`) | PCA, estabilidade, vibrações | O(n³) |

---

## Estrutura de arquivos

```
calculus/
├── main.cpp      ← quatro demos em sequência
└── meson.build
```

---

## Demo 1: Decomposição LU

### O que é?

Fatoração `A = P⁻¹ · L · U` onde:
- **P** = matriz de permutação (pivoteamento de linhas para estabilidade numérica)
- **L** = triangular inferior com 1s na diagonal
- **U** = triangular superior

### Para que serve?

- Resolver sistemas lineares `Ax = b` eficientemente
- Uma vez fatorada A, resolver múltiplos `b` diferentes custa apenas O(n²) cada
- Calcular determinante: `det(A) = produto da diagonal de U`

```cpp
Matrix A(3, 3);
A << 2, 1, -1,
    -3, -1,  2,
    -2,  1,  2;

Vector b(3);
b << 8, -11, -3;

Eigen::PartialPivLU<Matrix> lu(A);
Vector x = lu.solve(b);        // resolve Ax = b
double det = lu.determinant(); // det(A) sem custo extra
```

---

## Demo 2: Decomposição QR

### O que é?

Fatoração `A = Q · R` onde:
- **Q** = matriz ortogonal (Q^T · Q = I, colunas ortogonais unitárias)
- **R** = triangular superior

### Para que serve?

**Mínimos quadrados**: quando temos mais equações que incógnitas (m > n), não há solução exata — QR encontra x que minimiza `||Ax - b||²`. Isso é a base de **regressão linear**.

```cpp
// Regressão: 4 pontos, ajustar y = a + b*x
Matrix A(4, 2);   // coluna 1 = 1 (intercepto), coluna 2 = x
A << 1, 1,
     1, 2,
     1, 3,
     1, 4;

Vector b(4);
b << 2.1, 3.9, 6.2, 7.8;  // y com ruído

Eigen::HouseholderQR<Matrix> qr(A);
Vector x = qr.solve(b);   // x = [intercepto, inclinação]
// resultado: y ≈ 0.08 + 1.93 * x
```

---

## Demo 3: SVD (Decomposição em Valores Singulares)

### O que é?

Fatoração `A = U · Σ · V^T` onde:
- **U** = matriz ortogonal (vetores singulares esquerdos)
- **Σ** = diagonal com valores singulares σ₁ ≥ σ₂ ≥ ... ≥ 0
- **V** = matriz ortogonal (vetores singulares direitos)

### Para que serve?

| Aplicação | Como usa a SVD |
|---|---|
| **Rank** | Número de σᵢ > threshold numérico |
| **Número de condição** | σ_max / σ_min (próximo de 1 = estável) |
| **Compressão (rank-k)** | Usar só os k maiores valores singulares |
| **PCA** | SVD da matriz de dados centralizada |
| **Pseudo-inversa** | A⁺ = V · Σ⁺ · U^T |

```cpp
// Compressão: aproximação de rank 1
Matrix A_rank1 = sv(0) * svd.matrixU().col(0) * svd.matrixV().col(0).transpose();
```

---

## Demo 4: Autovalores e Autovetores

### O que é?

Para uma matriz A e um vetor v: `A · v = λ · v`
- **λ** (lambda) = autovalor: quanto A "estica" na direção de v
- **v** = autovetor: direção que A não rotaciona, apenas escala

### `SelfAdjointEigenSolver` vs `EigenSolver`

Para matrizes **simétricas** (A = A^T, como matrizes de covariância), use `SelfAdjointEigenSolver`:
- Mais rápido (explora simetria)
- Numericamente mais estável
- Garante autovalores reais

```cpp
Matrix C(3, 3);
C << 4, 2, 0.6,
     2, 2, 0.5,
     0.6, 0.5, 1;

Eigen::SelfAdjointEigenSolver<Matrix> solver(C);

// Autovalores em ordem crescente
auto eigenvalues  = solver.eigenvalues();   // vetor de λ
auto eigenvectors = solver.eigenvectors();  // colunas = autovetores

// PCA: variância explicada por componente principal
double total = eigenvalues.sum();
// PC1 explica: 100 * eigenvalues(2) / total %  (maior autovalor último)
```

---

## A biblioteca Eigen3

Eigen é uma biblioteca header-only de álgebra linear para C++:

```cpp
using Matrix = Eigen::MatrixXd;  // matriz de doubles, tamanho dinâmico
using Vector = Eigen::VectorXd;  // vetor de doubles

Matrix A = Matrix::Identity(3, 3);  // identidade 3x3
Matrix B = Matrix::Zero(3, 3);      // zeros
Matrix C = Matrix::Random(3, 3);    // valores aleatórios

// Operações básicas
Matrix D = A * B;           // multiplicação
double n = A.norm();        // norma de Frobenius
double d = A.determinant(); // determinante
Matrix T = A.transpose();   // transposta
Matrix I = A.inverse();     // inversa (evitar! use solve())
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja calculus
./bin/calculus
```

---

## Dependências

- `Eigen3` — álgebra linear (header-only)
- `fmt` — formatação de saída
