#!/usr/bin/env python3
"""
Export YOLOv10 para ONNX.
Uso: python export_yolo.py [--model yolov10n] [--imgsz 640] [--opset 13]
"""

import argparse
from pathlib import Path
from ultralytics import YOLO


def exportar(model_name: str, imgsz: int, opset: int, output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)

    pt_path = output_dir / f"{model_name}.pt"
    print(f"[export] carregando {pt_path} ...")
    model = YOLO(str(pt_path))  # baixa automaticamente se não existir

    print(f"[export] exportando para ONNX (opset={opset}, imgsz={imgsz}) ...")
    result = model.export(
        format="onnx",
        opset=opset,
        simplify=True,
        imgsz=imgsz,
    )

    onnx_path = Path(result)
    dest = output_dir / onnx_path.name
    if onnx_path != dest:
        onnx_path.rename(dest)

    print(f"[export] salvo em: {dest}")
    return dest


def main() -> None:
    parser = argparse.ArgumentParser(description="Export YOLOv10 → ONNX")
    parser.add_argument("--model",  default="yolov10n",
                        help="variante do modelo: yolov10n/s/m/b/l/x (default: yolov10n)")
    parser.add_argument("--imgsz",  type=int, default=640,
                        help="tamanho de entrada (default: 640)")
    parser.add_argument("--opset",  type=int, default=13,
                        help="opset ONNX (default: 13)")
    parser.add_argument("--outdir", default="models",
                        help="diretório de saída (default: ./models)")
    args = parser.parse_args()

    onnx_path = exportar(
        model_name=args.model,
        imgsz=args.imgsz,
        opset=args.opset,
        output_dir=Path(args.outdir),
    )

    print(f"\n✓ Pronto. Use no C++:")
    print(f"  ./yolo {onnx_path} sua_imagem.jpg")


if __name__ == "__main__":
    main()
    
