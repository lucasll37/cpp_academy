#!/usr/bin/env python3
"""
Verifica os blocos de codigo que o relatorio apresenta como TRANSCRICAO do fonte.

A convencao declarada na introducao do manual e:

    quando a legenda de um bloco traz um caminho de arquivo, o conteudo e
    transcricao do codigo-fonte -- eventualmente condensado, com as omissoes
    marcadas por "// ...", mas nunca reescrito.

Este script confere essa promessa. Para cada  \begin{lstlisting}[...]  cuja
legenda contenha  \texttt{src/...}  ou  \texttt{include/...}, ele:

  1. localiza o arquivo citado na arvore do repositorio (../src/...);
  2. reduz bloco e fonte a sequencias de TOKENS, descartando comentarios,
     espacos, quebras de linha e qualificadores de namespace;
  3. mede que fracao dos n-gramas de tokens do bloco ocorre no arquivo citado.

A medida por n-gramas -- e nao por linha -- e o que torna o teste utilizavel:
ela sobrevive a reindentacao, a rebordo de linha e a traducao de comentarios,
mas nao a parafrase nem a invencao.

Um bloco abaixo do limiar significa uma de duas coisas, e as duas precisam de
acao: ou a transcricao esta infiel (corrigir), ou o bloco nao e transcricao e a
legenda nao deveria sugerir que fosse.

Para o segundo caso o relatorio usa uma segunda convencao, tambem explicita: a
legenda diz "resumo de" antes do caminho. E o caso tipico de um bloco que
condensa a interface de uma classe, reordenando membros e traduzindo
comentarios -- util para ensinar, mas nao uma copia. O script reconhece essas
legendas e nao as cobra.

    caption={Interface de \\texttt{Cofre} (resumo de
             \\texttt{src/value_semantics/demo_rule_of_five.hpp}).}

Uma terceira convencao cobre as saidas de programa: quando o caminho da
legenda termina em "/", ele nomeia o SUBPROJETO que produziu aquela saida, e
nao um arquivo a transcrever.

    caption={Saida de \\texttt{src/errors/}: 20.000 iteracoes.}

Alternativamente, para um bloco cujo corpo e claramente ilustrativo, basta a
marca no corpo:

    // check-fontes: esquematico

Uso:  python3 scripts/check-fontes.py [--limiar N] [--listar] [--quiet]
Saida: relatorio; codigo 1 se houver bloco reprovado.
"""
import argparse
import glob
import os
import re
import sys

REPOSITORIO = '..'    # a raiz do cpp_academy, um nivel acima de report/
CHAPTERS = 'chapters/*.tex'
LIMIAR_PADRAO = 60          # % dos n-gramas do bloco que devem ocorrer no fonte
MARCA_ESQUEMATICO = 'check-fontes: esquematico'


JANELA = 4      # tamanho do n-grama de tokens


def tokeniza(texto):
    """Sequencia de tokens de um trecho de C++, ignorando comentarios.

    Tres diferencas entre o relatorio e o fonte sao legitimas e nao podem contar
    como infidelidade:

      - espacos, indentacao e QUEBRA DE LINHA: o relatorio condensa
        varias linhas numa so para deixar visivel a simetria de um switch;
      - qualificadores de namespace: o fonte escreve `demo_rtti::Circulo` e o
        relatorio escreve `Circulo`, porque dentro do capitulo o prefixo e ruido;
      - comentarios, que o fonte escreve em ingles e o relatorio traduz.

    Trabalhar sobre a sequencia de tokens -- e nao sobre linhas -- neutraliza
    as tres. O que sobra sao identificadores, literais e operadores: e ai que
    parafrase e invencao aparecem.
    """
    texto = re.sub(r'/\*.*?\*/', ' ', texto, flags=re.S)
    texto = re.sub(r'//[^\n]*', ' ', texto)
    texto = re.sub(r'\b(?:[A-Za-z_]\w*::)+', '', texto)
    return re.findall(r'[A-Za-z_]\w*|\d+\.?\d*|[^\s\w]', texto)


def ngramas(tokens, n=JANELA):
    return {tuple(tokens[i:i + n]) for i in range(len(tokens) - n + 1)}


