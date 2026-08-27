# calculus — Álgebra Linear Numérica com Eigen3

## O que é este projeto?

Um executável que mostra as quatro **decomposições matriciais** que sustentam praticamente toda computação numérica —
**LU**, **QR**, **SVD** e **autovalores/autovetores**. Cada demo resolve um problema concreto, imprime as matrizes
intermediárias e **mede o próprio erro**.

O alvo compilado chama-se **`eigen_demo`** (não `calculus` — veja o `meson.build` ao lado):

```bash
make configure                      # uma vez só: Conan baixa Eigen3 + fmt e configura o Meson
meson compile -C build eigen_demo   # compila só este subprojeto
./build/src/calculus/eigen_demo     # roda direto do diretório de build

make install && ./dist/bin/eigen_demo   # ou, a partir do diretório instalado
```

> Se o `meson compile` reclamar de dependência não encontrada, rode antes `source build/conanbuild.sh`.

## Antes de começar: os conceitos que você precisa

Esta seção pressupõe **apenas C++ básico** (funções, classes, `#include`). Todo termo usado no resto do README é
definido aqui. Leia na ordem.

### Matriz, vetor, e o que significa multiplicar

Um **vetor** é uma lista de números: `v = [2, 3, -1]`. Uma **matriz** é uma tabela com `m` linhas e `n` colunas. O
produto `A·v` produz um novo vetor, e a leitura que importa é geométrica: **uma matriz é uma máquina que pega um vetor
e o transforma** — estica, encolhe, gira, achata. Este projeto é sobre descobrir *que* transformação uma matriz faz e
como desmontá-la em transformações simples.

### Sistema linear `Ax = b`

```
 2x₁ +  x₂ -  x₃ =   8          ⎡  2   1  -1 ⎤ ⎡x₁⎤   ⎡  8 ⎤
-3x₁ -  x₂ + 2x₃ = -11    ⟺     ⎢ -3  -1   2 ⎥ ⎢x₂⎥ = ⎢-11 ⎥
-2x₁ +  x₂ + 2x₃ =  -3          ⎣ -2   1   2 ⎦ ⎣x₃⎦   ⎣ -3 ⎦
                                        A        x        b
```

"Resolver `Ax = b`" é achar o `x` que, transformado por `A`, dá `b`. É o problema mais comum da computação científica:
circuitos, estruturas, fluidos, ajuste de curvas — todos viram sistemas lineares em algum ponto.

### Triangular inferior, triangular superior, e por que isso importa

Uma matriz é **triangular superior** (`U`, de *upper*) quando tudo abaixo da diagonal é zero; **triangular inferior**
(`L`, de *lower*) quando tudo acima é zero. Sistemas triangulares são **triviais de resolver**: em `Ux = y`, a última
linha tem uma só incógnita (`0.2·x₃ = y₃` → `x₃ = y₃/0.2`); com `x₃` conhecido, a linha acima também passa a ter uma
só, e assim por diante. Isso é a **substituição retroativa**, que custa `O(n²)` contra `O(n³)` de uma eliminação
gaussiana completa. **Toda a razão de ser da decomposição LU está nesta frase.** (As duas matrizes aparecem impressas
na saída do demo 1, logo abaixo.)

### Identidade e matriz de permutação

A **identidade** `I` é a matriz que não faz nada: `I·v = v`. Uma **matriz de permutação** `P` é apenas uma identidade
com as linhas trocadas de lugar; multiplicar por ela reordena linhas. E `det(P) = (-1)^(número de trocas de duas
linhas)` — guarde isso para a seção do determinante.

```
⎡0 1 0⎤   ⎡ linha A ⎤     ⎡ linha B ⎤
⎢0 0 1⎥ · ⎢ linha B ⎥  =  ⎢ linha C ⎥      (P é só "quem vai para onde")
⎣1 0 0⎦   ⎣ linha C ⎦     ⎣ linha A ⎦
```

### O que é uma decomposição (ou fatoração) matricial

