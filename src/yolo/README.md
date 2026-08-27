# yolo — Detecção de Objetos com YOLOv10 e ONNX Runtime em C++

## O que é este projeto?

Pegar uma imagem e responder **o que há nela e onde**: uma rede neural já treinada
(YOLOv10) é carregada de um arquivo `.onnx` e executada em C++ pelo ONNX Runtime,
sem Python nenhum em tempo de execução.

Os modelos **já estão versionados neste repositório** — não é preciso exportar nada
para rodar:

```bash
# a partir da RAIZ do repositório
make configure && make build && make install
./dist/bin/yolo src/yolo/models/yolov10n.onnx src/yolo/images/teste.jpg
```

> ⚠ O alvo deste subprojeto pode estar desligado. Abra `src/meson.build` e confira a
> linha `subdir('yolo')`: se ela estiver comentada (`# subdir('yolo')`), descomente
> antes de rodar `make configure`. Sem isso o binário não é gerado.

> ⚠ Rode **a partir da raiz do repositório**. Os caminhos acima (`src/yolo/models/…`)
> são relativos ao diretório de trabalho; de dentro de `dist/` eles não existem.

O executável se chama **`yolo`** (é o nome declarado em `executable('yolo', …)` no
`meson.build` deste diretório). Ele vive em dois lugares:

| Caminho | Quando existe |
|---|---|
| `build/src/yolo/yolo` | logo após `make build` |
| `dist/bin/yolo` | após `make install` |

---

## Antes de começar: os conceitos que você precisa

Esta é a seção mais importante do documento. Nada aqui pressupõe visão computacional,
aprendizado de máquina ou ONNX — só C++ básico. Cada termo usado no resto do README é
definido abaixo, na ordem em que o pipeline os encontra.

### Imagem é uma matriz de bytes

Uma foto colorida de 672 por 448 pixels é, na memória, um bloco de
`448 × 672 × 3 = 903 168` bytes. Três bytes por pixel, um por canal de cor, cada um de
0 a 255. É exatamente isso que um `cv::Mat` do OpenCV guarda.

Dois detalhes que causam bugs:

- **O OpenCV usa BGR, não RGB.** Por herança dos formatos de arquivo do Windows, o
  primeiro byte de cada pixel é o azul. Quase todo modelo de rede neural foi treinado
  em RGB. Trocar a ordem é obrigatório — é o `cv::cvtColor(…, cv::COLOR_BGR2RGB)`.
- **A ordem dos eixos é HWC**: primeiro a linha (Height), depois a coluna (Width),
  depois o canal (Channel). Os três bytes de um mesmo pixel ficam **grudados**.

### Tensor

**Tensor** é só o nome que se dá a um array multidimensional numa biblioteca de redes
neurais, junto com a lista de tamanhos de cada dimensão — o **shape**.

- shape `[448, 672, 3]` → a imagem acima.
- shape `[1, 3, 640, 640]` → o que este modelo exige na entrada. Lê-se:
  1 imagem por vez (**batch** = 1), 3 canais, 640 linhas, 640 colunas.
- shape `[1, 300, 6]` → o que este modelo devolve.

O modelo aqui usado tem a entrada **fixa** em `[1, 3, 640, 640]`. O programa imprime
esse shape ao carregar, lido do próprio arquivo:

```
[YOLOv10] modelo carregado: src/yolo/models/yolov10n.onnx → input shape [1,3,640,640]
```

**Batch** é quantas imagens vão juntas numa única chamada. Batch 1 é o caso de latência
mínima (uma câmera ao vivo); batches maiores rendem mais imagens por segundo, mas cada
uma espera as outras.

### Bounding box, score, classe, detecção

**Bounding box** (caixa delimitadora) é o retângulo que envolve um objeto. Duas
convenções coexistem neste projeto, e confundi-las desloca todas as caixas:

- o **modelo** devolve `x1, y1, x2, y2` — canto superior-esquerdo e canto
  inferior-direito;
- o **`cv::Rect2f`** guarda `x, y, largura, altura` — canto superior-esquerdo e tamanho.

A conversão é literalmente a linha `cv::Rect2f(x1, y1, x2 - x1, y2 - y1)` em
`yolov10.hpp`.

