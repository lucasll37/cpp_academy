// =============================================================================
//  src/yolo/main.cpp
//
//  Demo YOLOv10 com ONNX Runtime
//  ──────────────────────────────
//  Uso:
//    ./yolo models/yolov10n.onnx imagem.jpg          # inferência em imagem
//    ./yolo models/yolov10n.onnx 0                   # webcam (índice 0)
//    ./yolo models/yolov10n.onnx imagem.jpg --bench  # benchmark 100 frames
//
//  Antes de compilar, exporte o modelo em Python:
//    pip install ultralytics
//    yolo export model=yolov10n.pt format=onnx opset=13 simplify=True
//    cp yolov10n.onnx src/yolo/models/
// =============================================================================

#include "yolov10.hpp"

#include <fmt/core.h>
#include <fmt/color.h>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <string>
#include <stdexcept>

using namespace std::chrono;

// ─────────────────────────────────────────────────────────────────────────────
//  Inferência em imagem estática
// ─────────────────────────────────────────────────────────────────────────────
void demo_imagem(yolo::YOLOv10& detector, const std::string& caminho) {
    fmt::print("\n── Inferência em imagem: {} ──\n", caminho);

    cv::Mat img = cv::imread(caminho);
    if (img.empty())
        throw std::runtime_error("Não foi possível abrir: " + caminho);

    fmt::print("  imagem: {}x{}\n", img.cols, img.rows);

    auto t0 = high_resolution_clock::now();
    auto dets = detector.detect(img);
    auto ms = duration_cast<milliseconds>(high_resolution_clock::now() - t0).count();

    fmt::print("  inferência: {}ms\n", ms);
    fmt::print("  detecções: {}\n", dets.size());

    for (const auto& d : dets) {
        fmt::print("    {:15s} {:.1f}%  bbox=({:.0f},{:.0f},{:.0f},{:.0f})\n",
                   d.label(), d.score * 100.0f,
                   d.bbox.x, d.bbox.y,
                   d.bbox.x + d.bbox.width,
                   d.bbox.y + d.bbox.height);
    }

    // Salva resultado com boxes desenhados
    cv::Mat resultado = detector.desenhar(img, dets);
    std::string saida = "resultado_yolov10.jpg";
    cv::imwrite(saida, resultado);
    fmt::print("  resultado salvo: {}\n", saida);

    // Mostra na tela se houver display
    cv::imshow("YOLOv10 - pressione qualquer tecla", resultado);
    cv::waitKey(0);
    cv::destroyAllWindows();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Inferência em webcam em tempo real
// ─────────────────────────────────────────────────────────────────────────────
void demo_webcam(yolo::YOLOv10& detector, int camera_id) {
    fmt::print("\n── Webcam em tempo real (câmera {}) ──\n", camera_id);
    fmt::print("  Pressione 'q' para sair\n");

    // cv::VideoCapture cap(camera_id);
    cv::VideoCapture cap(camera_id, cv::CAP_V4L2); // ffmpeg OFF conanfile
    if (!cap.isOpened())
        throw std::runtime_error("Não foi possível abrir a câmera " +
                                 std::to_string(camera_id));

    cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    cv::Mat frame;
    int frame_count = 0;
    auto t_start = high_resolution_clock::now();

    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        auto t0   = high_resolution_clock::now();
        auto dets = detector.detect(frame);
        auto ms   = duration_cast<milliseconds>(
                        high_resolution_clock::now() - t0).count();

        // FPS médio
        ++frame_count;
        auto elapsed = duration_cast<seconds>(
                           high_resolution_clock::now() - t_start).count();
        float fps = elapsed > 0 ?
                    static_cast<float>(frame_count) / elapsed : 0.0f;

        cv::Mat resultado = detector.desenhar(frame, dets);

        // HUD
        cv::putText(resultado,
            fmt::format("FPS: {:.1f} | dets: {} | {}ms",
                        fps, dets.size(), ms),
            cv::Point(10, 30),
            cv::FONT_HERSHEY_SIMPLEX, 0.7,
            cv::Scalar(0, 255, 0), 2);

        cv::imshow("YOLOv10 Webcam", resultado);

        if (cv::waitKey(1) == 'q') break;
    }

    cv::destroyAllWindows();
    fmt::print("  {} frames processados, FPS médio: {:.1f}\n",
               frame_count,
               static_cast<float>(frame_count) /
               duration_cast<seconds>(
                   high_resolution_clock::now() - t_start).count());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark — mede latência média em N inferências
// ─────────────────────────────────────────────────────────────────────────────
void demo_benchmark(yolo::YOLOv10& detector, const std::string& img_path,
                    int n_iter = 100) {
    fmt::print("\n── Benchmark ({} inferências) ──\n", n_iter);

    cv::Mat img = cv::imread(img_path);
    if (img.empty())
        throw std::runtime_error("Benchmark: não foi possível abrir: " + img_path);

    // Warmup
    for (int i = 0; i < 5; ++i) detector.detect(img);

    // Medição
    std::vector<long long> tempos;
    tempos.reserve(n_iter);

    for (int i = 0; i < n_iter; ++i) {
        auto t0 = high_resolution_clock::now();
        detector.detect(img);
        tempos.push_back(
            duration_cast<microseconds>(
                high_resolution_clock::now() - t0).count());
    }

    std::sort(tempos.begin(), tempos.end());
    double media = 0;
    for (auto t : tempos) media += t;
    media /= n_iter;

    fmt::print("  média:   {:.2f}ms\n", media / 1000.0);
    fmt::print("  mediana: {:.2f}ms\n", tempos[n_iter / 2] / 1000.0);
    fmt::print("  p95:     {:.2f}ms\n", tempos[n_iter * 95 / 100] / 1000.0);
    fmt::print("  p99:     {:.2f}ms\n", tempos[n_iter * 99 / 100] / 1000.0);
    fmt::print("  min:     {:.2f}ms\n", tempos.front() / 1000.0);
    fmt::print("  max:     {:.2f}ms\n", tempos.back() / 1000.0);
    fmt::print("  FPS teórico (média): {:.1f}\n", 1000.0 / (media / 1000.0));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    fmt::print(fmt::fg(fmt::color::yellow) | fmt::emphasis::bold,
               "\n  YOLOv10 + ONNX Runtime + OpenCV\n\n");

    if (argc < 3) {
        fmt::print("Uso:\n");
        fmt::print("  {} <modelo.onnx> <imagem.jpg>          # imagem\n", argv[0]);
        fmt::print("  {} <modelo.onnx> <N>                   # webcam N\n", argv[0]);
        fmt::print("  {} <modelo.onnx> <imagem.jpg> --bench  # benchmark\n", argv[0]);
        return 1;
    }

    std::string model_path  = argv[1];
    std::string input_path  = argv[2];
    bool        bench_mode  = argc > 3 &&
                              std::string(argv[3]) == "--bench";

    try {
        fmt::print("  carregando modelo: {}\n", model_path);
        yolo::YOLOv10 detector(model_path,
                               /*score_threshold=*/ 0.25f,
                               /*input_size=*/      640,
                               /*use_cuda=*/        false);

        // Decide modo: número = webcam, string com extensão = imagem
        bool is_camera = (input_path.size() == 1 &&
                          input_path[0] >= '0' &&
                          input_path[0] <= '9');

        if (bench_mode) {
            demo_benchmark(detector, input_path);
        } else if (is_camera) {
            demo_webcam(detector, std::stoi(input_path));
        } else {
            demo_imagem(detector, input_path);
        }

    } catch (const Ort::Exception& e) {
        fmt::print(fmt::fg(fmt::color::red),
                   "ONNX Runtime error: {}\n", e.what());
        return 1;
    } catch (const std::exception& e) {
        fmt::print(fmt::fg(fmt::color::red),
                   "Erro: {}\n", e.what());
        return 1;
    }

    return 0;
}