Fatorar um número é escrever `12 = 2·2·3`. Fatorar uma **matriz** é escrever `A = X·Y·Z` com `X`, `Y`, `Z` de tipo
*fácil* (triangular, ortogonal, diagonal). A frase-guia deste README inteiro:

> **Toda decomposição troca uma matriz difícil por um produto de matrizes fáceis.**

Por que compensa? Suponha resolver `Ax = b` para **200 vetores `b` diferentes** (200 cenários de carga numa ponte, 200
quadros de uma simulação). Com eliminação gaussiana você refaz 200 vezes o mesmo trabalho sobre a *mesma* `A`.
Fatorando `A` uma vez em `L·U` (custo `O(n³)`), cada novo `b` custa duas substituições, `O(n²)`. Medido nesta máquina,
`n = 400` e 200 vetores `b`:

| Estratégia | Tempo medido |
|---|---|
| fatorar uma vez (`PartialPivLU lu(M)`) | **2,8 ms** |
| + 200 chamadas a `lu.solve(b)` | **5,0 ms** |
| 200 × `M.partialPivLu().solve(b)` (refatorando toda vez) | **479 ms** |

Mesma resposta, **~60× mais lento**. A diferença inteira é "reutilizei a fatoração".

### Matriz ortogonal

`Q` é **ortogonal** quando `Qᵀ·Q = I`: suas colunas têm comprimento 1 e são perpendiculares entre si. A consequência
que interessa é geométrica — **uma matriz ortogonal é rotação/reflexão pura: não estica nem encolhe nada**, ou seja
`‖Q·v‖ = ‖v‖` para todo `v`. Daí a estabilidade numérica do QR: multiplicar por `Q` nunca amplifica um erro de
arredondamento, porque não amplifica coisa alguma. Bônus: `Q⁻¹ = Qᵀ`, inverter uma ortogonal é só transpor.

### Norma, resíduo e rank

A **norma** `‖v‖` é o comprimento do vetor, `√(v₁² + v₂² + …)`. Para matrizes, `A.norm()` na Eigen é a **norma de
Frobenius**: raiz da soma dos quadrados de *todos* os elementos, como se a matriz fosse um vetor longo. O **resíduo**
de uma solução candidata `x` é `‖A·x − b‖`; em aritmética exata seria 0, em `double` (~16 dígitos) esperamos ~`1e-15`.
**O resíduo é o teste de corretude usado em todo este projeto.**

O **rank** (posto) é o número de direções *realmente independentes* que a matriz produz. Uma 3×3 de rank 3 mapeia o 3D
em todo o 3D; uma de rank 2 **achata** o 3D num plano, e informação é perdida sem volta. Matriz quadrada com rank `< n`
é **rank deficiente** (singular): não tem inversa, e `Ax = b` ou não tem solução ou tem infinitas.

### Autovalor, autovetor e valor singular

Para quase todo `v`, `A·v` aponta para outra direção. Mas há direções especiais em que `A` só **estica ou encolhe**,
sem girar: `A·v = λ·v`. O **autovetor** `v` é a direção preservada; o **autovalor** `λ` é o fator de esticamento
(`λ = 2` dobra o comprimento naquela direção, `λ = 0` esmaga a direção até virar zero).

Autovalores só existem para matrizes quadradas e podem ser complexos. Os **valores singulares** `σ₁ ≥ σ₂ ≥ … ≥ 0` são
o análogo universal: existem para *qualquer* matriz `m×n`, sempre reais e não negativos. Definição operacional: **`σᵢ`
é o quanto `A` estica o espaço na i-ésima direção principal**; `σ₁` é o maior esticamento possível, o máximo de `‖A·v‖`
sobre os `v` de comprimento 1. Verificado com a matriz do demo 3 (`1 2 3 / 4 5 6 / 7 8 9`): amostrando 200 000 vetores
unitários aleatórios, o maior `‖A·v‖` observado foi **16,8481** — exatamente o `σ₁` impresso. Já na direção do terceiro
vetor singular, `‖A·v‖ = 1,3e-15`: uma direção que essa matriz **esmaga**. Daí a regra:

> **rank = quantos valores singulares estão acima da tolerância numérica.**

### Número de condição

`cond(A) = σ_max / σ_min` responde: "se a entrada tiver um errinho, quanto ele cresce na resposta?". Regra de bolso:
com `cond ≈ 10^k` você perde ~`k` dígitos. Como `double` tem ~16, `cond ≈ 1e16` significa **zero dígitos confiáveis**.
Medido com a matriz de Hilbert 12×12 (`H(i,j) = 1/(i+j+1)`, o exemplo clássico de matriz mal-condicionada), num sistema
cuja resposta exata é `x = [1,1,…,1]`: `cond(H) = 1,7e+16`, resíduo `‖Hx − b‖ = 5,2e-16` (**parece perfeito**), mas o
erro real `‖x − x_exato‖ = 2,8e-01` (**28% de erro**). Esta é a armadilha central do assunto: **resíduo pequeno não
garante solução correta** — com `cond` grande, o resíduo mente.

### PCA em cinco linhas

**PCA** (Análise de Componentes Principais) acha as direções em que um conjunto de dados mais varia: (1) calcule a
**matriz de covariância** `C` dos dados, sempre simétrica; (2) os **autovetores** de `C` são as direções principais;
(3) o **autovalor** `λᵢ` é a **variância** naquela direção; (4) a **variância explicada** pelo componente `i` é
`λᵢ/Σλ`; (5) descartar os de `λ` pequeno comprime os dados perdendo pouco.

### A biblioteca Eigen3 — o mínimo para ler o código

**Header-only** significa que a Eigen não tem `.so` nem `.a`: é só código em cabeçalhos. No build, isso quer dizer que
**não existe etapa de link** para ela — basta o compilador achar os `#include`. Aqui o Conan traz `eigen/3.4.0` e o
`meson.build` da raiz o expõe como `eigen_dep`.

```cpp
#include <Eigen/Dense>                // matrizes densas, decomposições e solvers

using Matrix = Eigen::MatrixXd;       // X = dimensão dinâmica, d = double
using Vector = Eigen::VectorXd;

Matrix A(3, 3);                       // aloca no heap; tamanho decidido em runtime
A << 2, 1, -1,                        // o operador << preenche linha a linha
    -3, -1,  2,
    -2,  1,  2;

Eigen::Matrix3d B;                    // tamanho FIXO em compile time: mora na pilha, sem alocação,
                                      // e o compilador desenrola os laços
Matrix I = Matrix::Identity(3, 3);    // também Zero(3,3) e Random(3,3), esta com valores em [-1,1]
Matrix D = A * B;                     // produto
double n = A.norm();                  // norma de Frobenius
Matrix T = A.transpose();
```

> ⚠️ **`auto` é uma armadilha na Eigen.** Ela usa *expression templates*: `A * B` **não** devolve uma matriz, devolve
> um objeto que *descreve* a multiplicação, e a conta só ocorre na atribuição a uma `Matrix`. É isso que evita
> temporários e faz `X = A + B + C` percorrer a memória uma vez só. O preço: `auto D = A * B;` guarda o nó de
> expressão, com referências a `A` e `B` — se elas morrerem antes da avaliação, você lê memória inválida. Escreva o
> tipo concreto: `Matrix D = A * B;`.

> ⚠️ **Não use `A.inverse()` para resolver sistemas.** Medido com `n = 600`: `A.partialPivLu().solve(b)` levou
> **8,3 ms**, contra **30,2 ms** de `A.inverse() * b` (3,6× mais lento). E na Hilbert 12×12 acima, o erro de
> `inverse()*b` foi **48,8** contra **0,28** do `solve()` — 170× pior. Inverter faz mais contas e acumula mais
> arredondamento para responder à mesma pergunta.

## Por que estudar isto?