**Score** (confiança) é um número em `[0, 1]`: o quanto o modelo aposta que aquela caixa
contém mesmo um objeto daquela classe. Não é probabilidade calibrada, é uma nota. O
`score_threshold` (padrão `0.25f`) descarta tudo abaixo dele: **subir** o limiar dá menos
caixas e menos falsos positivos; **descer** dá mais caixas e mais lixo. Nas medições deste
README, o mesmo modelo na mesma imagem entrega 9 detecções a 0.25 e entregaria 5 a 0.50 —
as quatro descartadas têm score entre 0.30 e 0.40.

**Classe** é o índice do tipo do objeto, de 0 a 79. O nome vem da lista
`COCO_CLASSES` em `yolov10.hpp`.

**COCO** (*Common Objects in Context*) é o conjunto de dados em que o modelo foi
treinado: cerca de 120 mil fotos anotadas em **80 categorias** — pessoa, carro,
bicicleta, cachorro, garrafa, laptop… A consequência prática é dura e precisa estar
clara: **este detector só enxerga essas 80 coisas**. Ele nunca vai encontrar um
parafuso, uma placa de circuito ou um tumor, por melhor que a foto seja.

**Detecção**, então, é a tripla `(caixa, score, classe)` — exatamente o `struct Detection`
do código.

### O que é ONNX e o que é o ONNX Runtime

Uma rede neural treinada é duas coisas: um **grafo** de operações (convolução → soma →
ativação → …) e os **pesos**, as centenas de milhares de números que essas operações
usam.

**ONNX** (*Open Neural Network Exchange*) é um **formato de arquivo** que guarda as duas.
Um `.onnx` é um Protobuf contendo a lista de nós do grafo, o nome de cada tensor e os
pesos em binário. O `yolov10n.onnx` deste repositório tem 9,1 MB; o `yolov10m.onnx`,
59 MB — a diferença é quantidade de pesos.

**ONNX Runtime** é o **motor** que lê esse arquivo e executa o grafo. Ele não sabe o que é
YOLO; ele sabe executar convoluções. O vocabulário da API C++ que aparece no código:

| Tipo | O que é |
|---|---|
| `Ort::Env` | ambiente global do runtime (logging, thread pools). Um por processo. |
| `Ort::SessionOptions` | as opções com que a sessão será criada (threads, otimizações, provedores). |
| `Ort::Session` | **um modelo carregado**, pronto para executar. É o objeto caro. |
| `Ort::Value` | um tensor (entrada ou saída). |
| `Ort::MemoryInfo` | descreve **onde** o tensor mora (CPU, GPU). |
| `Ort::AllocatorWithDefaultOptions` | alocador usado para devolver as strings de nomes. |

Por que o código guarda `std::unique_ptr<Ort::Session> session_` e não `Ort::Session`
por valor? Porque **`Ort::Session` não é construtível por padrão** — só existe a partir de
um arquivo de modelo. Como o construtor precisa montar as `SessionOptions` antes, a
sessão só pode nascer no corpo do construtor, e a única forma de adiar a construção de um
membro é um ponteiro.

**Execution provider** é o back-end de hardware para o qual o runtime despacha os nós do
grafo: `CPUExecutionProvider` (sempre presente), `CUDAExecutionProvider` (NVIDIA),
TensorRT, DirectML, CoreML… É isso que o parâmetro `use_cuda` do construtor liga, via
`opts.AppendExecutionProvider_CUDA(...)`. O restante do código não muda em nada: o mesmo
`Run()` roda em CPU ou GPU.

> ⚠ Neste repositório o `onnxruntime` vem do Conan na variante **só CPU**. Passar
> `use_cuda = true` imprime `[YOLOv10] usando CUDA` e depois falha ao criar a sessão. O
> `main.cpp` sempre passa `false`.

### NMS — e por que o YOLOv10 não precisa dele

Um detector clássico (YOLOv5, YOLOv8, todos os *anchor-based*) emite **milhares** de
caixas candidatas, e dezenas delas cobrem o mesmo carro com scores parecidos. A limpeza
se chama **NMS**, *non-maximum suppression* (supressão de não-máximos):

```
1. ordene todas as caixas por score, da maior para a menor
2. pegue a de maior score → é uma detecção final
3. descarte toda caixa que se sobrepõe demais a ela (IoU > 0.45)
4. repita com a maior das restantes
```

