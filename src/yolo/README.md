# padrão — gera models/yolov10n.onnx
python export_yolo.py

# variante maior
python export_yolo.py --model yolov10s

# tudo explícito
python export_yolo.py --model yolov10m --imgsz 640 --opset 13 --outdir src/yolo/models

# Imagem
./dist/bin/yolo ./src/yolo/models/yolov10m.onnx sua_imagem.jpg

# Webcam
./dist/bin/yolo ./src/yolo/models/yolov10m.onnx 0

# Benchmark (100 inferências, mede latência p50/p95/p99)
./dist/bin/yolo ./src/yolo/models/yolov10m.onnx sua_imagem.jpg --bench