| Sem decomposições | Com decomposições |
|---|---|
| 200 sistemas com a mesma `A` = 200 eliminações → **479 ms** | fatora uma vez, resolve 200 vezes → **7,8 ms** |
| "minha solução parece errada", e nenhum jeito de saber | `cond(A)` diz de antemão quantos dígitos se perdem |
| ajuste via equações normais (`AᵀA`), que **eleva o `cond` ao quadrado** | QR ajusta sem nunca formar `AᵀA` |
| matriz singular só se revela quando aparece `inf`/`nan` | SVD dá o rank numérico e *quanta* informação sobrou |

Onde cada uma aparece na prática: **LU** em malhas de circuitos, redes de tubulação, análise estrutural e simulações
que resolvem o mesmo sistema a cada passo de tempo; **QR** na regressão linear dentro de `numpy.polyfit` e do `lm()` do
R; **SVD** em compressão de imagem, recomendação, pseudo-inversa e cálculo de rank; **autovalores** em PCA, frequências
naturais de uma ponte ou de uma asa (o modo de vibração *é* o autovetor), controle e PageRank.

## Exemplo mínimo

O menor programa completo. Copie, compile e rode **antes** de ler o resto:

```cpp
#include <Eigen/Dense>
#include <iostream>

int main() {
    Eigen::Matrix2d A;
    A << 3, 1,
         1, 2;
    Eigen::Vector2d b(9, 8);

    Eigen::Vector2d x = A.partialPivLu().solve(b);

    std::cout << "x = " << x.transpose() << '\n';             // 2 3
    std::cout << "residuo = " << (A * x - b).norm() << '\n';  // ~0 (1e-16)
}
```

```bash
g++ -O2 -I $(find ~/.conan2 -type d -name eigen3 | head -1) minimo.cpp -o minimo && ./minimo
```

## Estrutura de arquivos

```
src/calculus/
├── main.cpp      ← os quatro demos, na ordem: LU → QR → SVD → autovalores
├── meson.build   ← declara executable('eigen_demo', ...) com fmt_dep + eigen_dep
└── README.md     ← este arquivo
```

`main.cpp` tem duas utilitárias (`print` e `print_vec`, que só formatam com `fmt`) e quatro `demo_*` em sequência.
## Demo 1 — Decomposição LU

**O problema.** Resolver `Ax = b` sem refazer a eliminação gaussiana a cada novo `b`.

**A ideia.** A eliminação gaussiana já *é* uma fatoração, só que ninguém guarda o resultado. A LU guarda: os
multiplicadores usados para zerar cada coluna viram `L`, e a matriz escalonada final vira `U`. A Eigen ainda registra
as trocas de linha em `P`, de modo que `P · A = L · U` ⟺ `A = P⁻¹ · L · U`. Resolver `Ax = b` vira então três passos
baratos: permutar `b`, substituição direta com `L`, substituição retroativa com `U`.

```cpp
Matrix A(3, 3);
A << 2, 1, -1,
    -3, -1,  2,
    -2,  1,  2;

Vector b(3);
b << 8, -11, -3;

Eigen::PartialPivLU<Matrix> lu(A);   // O(n³): a fatoração acontece AQUI, no construtor
Vector x   = lu.solve(b);            // O(n²): pode ser chamado quantas vezes quiser
double det = lu.determinant();       // reaproveita a mesma fatoração
```

**Saída real deste demo:**

```
L (triangular inferior):          U (triangular superior):
    1.0000   0.0000   0.0000         -3.0000  -1.0000   2.0000
    0.6667   1.0000   0.0000          0.0000   1.6667   0.6667
   -0.6667   0.2000   1.0000          0.0000   0.0000   0.2000
x (solução de Ax = b):     2.0000   3.0000  -1.0000
norma do resíduo ||Ax - b||: 8.88e-16     ← ~0: a solução está correta
det(A): -1.0000
```

### Por que pivotear