**IoU** (*Intersection over Union*) é a medida de sobreposição: área da interseção
dividida pela área da união de duas caixas. 1.0 = idênticas, 0.0 = disjuntas.

O YOLOv8 devolve `[1, 84, 8400]` — 8400 candidatas — e obriga o programador a escrever
NMS à mão em C++, com um limiar de IoU para acertar. **O YOLOv10 elimina essa etapa**: ele
é treinado com atribuição *one-to-one* (cada objeto recebe uma única caixa responsável
durante o treino), então o próprio modelo já entrega a lista final, no máximo 300 linhas,
ordenada. É por isso que o `postprocess()` deste projeto tem 30 linhas em vez de 150 — e
por que ele **não tem** nenhum parâmetro de IoU.

### Letterbox — e a matemática da volta

O modelo exige 640×640. A foto tem 672×448. Esticar a foto para o quadrado distorceria
as proporções, e o modelo foi treinado com proporções corretas. A solução é **letterbox**:
encolher preservando a proporção e preencher o que sobra com cinza.

```
scale = min(640/largura, 640/altura)
      = min(640/672, 640/448) = min(0,952, 1,428) = 0,952

nova_largura = 672 × 0,952 = 640      pad_x = (640 - 640) / 2 =   0
nova_altura  = 448 × 0,952 = 426      pad_y = (640 - 426) / 2 = 107

        0                                640
        ┌────────────────────────────────┐  0
        │        cinza 114,114,114       │        ← pad_y = 107
        ├────────────────────────────────┤  107
        │                                │
        │      imagem 640 × 426          │
        │                                │
        ├────────────────────────────────┤  533
        │        cinza 114,114,114       │        ← pad_y = 107
        └────────────────────────────────┘  640
```

O cinza `114` não é arbitrário: é o valor que o treinamento do YOLO usa para o
preenchimento, então o modelo já "conhece" essa cor como fundo neutro.

A parte que ninguém acerta de primeira é a **volta**. As caixas saem do modelo em
coordenadas do quadrado 640×640; para virarem pixels da foto original é preciso desfazer
as duas operações, **na ordem inversa**: primeiro tirar o padding, depois dividir pela
escala.

```
x_original = (x_640 - pad_x) / scale
y_original = (y_640 - pad_y) / scale
```

> ⚠ **Este é o bug número um de YOLO em C++.** Se você esquecer o `- pad_y`, todas as
> caixas descem 107 pixels e o resultado fica visivelmente torto — mas o programa não
> reclama de nada. É por isso que o `preprocess()` devolve um `LetterboxInfo { scale,
> pad_x, pad_y }` em vez de descartar esses números: essa struct é a **única** ponte entre
> o pré e o pós-processamento.

### Por que dividir por 255, e por que CHW

Duas transformações a mais, e a razão de cada uma:

**`/ 255.0`** — o modelo foi treinado com pixels em `[0, 1]`, não em `[0, 255]`. Note o
que **não** se faz aqui: subtrair média e dividir por desvio-padrão (a normalização
"ImageNet", com `mean=[0.485, 0.456, 0.406]`). O YOLO **não usa** essa normalização.
Aplicá-la por hábito degrada as detecções silenciosamente.

**HWC → CHW** — as convoluções do modelo esperam os canais como dimensão **externa**: todo
o plano vermelho, depois todo o verde, depois todo o azul. A imagem do OpenCV está
intercalada (`R G B R G B …`). A conversão é o `cv::split` (que separa os três planos)
seguido de um `std::memcpy` por canal para dentro de um buffer contíguo:

```
HWC (OpenCV):   R G B  R G B  R G B  R G B  …
CHW (modelo):   R R R R … (409 600)  G G G G … (409 600)  B B B B … (409 600)
```

### Percentil de latência — e por que a média engana

Se você medir 100 inferências e ordenar os tempos do menor para o maior, o **p95** é o
valor na posição 95: 95% das inferências foram **mais rápidas** que ele. O **p50** é a
mediana.

Por que não olhar só a média? Porque a média esconde a **cauda**. Nas medições deste
README, a média do `yolov10n` foi 199 ms e o máximo 279 ms — 40% pior. Numa webcam a 5
quadros por segundo, esse pior caso é exatamente o engasgo que o usuário vê. Sistemas de
tempo real se especificam por p99, nunca por média.

