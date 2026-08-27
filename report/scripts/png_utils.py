#!/usr/bin/env python3
"""
Leitor minimo de PNG em Python puro -- sem dependencias externas.

Existe porque esta maquina nao tem Pillow, matplotlib nem ImageMagick, e o
projeto precisa de tres coisas sobre os arquivos em figures/:

  1. ler largura/altura sem descomprimir nada  (geometria)
  2. amostrar a cor de um pixel                (conferir a paleta das figuras
                                                antigas ao re-autora-las)
  3. varrer as bordas                          (detectar conteudo cortado)

Suporta o unico formato que aparece no corpus: 8 bits por canal, nao
entrelacado, colortype 2 (RGB) ou 6 (RGBA).

Uso como ferramenta:
    python3 scripts/png_utils.py dim   FIG.png
    python3 scripts/png_utils.py pixel FIG.png X Y
    python3 scripts/png_utils.py borda FIG.png [margem]
    python3 scripts/png_utils.py cores FIG.png [n]
"""
import struct
import sys
import zlib
from collections import Counter


class PngError(Exception):
    pass


def _chunks(data):
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise PngError('assinatura PNG invalida')
    i = 8
    while i < len(data):
        (ln,) = struct.unpack('>I', data[i:i + 4])
        typ = data[i + 4:i + 8]
        yield typ, data[i + 8:i + 8 + ln]
        i += 12 + ln


def dimensions(path):
    """(largura, altura, colortype) lendo apenas o IHDR."""
    with open(path, 'rb') as fh:
        head = fh.read(26)
    if head[:8] != b'\x89PNG\r\n\x1a\n':
        raise PngError('assinatura PNG invalida: %s' % path)
    w, h = struct.unpack('>II', head[16:24])
    return w, h, head[25]


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def load_rgba(path):
    """Devolve (largura, altura, bytearray RGBA). Descomprime a imagem inteira."""
    data = open(path, 'rb').read()
    w = h = None
    bitdepth = colortype = None
    idat = bytearray()
    for typ, payload in _chunks(data):
        if typ == b'IHDR':
            w, h = struct.unpack('>II', payload[0:8])
            bitdepth, colortype = payload[8], payload[9]
            interlace = payload[12]
            if bitdepth != 8 or colortype not in (2, 6) or interlace != 0:
                raise PngError(
                    'so suporto 8bpc, colortype 2/6, nao entrelacado '
                    '(este: bd=%d ct=%d il=%d)' % (bitdepth, colortype, interlace))
        elif typ == b'IDAT':
            idat += payload
        elif typ == b'IEND':
            break

    bpp = 3 if colortype == 2 else 4
    raw = zlib.decompress(bytes(idat))
    stride = w * bpp
    out = bytearray(w * h * 4)
    prev = bytearray(stride)
    pos = 0
    for y in range(h):
        filt = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        if filt == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x - bpp]) & 255
        elif filt == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 255
        elif filt == 3:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 255
        elif filt == 4:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                c = prev[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + _paeth(a, prev[x], c)) & 255
        elif filt != 0:
            raise PngError('filtro PNG desconhecido: %d' % filt)

        base = y * w * 4
        if bpp == 4:
            out[base:base + stride] = line
        else:
            for x in range(w):
                o, s = base + x * 4, x * 3
                out[o:o + 3] = line[s:s + 3]
                out[o + 3] = 255
        prev = line
    return w, h, out


def pixel(rgba, w, x, y):
    o = (y * w + x) * 4
    return tuple(rgba[o:o + 4])


def _is_fundo(px):
    """Fundo = transparente, ou branco/quase-branco opaco."""
    r, g, b, a = px
    return a == 0 or (r > 248 and g > 248 and b > 248)


def borda_limpa(path, margem=8):
    """
    Devolve a lista de bordas ('esquerda','direita','topo','base') em que ha
    conteudo dentro de `margem` pixels -- indicio de corte.
    """
    w, h, rgba = load_rgba(path)
    sujas = []
    for x in range(min(margem, w)):
        if any(not _is_fundo(pixel(rgba, w, x, y)) for y in range(h)):
            sujas.append('esquerda')
            break
    for x in range(max(0, w - margem), w):
        if any(not _is_fundo(pixel(rgba, w, x, y)) for y in range(h)):
            sujas.append('direita')
            break
    for y in range(min(margem, h)):
        if any(not _is_fundo(pixel(rgba, w, x, y)) for x in range(w)):
            sujas.append('topo')
            break
    for y in range(max(0, h - margem), h):
        if any(not _is_fundo(pixel(rgba, w, x, y)) for x in range(w)):
            sujas.append('base')
            break
    return w, h, sujas


def cores_dominantes(path, n=12):
    w, h, rgba = load_rgba(path)
    cont = Counter()
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            cont[pixel(rgba, w, x, y)] += 1
    return w, h, cont.most_common(n)


def _hex(px):
    return '#%02x%02x%02x' % px[:3] + ('' if px[3] == 255 else ' a=%d' % px[3])


def main(argv):
    if len(argv) < 3:
        print(__doc__.strip())
        return 2
    cmd, path = argv[1], argv[2]
    if cmd == 'dim':
        w, h, ct = dimensions(path)
        print('%dx%d colortype=%d' % (w, h, ct))
    elif cmd == 'pixel':
        x, y = int(argv[3]), int(argv[4])
        w, h, rgba = load_rgba(path)
        print('%s  em (%d,%d)' % (_hex(pixel(rgba, w, x, y)), x, y))
    elif cmd == 'borda':
        margem = int(argv[3]) if len(argv) > 3 else 8
        w, h, sujas = borda_limpa(path, margem)
        if sujas:
            print('%dx%d  CONTEUDO A MENOS DE %dpx DA BORDA: %s'
                  % (w, h, margem, ', '.join(sujas)))
            return 1
        print('%dx%d  bordas limpas (margem %dpx)' % (w, h, margem))
    elif cmd == 'cores':
        n = int(argv[3]) if len(argv) > 3 else 12
        w, h, top = cores_dominantes(path, n)
        print('%dx%d -- %d cores mais frequentes:' % (w, h, n))
        for px, cnt in top:
            print('  %-18s %8d' % (_hex(px), cnt))
    else:
        print('comando desconhecido: %s' % cmd)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