**Pivoteamento parcial** é trocar linhas para que o divisor (o *pivô*) seja o de maior valor absoluto na coluna. Sem
isso, um pivô minúsculo destrói a resposta. Em `[[1e-18, 1], [1, 1]]·x = [1, 2]` (resposta exata `x ≈ [1, 1]`), sem
pivoteamento o multiplicador é `1/1e-18 = 1e18` e a conta `1 − 1e18·1` arredonda para `−1e18`, **absorvendo e
perdendo** a informação do `1` original. Resultado medido:

| Método | `x` obtido |
|---|---|
| eliminação ingênua (pivô = 1e-18) | `[0, 1]` ← **x₁ 100% errado** |
| `partialPivLu()` (troca as linhas) | `[1, 1]` ← correto |

> ⚠️ É `A = P⁻¹·L·U`, não `A = L·U`. Ignorar `P` numa implementação própria dá exatamente o desastre acima.

### O determinante, com o sinal certo

Como `det(L) = 1` (diagonal só de 1s) e `det(P) = ±1`, vale
`det(A) = (-1)^(nº de trocas de linha) · (produto da diagonal de U)`.

> ⚠️ O sinal é onde quase todo mundo erra ao implementar à mão. Neste demo, `P` faz uma rotação cíclica das três
> linhas — **duas** transposições, logo `det(P) = +1` — e o produto da diagonal de `U` é `(-3)·1,6667·0,2 = -1`.
> Confere com o `-1.0000` impresso.

## Demo 2 — Decomposição QR e mínimos quadrados

**O problema.** Você mediu quatro pontos e quer a reta `y = a + b·x` que melhor os descreve. São **4 equações e 2
incógnitas**: um **sistema sobredeterminado**. Como os dados têm ruído, **nenhuma reta passa pelos quatro pontos** —
não existe solução exata.

```
 y                              ● (4, 7.8)
 6 |            ● (3, 6.2) ╱
   |       ╱   ┊ ← resíduo: distância vertical do ponto à reta
 4 |  ● (2, 3.9)
 2 |● (1, 2.1)
   +--------------------------- x
     1    2    3    4
```

**Mínimos quadrados** é escolher a reta que minimiza `‖Ax − b‖²`, ou seja **a soma dos quadrados daqueles segmentos
verticais**. Ao quadrado porque assim erros para cima e para baixo não se cancelam, e desvios grandes pesam mais.

**Por que o QR resolve isso.** Como `Q` é ortogonal e preserva comprimentos,
`‖Ax − b‖ = ‖Qᵀ(Ax − b)‖ = ‖Rx − Qᵀb‖`. Minimizar um vira minimizar o outro — mas `R` é triangular e suas últimas
linhas são zeros. Basta resolver o sistema triangular das duas primeiras linhas; o que sobra nas linhas de baixo *é* o
erro mínimo inevitável.

```cpp
Matrix A(4, 2);          // coluna 1 = 1s (o intercepto), coluna 2 = os x
A << 1, 1,
     1, 2,
     1, 3,
     1, 4;

Vector b(4);
b << 2.1, 3.9, 6.2, 7.8; // os y medidos, com ruído

Eigen::HouseholderQR<Matrix> qr(A);
Vector x = qr.solve(b);  // x(0) = intercepto, x(1) = inclinação
```

**Saída real:** `reta ajustada: y = 0.1500 + 1.9400 * x`, com `resíduo ||Ax - b||: 0.2864`.

> ⚠️ Aqui o resíduo **não** vai a zero, e isso é esperado: ele *é* o erro de ajuste, a distância que sobra entre os
> dados e a melhor reta possível. Confundi-lo com "erro numérico" é o mal-entendido mais comum do QR.

### Por que `Q` é 4×4 e `R` é 4×2

É a primeira dúvida de quem olha a saída. O `HouseholderQR` faz o **QR completo**: `Q` é `m×m` (4×4, uma base
ortonormal do espaço inteiro) e `R` é `m×n` (4×2), com as duas últimas linhas zeradas — `[-2, -5; 0, -2.2361; 0, 0;
0, 0]`, exatamente como o programa imprime. No **QR reduzido** guarda-se só as `n` primeiras colunas de `Q` (4×2) e o
bloco `n×n` de `R` (2×2), tudo de que o `solve` precisa. A Eigen só materializa `Q` se você pedir
(`qr.householderQ()`); internamente guarda apenas os refletores de Householder, mais compactos e mais estáveis.