> ⚠ O código calcula `p99 = tempos[n_iter * 99 / 100]`. Com `n_iter = 100` isso é
> `tempos[99]` — o **último** elemento do vetor ordenado, ou seja, exatamente o `max`.
> Confira nas saídas medidas mais abaixo: `p99` e `max` são sempre o mesmo número. É o
> clássico erro de um a menos em estimador de percentil, e vale conhecê-lo.

---

## Por que estudar isto?

O treino de uma rede acontece em Python. A **inferência** — rodar a rede já treinada,
milhões de vezes, dentro de um produto — quase nunca.

| Sem inferência em C++ | Com inferência em C++ |
|---|---|
| o produto embute um interpretador Python inteiro | um binário e um arquivo `.onnx` |
| o GIL serializa as threads da aplicação | threads reais, paralelismo real |
| o modelo está preso ao framework que o treinou | ONNX é lido por qualquer runtime |
| medir latência é adivinhação | `high_resolution_clock` em volta da chamada |
| não roda em placa embarcada sem sistema de arquivos gordo | roda |

E por que ONNX em vez da `libtorch`? Porque o `.onnx` é um formato **neutro**: o mesmo
arquivo roda no ONNX Runtime, no TensorRT, no OpenVINO, no navegador. Você troca de
motor sem reexportar o modelo.

---

## Estrutura de arquivos

```
src/yolo/
├── README.md          ← este documento
├── meson.build        ← declara executable('yolo', ['main.cpp'], …)
├── main.cpp           ← os três modos: imagem, webcam, benchmark
├── yolov10.hpp        ← a classe YOLOv10 inteira: pré, Run, pós, desenho
├── export_yolo.py     ← exporta um .pt do ultralytics para .onnx (opcional)
├── images/
│   └── teste.jpg      ← 672×448, cena urbana (carros, pessoa, bicicleta)
└── models/
    ├── yolov10n.onnx  ← 9,1 MB — "nano", o padrão dos exemplos
    └── yolov10m.onnx  ← 59 MB — "medium", mais preciso e ~4,6× mais lento
```

---

## Os demos, um a um

### O exemplo mínimo

Antes de qualquer parâmetro, o programa inteiro cabe em cinco linhas úteis:

```cpp
#include "yolov10.hpp"

int main() {
    yolo::YOLOv10 detector("src/yolo/models/yolov10n.onnx");   // carrega
    cv::Mat img = cv::imread("src/yolo/images/teste.jpg");     // lê a foto
    for (const auto& d : detector.detect(img))                 // detecta
        fmt::print("{} {:.0f}%\n", d.label(), d.score * 100);
}
```

Os três outros parâmetros do construtor têm padrão:

```cpp
explicit YOLOv10(const std::string& model_path,
                 float score_threshold = 0.25f,   // descarta caixas fracas
                 int   input_size      = 640,     // tem que casar com o modelo
                 bool  use_cuda        = false);  // execution provider
```

E a interface pública completa é só isto — mais o `struct` que ela devolve:

```cpp
struct Detection {
    cv::Rect2f  bbox;      // já em pixels da imagem ORIGINAL
    float       score;     // [0, 1]
    int         class_id;  // 0..79
    std::string label() const;   // devolve std::string, não const char*
};

struct LetterboxInfo { float scale; int pad_x, pad_y; };  // a ponte pré → pós

std::vector<Detection> detect(const cv::Mat& image);
cv::Mat desenhar(const cv::Mat& image, const std::vector<Detection>& dets) const;
```

### Dentro do `detect()`: o pipeline

Agora que todos os termos estão definidos, o diagrama se lê inteiro:

```
cv::Mat BGR 672×448 uint8
   │
   │ preprocess()
   ├─ letterbox → 640×640, cinza 114, devolve {scale=0,952, pad_x=0, pad_y=107}
   ├─ cvtColor BGR→RGB
   ├─ convertTo CV_32F com fator 1/255
   └─ split + memcpy → buffer CHW contíguo de 3·640·640 floats
   │
   ▼
Ort::Value::CreateTensor<float>(…, shape {1,3,640,640})
   │
   │ session_->Run(RunOptions, in_names, &input, 1, out_names, 1)
   ▼
Ort::Value de saída, shape [1, 300, 6]
   │
   │ postprocess()
   ├─ para cada uma das shape[1] linhas: row = data + i*6
   ├─ descarta se row[4] < score_threshold
   ├─ desfaz o letterbox: (x - pad) / scale
   ├─ clamp às bordas [0, largura] × [0, altura]
   └─ std::sort por score decrescente
   │
   ▼
std::vector<Detection>
```

