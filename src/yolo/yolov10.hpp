// =============================================================================
//  src/yolo/yolov10.hpp
//
//  YOLOv10 — Inferência com ONNX Runtime
//  ──────────────────────────────────────
//  YOLOv10 é NMS-free: o modelo já retorna as detecções filtradas.
//  Output shape: [1, 300, 6]  →  300 detecções máximas, cada uma:
//    [x1, y1, x2, y2, score, class_id]
//  Coordenadas em escala do input (640x640) — precisa redimensionar para a
//  imagem original.
//
//  Pipeline:
//    1. Carregar imagem  (cv::Mat BGR)
//    2. Letterbox resize → 640x640 preservando aspect ratio
//    3. BGR → RGB, HWC → CHW, uint8 → float32 /255.0
//    4. Inferência ONNX Runtime
//    5. Filtrar por score_threshold
//    6. Reescalar coordenadas para imagem original
// =============================================================================
#pragma once

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <fmt/core.h>

#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace yolo {

// ── Labels COCO (80 classes) ─────────────────────────────────────────────────
inline const std::vector<std::string> COCO_CLASSES = {
    "person","bicycle","car","motorcycle","airplane","bus","train","truck",
    "boat","traffic light","fire hydrant","stop sign","parking meter","bench",
    "bird","cat","dog","horse","sheep","cow","elephant","bear","zebra","giraffe",
    "backpack","umbrella","handbag","tie","suitcase","frisbee","skis","snowboard",
    "sports ball","kite","baseball bat","baseball glove","skateboard","surfboard",
    "tennis racket","bottle","wine glass","cup","fork","knife","spoon","bowl",
    "banana","apple","sandwich","orange","broccoli","carrot","hot dog","pizza",
    "donut","cake","chair","couch","potted plant","bed","dining table","toilet",
    "tv","laptop","mouse","remote","keyboard","cell phone","microwave","oven",
    "toaster","sink","refrigerator","book","clock","vase","scissors","teddy bear",
    "hair drier","toothbrush"
};

// ── Detecção individual ───────────────────────────────────────────────────────
struct Detection {
    cv::Rect2f bbox;       // bounding box em coordenadas da imagem original
    float      score;      // confiança [0, 1]
    int        class_id;   // índice da classe
    std::string label() const {
        if (class_id >= 0 && class_id < static_cast<int>(COCO_CLASSES.size()))
            return COCO_CLASSES[class_id];
        return "class_" + std::to_string(class_id);
    }
};

// ── Resultado de letterbox (para reverter coordenadas) ───────────────────────
struct LetterboxInfo {
    float scale;    // fator de escala aplicado
    int   pad_x;    // padding horizontal em pixels
    int   pad_y;    // padding vertical em pixels
};

// =============================================================================
//  YOLOv10 — inferenciador principal
// =============================================================================
class YOLOv10 {
public:
    explicit YOLOv10(const std::string& model_path,
                     float score_threshold = 0.25f,
                     int   input_size      = 640,
                     bool  use_cuda        = false)
        : score_thresh_(score_threshold)
        , input_size_(input_size)
        , env_(ORT_LOGGING_LEVEL_WARNING, "yolov10")
    {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        if (use_cuda) {
            // Habilita GPU NVIDIA se disponível
            OrtCUDAProviderOptions cuda_opts{};
            opts.AppendExecutionProvider_CUDA(cuda_opts);
            fmt::print("  [YOLOv10] usando CUDA\n");
        }

        session_ = std::make_unique<Ort::Session>(
            env_, model_path.c_str(), opts);

        // Lê nomes dos tensores de entrada/saída
        Ort::AllocatorWithDefaultOptions alloc;
        auto in_name  = session_->GetInputNameAllocated(0, alloc);
        auto out_name = session_->GetOutputNameAllocated(0, alloc);
        input_name_  = in_name.get();
        output_name_ = out_name.get();

        // Verifica shape de entrada: [1, 3, H, W]
        auto in_shape = session_->GetInputTypeInfo(0)
                            .GetTensorTypeAndShapeInfo().GetShape();
        fmt::print("  [YOLOv10] modelo carregado: {} → input shape [",
                   model_path);
        for (std::size_t i = 0; i < in_shape.size(); ++i)
            fmt::print("{}{}", in_shape[i], i+1 < in_shape.size() ? "," : "");
        fmt::print("]\n");
    }