> ⚠️ Existe um caminho mais curto para mínimos quadrados: as **equações normais** `AᵀA x = Aᵀb`. Evite. Formar `AᵀA`
> **eleva o número de condição ao quadrado** — um problema com `cond = 1e8` (tolerável) vira `1e16` (irrecuperável). O
> QR chega à mesma resposta sem nunca formar esse produto.

## Demo 3 — SVD (Decomposição em Valores Singulares)

**O problema.** E se a matriz não for quadrada, ou for singular, ou você quiser saber *quão perto* de singular ela
está? LU e QR não respondem. A SVD responde tudo, via `A = U · Σ · Vᵀ`.

Leia da direita para a esquerda, como **três passos geométricos**: `Vᵀ` **gira** o espaço para alinhar os eixos com as
direções principais de `A`; `Σ` (diagonal) **estica** cada eixo pelo fator `σᵢ`; `U` **gira** de novo para a orientação
final. Ou seja: *toda* matriz, de qualquer formato, é rotação → esticamento por eixo → rotação.

```cpp
Matrix A(3, 3);
A << 1, 2, 3,
     4, 5, 6,
     7, 8, 9;

Eigen::JacobiSVD<Matrix> svd(A, Eigen::ComputeThinU | Eigen::ComputeThinV);

Vector sv   = svd.singularValues();        // já vem em ordem DEcrescente
double cond = sv(0) / sv(sv.size() - 1);   // número de condição
int    r    = (int) svd.rank();            // rank numérico

// aproximação de rank 1: só a direção de maior esticamento
Matrix A_rank1 = sv(0) * svd.matrixU().col(0) * svd.matrixV().col(0).transpose();
```

**Saída real:**

```
Σ (valores singulares):   16.8481    1.0684    0.0000   ← o terceiro é 2.1e-16, ou seja, ZERO
número de condição: 7.84e+16
rank numérico: 2
erro de aproximação rank-1: 1.0684
```

### A lição escondida: esta matriz é deliberadamente singular

`1 2 3 / 4 5 6 / 7 8 9` é **rank deficiente de propósito**: a terceira linha é `2·(segunda) − (primeira)`. Há só duas
linhas independentes, logo `rank = 2`, logo `σ₃ = 0` a menos de arredondamento (`2,1e-16`) — e por isso
`cond = σ₁/σ₃ = 7,8e16` explode. **O `cond` astronômico não é defeito do algoritmo, é o diagnóstico correto de uma
matriz sem inversa.** Um `A.inverse()` aqui produziria lixo silencioso.

### Compressão de rank reduzido e pseudo-inversa

Somando só os `k` primeiros termos `σᵢ·uᵢ·vᵢᵀ` obtém-se a melhor aproximação possível de rank `k` (teorema de
Eckart–Young). Com `k = 1`, o erro de Frobenius medido foi **1,0684** — exatamente `σ₂`, como o teorema prevê, já que
`σ₃ ≈ 0`. É o princípio da compressão de imagem por SVD e dos sistemas de recomendação.

A **pseudo-inversa de Moore-Penrose** `A⁺ = V·Σ⁺·Uᵀ` (onde `Σ⁺` inverte cada `σᵢ` **não nulo** e deixa os zeros como
zeros) dá resposta sensata a `Ax = b` mesmo sem inversa: `x = A⁺b` é a solução de menor norma entre as de menor
resíduo. Ignorar os `σ` minúsculos, em vez de dividir por eles, é o que impede o resultado de explodir.

### Thin vs full, e por que `U` aqui é 3×3