O tensor de saída merece a leitura dimensão por dimensão:

| Dimensão | Valor | Significado |
|---|---|---|
| 0 | 1 | batch — uma imagem por chamada |
| 1 | 300 | teto de detecções que o modelo emite |
| 2 | 6 | `[x1, y1, x2, y2, score, class_id]` |

Por isso o acesso é `const float* row = data + i * 6;` — o buffer é plano e cada
detecção ocupa 6 floats consecutivos. O código **não** cravou 300: ele lê
`shape[1]` da própria saída, então continua correto se você exportar com outro teto.

O `clamp` às bordas existe porque a caixa prevista pode extrapolar a foto (um carro
cortado na margem). Sem ele, `cv::rectangle` receberia coordenadas negativas.

### Demo 1 — imagem estática

Aciona-se passando um caminho de arquivo como segundo argumento:

```bash
./dist/bin/yolo src/yolo/models/yolov10n.onnx src/yolo/images/teste.jpg
```

O `main.cpp` mede o tempo, imprime cada detecção, grava `resultado_yolov10.jpg` **no
diretório de trabalho atual** e só então abre uma janela:

```cpp
cv::imwrite("resultado_yolov10.jpg", detector.desenhar(img, dets));  // grava ANTES
cv::imshow("YOLOv10 - pressione qualquer tecla", resultado);         // janela
cv::waitKey(0);                                                      // BLOQUEIA
cv::destroyAllWindows();
```

> ⚠ **Essas três últimas linhas são incondicionais.** Num terminal sem servidor gráfico
> (SSH sem `-X`, WSL sem WSLg, container), `cv::imshow` lança exceção ou trava para
> sempre. O arquivo de saída já foi gravado nesse ponto, então nada se perde — mas o
> processo não termina sozinho. Para ambiente sem display, use o modo `--bench`, que
> nunca abre janela.

### Demo 2 — webcam

Aciona-se passando **um único dígito** como segundo argumento (é assim que o `main.cpp`
distingue os modos: `input_path.size() == 1 && input_path[0] >= '0' && <= '9'`).

```bash
./dist/bin/yolo src/yolo/models/yolov10n.onnx 0    # câmera 0; 'q' encerra
```

O laço lê um quadro, detecta, desenha e sobrepõe um HUD com três números:

```cpp
cv::VideoCapture cap(camera_id, cv::CAP_V4L2);   // ver aviso abaixo
cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
…
cv::putText(resultado,
    fmt::format("FPS: {:.1f} | dets: {} | {}ms", fps, dets.size(), ms),
    cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
```

O `fps` exibido é **acumulado desde o início** (`frame_count / segundos_decorridos`), não
instantâneo — ele sobe devagar e alisa engasgos. O `ms` ao lado é a latência do quadro
atual, esse sim instantâneo.

> ⚠ `cv::CAP_V4L2` é o back-end *Video4Linux2*, **exclusivo de Linux**. O comentário no
> código (`// ffmpeg OFF conanfile`) explica por que ele foi fixado: o OpenCV construído
> pelo Conan neste repositório vem sem FFmpeg, e o back-end automático falharia. Em
> macOS ou Windows, troque por `cv::VideoCapture cap(camera_id);` sem back-end.

### Demo 3 — benchmark

```bash
./dist/bin/yolo src/yolo/models/yolov10n.onnx src/yolo/images/teste.jpg --bench
```

Cinco inferências de **warmup** antes de medir. O que elas aquecem, concretamente:

- as **arenas de memória** do ONNX Runtime, que crescem sob demanda na primeira execução;
- a inicialização preguiçosa de kernels e o layout de pesos que alguns operadores
  reorganizam no primeiro `Run`;
- as caches L2/L3 com os pesos do modelo;
- a **frequência da CPU**, que sobe do estado ocioso ao carregar.