    // ── Detecta objetos em uma imagem BGR (cv::Mat) ───────────────────────────
    std::vector<Detection> detect(const cv::Mat& image) {
        if (image.empty())
            throw std::runtime_error("YOLOv10::detect: imagem vazia");

        // 1. Pré-processamento
        cv::Mat blob;
        LetterboxInfo info = preprocess(image, blob);

        // 2. Monta tensor de entrada [1, 3, H, W]
        std::array<int64_t, 4> input_shape{
            1, 3, input_size_, input_size_};
        std::size_t input_bytes = 1 * 3 * input_size_ * input_size_;

        auto memory_info = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);

        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            reinterpret_cast<float*>(blob.data),
            input_bytes,
            input_shape.data(), input_shape.size());

        // 3. Inferência
        const char* in_names[]  = { input_name_.c_str() };
        const char* out_names[] = { output_name_.c_str() };

        auto outputs = session_->Run(
            Ort::RunOptions{nullptr},
            in_names,  &input_tensor, 1,
            out_names, 1);

        // 4. Pós-processamento
        return postprocess(outputs[0], info, image.cols, image.rows);
    }

    // ── Desenha detecções na imagem ───────────────────────────────────────────
    cv::Mat desenhar(const cv::Mat& image,
                     const std::vector<Detection>& dets) const {
        cv::Mat out = image.clone();
        for (const auto& d : dets) {
            // Caixa colorida por classe
            cv::Scalar cor(
                (d.class_id * 37) % 256,
                (d.class_id * 97) % 256,
                (d.class_id * 157) % 256);

            cv::rectangle(out, d.bbox, cor, 2);

            std::string texto = fmt::format("{} {:.0f}%",
                                            d.label(),
                                            d.score * 100.0f);

            int baseline = 0;
            cv::Size ts = cv::getTextSize(texto,
                cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

            cv::rectangle(out,
                cv::Point(static_cast<int>(d.bbox.x),
                          static_cast<int>(d.bbox.y) - ts.height - 4),
                cv::Point(static_cast<int>(d.bbox.x) + ts.width,
                          static_cast<int>(d.bbox.y)),
                cor, -1); // fundo sólido

            cv::putText(out, texto,
                cv::Point(static_cast<int>(d.bbox.x),
                          static_cast<int>(d.bbox.y) - 2),
                cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(255, 255, 255), 1);
        }
        return out;
    }

private:
    // ── Letterbox resize — preserva aspect ratio com padding cinza ────────────
    LetterboxInfo preprocess(const cv::Mat& src, cv::Mat& out) {
        float scale = std::min(
            static_cast<float>(input_size_) / src.cols,
            static_cast<float>(input_size_) / src.rows);

        int new_w = static_cast<int>(src.cols * scale);
        int new_h = static_cast<int>(src.rows * scale);
        int pad_x = (input_size_ - new_w) / 2;
        int pad_y = (input_size_ - new_h) / 2;

        cv::Mat resized;
        cv::resize(src, resized, cv::Size(new_w, new_h));

        cv::Mat padded(input_size_, input_size_,
                       CV_8UC3, cv::Scalar(114, 114, 114));
        resized.copyTo(padded(cv::Rect(pad_x, pad_y, new_w, new_h)));

        // BGR → RGB
        cv::Mat rgb;
        cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);

        // HWC → CHW e normaliza para float [0, 1]
        // Resultado: [3, H, W] float32 contíguo
        out.create(1, 3 * input_size_ * input_size_, CV_32F);
        std::vector<cv::Mat> channels(3);
        cv::split(rgb, channels);
        float* ptr = out.ptr<float>();
        for (int c = 0; c < 3; ++c) {
            cv::Mat ch_float;
            channels[c].convertTo(ch_float, CV_32F, 1.0 / 255.0);
            std::memcpy(ptr + c * input_size_ * input_size_,
                        ch_float.data,
                        input_size_ * input_size_ * sizeof(float));
        }

        return { scale, pad_x, pad_y };
    }

    // ── Pós-processamento: output [1, 300, 6] → detecções filtradas ───────────
    std::vector<Detection> postprocess(const Ort::Value& output,
                                       const LetterboxInfo& info,
                                       int orig_w, int orig_h) {
        // Shape: [1, 300, 6]
        auto shape = output.GetTensorTypeAndShapeInfo().GetShape();
        int  num_dets = static_cast<int>(shape[1]);  // 300
        const float* data = output.GetTensorData<float>();

        std::vector<Detection> result;
        result.reserve(32);

        for (int i = 0; i < num_dets; ++i) {
            const float* row = data + i * 6;
            float score    = row[4];
            if (score < score_thresh_) continue;

            float x1 = row[0], y1 = row[1];
            float x2 = row[2], y2 = row[3];
            int   cls = static_cast<int>(row[5]);

            // Reverte letterbox: remove padding e reescala
            x1 = (x1 - info.pad_x) / info.scale;
            y1 = (y1 - info.pad_y) / info.scale;
            x2 = (x2 - info.pad_x) / info.scale;
            y2 = (y2 - info.pad_y) / info.scale;

            // Clip às bordas da imagem
            x1 = std::clamp(x1, 0.0f, static_cast<float>(orig_w));
            y1 = std::clamp(y1, 0.0f, static_cast<float>(orig_h));
            x2 = std::clamp(x2, 0.0f, static_cast<float>(orig_w));
            y2 = std::clamp(y2, 0.0f, static_cast<float>(orig_h));

            result.push_back({
                cv::Rect2f(x1, y1, x2 - x1, y2 - y1),
                score,
                cls
            });
        }

        // Ordena por score decrescente
        std::sort(result.begin(), result.end(),
            [](const Detection& a, const Detection& b) {
                return a.score > b.score;
            });

        return result;
    }

    float       score_thresh_;
    int         input_size_;
    std::string input_name_;
    std::string output_name_;

    Ort::Env                          env_;
    std::unique_ptr<Ort::Session>     session_;
};

} // namespace yolo