`ComputeThinU | ComputeThinV` pede as versões **reduzidas**: para `A` de `m×n` com `m > n`, `U` sai `m×n` em vez de
`m×m` — as colunas extras do `U` completo multiplicam zeros de `Σ` e não influenciam nada. Neste demo `A` é quadrada,
então thin e full coincidem e `U` sai 3×3. Verificado com a matriz 4×2 do demo do QR: `ComputeThinU` dá `U` **4×2**;
`ComputeFullU` dá `U` **4×4** — o mesmo fenômeno do `Q` 4×4 do demo 2, onde o `HouseholderQR` entrega a versão completa.

> ⚠️ **`JacobiSVD` vs `BDCSVD`.** `JacobiSVD` é a mais precisa e a mais lenta (`O(n³)` com constante alta); use até
> algumas centenas de linhas. Para matrizes grandes, `BDCSVD` (*bidiagonal divide and conquer*) é ordens de grandeza
> mais rápida com precisão equivalente — a própria Eigen recomenda `BDCSVD` acima de ~16 linhas.

## Demo 4 — Autovalores, autovetores e PCA

**O problema.** Dado um conjunto de dados, quais são as direções em que ele mais varia? A **matriz de covariância** `C`
responde: `C(i,j)` mede o quanto as variáveis `i` e `j` variam juntas. Ela é **sempre simétrica** (`C = Cᵀ`), e é aí
que entra o solver especializado.

```cpp
Matrix C(3, 3);
C << 4,   2,   0.6,
     2,   2,   0.5,
     0.6, 0.5, 1;

Eigen::SelfAdjointEigenSolver<Matrix> solver(C);

Vector lambdas = solver.eigenvalues();    // ORDEM CRESCENTE
Matrix V       = solver.eigenvectors();   // colunas = autovetores, na mesma ordem
double total   = lambdas.sum();           // = traço de C = 4 + 2 + 1 = 7
```

**`SelfAdjointEigenSolver` vs `EigenSolver`:** para matrizes simétricas o primeiro é mais rápido (explora a simetria),
mais estável, e **garante autovalores reais** com autovetores ortogonais. O `EigenSolver` genérico devolve tipos
complexos — usá-lo numa matriz simétrica é pagar mais caro por um resultado pior.

**Saída real:**

```
autovalores:  0.6945   0.9326   5.3729       ← CRESCENTE
verificação C·v = λ·v (resíduo por autovetor):
  λ0 =  0.6945  ||C·v - λ·v|| = 1.14e-15
  λ1 =  0.9326  ||C·v - λ·v|| = 4.90e-16
  λ2 =  5.3729  ||C·v - λ·v|| = 2.22e-15
variância explicada por componente:
  PC1: 76.8%      PC2: 13.3%      PC3: 9.9%
```

> ⚠️ **O `SelfAdjointEigenSolver` devolve os autovalores em ordem CRESCENTE.** O componente principal PC1 é o
> **último** elemento (`eigenvalues()(n-1)`), não o primeiro, e o autovetor correspondente é a **última coluna** de
> `eigenvectors()`. É por isso que o laço do demo é `for (int i = 2; i >= 0; --i)`. Trocar por `i = 0..2` faria o
> programa reportar a direção de *menor* variância como componente principal. (A `JacobiSVD` faz o oposto: ordem
> decrescente.)

Interpretação PCA: **76,8% de toda a variação cabe numa única direção**; descartar PC3 custaria 9,9% da informação.
Note que `0,6945 + 0,9326 + 5,3729 = 7,0000`, o **traço** (soma da diagonal) de `C` — a soma dos autovalores é sempre
o traço, e é um teste de sanidade grátis.

## Como validar

```bash
meson compile -C build eigen_demo && ./build/src/calculus/eigen_demo   # após um `make configure`
```

O programa é o próprio teste. Confira, na ordem em que aparecem:

| # | O que olhar | Esperado | Se falhar |
|---|---|---|---|
| 1 | `‖Ax - b‖` do demo LU | `8.88e-16` (~0) | a solução do sistema está errada |
| 2 | `det(A)` do demo LU | `-1.0000` | sinal da permutação ignorado |
| 3 | `resíduo` do demo QR | `0.2864`, **não** zero | é o erro de ajuste, e deve ser assim |
| 4 | `Σ` do demo SVD | `16.8481, 1.0684, 0.0000` | o terceiro σ ≈ 0 confirma o rank 2 |
| 5 | `rank numérico` | `2` | a matriz do demo é singular de propósito |
| 6 | `erro rank-1` | `1.0684`, igual a `σ₂` | Eckart–Young confirmado |
| 7 | `‖C·v - λ·v‖` (×3) | `~1e-15` | algum par (λ, v) está incorreto |
| 8 | soma das % de variância | `100.0%` | e a soma dos λ deve dar 7, o traço de `C` |

**Por que `1e-15` e não `0`?** Um `double` guarda ~15–17 dígitos decimais, e o menor passo relativo representável
(*epsilon* da máquina) é `2,2e-16`. Cada operação arredonda e os erros se acumulam. Portanto `8,88e-16` **é** zero para
todos os efeitos. Nunca compare ponto flutuante com `== 0`: use tolerância proporcional à escala do problema, algo como
`residuo < 1e-12 * A.norm()`.

## Escolhendo um solver diante de um problema novo

**Mais robusto significa mais lento.** Comece pela linha de cima e desça só se precisar.
| Classe | Use quando | Custo | Observação |
|---|---|---|---|
| `LLT` | simétrica **positiva definida** | `~n³/3` | o mais rápido de todos (Cholesky) |
| `PartialPivLU` | quadrada e **sabidamente invertível** | `~2n³/3` | o padrão para `Ax = b` |
| `FullPivLU` | quadrada, pode ser singular | `~n³` | pivoteia linhas *e* colunas; dá rank e núcleo |
| `HouseholderQR` | mínimos quadrados, posto cheio | `~2mn²` | o QR mais rápido, sem pivoteamento |
| `ColPivHouseholderQR` | mínimos quadrados, posto duvidoso | `~2mn²` | pivoteia colunas; detecta deficiência de posto |
| `BDCSVD` | matrizes grandes, qualquer formato | alto | a SVD de uso geral acima de ~16 linhas |
| `JacobiSVD` | matrizes pequenas, precisão máxima | muito alto | a mais precisa que existe |
| `SelfAdjointEigenSolver` | autovalores de `A = Aᵀ` | `~9n³` | reais, ordem **crescente** |

Na Hilbert 12×12 da seção do número de condição, o erro medido foi `2,8e-01` com `PartialPivLU` e `1,6e-01` com
`ColPivHouseholderQR` — a robustez extra do pivoteamento de colunas aparece exatamente nos casos mal-condicionados.

## Referências

Subprojetos irmãos deste repositório:

- [`../performance/README.md`](../performance/README.md) — como medir tempo honestamente, o método deste README.
- [`../templates/README.md`](../templates/README.md) — templates e CRTP, a máquina das *expression templates*.
- [`../layout/README.md`](../layout/README.md) — alinhamento e padding, por que a Eigen exige 16/32 bytes.
- [`../../README.md`](../../README.md) — build do repositório inteiro.

Documentação oficial da Eigen 3.4 (a versão usada aqui, via Conan):

- [Linear algebra and decompositions](https://eigen.tuxfamily.org/dox/group__TutorialLinearAlgebra.html) — cobre os
  demos 1, 2 e 3.
- [Eigenvalues module](https://eigen.tuxfamily.org/dox/group__Eigenvalues__Module.html) — cobre o demo 4.
- [Catalogue of dense decompositions](https://eigen.tuxfamily.org/dox/group__TopicLinearAlgebraDecompositions.html) —
  a tabela oficial de velocidade × robustez que a seção acima resume.
- [Quick reference guide](https://eigen.tuxfamily.org/dox/group__QuickRefPage.html) — folha de consulta da API densa.
- [Common pitfalls](https://eigen.tuxfamily.org/dox/TopicPitfalls.html) — a armadilha do `auto`.

Livro canônico: Trefethen & Bau, *Numerical Linear Algebra* — a exposição mais clara de QR e SVD.