Depois, 100 inferências cronometradas com `high_resolution_clock`, ordenadas com
`std::sort` e resumidas em média, mediana, p95, p99, mínimo e máximo.

### Números medidos

Máquina desta medição: 24 threads lógicas, GCC 13.3.0, **build `debug` (`-O0`)** — é a
configuração padrão do `Makefile` deste repositório. Um build `release` é
substancialmente mais rápido; o que importa aqui é a **comparação relativa**, medida com
o mesmo binário e a mesma imagem.

| Modelo | Threads (intra-op) | média | mediana | p95 | p99 = max | mín |
|---|---|---|---|---|---|---|
| `yolov10n` (9,1 MB) | 1 | 199,17 ms | 194,67 ms | 221,82 ms | 279,01 ms | 191,68 ms |
| `yolov10n` (9,1 MB) | todas (24) | 104,16 ms | 101,77 ms | 127,97 ms | 150,83 ms | 91,69 ms |
| `yolov10m` (59 MB) | 1 | 908,09 ms | 895,93 ms | 998,69 ms | 1049,42 ms | 879,99 ms |

Duas leituras:

**Por que uma thread?** O construtor fixa `opts.SetIntraOpNumThreads(1)`. *Intra-op* é o
paralelismo **dentro** de um operador — uma convolução dividida entre núcleos. Fixar em 1
torna a medida reprodutível e deixa os outros 23 núcleos para o resto da aplicação (é o
que se quer num servidor que atende várias câmeras: uma thread por câmera rende mais que
todas as threads numa câmera). Mudar essa **única linha** para
`opts.SetIntraOpNumThreads(0)` — 0 significa "escolha por mim" — quase dobra a
velocidade, como a tabela mostra. É a linha mais importante do arquivo para desempenho.

**`ORT_ENABLE_ALL`** é o outro botão: `opts.SetGraphOptimizationLevel(...ORT_ENABLE_ALL)`
libera todas as reescritas de grafo do runtime — fundir convolução com a normalização
seguinte, dobrar constantes, eliminar nós mortos. Custa um pouco no carregamento e paga
em toda inferência.

**Nano contra médio:** 4,6× mais lento em troca de precisão. Na imagem de teste, o `n`
encontra 9 objetos; o `m` encontra os mesmos objetos com scores mais altos e acerta mais
os semáforos distantes. A escolha é sempre essa troca.

---

## Preparando o modelo (opcional)

> ⚠ **Você provavelmente não precisa desta seção.** Os dois modelos já estão versionados
> em `src/yolo/models/`. Ela só interessa para gerar outra variante (`s`, `b`, `l`, `x`)
> ou outro `imgsz`.

```bash
pip install ultralytics
python src/yolo/export_yolo.py --outdir src/yolo/models
python src/yolo/export_yolo.py --model yolov10s --outdir src/yolo/models
```

> ⚠ O `--outdir` tem valor padrão `"models"`, **relativo ao diretório atual**. Rodando da
> raiz do repositório sem passar a flag, o arquivo cai em `./models/` e não em
> `src/yolo/models/` — e o comando de execução não vai encontrá-lo. Passe sempre o
> `--outdir` explicitamente.

As flags, uma a uma:

| Flag | Padrão | O que significa |
|---|---|---|
| `--model` | `yolov10n` | variante: `n` < `s` < `m` < `b` < `l` < `x`, do menor/mais rápido ao maior/mais preciso |
| `--imgsz` | `640` | lado do quadrado de entrada. **Tem que casar com `input_size` do C++** |
| `--opset` | `13` | versão do conjunto de operadores ONNX. Mais alto = operadores mais novos, menos runtimes compatíveis |
| `--outdir` | `models` | onde gravar o `.onnx` (e onde procurar/baixar o `.pt`) |

---

## Como validar

Rodando exatamente estes comandos a partir da raiz do repositório:

```bash
./dist/bin/yolo src/yolo/models/yolov10n.onnx src/yolo/images/teste.jpg
```

A saída medida — mesma máquina, mesmo arquivo versionado:

