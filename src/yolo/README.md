# yolo — Detecção de Objetos com YOLOv10 + ONNX Runtime

## O que é este projeto?

Implementa **detecção de objetos em tempo real** usando o modelo **YOLOv10** através do **ONNX Runtime** em C++. Suporta três modos: inferência em imagem estática, webcam em tempo real e benchmark de latência.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| ONNX Runtime em C++ | Carregar e executar modelos de deep learning |
| Pré-processamento de imagem | Redimensionar, normalizar, converter BGR→RGB |
| Pós-processamento YOLO | Parsear saídas do modelo (bounding boxes + scores) |
| Benchmarking com percentis | Medir latência p50, p95, p99 de inferência |
| `cv::VideoCapture` | Captura de frames de webcam com OpenCV |

---

## Estrutura de arquivos

```
yolo/
├── main.cpp      ← 3 modos: imagem, webcam, benchmark
├── yolov10.hpp   ← classe YOLOv10: carregamento do modelo, inferência, desenho
└── meson.build
```

---

## Como usar

```bash
# Modo 1: inferência em imagem estática
./yolo models/yolov10n.onnx imagem.jpg

# Modo 2: webcam em tempo real (câmera 0)
./yolo models/yolov10n.onnx 0

# Modo 3: benchmark de latência (100 inferências)
./yolo models/yolov10n.onnx imagem.jpg --bench
```

---

## Preparando o modelo ONNX

```bash
# Script incluído no projeto
python export_yolo.py                  # padrão: yolov10n.onnx
python export_yolo.py --model yolov10s # variante maior
python export_yolo.py --model yolov10m --imgsz 640 --opset 13 --outdir src/yolo/models

# Ou manualmente com ultralytics:
pip install ultralytics
yolo export model=yolov10n.pt format=onnx opset=13 simplify=True
```

---

## A classe `YOLOv10`

```cpp
class YOLOv10 {
    Ort::Env            env_;
    Ort::Session        session_;

    float score_threshold_;   // limiar de confiança (padrão: 0.25)
    int   input_size_;        // tamanho de entrada (padrão: 640x640)

public:
    YOLOv10(const std::string& model_path,
            float score_threshold = 0.25f,
            int input_size = 640,
            bool use_cuda = false);

    std::vector<Detection> detect(const cv::Mat& img);
    cv::Mat desenhar(const cv::Mat& img, const std::vector<Detection>& dets);
};

struct Detection {
    cv::Rect2f bbox;     // bounding box (x, y, w, h) em pixels
    float      score;    // confiança (0.0 a 1.0)
    int        class_id; // índice da classe (0-79 para COCO)
    const char* label() const; // nome da classe
};
```

---

## Pipeline de inferência

```
Imagem original (qualquer tamanho)
        │
        ▼
Pré-processamento:
  1. cv::resize → 640x640 com letterboxing (mantém proporção)
  2. BGR → RGB
  3. uint8 → float32 / 255.0 (normaliza para [0, 1])
  4. HWC → CHW (Height×Width×Channels → Channels×Height×Width)
        │
        ▼
Inferência ONNX:
  Ort::Session::Run(inputs, input_names, output_names)
        │
        ▼
Pós-processamento:
  1. Parseia tensor de saída [N, 6]: [x1, y1, x2, y2, score, class_id]
  2. Filtra por score_threshold
  3. Mapeia coordenadas → pixels da imagem original
        │
        ▼
std::vector<Detection>
```

---

## Demo 1: Inferência em imagem

```cpp
cv::Mat img = cv::imread(caminho);

auto t0   = high_resolution_clock::now();
auto dets = detector.detect(img);
auto ms   = duration_cast<milliseconds>(now() - t0).count();

fmt::print("inferência: {}ms, detecções: {}\n", ms, dets.size());

for (const auto& d : dets) {
    fmt::print("  {:15s} {:.1f}%  bbox=({:.0f},{:.0f},{:.0f},{:.0f})\n",
               d.label(), d.score * 100.0f,
               d.bbox.x, d.bbox.y,
               d.bbox.x + d.bbox.width,
               d.bbox.y + d.bbox.height);
}

cv::imwrite("resultado_yolov10.jpg", detector.desenhar(img, dets));
```

---

## Demo 2: Webcam em tempo real

```cpp
cv::VideoCapture cap(camera_id, cv::CAP_V4L2);
cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

while (true) {
    cv::Mat frame;
    cap >> frame;

    auto dets = detector.detect(frame);
    cv::Mat resultado = detector.desenhar(frame, dets);

    // HUD: FPS e número de detecções
    cv::putText(resultado,
        fmt::format("FPS: {:.1f} | {} objs", fps, dets.size()),
        cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.7,
        cv::Scalar(0, 255, 0), 2);

    cv::imshow("YOLOv10 Webcam", resultado);
    if (cv::waitKey(1) == 'q') break;
}
```

---

## Demo 3: Benchmark de latência

```cpp
// Warmup: 5 inferências para aquecer caches
for (int i = 0; i < 5; ++i) detector.detect(img);

// Medição de 100 inferências
std::vector<long long> tempos(100);
for (int i = 0; i < 100; ++i) {
    auto t0 = high_resolution_clock::now();
    detector.detect(img);
    tempos[i] = duration_cast<microseconds>(now() - t0).count();
}

std::sort(tempos.begin(), tempos.end());
fmt::print("média:   {:.2f}ms\n", mean(tempos) / 1000.0);
fmt::print("p95:     {:.2f}ms\n", tempos[95] / 1000.0);
fmt::print("p99:     {:.2f}ms\n", tempos[99] / 1000.0);
fmt::print("FPS:     {:.1f}\n",   1000.0 / (mean(tempos) / 1000.0));
```

---

## Saída esperada (benchmark com yolov10n, CPU)

```
média:   28.50ms
mediana: 27.80ms
p95:     35.20ms
p99:     42.10ms
min:     25.30ms
max:     55.10ms
FPS teórico (média): 35.1
```

---

## Classes COCO suportadas

O modelo detecta 80 classes: `person`, `bicycle`, `car`, `motorcycle`, `airplane`, `bus`, `train`, `truck`, `boat`, `traffic light`, `cat`, `dog`, `horse`, `cow`, `bird`, `backpack`, `umbrella`, etc.

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja yolo

# Exportar modelo primeiro
python src/yolo/export_yolo.py

# Rodar em imagem
./bin/yolo src/yolo/models/yolov10m.onnx src/yolo/images/teste.jpg

# Benchmark
./bin/yolo src/yolo/models/yolov10m.onnx src/yolo/images/teste.jpg --bench
```

---

## Dependências

- `onnxruntime` — runtime de inferência de modelos ONNX
- `OpenCV` — visão computacional, captura de câmera
- `fmt` — formatação de saída