def confere_bloco(corpo, caminho_fonte):
    """Fracao dos n-gramas do bloco que ocorrem no arquivo citado.

    Devolve (percentual, n_gramas_que_batem, n_gramas_do_bloco), ou None se o
    bloco for curto demais para a medida ter sentido.
    """
    tok_bloco = tokeniza(corpo)
    if len(tok_bloco) < JANELA * 2:
        return None

    tok_fonte = tokeniza(open(caminho_fonte, errors='ignore').read())
    fonte = ngramas(tok_fonte)
    bloco = ngramas(tok_bloco)
    if not bloco:
        return None

    batem = sum(1 for g in bloco if g in fonte)
    return 100 * batem // len(bloco), batem, len(bloco)


def blocos_com_fonte():
    """(arquivo_tex, linha, caminho_citado, corpo) de cada bloco rotulado."""
    for tex in sorted(glob.glob(CHAPTERS)):
        texto = open(tex).read()
        padrao = r'\\begin\{lstlisting\}\[(.*?)\](.*?)\\end\{lstlisting\}'
        for m in re.finditer(padrao, texto, re.S):
            opcoes, corpo = m.group(1), m.group(2)
            if MARCA_ESQUEMATICO in corpo:
                continue
            # Legenda que anuncia condensacao: "resumo de <caminho>".
            if re.search(r'resumo\s+de\b', opcoes, re.I):
                continue
            caminhos = re.findall(
                r'\\texttt\{((?:src|include)/[^}]*)\}',
                opcoes.replace('\\_', '_'))
            # Um caminho terminado em "/" nomeia o SUBPROJETO, nao um arquivo:
            # e a legenda das saidas de programa ("Saida de src/errors/"). Nao
            # ha o que transcrever ali, entao o bloco nao e cobrado.
            caminhos = [c for c in caminhos if not c.endswith('/')]
            if not caminhos:
                continue
            linha = texto[:m.start()].count('\n') + 1
            yield tex, linha, caminhos[0], corpo


def confere(limiar):
    aprovados, reprovados = [], []
    for tex, linha, caminho, corpo in blocos_com_fonte():
        alvo = os.path.join(REPOSITORIO, caminho)
        if not os.path.isfile(alvo):
            reprovados.append((tex, linha, caminho, -1, 0, 0))
            continue

        medida = confere_bloco(corpo, alvo)
        if medida is None:
            continue

        pct, batem, total = medida
        registro = (tex, linha, caminho, pct, batem, total)
        (aprovados if pct >= limiar else reprovados).append(registro)
    return aprovados, reprovados


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--limiar', type=int, default=LIMIAR_PADRAO)
    ap.add_argument('--listar', action='store_true',
                    help='mostra tambem os blocos aprovados')
    ap.add_argument('--quiet', action='store_true')
    args = ap.parse_args()

    aprovados, reprovados = confere(args.limiar)
    total = len(aprovados) + len(reprovados)

    if not args.quiet:
        print(f'blocos rotulados com caminho de fonte: {total}')
        print(f'limiar de literalidade: {args.limiar}%\n')

    if args.listar:
        for tex, ln, cam, pct, b, n in sorted(aprovados, key=lambda r: r[3]):
            print(f'  ok  {pct:>3}% ({b}/{n})  {tex}:{ln}  {cam}')
        print()

    if reprovados:
        print(f'{len(reprovados)} BLOCOS REPROVADOS:\n')
        for tex, ln, cam, pct, b, n in sorted(reprovados, key=lambda r: r[3]):
            if pct < 0:
                print(f'  {tex}:{ln}')
                print(f'      arquivo citado NAO EXISTE: {cam}')
            else:
                print(f'  {tex}:{ln}   {pct}% dos n-gramas no fonte ({b}/{n})')
                print(f'      {cam}')
        print('\nPara cada um: ou corrija a transcricao, ou remova o caminho')
        print(f'da legenda e marque o corpo com "{MARCA_ESQUEMATICO}".')
        return 1

    print(f'transcricoes: todos os {total} blocos acima de {args.limiar}%.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