```
  carregando modelo: src/yolo/models/yolov10n.onnx
  [YOLOv10] modelo carregado: … → input shape [1,3,640,640]

── Inferência em imagem: src/yolo/images/teste.jpg ──
  imagem: 672x448
  inferência: 207ms
  detecções: 9
    car             86.3%  bbox=(128,127,275,239)
    car             85.9%  bbox=(247,124,334,195)
    bicycle         84.5%  bbox=(305,171,455,306)
    car             81.0%  bbox=(372,126,492,225)
    person          75.6%  bbox=(319,100,416,275)
    traffic light   39.1%  bbox=(285,63,301,97)
    car             36.3%  bbox=(117,122,235,191)
    car             30.5%  bbox=(111,116,144,147)
    traffic light   30.5%  bbox=(199,82,212,104)
  resultado salvo: resultado_yolov10.jpg
```

Três verificações que provam que o pipeline está inteiro:

1. **As caixas cabem na imagem.** Todos os `bbox` acima estão dentro de 672×448. Se
   aparecerem coordenadas maiores que isso, ou todas deslocadas verticalmente por
   ~107 px, a reversão do letterbox quebrou.
2. **A lista sai ordenada** por score decrescente — é o `std::sort` do `postprocess()`.
3. **O arquivo foi gravado:** `ls -l resultado_yolov10.jpg` mostra ~170 KB. Abra e
   confira visualmente que as caixas caem sobre os objetos.

E o benchmark, que não abre janela nenhuma:

```bash
./dist/bin/yolo src/yolo/models/yolov10n.onnx src/yolo/images/teste.jpg --bench
```

---

## Quando dá errado

| Sintoma | Causa provável |
|---|---|
| Todas as caixas deslocadas por um valor constante | reversão do letterbox sem subtrair `pad_x`/`pad_y` |
| Caixas em posições aleatórias, scores baixos | esqueceu o `BGR → RGB`, ou normalizou com média/desvio do ImageNet |
| Nenhuma detecção, mas o `Run` funciona | `score_threshold` alto demais, ou o objeto não é uma das 80 classes COCO |
| `Ort::Exception` dentro de `Run` sobre shape | `input_size` do C++ diferente do `imgsz` com que o modelo foi exportado |
| `Ort::Exception` ao **criar** a sessão | opset do arquivo mais novo que o ONNX Runtime instalado |
| A saída tem 8400 linhas e 84 colunas | você exportou um **YOLOv8**, não v10 — precisaria de NMS à mão |
| Trava depois de imprimir "resultado salvo" | `cv::waitKey(0)` sem display. Use `--bench` |
| `Não foi possível abrir a câmera 0` | sem `/dev/video0`, ou `CAP_V4L2` fora do Linux |
| `[YOLOv10] usando CUDA` seguido de falha | o `onnxruntime` do Conan aqui é só CPU |

---

## Referências

Subprojetos irmãos deste repositório:

- [`src/chrono/`](../chrono/) — como cronometrar direito: relógios, `duration`, os erros
  clássicos de medição. É a base do modo `--bench`.
- [`src/performance/`](../performance/) — medir antes de otimizar, perfis e caches.
- [`src/concurrency2/`](../concurrency2/) — modelo de memória e paralelismo, o assunto por
  trás de `SetIntraOpNumThreads`.
- [`src/embed-python/`](../embed-python/) — o caminho oposto: embutir o interpretador
  Python em C++ em vez de exportar o modelo.
- [`src/mandelbrot/`](../mandelbrot/) — paralelismo de dados medido, com a mesma disciplina
  de números reais.
- [`README.md`](../../README.md) — o índice de todos os subprojetos.

Fontes externas canônicas:

- YOLOv10: *Real-Time End-to-End Object Detection* — <https://arxiv.org/abs/2405.14458>
  (a seção sobre treino *one-to-one* é a que justifica a ausência de NMS)
- ONNX Runtime, API C/C++ — <https://onnxruntime.ai/docs/api/c/>
- ONNX Runtime, execution providers — <https://onnxruntime.ai/docs/execution-providers/>
- Formato ONNX, especificação — <https://github.com/onnx/onnx/blob/main/docs/IR.md>
- `cv::VideoCapture` — <https://docs.opencv.org/4.x/d8/dfe/classcv_1_1VideoCapture.html>
- Dataset COCO e as 80 categorias — <https://cocodataset.org/>
- Ultralytics, exportação para ONNX — <https://docs.ultralytics.com/modes/export/>
