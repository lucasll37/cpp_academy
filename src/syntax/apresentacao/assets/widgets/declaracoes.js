/* =============================================================================
   widgets/declaracoes.js — Capítulo 1 · Declarações (demo_variaveis.hpp)

   Widgets (todos com passo(k) idempotente: o estado k é desenhado do zero):
     declaracoes/espiral    leitor de declarações (regra da espiral)
     declaracoes/tamanhos   sizeof medido: LP64 × LLP64, o que o padrão garante
     declaracoes/init       laboratório: forma de inicializar × contexto
     declaracoes/deducao    o que o auto deduz, o que ele joga fora
     declaracoes/constptr   const à esquerda/direita do * — testes de compilação
     declaracoes/escopo     blocos aninhados, sombreamento, destruição
     declaracoes/static     static local × quadros da pilha
     declaracoes/memoria    ponteiro × referência (mesmo objeto; referência não religa)
     declaracoes/decay      array → ponteiro: o tamanho que se perde
     declaracoes/literais   conversor de literais: bases, sufixos e o tipo resultante

   Números e mensagens: saída real do ./dist/bin/syntax ou do GCC 13.3 (verificados).
   ============================================================================= */
(function () {
  'use strict';
  const { el, svg } = Deck.util;
  const realcar = (t) => Deck.util.realcar(t, 'cpp');
  const cod = (t, classe) => el('code', { class: 'cod' + (classe ? ' ' + classe : ''), html: realcar(t) });
  const limpar = (no) => { while (no.firstChild) no.removeChild(no.firstChild); return no; };

  // ───────────────────────────────────────────────────────────────────────────
  //  1. Regra da espiral
  // ───────────────────────────────────────────────────────────────────────────
  const DECLS = {
    p: {
      rotulo: 'int* p[5]', tokens: ['int', '*', 'p', '[5]', ';'], nome: 2,
      passos: [
        { i: 2, frase: 'p é', dica: 'comece no nome' },
        { i: 3, frase: 'array de 5', dica: 'à direita: [5] — siga para a direita enquanto puder' },
        { i: 1, frase: 'ponteiros para', dica: 'à direita só resta ; — agora para a esquerda: *' },
        { i: 0, frase: 'int', dica: 'esquerda de novo: o tipo' },
      ],
      using: 'using PtrInt = int*;   PtrInt p[5];',
    },
    q: {
      rotulo: 'int (*q)[5]', tokens: ['int', '(', '*', 'q', ')', '[5]', ';'], nome: 3,
      passos: [
        { i: 3, frase: 'q é', dica: 'comece no nome' },
        { i: 2, frase: 'ponteiro para', dica: 'à direita há ) — o parêntese manda: volte para a esquerda', bloq: 4 },
        { i: 5, frase: 'array de 5', dica: 'saia dos parênteses e siga à direita: [5]' },
        { i: 0, frase: 'int', dica: 'nada mais à direita: esquerda, o tipo' },
      ],
      using: 'using Linha = int[5];   Linha* q;',
    },
    f: {
      rotulo: 'int (*f)(int)', tokens: ['int', '(', '*', 'f', ')', '(int)', ';'], nome: 3,
      passos: [
        { i: 3, frase: 'f é', dica: 'comece no nome' },
        { i: 2, frase: 'ponteiro para', dica: ') bloqueia a direita: volte — *', bloq: 4 },
        { i: 5, frase: 'função(int) que devolve', dica: 'fora dos parênteses, à direita: (int) é uma função' },
        { i: 0, frase: 'int', dica: 'esquerda: o tipo de retorno' },
      ],
      using: 'using Callback = int(*)(int);   Callback f;',
    },
    m: {
      rotulo: 'int (*f[3])(int)', tokens: ['int', '(', '*', 'f', '[3]', ')', '(int)', ';'], nome: 3,
      passos: [
        { i: 3, frase: 'f é', dica: 'comece no nome' },
        { i: 4, frase: 'array de 3', dica: 'direita: [3]' },
        { i: 2, frase: 'ponteiros para', dica: ') bloqueia: volte para a esquerda — *', bloq: 5 },
        { i: 6, frase: 'função(int) que devolve', dica: 'fora dos parênteses, à direita: (int)' },
        { i: 0, frase: 'int', dica: 'esquerda: o tipo de retorno' },
      ],
      using: 'using Callback = int(*)(int);   Callback f[3];',
    },
  };

  Deck.widget('declaracoes/espiral', {
    iniciar(raiz) {
      raiz.classList.add('w-espiral');
      raiz._sel = el('div', { class: 'seletor' });
      Object.entries(DECLS).forEach(([k, d]) => {
        const b = el('button', { class: 'botao cod-btn', type: 'button', dataset: { d: k } }, cod(d.rotulo + ';'));
        b.addEventListener('click', () => tocarEspiral(raiz, k));
        raiz._sel.append(b);
      });
      raiz._palco = el('div', { class: 'palco-esp' });
      raiz._linha = el('div', { class: 'tokens' });
      raiz._svg = svg('svg', { class: 'arcos', 'aria-hidden': 'true' });
      raiz._palco.append(raiz._svg, raiz._linha);
      raiz._frase = el('div', { class: 'frase' });
      raiz._dica = el('div', { class: 'dica' });
      raiz._regras = el('div', { class: 'regras' },
        el('span', null, el('b', null, '①'), ' comece no nome'),
        el('span', null, el('b', null, '②'), ' direita enquanto puder: ', el('code', null, '[ ]'), ' ', el('code', null, '( )')),
        el('span', null, el('b', null, '③'), ' depois esquerda: ', el('code', null, '*'), ' e o tipo'),
        el('span', { class: 'manda' }, 'parênteses mandam'));
      raiz.append(raiz._sel, raiz._palco, raiz._frase, raiz._dica, raiz._regras);
      raiz._decl = raiz.dataset.decl || 'q';
    },
    passo(raiz, k) {
      raiz._play = (raiz._play || 0) + 1;
      raiz._decl = raiz.dataset.decl || 'q';
      desenharEspiral(raiz, raiz._decl, k);
    },
    sair(raiz) { raiz._play = (raiz._play || 0) + 1; },
    entrar(raiz) {
      // as larguras dos tokens dependem da fonte: redesenha quando ela estiver pronta
      const refazer = () => { if (raiz._estado) desenharEspiral(raiz, raiz._estado.d, raiz._estado.s, true); };
      if (document.fonts && document.fonts.ready) document.fonts.ready.then(refazer);
      requestAnimationFrame(refazer);
    },
  });

  async function tocarEspiral(raiz, d) {
    const meu = (raiz._play = (raiz._play || 0) + 1);
    const total = DECLS[d].passos.length + 1;
    for (let s = 0; s <= total; s++) {
      if (raiz._play !== meu) return;
      desenharEspiral(raiz, d, s);
      await Deck.util.esperar(s === 0 ? 350 : 1050);
    }
  }

  function desenharEspiral(raiz, d, s, semAnim) {
    const D = DECLS[d];
    raiz._estado = { d, s };
    const N = D.passos.length;
    const final = s > N;
    const feitos = D.passos.slice(0, Math.min(s, N));
    raiz.querySelectorAll('.seletor .botao').forEach((b) => b.classList.toggle('selecionado', b.dataset.d === d));

    // tokens
    limpar(raiz._linha);
    const toks = D.tokens.map((t, i) => {
      const no = el('span', { class: 'tok' + (i === D.nome ? ' nome' : '') + (t === ';' ? ' pv' : ''), html: realcar(t) });
      const ordem = feitos.findIndex((p) => p.i === i);
      if (ordem >= 0) {
        no.classList.add('visto');
        no.append(el('i', { class: 'badge' }, String(ordem + 1)));
        if (ordem === feitos.length - 1 && !final) no.classList.add('atual');
      }
      const atual = feitos[feitos.length - 1];
      if (!final && atual && atual.bloq === i) no.classList.add('bloqueio');
      raiz._linha.append(no);
      return no;
    });

    // arcos entre tokens visitados (direita por cima, esquerda por baixo)
    limpar(raiz._svg);
    const W = raiz._linha.offsetWidth, H = raiz._linha.offsetHeight;
    const PAD = 64;
    raiz._svg.setAttribute('width', W);
    raiz._svg.setAttribute('height', H + 2 * PAD);
    raiz._svg.setAttribute('viewBox', `0 ${-PAD} ${W} ${H + 2 * PAD}`);
    for (let j = 1; j < feitos.length; j++) {
      const a = toks[feitos[j - 1].i], b = toks[feitos[j].i];
      if (!a || !b || !W) continue;
      const x1 = a.offsetLeft + a.offsetWidth / 2, x2 = b.offsetLeft + b.offsetWidth / 2;
      const dir = x2 > x1 ? 1 : -1;
      const y = dir > 0 ? a.offsetTop - 6 : a.offsetTop + a.offsetHeight + 6;
      const alt = Math.min(58, 22 + Math.abs(x2 - x1) * 0.12) * (dir > 0 ? -1 : 1);
      const cx = (x1 + x2) / 2, cy = y + alt * 1.6;
      const novo = j === feitos.length - 1 && !semAnim && !final;
      const caminho = svg('path', { d: `M${x1},${y} Q${cx},${cy} ${x2},${y}`, class: 'arco' + (dir > 0 ? ' dir' : ' esq') + (novo ? ' novo' : ''), pathLength: 1 });
      // ponta da seta: tangente no fim = (x2 - cx, y - cy)
      const tx = x2 - cx, ty = y - cy, n = Math.hypot(tx, ty) || 1;
      const ux = tx / n, uy = ty / n, px = -uy, py = ux, L = 15, Wd = 8;
      const ponta = svg('path', {
        d: `M${x2},${y} L${x2 - ux * L + px * Wd},${y - uy * L + py * Wd} L${x2 - ux * L - px * Wd},${y - uy * L - py * Wd} Z`,
        class: 'ponta' + (dir > 0 ? ' dir' : ' esq') + (novo ? ' novo' : ''),
      });
      raiz._svg.append(caminho, ponta);
    }

    // frase
    limpar(raiz._frase);
    if (!feitos.length) raiz._frase.append(el('span', { class: 'vazia' }, 'comece pelo nome…'));
    feitos.forEach((p, j) => {
      if (j) raiz._frase.append(' ');   // espaço real entre os pedaços (cópia e leitor de tela); o flex o ignora
      raiz._frase.append(el('span', { class: 'pedaco' + (j === feitos.length - 1 && !final ? ' novo' : '') }, p.frase));
    });
    raiz._frase.classList.toggle('completa', final);

    // dica / conclusão
    limpar(raiz._dica);
    if (final) {
      raiz._dica.append(el('span', { class: 'rot' }, 'com nomes'), cod(D.using));
    } else if (feitos.length) {
      const p = feitos[feitos.length - 1];
      raiz._dica.append(el('span', { class: 'rot' }, `passo ${feitos.length}`), el('span', null, p.dica));
    } else {
      raiz._dica.append(el('span', { class: 'rot' }, 'como ler'), el('span', null, 'o tipo se lê a partir do nome, não da esquerda para a direita'));
    }
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  2. Tamanhos medidos (saída real: Linux x86-64, GCC 13.3)
  // ───────────────────────────────────────────────────────────────────────────
  //  [tipo, bytes no Linux (LP64), bytes no Windows x64 (LLP64) ou null = depende do compilador]
  const TIPOS = [
    ['bool', 1, 1], ['char', 1, 1], ['signed char', 1, 1], ['short', 2, 2], ['int', 4, 4],
    ['long', 8, 4], ['long long', 8, 8], ['float', 4, 4], ['double', 8, 8], ['long double', 16, null], ['void*', 8, 8],
  ];
  const MINIMO = { char: 1, 'signed char': 1, short: 2, int: 2, long: 4, 'long long': 8 };   // 8/8/16/16/32/64 bits

  Deck.widget('declaracoes/tamanhos', {
    iniciar(raiz) {
      raiz.classList.add('w-tamanhos');
      const barras = el('div', { class: 'barras' });
      raiz._linhas = TIPOS.map(([t]) => {
        const trilho = el('div', { class: 'trilho' });
        for (let b = 0; b < 16; b++) { const i = el('i', { class: 'byte' }); i.style.setProperty('--b', b); trilho.append(i); }
        trilho.append(el('span', { class: 'minimo' }));
        const valor = el('span', { class: 'valor' });
        const lin = el('div', { class: 'tipo', dataset: { t } }, el('code', { class: 'nome' }, t), trilho, valor);
        barras.append(lin);
        return { t, lin, trilho, valor };
      });
      raiz._plat = el('div', { class: 'botoes plataformas' },
        el('button', { class: 'botao', type: 'button', dataset: { p: 'lin' }, onclick: () => tamanhos(raiz, raiz._k ?? 0, 'lin') }, 'Linux x86-64 · LP64'),
        el('button', { class: 'botao', type: 'button', dataset: { p: 'win' }, onclick: () => tamanhos(raiz, raiz._k ?? 0, 'win') }, 'Windows x64 · LLP64'));
      raiz._painel = el('div', { class: 'painel' });
      raiz.append(barras, el('div', { class: 'lado' }, raiz._plat, raiz._painel));
    },
    passo(raiz, k) { raiz._k = k; tamanhos(raiz, k, k >= 2 ? 'win' : 'lin'); },
    entrar(raiz, ctx) {
      if (ctx.reduzido) return;
      raiz.classList.remove('crescendo'); void raiz.offsetWidth; raiz.classList.add('crescendo');
    },
    sair(raiz) { raiz.classList.remove('crescendo'); },
  });

  function tamanhos(raiz, k, plat) {
    raiz.dataset.plat = plat;
    raiz.dataset.k = k;
    raiz._plat.querySelectorAll('.botao').forEach((b) => b.classList.toggle('selecionado', b.dataset.p === plat));
    raiz._linhas.forEach(({ t, lin, trilho, valor }) => {
      const [, linux, win] = TIPOS.find((x) => x[0] === t);
      const n = plat === 'win' ? win : linux;
      const mudou = plat === 'win' && win !== linux;
      trilho.querySelectorAll('.byte').forEach((b, j) => {
        b.classList.toggle('on', n != null && j < n);
        b.classList.toggle('perdido', mudou && n != null && j >= n && j < linux);
        b.classList.toggle('duvida', n == null && j < linux);
      });
      valor.textContent = n == null ? '?' : n + (n === 1 ? ' byte' : ' bytes');
      lin.classList.toggle('mudou', mudou);
      lin.classList.toggle('garantia', k === 1 && t in MINIMO);
      lin.classList.toggle('apaga', k === 1 && !(t in MINIMO));
      const m = trilho.querySelector('.minimo');
      m.style.setProperty('--m', MINIMO[t] || 0);
    });
    const p = limpar(raiz._painel);
    if (k === 1) {
      const c = (t) => el('code', null, t);
      p.append(el('span', { class: 'rot' }, 'o padrão só garante'),
        el('div', { class: 'cadeia' }, el('p', null, c('1 == sizeof(char)')), el('p', null, '≤ ', c('short'), ' ≤ ', c('int'), ' ≤ ', c('long'), ' ≤ ', c('long long'))),
        el('p', { class: 'min-tit' }, el('b', null, 'larguras mínimas'), ' em bits (tracejado)'),
        el('div', { class: 'minimos' }, ...[['char', 8], ['short', 16], ['int', 16], ['long', 32], ['long long', 64]].map(([t, b]) =>
          el('span', null, el('code', null, t), el('b', null, String(b))))),
        el('ul', { class: 'limpa garantias' },
          el('li', null, c('char'), ', ', c('signed char'), ' e ', c('unsigned char'), ': 1 por definição'),
          el('li', null, 'ponto flutuante: só a precisão, ', c('float'), ' ≤ ', c('double'), ' ≤ ', c('long double')),
          el('li', null, 'o resto — até ', c('bool'), ' = 1 — é escolha da plataforma')));
    } else if (plat === 'win') {
      p.append(el('span', { class: 'rot alerta' }, 'mesmo código, outro modelo'),
        el('p', null, el('code', null, 'long'), ' tem ', el('b', { class: 'aviso-txt' }, '4 bytes'), ' no Windows x64 (LLP64) e ', el('b', null, '8'), ' no Linux x86-64 (LP64).'),
        el('p', { class: 'mini suave' }, 'o próprio programa avisa: “long = 8 bytes (4 no Windows, 8 no Linux!)”. ', el('code', null, 'long double'), ' também muda de compilador para compilador.'));
    } else {
      p.append(el('span', { class: 'rot' }, 'saída real · GCC 13.3'),
        el('p', null, el('code', null, 'sizeof'), ' medido neste Linux x86-64 — cada quadradinho é um byte.'),
        el('p', { class: 'mini suave' }, el('code', null, 'long double'), ' = 16: formato x87 de 80 bits (64 de mantissa) alinhado em 16.'));
    }
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  3. Laboratório de inicialização: forma × contexto (16 células compiladas)
  // ───────────────────────────────────────────────────────────────────────────
  const CTX = [
    { t: 'local', s: 'void f() { int x; }' },
    { t: 'global', s: 'int x;  // namespace' },
    { t: 'static local', s: 'static int x;' },
    { t: 'membro', s: 'struct T { int x; };\nvoid f() { T t; }' },
  ];
  const UB = { v: '???', c: 'ub', leg: 'indeterminado' };
  const ZERO = { v: '0', c: 'zero', leg: 'zero-init' };
  const FORMAS = [
    {
      f: 'int x;', cel: [UB, ZERO, ZERO, UB],
      resumo: ['Mesma forma, resultados opostos: duração ', el('b', null, 'estática'), ' (global, static) → zerada antes de tudo; duração ', el('b', null, 'automática'), ' → nada é feito.'],
      det: [
        ['int f() { int x; return x; }', 'warning: ‘x’ is used uninitialized [-Wuninitialized]', 'ler é UB — não é “valor aleatório”: não há contrato nenhum'],
        ['int x;   // fora de função', null, 'duração estática: zero-inicializada antes do main'],
        ['void f() { static int x; }', null, 'duração estática: zero, mesmo sem inicializador'],
        ['int g() { T t; return t.x; }', 'warning: ‘t.T::x’ is used uninitialized [-Wuninitialized]', 'o membro segue o objeto: T local → indeterminado (num T global, t.x seria 0)'],
      ],
    },
    {
      f: 'int x{};', cel: Array(4).fill({ v: '0', c: 'ok', leg: 'value-init' }),
      resumo: [el('code', null, '{}'), ' vazio = value-init: ', el('b', null, 'zero em todo lugar'), ' — a forma que dispensa decorar a tabela.'],
      det: Array(4).fill(['int x{};', null, 'value-init: zero garantido, em qualquer contexto']),
    },
    {
      f: 'int x = 4.2;', cel: Array(4).fill({ v: '4', c: 'aviso', leg: 'trunca calado' }),
      resumo: ['copy-init converte ', el('code', null, 'double → int'), ' truncando: ', el('b', null, '4'), '. Nenhum aviso com ', el('code', null, '-Wall -Wextra'), '.'],
      det: Array(4).fill(['int x = 4.2;', null, 'compila sem aviso (só -Wconversion avisa) e vale 4']),
    },
    {
      f: 'int x{4.2};', cel: Array(4).fill({ v: 'erro', c: 'erro', leg: 'narrowing' }),
      resumo: ['Narrowing de constante em ', el('code', null, '{}'), ': ', el('b', { class: 'erro-txt' }, 'erro'), ' em todo contexto.'],
      det: Array(4).fill(['int x{4.2};', 'error: narrowing conversion of ‘4.2000000000000002e+0’ from ‘double’ to ‘int’ [-Wnarrowing]', 'as chaves bloqueiam a perda de dados']),
    },
  ];

  Deck.widget('declaracoes/init', {
    iniciar(raiz) {
      raiz.classList.add('w-init');
      const grade = el('div', { class: 'grade' });
      grade.append(el('div', { class: 'canto' }, el('span', null, 'forma ↓'), el('span', null, 'onde →')));
      CTX.forEach((c) => grade.append(el('div', { class: 'ctx' }, el('b', null, c.t), cod(c.s))));
      raiz._cels = FORMAS.map((F, r) => {
        grade.append(el('div', { class: 'forma', dataset: { r } }, cod(F.f)));
        return F.cel.map((cel, c) => {
          const no = el('button', { class: 'cel', type: 'button', dataset: { r, c } }, el('span', { class: 'v' }), el('span', { class: 'leg' }));
          no.addEventListener('click', () => detalheInit(raiz, r, c));
          grade.append(no);
          return no;
        });
      });
      raiz._grade = grade;
      raiz._det = el('div', { class: 'detalhe' });
      raiz.append(grade, raiz._det);
    },
    passo(raiz, k) {
      raiz.dataset.k = k;
      raiz._cels.forEach((linha, r) => linha.forEach((no, c) => {
        const vis = r < k;
        const cel = FORMAS[r].cel[c];
        no.className = 'cel' + (vis ? ' visivel ' + cel.c : '') + (r === k - 1 ? ' nova' : '');
        no.style.setProperty('--d', c);
        no.querySelector('.v').textContent = vis ? cel.v : '·';
        no.querySelector('.leg').textContent = vis ? cel.leg : '';
        no.disabled = !vis;
      }));
      raiz._grade.querySelectorAll('.forma').forEach((f) => f.classList.toggle('atual', +f.dataset.r === k - 1));
      const d = limpar(raiz._det);
      if (k === 0) {
        d.append(el('span', { class: 'rot' }, 'laboratório'), el('p', null, 'A ', el('b', null, 'mesma'), ' forma de declarar, em quatro lugares. Clique numa célula revelada para ver o código e a mensagem do GCC.'));
      } else {
        d.append(el('div', { class: 'det-cab' }, el('span', { class: 'rot' }, `linha ${k}`), cod(FORMAS[k - 1].f)),
          el('p', null, ...FORMAS[k - 1].resumo.map((x) => (x.nodeType ? x.cloneNode(true) : x))));
      }
    },
  });

  function detalheInit(raiz, r, c) {
    const [codigo, msg, txt] = FORMAS[r].det[c];
    raiz._cels.flat().forEach((n) => n.classList.remove('escolhida'));
    raiz._cels[r][c].classList.add('escolhida');
    const d = limpar(raiz._det);
    d.append(el('div', { class: 'det-cab' }, el('span', { class: 'rot' }, CTX[c].t), cod(FORMAS[r].f)),
      el('div', { class: 'det-linha' }, cod(codigo), el('span', null, txt)));
    if (msg) d.append(el('p', { class: 'msg mono' + (/^error/.test(msg) ? ' erro' : '') }, msg));
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  4. Dedução do auto (todos os casos verificados com static_assert)
  // ───────────────────────────────────────────────────────────────────────────
  //  chips: [texto, estado]  estado: '' fica · 'cai' é descartado · 'novo' é acrescentado
  const CASOS = [
    { decl: 'auto s = "texto";', ctx: '// "texto" é um const char[6]', antes: [['const'], ['char'], ['[6]']], depois: [['const'], ['char'], ['[6]', 'cai'], ['*', 'novo']],
      regra: 'por valor, array decai para ponteiro', res: 'const char*', nota: '⚠ NÃO é <code>std::string</code>' },
    { decl: 'auto copia = origem;', ctx: 'const int& origem = i;', antes: [['const'], ['int'], ['&']], depois: [['const', 'cai'], ['int'], ['&', 'cai']],
      regra: 'auto puro: tira a & e o const de topo', res: 'int', nota: '<code>copia = 999</code> não mexe em <code>i</code> (saída real)' },
    { decl: 'const auto& cr = i;', ctx: 'int i = 42;', antes: [['int']], depois: [['const', 'novo'], ['int'], ['&', 'novo']],
      regra: 'com & em volta, auto é o T de const T&: nada cai', res: 'const int&', nota: 'preserva tudo, sem cópia' },
    { decl: 'auto&& fw = i;', ctx: 'int i = 42;  // lvalue', antes: [['int'], ['lvalue', 'cat']], depois: [['int'], ['&', 'novo'], ['lvalue', 'cai']],
      regra: 'auto&& + lvalue: int& && colapsa para int&', res: 'int&', nota: 'forwarding reference: “colapso de referências”' },
    { decl: 'auto x = {42};', ctx: '// C++17: “= {…}” e “{…}” deduzem diferente', antes: [['{42}'], ['sem tipo', 'cat']], depois: [['{42}', 'cai'], ['std::initializer_list<int>', 'novo']],
      regra: 'lista entre chaves não tem tipo: = { } vira initializer_list', res: 'std::initializer_list<int>', nota: 'já <code>auto y{42};</code> → <code>int</code>' },
    { decl: 'auto&& t = 42;', ctx: '// 42 é um prvalue', antes: [['int'], ['prvalue', 'cat']], depois: [['int'], ['&&', 'novo'], ['prvalue', 'cai']],
      regra: 'auto&& + rvalue: referência de rvalue', res: 'int&&', nota: 'estende a vida do temporário' },
    { decl: 'auto q = cp;', ctx: 'int* const cp = &i;', antes: [['int'], ['*'], ['const']], depois: [['int'], ['*'], ['const', 'cai']],
      regra: 'const de TOPO (do próprio ponteiro) cai', res: 'int*', nota: 'a cópia do ponteiro pode mudar' },
    { decl: 'auto q = pc;', ctx: 'const int* pc = &i;', antes: [['const'], ['int'], ['*']], depois: [['const'], ['int'], ['*']],
      regra: 'const de BAIXO NÍVEL (do alvo) fica', res: 'const int*', nota: 'o alvo continua protegido' },
  ];

  const SAIDA_AUTO = [   // linhas 50–55 da saída real (seção 1.3)
    'auto i = 42;        → int      (4 bytes)',
    'auto d = 42.0;      → double   (8 bytes)',
    'auto c = \'x\';       → char     (1 byte)',
    'auto s = "texto";   → const char* — NÃO é std::string',
    'auto* p = &i;       → int*',
    'auto& r = i;        → int&      (r e i são o mesmo objeto)',
  ];

  Deck.widget('declaracoes/deducao', {
    iniciar(raiz, ctx) {
      raiz.classList.add('w-deducao');
      raiz._lista = el('div', { class: 'lista' });
      raiz._botoes = CASOS.map((c, j) => {
        const b = el('button', { class: 'caso', type: 'button' }, cod(c.decl));
        b.addEventListener('click', () => deduzir(raiz, j, ctx, true));
        raiz._lista.append(b);
        return b;
      });
      raiz._painel = el('div', { class: 'painel' });
      raiz.append(el('div', { class: 'col-lista' }, el('span', { class: 'rot' }, 'casos'), raiz._lista), raiz._painel);
    },
    passo(raiz, k, ctx) {
      if (k === 0) return introDeducao(raiz);
      deduzir(raiz, k - 1, ctx, false);
    },
  });

  function introDeducao(raiz) {
    raiz._botoes.forEach((b) => b.classList.remove('selecionado'));
    const p = limpar(raiz._painel);
    p.classList.remove('feito');
    p.append(el('span', { class: 'rot' }, 'a regra'),
      el('p', { class: 'grande' }, el('code', null, 'auto'), ' puro deduz como um parâmetro de template ', el('b', null, 'por valor'), ':'),
      el('ul', { class: 'setas' },
        el('li', null, 'tira a ', el('b', null, 'referência')),
        el('li', null, 'tira ', el('code', null, 'const'), '/', el('code', null, 'volatile'), ' ', el('b', null, 'de topo')),
        el('li', null, 'array e função ', el('b', null, 'decaem'), ' para ponteiro')),
      el('p', { class: 'mini suave' }, 'com algo em volta, ', el('code', null, 'auto'), ' faz o papel do T de ', el('code', null, 'f(const T&)'), ', ', el('code', null, 'f(T*)'), ', ', el('code', null, 'f(T&&)'), ': com referência, nada decai e o const do objeto fica.'),
      el('span', { class: 'rot rot-saida' }, 'saída real · os casos fáceis'),
      el('div', { class: 'saida-mini' }, ...SAIDA_AUTO.map((x) => el('div', { class: 'ls' }, x))));
  }

  async function deduzir(raiz, j, ctx, clique) {
    const C = CASOS[j];
    const meu = (raiz._vez = (raiz._vez || 0) + 1);
    raiz._botoes.forEach((b, x) => b.classList.toggle('selecionado', x === j));
    const p = limpar(raiz._painel);
    p.classList.remove('feito');
    const chips = (lista) => el('div', { class: 'chips' }, ...lista.map(([t, e]) => el('span', { class: 'chip' + (e ? ' ' + e : '') }, t)));
    const antes = chips(C.antes);
    const depois = chips(C.depois);
    const res = el('div', { class: 'res' }, el('span', { class: 'rot' }, 'tipo deduzido'), cod(C.res), el('span', { class: 'nota', html: C.nota }));
    p.append(
      el('div', { class: 'decl' }, cod(C.ctx, 'ctx'), cod(C.decl, 'principal')),
      el('div', { class: 'etapa' }, el('span', { class: 'rot' }, 'tipo do inicializador'), antes),
      el('div', { class: 'etapa regra' }, el('span', { class: 'seta' }, '↓'), el('span', null, C.regra)),
      el('div', { class: 'etapa' }, el('span', { class: 'rot' }, 'o que sobra'), depois),
      res);
    const anima = !ctx.reduzido;
    if (anima) {
      p.classList.add('preparando');
      await Deck.util.esperar(clique ? 120 : 380);
      if (raiz._vez !== meu) return;
      p.classList.remove('preparando');
    }
    p.classList.add('feito');
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  5. const e ponteiros (mensagens reais do GCC 13.3)
  // ───────────────────────────────────────────────────────────────────────────
  const ESTADOS_CP = [[false, false], [true, false], [false, true], [true, true]];
  const DECL_CP = ['int* p', 'const int* p', 'int* const p', 'const int* const p'];

  Deck.widget('declaracoes/constptr', {
    iniciar(raiz) {
      raiz.classList.add('w-constptr');
      raiz._esq = false; raiz._dir = false;
      const slot = (lado) => {
        const b = el('button', { class: 'slot', type: 'button', title: 'liga/desliga const' }, 'const');
        b.addEventListener('click', () => { raiz['_' + lado] = !raiz['_' + lado]; desenharCP(raiz); });
        return b;
      };
      raiz._sEsq = slot('esq'); raiz._sDir = slot('dir');
      const decl = el('div', { class: 'decl-cp' }, raiz._sEsq, el('span', { class: 't-int' }, 'int'), el('span', { class: 't-ast' }, '*'), raiz._sDir,
        el('span', { class: 't-p' }, 'p'), el('span', { class: 't-resto' }, ' = &a;'));
      raiz._leitura = el('div', { class: 'leitura' });

      // diagrama: posições fixas no palco (sem medições)
      raiz._p = el('div', { class: 'cx cx-p' }, el('span', { class: 'n' }, 'p'), el('span', { class: 'c' }, '&a'), el('i', { class: 'cadeado' }, '🔒'));
      raiz._a = el('div', { class: 'cx cx-a' }, el('span', { class: 'n' }, 'a'), el('span', { class: 'c' }, '1'));
      raiz._b = el('div', { class: 'cx cx-b' }, el('span', { class: 'n' }, 'b'), el('span', { class: 'c' }, '2'));
      raiz._seta = svg('path', { class: 'seta-cp' });
      raiz._ponta = svg('path', { class: 'ponta-cp' });
      raiz._ro = el('span', { class: 'ro' }, '🔒 só leitura via p');
      const fio = svg('svg', { class: 'fio', viewBox: '0 0 640 240', width: 640, height: 240, 'aria-hidden': 'true' }, raiz._seta, raiz._ponta);
      const diagrama = el('div', { class: 'diagrama-cp' }, fio, raiz._p, raiz._a, raiz._b, raiz._ro);

      const teste = (txt, qual) => {
        const r = el('div', { class: 'teste', dataset: { q: qual } },
          el('button', { class: 'botao cod-btn', type: 'button', title: 'executar' }, '▶ ', cod(txt)),
          el('span', { class: 'veredito' }), el('span', { class: 'msg mono' }));
        r.querySelector('button').addEventListener('click', () => executarCP(raiz, qual));
        return r;
      };
      raiz._tEsc = teste('*p = 9;', 'esc');
      raiz._tRea = teste('p = &b;', 'rea');
      raiz._tab = el('table', { class: 'tabela compacta tab-cp' },
        el('thead', null, el('tr', null, el('th', null, 'declaração'), el('th', null, '*p = x'), el('th', null, 'p = &y'))),
        el('tbody', null, ...ESTADOS_CP.map(([e, d], j) => {
          const tr = el('tr', { title: 'mostrar este caso' }, el('td', null, cod(DECL_CP[j])),
            el('td', { class: e ? 'nao' : 'sim' }, e ? 'não' : 'sim'), el('td', { class: d ? 'nao' : 'sim' }, d ? 'não' : 'sim'));
          tr.addEventListener('click', () => { raiz._esq = e; raiz._dir = d; desenharCP(raiz); });
          return tr;
        })));
      raiz.append(el('div', { class: 'topo-cp' }, decl, raiz._leitura),
        el('div', { class: 'meio-cp' }, el('div', { class: 'esq-cp' }, diagrama, raiz._tab),
          el('div', { class: 'testes' }, el('span', { class: 'rot' }, 'compila? (GCC 13.3)'), raiz._tEsc, raiz._tRea,
            el('p', { class: 'mini suave' }, 'clique nos slots ', el('code', null, 'const'), ', nos testes e nas linhas da tabela'))));
    },
    passo(raiz, k) {
      [raiz._esq, raiz._dir] = ESTADOS_CP[Math.min(k, 3)];
      desenharCP(raiz);
    },
  });

  function msgCP(raiz, q) {
    if (q === 'esc') return raiz._esq ? (raiz._dir ? 'error: assignment of read-only location ‘*(const int*)p’' : 'error: assignment of read-only location ‘* p’') : '';
    return raiz._dir ? 'error: assignment of read-only variable ‘p’' : '';
  }

  function desenharCP(raiz) {
    raiz._alvo = 'a'; raiz._va = 1; raiz._vb = 2;   // cada caso recomeça do zero: p = &a, a = 1, b = 2
    raiz.classList.toggle('c-esq', raiz._esq);
    raiz.classList.toggle('c-dir', raiz._dir);
    raiz._sEsq.classList.toggle('on', raiz._esq);
    raiz._sDir.classList.toggle('on', raiz._dir);
    const frase = `p é ponteiro${raiz._dir ? ' const' : ''} para int${raiz._esq ? ' const' : ''}`;
    const idx = (raiz._esq ? 1 : 0) + (raiz._dir ? 2 : 0);
    raiz._tab.querySelectorAll('tbody tr').forEach((tr, j) => tr.classList.toggle('realce', j === idx));
    limpar(raiz._leitura).append(el('span', { class: 'rot' }, 'lendo ←'), el('span', { class: 'f' }, frase));
    [['esc', raiz._tEsc], ['rea', raiz._tRea]].forEach(([q, no]) => {
      const m = msgCP(raiz, q);
      no.classList.remove('rodou', 'falhou');
      no.classList.toggle('pode', !m);
      no.classList.toggle('naopode', !!m);
      no.querySelector('.veredito').textContent = m ? '✗ não compila' : '✓ compila';
      no.querySelector('.msg').textContent = m || (q === 'esc' ? 'escreve em a através de p' : 'p passa a apontar para b');
    });
    posicionarCP(raiz);
  }

  function posicionarCP(raiz) {
    raiz._a.querySelector('.c').textContent = String(raiz._va);
    raiz._b.querySelector('.c').textContent = String(raiz._vb);
    raiz._p.querySelector('.c').textContent = raiz._alvo === 'a' ? '&a' : '&b';
    raiz._a.classList.toggle('alvo', raiz._alvo === 'a');
    raiz._b.classList.toggle('alvo', raiz._alvo === 'b');
    const y = raiz._alvo === 'a' ? 62 : 182;
    const d = `M 214 122 C 310 122, 350 ${y}, 424 ${y}`;
    const ponta = `M 440 ${y} L 420 ${y - 10} L 420 ${y + 10} Z`;
    raiz._seta.setAttribute('d', d); raiz._seta.style.d = `path("${d}")`;
    raiz._ponta.setAttribute('d', ponta); raiz._ponta.style.d = `path("${ponta}")`;
    raiz._ro.style.top = (raiz._alvo === 'a' ? y - 52 : y + 16) + 'px';   // acima da seta para a, abaixo para b: nunca sobre ela
  }

  async function executarCP(raiz, q) {
    const no = q === 'esc' ? raiz._tEsc : raiz._tRea;
    const m = msgCP(raiz, q);
    no.classList.remove('rodou', 'falhou'); void no.offsetWidth;
    if (m) { no.classList.add('falhou'); return; }
    no.classList.add('rodou');
    if (q === 'esc') {
      if (raiz._alvo === 'a') raiz._va = 9; else raiz._vb = 9;
      const alvo = raiz._alvo === 'a' ? raiz._a : raiz._b;
      alvo.classList.remove('pulsa'); void alvo.offsetWidth; alvo.classList.add('pulsa');
    } else {
      raiz._alvo = 'b';
    }
    posicionarCP(raiz);
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  6. Escopo e tempo de vida (saída real da seção 1.5)
  // ───────────────────────────────────────────────────────────────────────────
  // os textos são os do demo (encurtados com …), para casar com a saída real ao lado
  const COD_ESCOPO = [
    'int x = 1;',
    'print("escopo externo: x = {}", x);',
    '{',
    '    int x = 2;   // sombreia o de fora',
    '    print("bloco interno: x = {} …", x);',
    '    {',
    '        int y = 3;',
    '        print("bloco mais interno: y = {}", y);',
    '    }   // y é destruído aqui',
    '}       // x (o 2) é destruído aqui',
    'print("de volta ao externo: x = {} …", x);',
  ].join('\n');
  const FOCO_ESCOPO = [[], [1, 2], [3, 4, 5], [6, 7, 8], [9], [10], [11]];
  const SAIDA_ESCOPO = [
    'escopo externo: x = 1',
    '  bloco interno: x = 2  ← sombreamento (shadowing)',
    '    bloco mais interno: y = 3',
    'de volta ao externo: x = 1  ← o de dentro foi destruído',
  ];

  Deck.widget('declaracoes/escopo', {
    iniciar(raiz) {
      raiz.classList.add('w-escopo');
      raiz._cod = Deck.util.codigo(COD_ESCOPO, { classe: 'compacto', numeros: true, titulo: 'demo_escopo() — simplificado (print = fmt::print)' });
      raiz._mem = el('div', { class: 'blocos' });
      raiz._saida = el('div', { class: 'saida-mini' });
      raiz.append(raiz._cod, el('div', { class: 'lado' }, el('span', { class: 'rot' }, 'objetos vivos'), raiz._mem, el('span', { class: 'rot' }, 'saída real'), raiz._saida));
    },
    passo(raiz, k) {
      const foco = new Set(FOCO_ESCOPO[k] || []);
      raiz._cod.classList.toggle('focando', foco.size > 0);
      raiz._cod.querySelectorAll('.ln').forEach((ln, j) => ln.classList.toggle('foco', foco.has(j + 1)));

      const m = limpar(raiz._mem);
      const bloco = (rot, estado, ...filhos) => el('div', { class: 'bloco ' + estado }, el('span', { class: 'b-rot' }, rot), ...filhos);
      const v = (nome, valor, estado) => el('div', { class: 'vv ' + (estado || '') }, el('span', { class: 'n' }, nome), el('span', { class: 'val' }, valor),
        estado === 'sombra' ? el('span', { class: 'tag' }, 'sombreada') : null, estado === 'morta' ? el('span', { class: 'tag' }, '✗ destruída') : null);
      const f1 = k >= 2 && k <= 5;
      const f2 = k >= 3 && k <= 4;
      const interno2 = f2 ? bloco('{ } mais interno', k === 4 ? 'morto' : (k === 3 ? 'novo' : ''), v('y', '3', k === 4 ? 'morta' : (k === 3 ? 'nasce' : ''))) : null;
      const interno1 = f1 ? bloco('{ } interno', k === 5 ? 'morto' : (k === 2 ? 'novo' : ''), v('x', '2', k === 5 ? 'morta' : (k === 2 ? 'nasce' : '')), interno2) : null;
      const externo = bloco('função demo_escopo()', 'raiz-bloco',
        k >= 1 ? v('x', '1', (k >= 2 && k <= 4) ? 'sombra' : (k === 1 ? 'nasce' : (k === 6 ? 'volta' : ''))) : el('div', { class: 'vazio-b' }, '—'),
        interno1);
      m.append(externo);

      const s = limpar(raiz._saida);
      const n = k >= 6 ? 4 : Math.min(k, 3);
      SAIDA_ESCOPO.slice(0, n).forEach((t, j) => s.append(el('div', { class: 'ls' + (j === n - 1 && k > 0 && (k <= 3 || k === 6) ? ' nova' : '') }, t)));
      if (!n) s.append(el('div', { class: 'ls apagado' }, '(nada impresso ainda)'));
    },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  7. static local × pilha
  // ───────────────────────────────────────────────────────────────────────────
  Deck.widget('declaracoes/static', {
    iniciar(raiz) {
      raiz.classList.add('w-static');
      raiz._pilha = el('div', { class: 'pilha-quadros' });
      raiz._valor = el('span', { class: 'val' }, '0');
      raiz._caixa = el('div', { class: 'caixa-static' }, el('span', { class: 'n' }, 'chamadas'), raiz._valor);
      raiz._log = el('div', { class: 'log-static' });
      raiz.append(
        el('div', { class: 'area' }, el('span', { class: 'rot' }, 'pilha · automático'), raiz._pilha),
        el('div', { class: 'area estatica' }, el('span', { class: 'rot' }, 'área estática · programa inteiro'), raiz._caixa,
          el('p', { class: 'mini suave' }, 'existe antes da 1ª chamada e continua depois da última'), raiz._log));
    },
    async passo(raiz, k, ctx, dir) {
      const quadro = (rot, txt, cls) => el('div', { class: 'quadro ' + (cls || '') }, el('span', { class: 'rotulo' }, rot), txt);
      const fantasma = (c) => quadro('quadro destruído', `contador_de_chamadas() → ${c}`, 'fantasma');
      const log = (c) => (c ? `chamada ${c} → ${c}` : 'nenhuma chamada ainda');
      const desenhar = (valor, extra, c) => {
        limpar(raiz._pilha).append(quadro('demo_escopo()', 'chama contador_de_chamadas()', 'base'));
        if (extra) raiz._pilha.append(extra);
        raiz._valor.textContent = String(valor);
        raiz._log.textContent = log(c);
        raiz._log.classList.toggle('vazio', !c);
      };
      raiz._caixa.classList.remove('pulsa');
      desenhar(k, k ? fantasma(k) : null, k);
      if (!(k > 0 && dir > 0 && !ctx.reduzido)) return;
      // animação da chamada k: empilha, incrementa, desempilha
      const q = quadro('contador_de_chamadas()', 'return ++chamadas;', 'ativo entrando');
      desenhar(k - 1, q, k - 1);
      if (!(await ctx.esperar(450))) return;
      raiz._valor.textContent = String(k);
      raiz._caixa.classList.remove('pulsa'); void raiz._caixa.offsetWidth; raiz._caixa.classList.add('pulsa');
      if (!(await ctx.esperar(600))) return;
      q.classList.add('saindo');
      raiz._log.textContent = log(k);
      raiz._log.classList.remove('vazio');
      if (!(await ctx.esperar(450))) return;
      q.replaceWith(fantasma(k));
    },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  8. Ponteiro × referência (endereços de uma execução real)
  // ───────────────────────────────────────────────────────────────────────────
  const END = { valor: '0x7ffd…9cd4', outro: '0x7ffd…9cd8', ptr: '0x7ffd…9ce0' };
  const PASSOS_MEM = [
    { cod: 'int valor = 42;  int outro = 7;\nint* ptr = &valor;   int& ref = valor;', txt: 'ptr guarda o endereço de valor; ref é outro nome para valor', valor: 42, outro: 7, alvo: 'valor' },
    { cod: '*ptr = 100;', txt: 'escreve através do ponteiro', saida: 'valor agora = 100   ref = 100   (todos o MESMO objeto)', valor: 100, outro: 7, alvo: 'valor', pulso: 'ptr' },
    { cod: 'ref = 200;', txt: 'escreve através do outro nome', saida: 'ref = 200;  →  valor = 200   *ptr = 200', valor: 200, outro: 7, alvo: 'valor', pulso: 'ref' },
    { cod: 'ptr = &outro;', txt: 'ponteiro é reapontável: muda o endereço guardado', valor: 200, outro: 7, alvo: 'outro' },
    { cod: 'ref = outro;', txt: '⚠ referência NÃO religa: copia 7 para dentro de valor', valor: 7, outro: 7, alvo: 'outro', pulso: 'copia', aviso: true },
  ];

  // geometria do palco (px): caixas no topo 96, conteúdo com 116 de altura (meio em 154)
  const X_MEM = { ptr: 180, valor: 580, outro: 1010 };   // centros das caixas

  Deck.widget('declaracoes/memoria', {
    iniciar(raiz) {
      raiz.classList.add('w-memoria');
      raiz._cod = el('div', { class: 'linha-cod' });
      raiz._txt = el('div', { class: 'txt' });
      const caixa = (id, tipo, bytes) => el('div', { class: 'mcx mcx-' + id },
        el('div', { class: 'tags' }, el('span', { class: 'tag' }, id), id === 'valor' ? el('span', { class: 'tag ref' }, '& ref') : null),
        el('div', { class: 'conteudo' }), el('span', { class: 'end' }, END[id]), el('span', { class: 'tipo' }, `${tipo} · ${bytes} bytes`));
      raiz._cx = { ptr: caixa('ptr', 'int*', 8), valor: caixa('valor', 'int', 4), outro: caixa('outro', 'int', 4) };
      raiz._fio = svg('path', { class: 'fio-mem' });
      raiz._ponta = svg('path', { class: 'ponta-mem' });
      // passo 4: a cópia de outro para valor (seta tracejada + o 7 que viaja)
      const copia = svg('g', { class: 'copia-mem' },
        svg('path', { class: 'fio-copia', d: 'M 876 154 L 736 154' }),
        svg('path', { class: 'ponta-copia', d: 'M 716 154 L 738 142 L 738 166 Z' }));
      raiz._pulso = el('i', { class: 'pulso' });
      raiz._voo = el('i', { class: 'voo' }, '7');
      raiz._rotCopia = el('span', { class: 'rot-copia' }, 'copia o 7');
      const palco = el('div', { class: 'palco-mem' },
        svg('svg', { class: 'fios', viewBox: '0 0 1200 320', width: 1200, height: 320, 'aria-hidden': 'true' }, raiz._fio, raiz._ponta, copia),
        raiz._cx.ptr, raiz._cx.valor, raiz._cx.outro, raiz._rotCopia, raiz._pulso, raiz._voo);
      raiz._saida = el('div', { class: 'saida-mem mono' });
      raiz.append(el('div', { class: 'cab' }, raiz._cod, raiz._txt), palco, raiz._saida);
    },
    async passo(raiz, k, ctx, dir) {
      const P = PASSOS_MEM[k];
      // indo para a frente, o valor antigo fica até o "efeito" chegar (pulso, cópia); voltando, estado final direto
      const anima = !ctx.reduzido && dir > 0 && !!P.pulso && k > 0;
      const valorEm = (v) => { raiz._cx.valor.querySelector('.conteudo').textContent = String(v); };
      raiz.dataset.k = k;
      limpar(raiz._cod).append(cod(P.cod));
      raiz._txt.textContent = P.txt;
      raiz._txt.classList.toggle('aviso-txt', !!P.aviso);
      valorEm(anima ? PASSOS_MEM[k - 1].valor : P.valor);
      raiz._cx.outro.querySelector('.conteudo').textContent = String(P.outro);
      raiz._cx.ptr.querySelector('.conteudo').textContent = END[P.alvo];
      raiz._cx.valor.classList.toggle('alvo', P.alvo === 'valor');
      raiz._cx.outro.classList.toggle('alvo', P.alvo === 'outro');
      raiz.classList.toggle('mostra-copia', P.pulso === 'copia');
      // fio do ponteiro: sai do topo de ptr e desce no topo do alvo
      const x2 = X_MEM[P.alvo];
      const fio = `M ${X_MEM.ptr} 94 C ${X_MEM.ptr + 90} -8, ${x2 - 90} -8, ${x2} 76`;
      const ponta = `M ${x2} 92 L ${x2 - 11} 72 L ${x2 + 11} 72 Z`;
      raiz._fio.setAttribute('d', fio); raiz._fio.style.d = `path("${fio}")`;
      raiz._ponta.setAttribute('d', ponta); raiz._ponta.style.d = `path("${ponta}")`;
      raiz._pulso.style.offsetPath = `path("${fio}")`;
      const s = limpar(raiz._saida);
      if (P.saida) s.append(el('span', { class: 'rot' }, 'saída real'), el('span', null, P.saida));
      else if (k === 4) s.append(el('span', { class: 'rot' }, 'verificado'), el('span', null, '&ref == &valor → true · valor = 7 · outro = 7'));
      else s.append(el('span', { class: 'rot' }, 'tamanhos'), el('span', null, 'sizeof(ptr) = 8 · sizeof(valor) = 4'));
      ['valor', 'outro'].forEach((n) => raiz._cx[n].classList.remove('pulsa'));
      raiz._pulso.classList.remove('vai');
      raiz._voo.classList.remove('vai');
      raiz.classList.remove('copiando', 'pelo-nome');
      if (!anima) return;
      if (P.pulso === 'ptr') {
        void raiz._pulso.offsetWidth; raiz._pulso.classList.add('vai');
        if (!(await ctx.esperar(650))) return;
      } else if (P.pulso === 'copia') {
        raiz.classList.add('copiando');
        void raiz._voo.offsetWidth; raiz._voo.classList.add('vai');
        if (!(await ctx.esperar(800))) return;
      } else {
        raiz.classList.add('pelo-nome');   // ref = 200: o outro nome acende antes de o valor mudar
        if (!(await ctx.esperar(450))) return;
      }
      valorEm(P.valor);
      const alvo = raiz._cx.valor;
      void alvo.offsetWidth; alvo.classList.add('pulsa');
    },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  9. Array e decay (saída real da seção 1.6; endereços de uma execução)
  // ───────────────────────────────────────────────────────────────────────────
  const PASSOS_DECAY = [
    { cod: 'int arr[5] = {1, 2, 3, 4, 5};', chave: 'sizeof(arr) = 20 = 5 * sizeof(int)', cls: 'inteiro',
      lado: ['no escopo de arr', 'arr[0]  arr[4]', 'o tipo int[5] carrega o tamanho', '⚠ mas não há verificação de limites'],
      real: ['int arr[5];  sizeof(arr) = 20 = 5 * sizeof(int)', 'arr[0]=1 arr[4]=5  — sem verificação de limites!'] },
    { cod: 'void recebe_array_decaido(const int* p, std::size_t n);\nrecebe_array_decaido(arr, std::size(arr));', chave: 'tamanho perdido', cls: 'perdido', seta: 0,
      lado: ['dentro da função', 'const int* p', '8 bytes · aponta para arr[0]', 'o tamanho tem de vir separado: n = 5'],
      real: 'dentro da função: sizeof(p) = 8 ← virou PONTEIRO, n veio separado (5)' },
    { cod: 'void recebe_array_por_ref(const int (&arr)[5]);\nrecebe_array_por_ref(arr);', chave: 'sizeof(arr) = 20', cls: 'inteiro sobreviveu',
      lado: ['dentro da função', 'const int (&arr)[5]', 'referência: o tipo ainda é int[5]', 'o tamanho faz parte do tipo'],
      real: 'por referência: sizeof(arr) = 20 ← o tamanho SOBREVIVEU' },
    { cod: 'arr[2] == *(arr + 2) == 2[arr]', chave: '+2 elementos = +8 bytes', cls: 'salto', seta: 2,
      lado: ['aritmética de ponteiro', '*(arr + 2)', 'arr + 2 anda 2 × sizeof(int) = 8 bytes', '2[arr] compila — mas não faça'],
      real: 'arr[2] == *(arr + 2) == 2[arr]?  3 3 3' },
    { cod: 'std::array<int, 5> moderno{1, 2, 3, 4, 5};', chave: 'sizeof = 20 · .size() = 5', cls: 'inteiro moderno',
      lado: ['std::array', 'moderno.at(7)', 'lança std::out_of_range', 'por valor, não decai: o tamanho vai junto'],
      real: ['std::array<int,5>: sizeof = 20 (idêntico), mas com .size()=5,', '  .at() com verificação, e SEM decay ao passar por valor.'] },
  ];

  Deck.widget('declaracoes/decay', {
    iniciar(raiz) {
      raiz.classList.add('w-decay');
      raiz._cod = el('div', { class: 'linha-cod' });
      raiz._cels = [1, 2, 3, 4, 5].map((v, j) => el('div', { class: 'dc' }, el('span', { class: 'idx' }, `[${j}]`), el('span', { class: 'v' }, String(v)),
        el('span', { class: 'end' }, '0x…' + (0x9cf0 + 4 * j).toString(16))));
      raiz._tira = el('div', { class: 'tira' }, ...raiz._cels);
      raiz._rotTira = el('span', { class: 'rot-tira' });
      raiz._chave = el('div', { class: 'chave-dc' }, el('span'));
      raiz._func = el('div', { class: 'func' });
      raiz._marcador = el('div', { class: 'marcador' }, el('span', null, 'p'));
      raiz._real = el('div', { class: 'saida-mem mono' });
      raiz.append(el('div', { class: 'cab' }, raiz._cod),
        el('div', { class: 'palco-dc' }, el('div', { class: 'memoria' }, raiz._rotTira, raiz._tira, raiz._chave, raiz._marcador), raiz._func),
        raiz._real);
    },
    passo(raiz, k) {
      const P = PASSOS_DECAY[k];
      raiz.dataset.k = k;
      raiz.className = raiz.className.replace(/\bst-\S+/g, '').trim();
      P.cls.split(' ').forEach((c) => raiz.classList.add('st-' + c));
      limpar(raiz._cod).append(cod(P.cod));
      raiz._rotTira.textContent = k === 4 ? 'std::array<int, 5> moderno' : 'int arr[5]';
      raiz._chave.firstChild.textContent = P.chave;
      raiz._cels.forEach((c, j) => c.classList.toggle('alvo', P.seta != null && j === P.seta));
      raiz._marcador.style.setProperty('--j', P.seta ?? 0);
      raiz._marcador.classList.toggle('visivel', P.seta != null);
      raiz._marcador.firstChild.textContent = k === 3 ? 'arr + 2' : 'p';   // passo 1: o parâmetro p · passo 3: a expressão arr + 2
      const f = limpar(raiz._func);
      f.append(el('span', { class: 'rot' }, P.lado[0]), cod(P.lado[1]), el('span', { class: 'f-txt' }, P.lado[2]), el('span', { class: 'f-txt suave' }, P.lado[3]));
      f.classList.remove('entra'); void f.offsetWidth; f.classList.add('entra');
      limpar(raiz._real).append(el('span', { class: 'rot' }, 'saída real'),
        el('div', { class: 'linhas' }, ...[].concat(P.real).map((t) => el('div', null, t))));
    },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  10. Conversor de literais (Linux x86-64 / LP64; mensagens e tipos do GCC 13.3)
  //      Trata inteiros, ponto flutuante (decimal e hexadecimal), caracteres,
  //      strings (comuns e raw), true/false/nullptr — e explica o que não é literal.
  // ───────────────────────────────────────────────────────────────────────────
  const LIM = {
    int: 2n ** 31n - 1n, 'unsigned int': 2n ** 32n - 1n, long: 2n ** 63n - 1n, 'unsigned long': 2n ** 64n - 1n,
    'long long': 2n ** 63n - 1n, 'unsigned long long': 2n ** 64n - 1n,
  };
  const BYTES = { int: 4, 'unsigned int': 4, long: 8, 'unsigned long': 8, 'long long': 8, 'unsigned long long': 8 };
  const BASE_NOME = { 10: 'decimal', 8: 'octal', 16: 'hexadecimal', 2: 'binário' };
  const PRESETS = ['42', '052', '0x2A', '0b101010', '08', "1'000'000", '42u', '42L', '2147483648', '0x80000000', '3.14f', "'A'"];
  const SEQ_LIT = ['42', '052', '0x2A', '0b101010', '08', "1'000'000", '2147483648'];
  const SUF_INT = /^(?:[uU](?:ll|LL|l|L|z|Z)?|(?:ll|LL|l|L|z|Z)[uU]?)$/;
  const SUF_STD = {   // sufixos sem _ são reservados à biblioteca padrão
    h: 'std::chrono::hours', min: 'std::chrono::minutes', s: 'std::chrono::seconds', ms: 'std::chrono::milliseconds',
    us: 'std::chrono::microseconds', ns: 'std::chrono::nanoseconds', d: 'std::chrono::day', y: 'std::chrono::year',
    i: 'std::complex<double>', if: 'std::complex<float>', il: 'std::complex<long double>',
  };
  const COD = {   // prefixo de caractere/string → [tipo do elemento, bytes, codificação]
    '': ['char', 1, 'utf8'], u8: ['char8_t', 1, 'utf8'], u: ['char16_t', 2, 'utf16'], U: ['char32_t', 4, 'utf32'], L: ['wchar_t', 4, 'utf32'],
  };
  const gcc = (msg, explica, extra) => Object.assign({ erro: msg, gcc: true, explica }, extra);
  const problema = (msg, explica) => ({ erro: msg, gcc: false, explica });
  const hexDe = (v, bytes) => v.toString(16).toUpperCase().padStart(bytes * 2, '0').match(/../g);

  function candidatos(dec, suf) {
    const s = suf.toLowerCase();
    if (s.includes('z')) return s.includes('u') ? ['unsigned long'] : ['long'];   // C++23: size_t / make_signed_t<size_t>
    const u = s.includes('u'), ll = s.includes('ll'), l = !ll && s.includes('l');
    if (u && ll) return ['unsigned long long'];
    if (u && l) return ['unsigned long', 'unsigned long long'];
    if (u) return ['unsigned int', 'unsigned long', 'unsigned long long'];
    if (ll) return dec ? ['long long'] : ['long long', 'unsigned long long'];
    if (l) return dec ? ['long', 'long long'] : ['long', 'unsigned long', 'long long', 'unsigned long long'];
    return dec ? ['int', 'long', 'long long'] : ['int', 'unsigned int', 'long', 'unsigned long', 'long long', 'unsigned long long'];
  }

  function agrupar(s, n) {
    const out = [];
    for (let i = s.length; i > 0; i -= n) out.unshift(s.slice(Math.max(0, i - n), i));
    return out.join("'");
  }

  // sufixo que não é de inteiro nem de flutuante: literal de usuário, da biblioteca ou erro
  function sufixoEstranho(suf, oQue) {
    if (suf[0] === '_') {
      return gcc(`error: unable to find numeric literal operator ‘operator""${suf}’`,
        `literal definido pelo usuário: o tipo é o que operator""${suf} devolver — sem ele declarado, não compila (veja src/operators)`);
    }
    if (SUF_STD[suf]) {
      return problema(`sufixo ${suf}: literal da biblioteca padrão → ${SUF_STD[suf]}`,
        'exige using namespace std::literals; sem isso, o GCC procura um operator""' + suf + ' e não acha');
    }
    return gcc(`error: unable to find numeric literal operator ‘operator""${suf}’`,
      `“${suf}” não é sufixo de ${oQue}: o GCC o trata como literal de usuário — que não existe`);
  }

  // lê o conteúdo de um literal de caractere/string → unidades de código (números)
  function lerUnidades(corpo, enc, cru) {
    const un = [], avisos = [];
    const bits = { utf8: 8, utf16: 16, utf32: 32 }[enc];
    const max = 2 ** bits - 1;
    const SIMPLES = { n: 10, t: 9, r: 13, a: 7, b: 8, f: 12, v: 11, '\\': 92, "'": 39, '"': 34, '?': 63 };
    const ponto = (cp) => {
      if (enc === 'utf32') un.push(cp);
      else if (enc === 'utf16') { if (cp > 0xFFFF) { cp -= 0x10000; un.push(0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF)); } else un.push(cp); }
      else un.push(...new TextEncoder().encode(String.fromCodePoint(cp)));
    };
    for (let i = 0; i < corpo.length;) {
      const cp = corpo.codePointAt(i);
      if (cru || corpo[i] !== '\\') { ponto(cp); i += cp > 0xFFFF ? 2 : 1; continue; }
      const e = corpo[i + 1];
      if (e === undefined) return { erro: gcc('error: missing terminating character', 'a \\ no fim escapou a aspa que fechava o literal') };
      if (e in SIMPLES) { un.push(SIMPLES[e]); i += 2; continue; }
      if (/[0-7]/.test(e)) {
        let j = i + 1, s = '';
        while (j < corpo.length && s.length < 3 && /[0-7]/.test(corpo[j])) s += corpo[j++];
        let v = parseInt(s, 8);
        if (v > max) { avisos.push({ t: 'warning: octal escape sequence out of range', gcc: true }); v &= max; }
        un.push(v); i = j; continue;
      }
      if (e === 'x') {
        let j = i + 2, s = '';
        while (j < corpo.length && /[0-9a-fA-F]/.test(corpo[j])) s += corpo[j++];
        if (!s) return { erro: gcc('error: \\x used with no following hex digits', '\\x precisa de pelo menos um dígito hexadecimal') };
        let v = BigInt('0x' + s);
        if (v > BigInt(max)) { avisos.push({ t: 'warning: hex escape sequence out of range', gcc: true }); v &= BigInt(max); }
        un.push(Number(v)); i = j; continue;
      }
      if (e === 'u' || e === 'U') {
        const n = e === 'u' ? 4 : 8, s = corpo.slice(i + 2, i + 2 + n);
        if (!new RegExp(`^[0-9a-fA-F]{${n}}$`).test(s)) return { erro: gcc(`error: incomplete universal character name \\${e}${s}`, `\\${e} precisa de exatamente ${n} dígitos hexadecimais`) };
        const v = parseInt(s, 16);
        if (v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF)) return { erro: gcc(`error: \\${e}${s} is not a valid universal character`, 'não é um ponto de código Unicode válido') };
        ponto(v); i += 2 + n; continue;
      }
      avisos.push({ t: `warning: unknown escape sequence: '\\${e}'`, gcc: true });
      ponto(corpo.codePointAt(i + 1)); i += 2;
    }
    return { un, avisos };
  }

  // texto “visível” de uma unidade (para a célula de conteúdo)
  function visivel(t) { return t.length > 26 ? t.slice(0, 25) + '…' : t; }

  function caractere(pre, corpo) {
    const [tipoEl, tam, enc] = COD[pre];
    const lido = lerUnidades(corpo, enc, false);
    if (lido.erro) return lido.erro;
    const { un, avisos } = lido;
    const partes = [...(pre ? [['pre', pre, tipoEl]] : []), ['str', `'${corpo}'`, 'caractere']];
    if (!un.length) return gcc('error: empty character constant', "'' não tem caractere nenhum");
    if (un.length > 1) {
      if (pre && pre !== 'L') {
        return gcc('error: character constant too long for its type',
          enc === 'utf8' ? `em UTF-8 isso ocupa ${un.length} unidades de 1 byte — um ${tipoEl} só guarda uma` : `isso ocupa ${un.length} unidades de ${tipoEl} — o literal só pode ter uma`);
      }
      if (pre === 'L') {
        avisos.unshift({ t: 'warning: character constant too long for its type', gcc: true });
        const v = BigInt(un[un.length - 1]);
        return { tipo: 'wchar_t', bytes: 4, valor: v, base: 'char', partes, avisos, hex: hexDe(v, 4),
          motivo: 'L com mais de um caractere: o GCC avisa e fica só com o último' };
      }
      // literal multicaractere comum: tipo int, valor definido pela implementação
      avisos.unshift({ t: un.length > 4 ? 'warning: character constant too long for its type' : 'warning: multi-character character constant [-Wmultichar]', gcc: true });
      let v = 0n;
      for (const u of un) v = ((v << 8n) | BigInt(u & 0xFF)) & 0xFFFFFFFFn;
      const comSinal = v > 0x7FFFFFFFn ? v - 0x100000000n : v;
      return { tipo: 'int', bytes: 4, valor: v, decimal: comSinal.toString(), base: 'char', partes, avisos, hex: hexDe(v, 4),
        motivo: `${un.length} chars entre aspas simples: literal multicaractere — tipo int, valor definido pela implementação`,
        extra: un.length > 4 ? 'o GCC guarda só os 4 últimos bytes' : 'no GCC, cada char vira um byte do int (aqui, em UTF-8)' };
    }
    const u = un[0];
    let v = BigInt(u), decimal = String(u);
    if (!pre && u > 127) {
      decimal = String(u - 256);
      avisos.push({ t: `char tem sinal no x86-64: o byte 0x${u.toString(16).toUpperCase()} vale ${u - 256}` });
    }
    return { tipo: tipoEl, bytes: tam, valor: v, decimal, base: 'char', partes, avisos, hex: hexDe(v, tam),
      motivo: pre ? `prefixo ${pre} → ${tipoEl}` : 'literal de caractere: tipo char (em C seria int!)' };
  }

  function stringLit(pre, corpo, cru, delim, suf) {
    const [tipoEl, tam, enc] = COD[pre];
    const lido = lerUnidades(corpo, enc, cru);
    if (lido.erro) return lido.erro;
    const { un, avisos } = lido;
    const N = un.length + 1;
    const texto = cru ? `R"${delim}(${corpo})${delim}"` : `"${corpo}"`;
    const partes = [...((pre || cru) ? [['pre', pre + (cru ? 'R' : ''), cru ? 'raw' + (pre ? ' · ' + tipoEl : '') : tipoEl]] : []),
      ['str', cru ? texto.slice(1) : texto, 'string'], ...(suf ? [['suf', suf, suf === 's' ? 'std::string' : suf === 'sv' ? 'string_view' : 'usuário']] : [])];
    if (suf) {
      if (suf === 's' || suf === 'sv') {
        const sv = suf === 'sv';
        const nome = 'std::' + ({ '': '', u8: 'u8', u: 'u16', U: 'u32', L: 'w' }[pre]) + (sv ? 'string_view' : 'string');
        return { tipo: nome, bytes: sv ? 16 : 32, partes, avisos,
          celulas: [['conteúdo', visivel(corpo || '(vazia)')], ['.size()', String(un.length)]],
          motivo: `sufixo ${suf} da biblioteca (using namespace std::literals) → ${nome}`,
          extra: sv ? `sizeof(${nome}) = 16: ponteiro + tamanho` : `sizeof(${nome}) = 32 na libstdc++: o texto mora dentro (SSO) ou no heap` };
      }
      return gcc(`error: unable to find string literal operator ‘operator""${suf}’ with ‘const ${tipoEl} [${N}]’, ‘long unsigned int’ arguments`,
        suf[0] === '_' ? `literal de string definido pelo usuário: precisa de um operator""${suf} declarado (veja src/operators)` : `“${suf}” não é sufixo conhecido de string`);
    }
    const hex = un.map((x) => x.toString(16).toUpperCase().padStart(tam * 2, '0')).concat(['0'.repeat(tam * 2)]);
    return { tipo: `const ${tipoEl}[${N}]`, bytes: N * tam, partes, avisos,
      celulas: [['conteúdo', visivel(corpo || '(vazia)')], ['elementos', `${un.length} + o '\\0' = ${N}`]],
      hex: hex.length > 14 ? hex.slice(0, 13).concat(['…']) : hex, hexRot: tam === 1 ? `os ${N * tam} bytes` : `os ${N} ${tipoEl}`,
      hexNota: tam === 1 ? "o '\\0' final é o 00" : `cada ${tipoEl} ocupa ${tam} bytes`,
      motivo: cru ? 'raw string: nada é escapado — e ainda é um array com o \\0 final' : `string: array de const ${tipoEl} com o \\0 final (por valor, decai para const ${tipoEl}*)` };
  }

  // decimal → { exp10 da 1ª casa significativa, mantissa normalizada (1 ≤ m < 10) } ou null se zero
  function ordem(txt) {
    const [num, e = '0'] = txt.toLowerCase().split('e');
    const [ip, fp = ''] = num.split('.');
    const digs = (ip + fp).replace(/^0+/, '');
    if (!/[1-9]/.test(digs)) return null;
    const lead = (ip + fp).search(/[1-9]/);
    const exp10 = parseInt(e, 10) + (ip.length - 1 - lead);
    const m = parseFloat(digs[0] + '.' + digs.slice(1, 17));
    return { exp10, m };
  }

  // corpoTxt: texto do número · valorFn(): valor em double do JS · ordemFn(): {exp10, m} ou null (zero)
  function flutuante(corpoTxt, suf, partes, valorFn, ordemFn) {
    if (suf && !/^[fFlL]$/.test(suf)) {
      if (/^(?:f16|f32|f64|f128|bf16|F16|F32|F64|F128|BF16)$/.test(suf)) {
        const s = suf.toLowerCase();
        return gcc(`warning: ‘${s}’ or ‘${s.toUpperCase()}’ suffix on floating constant only available with ‘-std=c++2b’ or ‘-std=gnu++2b’ [-Wpedantic]`,
          `sufixo do C++23 (${s === 'bf16' ? 'std::bfloat16_t' : 'std::float' + s.slice(1) + '_t'}): em C++20 o GCC aceita, mas avisa`);
      }
      if (/^[dD]$/.test(suf)) {
        return gcc('warning: suffix for double constant is a GCC extension', 'o C++ não tem sufixo d: sem sufixo, o literal já é double');
      }
      if (/^(?:h|min|s|ms|us|ns)$/.test(suf)) {
        return problema(`sufixo ${suf}: literal da biblioteca padrão → std::chrono::duration<long double, …>`,
          'com ponto flutuante, o chrono usa long double; exige using namespace std::literals');
      }
      return sufixoEstranho(suf, 'ponto flutuante');
    }
    const tipo = /f/i.test(suf) ? 'float' : suf ? 'long double' : 'double';
    const bytes = tipo === 'float' ? 4 : tipo === 'double' ? 8 : 16;
    const o = ordemFn();
    const motivo = suf ? `sufixo ${suf} → ${tipo}` : 'sem sufixo, ponto flutuante é double';
    if (tipo === 'long double') {
      if (o && (o.exp10 > 4932 || (o.exp10 === 4932 && o.m > 1.18973149535723176))) {
        return gcc('warning: floating constant exceeds range of ‘long double’ [-Woverflow]', 'nem o long double de 80 bits chega lá (máx. ≈ 1,19e4932): vira infinito');
      }
      if (o && (o.exp10 < -4951 || (o.exp10 === -4951 && o.m < 1.8226))) {
        return gcc('warning: floating constant truncated to zero [-Woverflow]', 'menor que o menor long double (≈ 3,6e-4951): vira 0');
      }
      return { tipo, bytes, flutuante: true, num: null, texto: corpoTxt.replace(/'/g, ''), guardado: null, partes, motivo, avisos: [] };
    }
    const num = valorFn();
    const guardado = tipo === 'float' ? Math.fround(num) : num;
    if (!isFinite(guardado)) {
      return gcc(`warning: floating constant exceeds range of ‘${tipo}’ [-Woverflow]`,
        `maior que o maior ${tipo} (≈ ${tipo === 'float' ? '3,4e38' : '1,8e308'}): o valor vira infinito`);
    }
    if (guardado === 0 && o) {
      return gcc('warning: floating constant truncated to zero [-Woverflow]',
        `menor que o menor ${tipo} representável (≈ ${tipo === 'float' ? '1,4e-45' : '4,9e-324'}): vira 0`);
    }
    return { tipo, bytes, flutuante: true, num, guardado, partes, motivo, avisos: [] };
  }

  function inteiro(t) {
    let base, pre, resto;
    if (/^0[xX]/.test(t)) { base = 16; pre = t.slice(0, 2); resto = t.slice(2); }
    else if (/^0[bB]/.test(t)) { base = 2; pre = t.slice(0, 2); resto = t.slice(2); }
    else if (/^0/.test(t)) { base = 8; pre = '0'; resto = t.slice(1); }
    else if (/^[1-9]/.test(t)) { base = 10; pre = ''; resto = t; }
    else return null;
    const classe = base === 16 ? /[0-9a-fA-F']/ : /[0-9']/;
    let i = 0;
    while (i < resto.length && classe.test(resto[i])) i++;
    const dig = resto.slice(0, i), suf = resto.slice(i);
    if ((base === 16 || base === 2) && !dig) {
      return gcc(`error: unable to find numeric literal operator ‘operator""${t.slice(1)}’`,
        `sem dígitos depois de ${pre}, o GCC lê o literal 0 seguido do sufixo “${t.slice(1)}”`);
    }
    if ((base === 16 || base === 2) && dig[0] === "'") return gcc('error: digit separator after base indicator', `o separador ' não pode vir logo depois de ${pre}`);
    if (dig.includes("''")) return gcc('error: adjacent digit separators', "dois ' seguidos não separam nada");
    if (dig.endsWith("'")) {
      return suf ? gcc('error: digit separator outside digit sequence', "o ' só pode ficar entre dois dígitos")
        : gcc("error: missing terminating ' character", "um ' no fim abre um literal de caractere que nunca fecha");
    }
    const limpo = dig.replace(/'/g, '');
    for (const ch of limpo) {
      if (base === 8 && /[89]/.test(ch)) return gcc(`error: invalid digit "${ch}" in octal constant`, 'o 0 à esquerda fez dele um literal octal — e octal só tem os dígitos 0 a 7');
      if (base === 2 && /[2-9]/.test(ch)) return gcc(`error: invalid digit "${ch}" in binary constant`, 'o prefixo 0b fez dele um literal binário — só os dígitos 0 e 1');
    }
    if (suf && !/^[A-Za-z_]\w*$/.test(suf)) {
      if (/\..*\./.test(t)) return gcc('error: too many decimal points in number', 'um número só pode ter um ponto decimal');
      return null;
    }
    if (suf && !SUF_INT.test(suf)) return sufixoEstranho(suf, 'inteiro');
    const valor = limpo ? BigInt(base === 10 ? limpo : ({ 16: '0x', 8: '0o', 2: '0b' }[base]) + limpo) : 0n;
    const dec = base === 10;
    const lista = candidatos(dec, suf);
    const tipo = lista.find((x) => valor <= LIM[x]);
    const partes = [];
    if (base === 8 && !limpo) partes.push(['pre', '0', 'octal']);   // o literal 0: o próprio 0 é o “prefixo”
    else {
      if (pre) partes.push(['pre', pre, BASE_NOME[base]]);
      partes.push(['', dig, 'dígitos']);
    }
    if (suf) {
      const s = suf.toLowerCase();
      partes.push(['suf', suf, s.includes('z') ? (s.includes('u') ? 'size_t' : 'size_t c/ sinal') : s.includes('u') ? (s.includes('l') ? 'unsigned + long' : 'unsigned') : s.includes('ll') ? 'long long' : 'long']);
    }
    const motivo = `${dec ? 'decimal' : BASE_NOME[base]}${suf ? ' com sufixo ' + suf : ' sem sufixo'}: tenta ${lista.join(' → ')}`;
    if (!tipo) {
      if (valor <= LIM['unsigned long long'] && !/u/i.test(suf) && dec) {
        return gcc('warning: integer constant is so large that it is unsigned', 'não cabe em long long: o GCC recorre à extensão __int128 (16 bytes) — fora do padrão', { motivo });
      }
      return gcc('warning: integer constant is too large for its type', 'nenhum tipo inteiro comporta esse valor: o literal é mal-formado (o GCC só avisa e segue com lixo)', { motivo });
    }
    const avisos = [];
    if (/z/i.test(suf)) avisos.push({ t: `warning: use of C++23 ‘${/u/i.test(suf) ? 'size_t' : 'make_signed_t<size_t>'}’ integer constant`, gcc: true });
    if (base === 8 && limpo && /[1-7]/.test(limpo)) {
      const sd = limpo.replace(/^0+/, '');
      const termos = sd.split('').map((d, k, a) => { const pot = a.length - 1 - k; return pot === 0 ? d : `${d}×${8 ** pot}`; });
      avisos.push({ t: `⚠ zero à esquerda = OCTAL: ${termos.join(' + ')} = ${valor}` });
    }
    if (base === 8 && !limpo) avisos.push({ t: 'curiosidade: pela gramática, o literal 0 é octal' });
    if (dig.includes("'")) avisos.push({ t: "o apóstrofo (C++14) é só visual: o valor é o mesmo sem ele" });
    if (dec && !suf && tipo !== 'int') avisos.push({ t: `não coube em int (máx. 2147483647): virou ${tipo}` });
    if (!dec && tipo.startsWith('unsigned') && !/u/i.test(suf)) avisos.push({ t: `hex/octal/binário pode virar unsigned sem sufixo: ${tipo}` });
    return { tipo, bytes: BYTES[tipo], valor, base, partes, motivo, avisos };
  }

  function analisar(bruto) {
    const t = bruto.trim();
    if (!t) return { erro: 'digite um literal' };
    let m;
    // sinal: não faz parte do literal
    if (/^(?:--|\+\+)/.test(t)) return problema(`${t.slice(0, 2)} é o operador de ${t[0] === '+' ? 'incremento' : 'decremento'}`, 'ele exige uma variável (lvalue): o GCC diz “lvalue required”');
    if (/^[-+]/.test(t)) {
      const op = t[0], resto = t.slice(1).trim();
      const r = resto ? analisar(resto) : { erro: '…' };
      let explica = '';
      if (!r.erro && r.base !== undefined && typeof r.valor === 'bigint' && r.tipo in LIM) {
        const bits = BigInt(r.bytes * 8);
        const v = op === '+' ? r.valor : (r.tipo.startsWith('unsigned') ? ((1n << bits) - r.valor) % (1n << bits) : -r.valor);
        explica = `o resultado tem o tipo do literal (${r.tipo}) e vale ${v}` +
          (r.tipo.startsWith('unsigned') && op === '-' && r.valor ? ' — unsigned dá a volta!' : '') +
          (r.base === 10 && !r.tipo.startsWith('unsigned') && r.tipo !== 'int' && r.valor <= 2n ** 31n && op === '-' ? ` — por isso ${t} não é int` : '');
      } else if (!r.erro && r.flutuante) explica = `o resultado continua ${r.tipo}`;
      else if (!r.erro && (r.tipo === 'char' || r.tipo === 'bool')) explica = `${r.tipo} é promovido a int antes do ${op}: o resultado é int`;
      return problema(`não existe literal negativo: ${t} é o operador ${op} aplicado ao literal ${resto}`, explica);
    }
    // literais com nome
    if (t === 'true' || t === 'false') {
      const v = t === 'true' ? 1n : 0n;
      return { tipo: 'bool', bytes: 1, valor: v, partes: [['lit', t, 'booleano']], hex: hexDe(v, 1),
        celulas: [['valor', t], ['convertido para int', String(v)]], motivo: 'literal booleano: tipo bool (1 byte aqui — o padrão não fixa o tamanho)' };
    }
    if (t === 'nullptr') {
      return { tipo: 'std::nullptr_t', bytes: 8, partes: [['lit', t, 'ponteiro nulo']], hex: hexDe(0n, 8),
        celulas: [['converte para', 'qualquer ponteiro'], ['não converte para', 'int']], motivo: 'literal de ponteiro (C++11): o seu tipo é std::nullptr_t' };
    }
    // caractere: [u8|u|U|L]'…'
    m = /^(u8|u|U|L)?'([\s\S]*)'$/.exec(t);
    if (m) return caractere(m[1] || '', m[2]);
    // raw string: [u8|u|U|L]R"delim(…)delim"[sufixo]
    m = /^(u8|u|U|L)?R"([^()\\\s"]{0,16})\(([\s\S]*)\)\2"(\w*)$/.exec(t);
    if (m) return stringLit(m[1] || '', m[3], true, m[2], m[4]);
    // string: [u8|u|U|L]"…"[sufixo]
    m = /^(u8|u|U|L)?"((?:[^"\\]|\\[\s\S])*)"(\w*)$/.exec(t);
    if (m) return stringLit(m[1] || '', m[2], false, '', m[3]);
    if (/^(u8|u|U|L)?R?["']/.test(t)) return problema('não parece um literal único', 'caractere usa aspas simples e string, aspas duplas; as duas pontas precisam fechar — e é um literal por vez');
    // ponto flutuante hexadecimal (C++17): 0x1.8p3
    if (/^0[xX][0-9a-fA-F']*\.[0-9a-fA-F']*$/.test(t)) return gcc('error: hexadecimal floating constants require an exponent', 'em hexadecimal, o ponto exige o expoente binário p: 0x1.8p0');
    m = /^0[xX]((?:[0-9a-fA-F](?:'?[0-9a-fA-F])*)?)(?:\.((?:[0-9a-fA-F](?:'?[0-9a-fA-F])*)?))?[pP]([+-]?(?:\d(?:'?\d)*)?)([a-zA-Z_]\w*)?$/.exec(t);
    if (m) {
      const [, ip = '', fp = '', ex, suf = ''] = m;
      if (!(ip + fp).replace(/'/g, '')) return gcc('error: no digits in hexadecimal floating constant', 'faltam os dígitos antes do p');
      if (!/\d/.test(ex)) return gcc('error: exponent has no digits', 'o p precisa de um expoente: 0x1p3');
      const temPonto = t.includes('.');
      const mant = (ip + (temPonto ? '.' + fp : '')).replace(/'/g, '');
      const partes = [['pre', t.slice(0, 2), 'hexadecimal'], ['', mant || '0', 'mantissa'], ['exp', 'p' + ex.replace(/'/g, ''), `× 2^${parseInt(ex.replace(/'/g, ''), 10)}`],
        ...(suf ? [['suf', suf, /f/i.test(suf) ? 'float' : 'long double']] : [])];
      const valorHex = () => {
        const a = ip.replace(/'/g, ''), b = fp.replace(/'/g, '');
        let x = 0;
        for (const c of a) x = x * 16 + parseInt(c, 16);
        let peso = 1 / 16;
        for (const c of b) { x += parseInt(c, 16) * peso; peso /= 16; }
        return x * 2 ** parseInt(ex.replace(/'/g, ''), 10);
      };
      const ordemHex = () => {
        let x = 0, peso = 1 / 16;
        for (const c of ip.replace(/'/g, '')) x = x * 16 + parseInt(c, 16);
        for (const c of fp.replace(/'/g, '')) { x += parseInt(c, 16) * peso; peso /= 16; }
        if (!x) return null;
        const l = Math.log10(x) + parseInt(ex.replace(/'/g, ''), 10) * Math.log10(2);
        const exp10 = Math.floor(l);
        return { exp10, m: 10 ** (l - exp10) };
      };
      const r = flutuante(t, suf, partes, valorHex, ordemHex);
      if (!r.erro) r.motivo = (suf ? `sufixo ${suf} → ${r.tipo}` : 'sem sufixo → double') + ' · hexadecimal (C++17): mantissa × 2^expoente';
      return r;
    }
    // ponto flutuante decimal
    if (/^(?:\d[\d']*)?\.?[\d']*[eE][+-]?[a-zA-Z_]?\w*$/.test(t) && /\d/.test(t) && !/[eE][+-]?\d/.test(t)) {
      return gcc('error: exponent has no digits', 'depois do e vem o expoente: 1.5e3');
    }
    m = /^((?:\d(?:'?\d)*)?\.(?:\d(?:'?\d)*)?(?:[eE][+-]?\d(?:'?\d)*)?|\d(?:'?\d)*[eE][+-]?\d(?:'?\d)*)([a-zA-Z_]\w*)?$/.exec(t);
    if (m && /\d/.test(m[1])) {
      const suf = m[2] || '';
      const partes = [['', m[1], 'mantissa'], ...(suf && /^[fFlL]$/.test(suf) ? [['suf', suf, /f/i.test(suf) ? 'float' : 'long double']] : [])];
      const limpoF = m[1].replace(/'/g, '');
      return flutuante(m[1], suf, partes, () => parseFloat(limpoF), () => ordem(limpoF));
    }
    // inteiro
    const r = inteiro(t);
    if (r) return r;
    return problema('não parece um literal', 'experimente inteiros (42, 0x2A, 42u), ponto flutuante (3.14f, 0x1p3), caracteres (\'A\'), strings ("oi"), true ou nullptr');
  }

  // bytes do valor (do mais significativo ao menos), como hexadecimal
  function bytesDe(r) {
    if (r.hex) return r.hex;
    if (r.flutuante) {
      if (r.tipo === 'long double') return null;
      const dv = new DataView(new ArrayBuffer(r.bytes));
      if (r.tipo === 'float') dv.setFloat32(0, r.num); else dv.setFloat64(0, r.num);
      return Array.from({ length: r.bytes }, (_, j) => dv.getUint8(j).toString(16).toUpperCase().padStart(2, '0'));
    }
    if (typeof r.valor !== 'bigint') return null;
    return hexDe(r.valor, r.bytes);
  }

  Deck.widget('declaracoes/literais', {
    iniciar(raiz) {
      raiz.classList.add('w-literais');
      raiz._input = el('input', { type: 'text', spellcheck: 'false', autocomplete: 'off', 'aria-label': 'literal C++' });
      raiz._input.addEventListener('input', () => mostrarLiteral(raiz, raiz._input.value, false));
      // com o cursor no campo o deck ignora o teclado: Enter devolve o controle, e as teclas do
      // clicker (PageDown/PageUp, sem uso num campo de uma linha) navegam direto
      raiz._input.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') raiz._input.blur();
        else if (e.key === 'PageDown' || e.key === 'PageUp') {
          e.preventDefault();
          raiz._input.blur();
          if (e.key === 'PageDown') Deck.proximo(); else Deck.anterior();
        }
      });
      raiz._presets = el('div', { class: 'presets' }, ...PRESETS.map((p) => {
        const b = el('button', { class: 'pre-btn', type: 'button' }, p);
        b.addEventListener('click', () => { raiz._input.value = p; mostrarLiteral(raiz, p, false); });
        return b;
      }));
      raiz._decomp = el('div', { class: 'decomp' });
      raiz._bases = el('div', { class: 'bases' });
      raiz._tipo = el('div', { class: 'tipo-lit' });
      raiz._mem = el('div', { class: 'mem-lit' });
      raiz._avisos = el('div', { class: 'avisos' });
      raiz.append(
        el('div', { class: 'entrada-lit' }, el('label', null, el('span', { class: 'rot' }, 'literal'), raiz._input), raiz._presets),
        el('div', { class: 'resultado' }, raiz._decomp, raiz._bases, el('div', { class: 'rodape-lit' }, raiz._tipo, raiz._mem, raiz._avisos)));
    },
    passo(raiz, k) {
      const t = SEQ_LIT[Math.min(k, SEQ_LIT.length - 1)];
      raiz._input.value = t;
      mostrarLiteral(raiz, t, true);
    },
  });

  function mostrarLiteral(raiz, txt, anima) {
    const r = analisar(txt);
    const bruto = txt.trim();
    raiz.classList.toggle('com-erro', !!r.erro);
    raiz._presets.querySelectorAll('.pre-btn').forEach((b) => b.classList.toggle('selecionado', b.textContent === bruto));
    const dc = limpar(raiz._decomp);
    const bs = limpar(raiz._bases);
    const tp = limpar(raiz._tipo);
    const av = limpar(raiz._avisos);
    const mm = limpar(raiz._mem);
    dc.classList.toggle('longo', bruto.length > 10 && bruto.length <= 15);
    dc.classList.toggle('muito-longo', bruto.length > 15);
    if (r.erro) {
      dc.append(el('span', { class: 'parte erro' }, el('b', null, bruto || '…'), el('small', null, r.gcc ? 'GCC 13.3' : '')));
      bs.append(el('div', { class: 'erro-lit' + (r.gcc ? '' : ' nota-lit') },
        el('span', { class: 'rot' }, r.gcc ? 'o compilador diz' : 'atenção'),
        el('p', { class: 'msg ' + (r.gcc && /^error/.test(r.erro) ? 'erro' : 'aviso') + (r.gcc ? ' mono' : '') }, r.erro),
        r.explica ? el('p', { class: 'explica' }, r.explica) : null));
      if (r.motivo) tp.append(el('span', { class: 'rot' }, 'regra'), el('span', null, r.motivo));
      return;
    }
    r.partes.forEach(([c, t, leg]) => dc.append(el('span', { class: 'parte ' + c + (anima ? ' entra' : '') }, el('b', null, t), el('small', null, leg))));
    const cel = (rot, val, atual) => el('div', { class: 'base' + (atual ? ' atual' : '') }, el('span', { class: 'rot' }, rot), el('b', null, val));
    if (r.celulas) {
      r.celulas.forEach(([rot, val]) => bs.append(cel(rot, val)));
    } else if (r.flutuante) {
      bs.append(cel('valor', r.num == null ? r.texto : String(r.num)),
        cel('guardado (aprox.)', r.guardado == null ? 'x87: 64 bits de mantissa' : r.guardado.toPrecision(r.tipo === 'float' ? 11 : 17)));
    } else {
      const v = r.valor;
      bs.append(cel('decimal', r.decimal || v.toString(10), r.base === 10 || r.base === 'char'),
        cel('octal', v ? '0' + v.toString(8) : '0', r.base === 8),
        cel('hexadecimal', '0x' + v.toString(16).toUpperCase(), r.base === 16),
        cel('binário', '0b' + agrupar(v.toString(2), 4), r.base === 2));
    }
    tp.append(el('span', { class: 'rot' }, 'tipo'), el('b', { class: 'tipo-nome' }, r.tipo), el('span', { class: 'bytes' }, `${r.bytes} ${r.bytes === 1 ? 'byte' : 'bytes'}`), el('span', { class: 'motivo' }, r.motivo));
    const hex = bytesDe(r);
    if (hex) {
      mm.append(el('span', { class: 'rot' }, r.hexRot || (r.bytes === 1 ? 'o byte' : `os ${r.bytes} bytes`)), el('div', { class: 'caixas' }, ...hex.map((h, j) => {
        const c = el('b', { class: 'byte-lit' + (/^0+$/.test(h) && hex.slice(0, j + 1).every((x) => /^0+$/.test(x)) ? ' zero' : '') + (h === '…' ? ' mais' : '') }, h);
        c.style.setProperty('--j', j);
        return c;
      })), el('span', { class: 'mini suave' }, r.hexNota || (r.flutuante ? 'IEEE-754, do mais significativo ao menos' : 'do mais significativo ao menos')));
    } else if (r.tipo === 'long double') {
      mm.append(el('span', { class: 'rot' }, 'os 16 bytes'), el('span', { class: 'mini suave' }, 'formato x87 de 80 bits + 6 de alinhamento — sem equivalente em JavaScript'));
    }
    if (r.extra) mm.append(el('span', { class: 'mini suave' }, r.extra));
    (r.avisos || []).forEach((a) => av.append(el('p', { class: 'msg aviso' + (a.gcc ? ' mono' : '') }, a.t)));
  }
})();
