/* =============================================================================
   assets/widgets/expressoes.js — widgets do Capítulo 7 · Expressões
   (src/syntax/demo_expressoes.hpp)

   Todo número exibido aqui foi MEDIDO no g++ 13.3 (-std=c++20, x86-64 Linux)
   ou vem da saída real do ./dist/bin/syntax. Cada widget desenha o ESTADO k
   do zero em passo(k); as animações extras só rodam para frente e nunca com
   ?estatico / movimento reduzido.

     expressoes/divisao     17 / 5: grupos completos, resto, double, negativos
     expressoes/curto       && para no meio; & avalia os dois lados
     expressoes/bits        & | ^ << >> ~ coluna por coluna (a = 17, b = 5)
     expressoes/composta    c = 10 → 15 → 12 → 24 → 6 → 2 → 16 → 17
     expressoes/incremento  ++i devolve o próprio i; i++ devolve uma cópia
     expressoes/arvore      precedência = forma da árvore; avalia de baixo p/ cima
     expressoes/categorias  lvalue / xvalue / prvalue pelo teste decltype((e))
     expressoes/comum       promoção + conversões aritméticas usuais (a escada)
     expressoes/sinal       o odômetro de 32 bits: -1 < 1u é false
     expressoes/float       acima de 2^24 o float pula números
     expressoes/castc       o cast C tenta 5 casts nomeados, em ordem
     expressoes/padding     a ordem dos membros muda o sizeof
     expressoes/vazamento   new cru + exceção = vazamento; unique_ptr libera
   ============================================================================= */
(function () {
  'use strict';
  const { el, svg } = Deck.util;

  // ── utilitários ────────────────────────────────────────────────────────────
  /** <code class="cpp"> com realce de C++ (chip escuro, legível nos dois temas) */
  const chip = (txt, cls) => el('code', { class: 'cpp' + (cls ? ' ' + cls : ''), html: Deck.util.realcar(txt) });
  /** anima só para frente e sem movimento reduzido */
  const animar = (ctx, dir) => !ctx.reduzido && dir !== -1;
  /** foca linhas (1-based) de um bloco de código gerado por Deck.util.codigo */
  function focar(fig, linhas) {
    const alvo = linhas && linhas.length ? new Set(linhas) : null;
    fig.classList.toggle('focando', !!alvo);
    fig.querySelectorAll('.ln').forEach((ln, j) => ln.classList.toggle('foco', !!alvo && alvo.has(j + 1)));
  }
  const rotulo = (txt) => el('span', { class: 'ex-r' }, txt);
  /** mensagem do g++ com a flag final ("[-Wxxx]") numa peça só: a quebra não a parte no hífen */
  const comFlag = (txt) => {
    const m = /^(.*?)(\s*)(\[-W[^\]]+\])$/.exec(txt);
    return m ? [m[1] + m[2], el('span', { class: 'ex-nw' }, m[3])] : [txt];
  };
  const binario = (v, n) => (v >>> 0).toString(2).padStart(n, '0').slice(-n);
  /** token de animação por widget: um clique/passo novo invalida a animação anterior */
  const novoToken = (raiz) => (raiz._tok = (raiz._tok || 0) + 1);
  /** conta de `de` até `ate` em `ms`; para se outro token surgir */
  function contar(raiz, no, de, ate, ms) {
    const tok = novoToken(raiz);
    const t0 = performance.now();
    const quadro = (t) => {
      if (raiz._tok !== tok) return;
      const p = Math.min(1, (t - t0) / ms);
      no.textContent = String(Math.round(de + (ate - de) * (1 - Math.pow(1 - p, 3))));
      if (p < 1) requestAnimationFrame(quadro);
    };
    requestAnimationFrame(quadro);
  }

  // ════════════════════════════════════════════════════════════════════════════
  //  divisao — 17 fichas em grupos de 5
  // ════════════════════════════════════════════════════════════════════════════
  const DIV_LINHAS = [
    ['17 / 5', '3', 'grupos COMPLETOS — trunca', 1],
    ['17 % 5', '2', 'o que sobra', 2],
    ['17 / 5.0', '3.4', 'um double basta: divisão real', 3],
    ['-17 / 5', '-3', 'trunca para ZERO (não −4)', 4],
    ['-17 % 5', '-2', 'o resto leva o sinal do dividendo', 4],
  ];
  Deck.widget('expressoes/divisao', {
    iniciar(raiz) {
      raiz.classList.add('ex-div');
      const campo = el('div', { class: 'campo' });
      raiz._grupos = [0, 1, 2, 3].map((c) => {
        const g = el('div', { class: 'grupo' + (c === 3 ? ' parcial' : '') }, el('span', { class: 'n' }));
        g.style.left = (c * 86) + 'px';
        if (c === 3) g.append(el('i', { class: 'enche' }));
        campo.append(g);
        return g;
      });
      raiz._fichas = Array.from({ length: 17 }, (_, i) => {
        const f = el('i', { class: 'ficha' + (i >= 15 ? ' sobra' : '') });
        f.style.setProperty('--d', (i * 28) + 'ms');
        campo.append(f);
        return f;
      });
      raiz._dica = el('p', { class: 'dica' }, '17 fichas, divididas em grupos de 5…');
      const livro = el('div', { class: 'livro' }, raiz._dica);
      raiz._linhas = DIV_LINHAS.map(([e, v, n]) => {
        const l = el('div', { class: 'reg' }, chip(e), el('span', { class: 'ig' }, '='),
          el('b', { class: 'v' }, v), el('span', { class: 'nota' }, n));
        livro.append(l);
        return l;
      });
      raiz.append(campo, livro);
    },
    passo(raiz, k) {
      raiz.dataset.k = k;
      raiz._fichas.forEach((f, i) => {
        let x, y;
        if (k === 0) { x = (i % 6) * 54 + 4; y = Math.floor(i / 6) * 54 + 52; }
        else { x = Math.floor(i / 5) * 86 + 10; y = (i % 5) * 50 + 10; }
        f.style.setProperty('--x', x + 'px');
        f.style.setProperty('--y', y + 'px');
      });
      raiz._grupos.forEach((g, c) => {
        const n = g.querySelector('.n');
        if (c < 3) n.textContent = k >= 4 ? '−' + (c + 1) : String(c + 1);
        // o grupo incompleto: o resto (2 fichas) ou, com 5.0, a fração de grupo (2/5 = 0.4)
        else n.textContent = k === 2 ? 'resto 2' : k === 3 ? '0.4' : k >= 4 ? 'resto −2' : '';
      });
      raiz._dica.classList.toggle('some', k > 0);
      raiz._linhas.forEach((l, j) => {
        const ate = DIV_LINHAS[j][3];
        l.classList.toggle('on', k >= ate);
        l.classList.toggle('atual', k === ate);
      });
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  curto — && para no meio; & não
  // ════════════════════════════════════════════════════════════════════════════
  function pistaCurto(op, titulo, cls) {
    const val = () => el('span', { class: 'val' });
    const esq = el('div', { class: 'no esq' }, chip('ok(p)'), val());
    const dir = el('div', { class: 'no dir' }, chip('usa(p)'), val());
    const res = el('div', { class: 'res' }, rotulo('resultado'), el('b'));
    const log = el('p', { class: 'log' }, rotulo('chamadas'), el('span', { class: 'l' }));
    const p = el('div', { class: 'pista ' + cls },
      el('div', { class: 'tit' }, el('b', { class: 'op-g' }, op), el('span', null, titulo)),
      el('div', { class: 'fluxo' }, esq, el('span', { class: 'op' }, op), dir, el('span', { class: 'seta' }, '→'), res),
      log);
    p._esq = esq; p._dir = dir; p._res = res; p._log = log;
    return p;
  }
  function estadoNo(no, txt, cls) {
    no.classList.remove('falso', 'pulado', 'ub', 'verdade');
    if (cls) no.classList.add(cls);
    no.querySelector('.val').textContent = txt || '';
  }
  function desenharCurto(raiz, k, fase) {
    const A = raiz._a, B = raiz._b;
    // && : passo 1 avalia ok(p); passo 2 pula usa(p)
    estadoNo(A._esq, k >= 1 ? 'false' : '', k >= 1 ? 'falso' : '');
    estadoNo(A._dir, k >= 2 ? 'nem é chamada' : '', k >= 2 ? 'pulado' : '');
    A._res.querySelector('b').textContent = k >= 2 ? 'false' : '';
    A._res.classList.toggle('on', k >= 2);
    A._log.querySelector('.l').textContent = k >= 1 ? 'ok()' : '';
    // & : passo 3 avalia os dois
    const fb = fase == null ? 2 : fase;
    estadoNo(B._esq, k >= 3 && fb >= 1 ? 'false' : '', k >= 3 && fb >= 1 ? 'falso' : '');
    estadoNo(B._dir, k >= 3 && fb >= 2 ? 'p->x com p nulo: UB' : '', k >= 3 && fb >= 2 ? 'ub' : '');
    B._res.querySelector('b').textContent = k >= 3 && fb >= 2 ? 'UB' : '';
    B._res.classList.toggle('on', k >= 3 && fb >= 2);
    B._res.classList.toggle('ub', k >= 3 && fb >= 2);
    B._log.querySelector('.l').textContent = k >= 3 ? (fb >= 2 ? 'ok()  usa()' : 'ok()') : '';
    raiz.dataset.k = k;
  }
  Deck.widget('expressoes/curto', {
    iniciar(raiz) {
      raiz.classList.add('ex-curto');
      raiz._a = pistaCurto('&&', 'lógico — com curto-circuito', 'a');
      raiz._b = pistaCurto('&', 'bit a bit — sem curto-circuito', 'b');
      raiz.append(el('div', { class: 'decl' }, chip('const Ponto* p = nullptr;'),
        el('span', { class: 'mini suave' }, 'ok(p) testa o ponteiro · usa(p) lê p->x')), raiz._a, raiz._b);
    },
    async passo(raiz, k, ctx, dir) {
      desenharCurto(raiz, k);
      if (k === 3 && animar(ctx, dir)) {
        desenharCurto(raiz, k, 0);
        if (!(await ctx.esperar(350))) return;
        desenharCurto(raiz, k, 1);
        if (!(await ctx.esperar(650))) return;
        desenharCurto(raiz, k, 2);
      }
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  bits — a = 17, b = 5, coluna por coluna
  // ════════════════════════════════════════════════════════════════════════════
  const BIT_OPS = [
    { op: '&', ex: 'a & b', f: (a, b) => a & b, nome: 'AND', txt: '1 só onde os DOIS têm 1' },
    { op: '|', ex: 'a | b', f: (a, b) => a | b, nome: 'OR', txt: '1 onde QUALQUER um tem 1' },
    { op: '^', ex: 'a ^ b', f: (a, b) => a ^ b, nome: 'XOR', txt: '1 onde os bits são DIFERENTES' },
    { op: '<<', ex: 'a << 1', f: (a) => a << 1, desl: 1, nome: 'desloca (× 2)', txt: 'todos um passo à esquerda, entra um 0: × 2' },
    { op: '>>', ex: 'a >> 1', f: (a) => a >> 1, desl: -1, nome: 'desloca (÷ 2)', txt: 'todos um passo à direita, o último cai: ÷ 2 truncando' },
    { op: '~', ex: '~a', f: (a) => ~a, un: true, nome: 'NOT', txt: 'inverte TODOS os bits' },
  ];
  function linhaBits(nome, clicavel, aoClicar) {
    const bits = el('div', { class: 'bits' });
    for (let b = 7; b >= 0; b--) {
      const c = el('b', { 'data-b': b }, '0');
      if (clicavel) {
        c.classList.add('clic');
        c.title = 'bit ' + b + ' — clique para inverter';
        c.addEventListener('click', () => aoClicar(b));
      }
      bits.append(c);
      if (b === 4) bits.append(el('span', { class: 'sep' }));
    }
    const nomeEl = el('span', { class: 'nome' });
    const dec = el('span', { class: 'dec' });
    const linha = el('div', { class: 'lin ' + nome }, nomeEl, bits, dec);
    linha._bits = bits; linha._nome = nomeEl; linha._dec = dec;
    return linha;
  }
  function pintarBits(linha, v) {
    linha._bits.querySelectorAll('b').forEach((c) => {
      const on = v == null ? null : (v >>> +c.dataset.b) & 1;
      c.textContent = on == null ? '·' : String(on);
      c.classList.toggle('on', on === 1);
    });
  }
  function desenharBits(raiz) {
    const { a, b } = raiz._v;
    const o = raiz._op != null ? BIT_OPS[raiz._op] : null;
    raiz._la._nome.innerHTML = '';
    raiz._la._nome.append(chip('a'));
    pintarBits(raiz._la, a);
    raiz._la._dec.textContent = String(a);
    raiz._lb._nome.innerHTML = '';
    raiz._lb._nome.append(chip(o && o.desl ? 'desloca' : 'b'));
    raiz._lb.classList.toggle('apagada', !!(o && (o.un || o.desl)));
    pintarBits(raiz._lb, o && o.desl ? 1 : b);
    raiz._lb._dec.textContent = o && o.desl ? '1' : String(b);
    raiz._sinal.textContent = o ? o.op : '';
    raiz._botoes.forEach((bt, j) => bt.classList.toggle('selecionado', raiz._op === j));
    const lr = raiz._lr;
    lr._nome.innerHTML = '';
    if (!o) {
      lr._nome.append(el('span', { class: 'suave' }, 'resultado'));
      pintarBits(lr, null);
      lr._dec.textContent = '';
      raiz._cod.innerHTML = '';
      raiz._cod.append(chip('int a = ' + a + ', b = ' + b + ';'), el('span', { class: 'suave' }, '  escolha uma operação →'));
      raiz._txt.textContent = 'Os mesmos valores do demo: a = 17 = 0b10001, b = 5 = 0b00101.';
      return;
    }
    const r = o.f(a, b);
    lr._nome.append(chip(o.ex));
    const r8 = r & 0xff;
    pintarBits(lr, r8);
    lr._estouro = r > 255 || r < 0;
    if (o.un) lr._dec.innerHTML = r8 + '<small>int: ' + r + '</small>';
    else if (r > 255) lr._dec.innerHTML = r + '<small>9º bit</small>';
    else lr._dec.textContent = String(r);
    raiz._cod.innerHTML = '';
    // o << pode passar de 8 bits (a ≥ 128): o binário mostra o 9º bit, para bater com o decimal
    raiz._cod.append(chip(o.un ? '~a (em 8 bits) = ' + r8 : o.ex + ' = 0b' + binario(r, r > 255 ? 9 : 8) + ' = ' + r), el('span', { class: 'tag' }, o.nome));
    raiz._txt.textContent = o.txt + (o.un ? ' — em 8 bits: ' + r8 + '; como int (32 bits): ' + r + '.' : '.');
  }
  async function varrerBits(raiz, ctx) {
    const tok = novoToken(raiz);
    const o = BIT_OPS[raiz._op];
    const r8 = o.f(raiz._v.a, raiz._v.b) & 0xff;
    const cel = (linha, b) => linha._bits.querySelector('[data-b="' + b + '"]');
    raiz._lr._bits.querySelectorAll('b').forEach((c) => { c.textContent = '·'; c.classList.remove('on'); });
    raiz._lr._dec.style.opacity = '0';
    for (let b = 0; b <= 7; b++) {
      raiz.querySelectorAll('.scan').forEach((c) => c.classList.remove('scan'));
      const fonte = o.desl ? b - o.desl : b;
      if (fonte >= 0 && fonte <= 7) cel(raiz._la, fonte).classList.add('scan');
      if (!o.un && !o.desl) cel(raiz._lb, b).classList.add('scan');
      const c = cel(raiz._lr, b);
      c.classList.add('scan');
      const on = (r8 >>> b) & 1;
      c.textContent = String(on);
      c.classList.toggle('on', !!on);
      if (!(await ctx.esperar(95)) || raiz._tok !== tok) return;
    }
    raiz.querySelectorAll('.scan').forEach((c) => c.classList.remove('scan'));
    raiz._lr._dec.style.opacity = '';
  }
  Deck.widget('expressoes/bits', {
    iniciar(raiz, ctx) {
      raiz.classList.add('ex-bits');
      raiz._v = { a: 17, b: 5 };
      raiz._op = null;
      const alterna = (q) => (b) => {
        raiz._v[q] = (raiz._v[q] ^ (1 << b)) & 0xff;
        // um clique no meio da varredura a cancela (senão ela terminaria com os bits antigos)
        novoToken(raiz);
        raiz.querySelectorAll('.scan').forEach((c) => c.classList.remove('scan'));
        raiz._lr._dec.style.opacity = '';
        desenharBits(raiz);
      };
      raiz._botoes = BIT_OPS.map((o, j) => el('button', {
        class: 'botao', type: 'button', onclick: () => {
          raiz._op = j; desenharBits(raiz);
          if (!ctx.reduzido) varrerBits(raiz, ctx);
        },
      }, o.op));
      raiz._la = linhaBits('a', true, alterna('a'));
      raiz._lb = linhaBits('b', true, alterna('b'));
      raiz._sinal = el('div', { class: 'sinal' });
      raiz._lr = linhaBits('r', false);
      raiz._cod = el('div', { class: 'cod' });
      raiz._txt = el('p', { class: 'txt' });
      raiz.append(
        el('div', { class: 'topo-ops' }, rotulo('operação'), el('div', { class: 'botoes' }, raiz._botoes)),
        el('div', { class: 'grade' }, raiz._la, raiz._lb, raiz._sinal, raiz._lr),
        raiz._cod, raiz._txt);
    },
    async passo(raiz, k, ctx, dir) {
      raiz._v = { a: 17, b: 5 };
      raiz._op = k > 0 ? k - 1 : null;
      novoToken(raiz);
      raiz._lr._dec.style.opacity = '';
      desenharBits(raiz);
      if (k > 0 && animar(ctx, dir)) await varrerBits(raiz, ctx);
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  composta — a cadeia real do programa
  // ════════════════════════════════════════════════════════════════════════════
  const COMP = [
    { v: 10, eq: 'c começa em 10', conta: '' },
    { v: 15, eq: 'c = c + 5', conta: '10 + 5 = 15' },
    { v: 12, eq: 'c = c - 3', conta: '15 − 3 = 12' },
    { v: 24, eq: 'c = c * 2', conta: '12 × 2 = 24' },
    { v: 6, eq: 'c = c / 4', conta: '24 / 4 = 6' },
    { v: 2, eq: 'c = c % 4', conta: '6 % 4 = 2' },
    { v: 16, eq: 'c = c << 3', conta: '2 << 3 = 16' },
    { v: 17, eq: 'c = c | 1', conta: '16 | 1 = 17' },
  ];
  const COMP_COD = [
    'int c = 10;',
    'c += 5;    // c = c + 5',
    'c -= 3;    // c = c - 3',
    'c *= 2;    // c = c * 2',
    'c /= 4;    // inteira: trunca',
    'c %= 4;    // resto',
    'c <<= 3;   // × 8',
    'c |= 1;    // liga o bit 0',
  ].join('\n');
  Deck.widget('expressoes/composta', {
    iniciar(raiz) {
      raiz.classList.add('ex-comp');
      raiz._fig = Deck.util.codigo(COMP_COD, { titulo: 'composta.cpp' });
      raiz._val = el('b', { class: 'valor' }, '10');
      raiz._bits = el('div', { class: 'bits mini-bits' }, Array.from({ length: 8 }, (_, j) => el('b', { 'data-b': 7 - j }, '0')));
      raiz._eq = el('div', { class: 'eq' });
      raiz._trilha = el('div', { class: 'trilha' }, COMP.map((c, j) => el('span', { class: 'passo-t', 'data-j': j }, String(c.v))));
      raiz.append(raiz._fig, el('div', { class: 'painel' },
        el('div', { class: 'var-c' }, rotulo('int c'), raiz._val, raiz._bits),
        raiz._eq,
        el('div', { class: 'trilha-box' }, rotulo('a cadeia (saída real)'), raiz._trilha)));
    },
    passo(raiz, k, ctx, dir) {
      const c = COMP[k];
      focar(raiz._fig, [k + 1]);
      const ant = k > 0 ? COMP[k - 1].v : c.v;
      if (animar(ctx, dir) && k > 0) contar(raiz, raiz._val, ant, c.v, 700);
      else { novoToken(raiz); raiz._val.textContent = String(c.v); }
      raiz._bits.querySelectorAll('b').forEach((b) => {
        const on = (c.v >>> +b.dataset.b) & 1;
        b.textContent = String(on);
        b.classList.toggle('on', !!on);
      });
      raiz._eq.innerHTML = '';
      raiz._eq.append(k > 0 ? chip(c.eq) : el('span', { class: 'suave' }, c.eq));
      if (c.conta) raiz._eq.append(el('span', { class: 'conta' }, c.conta));
      raiz._trilha.querySelectorAll('.passo-t').forEach((s) => {
        const j = +s.dataset.j;
        s.classList.toggle('on', j <= k);
        s.classList.toggle('atual', j === k);
      });
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  incremento — ++i × i++
  // ════════════════════════════════════════════════════════════════════════════
  function caixaInc(cls, titulo) {
    const v = el('b', null, '5');
    const c = el('div', { class: 'cx ' + cls }, rotulo(titulo), v);
    c._v = v;
    return c;
  }
  Deck.widget('expressoes/incremento', {
    iniciar(raiz) {
      raiz.classList.add('ex-inc');
      const lado = (cls, expr, nome, tipo) => {
        const i = caixaInc('var-i', 'int i');
        // as duas colunas ficam alinhadas: no ++i o lugar da cópia fica VAZIO (ninguém paga)
        const copia = cls === 'pos' ? caixaInc('copia', 'cópia: antigo') : caixaInc('sem-copia', 'nenhuma cópia');
        if (cls !== 'pos') copia._v.textContent = '—';
        const dev = caixaInc('dev', 'a expressão vale');
        const tag = el('span', { class: 'tag' }, tipo);
        const nota = el('p', { class: 'nota' });
        const fluxo = el('div', { class: 'fluxo' }, i, el('span', { class: 'seta s1' }, '→'),
          copia, el('span', { class: 'seta s2' }, '→'), dev);
        const box = el('div', { class: 'lado ' + cls },
          el('div', { class: 'tit' }, chip(expr, 'grande'), el('span', null, nome), tag), fluxo, nota);
        Object.assign(box, { _i: i, _copia: cls === 'pos' ? copia : null, _vazia: cls === 'pos' ? null : copia, _dev: dev, _nota: nota, _tag: tag });
        return box;
      };
      raiz._pre = lado('pre', '++i', 'pré-incremento', 'lvalue · int&');
      raiz._pos = lado('pos', 'i++', 'pós-incremento', 'prvalue · int');
      raiz.append(raiz._pre, raiz._pos);
    },
    passo(raiz, k) {
      const P = raiz._pre, Q = raiz._pos;
      raiz.dataset.k = k;
      // pré
      P._i._v.textContent = k >= 1 ? '6' : '5';
      P._i.classList.toggle('mudou', k === 1);
      P._dev.classList.toggle('on', k >= 2);
      P._dev._v.textContent = k >= 2 ? '6' : '';
      P._dev.classList.toggle('e-i', k >= 2);
      P._tag.classList.toggle('on', k >= 2);
      P._vazia.classList.toggle('on', k >= 2);
      P._nota.textContent = k === 0 ? 'int i = 5;' : k === 1 ? '1 · incrementa i' : '2 · devolve O PRÓPRIO i (já 6) — sem cópia';
      // pós
      Q._copia.classList.toggle('on', k >= 3);
      Q._copia.classList.toggle('morre', k >= 5);
      Q._i._v.textContent = k >= 4 ? '6' : '5';
      Q._i.classList.toggle('mudou', k === 4);
      Q._dev.classList.toggle('on', k >= 5);
      Q._dev._v.textContent = k >= 5 ? '5' : '';
      Q._tag.classList.toggle('on', k >= 5);
      Q._nota.textContent = k < 3 ? 'int i = 5;' : k === 3 ? '1 · guarda uma CÓPIA do antigo' : k === 4 ? '2 · incrementa i' : '3 · devolve a cópia (5); ela morre no ;';
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  arvore — a árvore sintática de cada expressão
  // ════════════════════════════════════════════════════════════════════════════
  const N = (t, v, ...f) => ({ t, v, f });
  const ARVORES = [
    { txt: '2 + 3 * 4', tok: ['2', '+', '3', '*', '4'], raiz: N(1, '14', N(0, ''), N(3, '12', N(2, ''), N(4, ''))),
      le: '2 + (3 * 4)', res: '14', regra: '* (nível 5) vence + (nível 6)' },
    { txt: '(2 + 3) * 4', tok: ['(', '2', '+', '3', ')', '*', '4'], raiz: N(5, '20', N(2, '5', N(1, ''), N(3, '')), N(6, '')),
      le: '(2 + 3) * 4', res: '20', regra: 'parênteses mandam: o + desce na árvore' },
    { txt: '1 << 2 + 3', tok: ['1', '<<', '2', '+', '3'], raiz: N(1, '32', N(0, ''), N(3, '5', N(2, ''), N(4, ''))),
      le: '1 << (2 + 3)', res: '32', regra: '+ (nível 6) vence << (nível 7)',
      quis: '(1 << 2) + 3', quisVal: '7', aviso: 'suggest parentheses around ‘+’ inside ‘<<’ [-Wparentheses]' },
    { txt: 'a = b = c', tok: ['a', '=', 'b', '=', 'c'], vars: 'int a = 1, b = 2, c = 3;', raiz: N(1, 'a ← 3', N(0, ''), N(3, 'b ← 3', N(2, ''), N(4, '3'))),
      le: 'a = (b = c)', res: 'a=3 b=3 c=3', regra: '= associa à DIREITA: b = c roda primeiro' },
    { txt: 'x & 1 == 0', tok: ['x', '&', '1', '==', '0'], vars: 'int x = 4;', raiz: N(1, '0', N(0, '4'), N(3, 'false', N(2, ''), N(4, ''))),
      le: 'x & (1 == 0)', res: '0 → false', regra: '== (nível 10) vence & (nível 11)',
      quis: '(x & 1) == 0', quisVal: 'true: x é par', aviso: 'suggest parentheses around comparison in operand of ‘&’ [-Wparentheses]' },
    { txt: '!m == n', tok: ['!', 'm', '==', 'n'], vars: 'int m = 2, n = 3;', raiz: N(2, 'false', N(0, 'false', N(1, '2')), N(3, '3')),
      le: '(!m) == n', res: 'false', regra: '! unário (nível 3) vence == (nível 10)',
      quis: '!(m == n)', quisVal: 'true', aviso: 'logical not is only applied to the left hand side of comparison [-Wlogical-not-parentheses]' },
    { txt: '3 > 2 > 1', tok: ['3', '>', '2', '>', '1'], raiz: N(3, 'false', N(1, 'true', N(0, ''), N(2, '')), N(4, '')),
      le: '(3 > 2) > 1', res: 'false', regra: '> associa à esquerda: true > 1 vira 1 > 1',
      quis: '3 > 2 && 2 > 1', quisVal: 'true', aviso: 'comparisons like ‘X<=Y<=Z’ do not have their mathematical meaning [-Wparentheses]' },
    { txt: '*p++', tok: ['*', 'p', '++'], vars: 'int arr[] = {10, 20, 30}; int* p = arr;', raiz: N(0, '10', N(2, 'p antigo', N(1, '→10'))),
      le: '*(p++)', res: '10, e p passa a apontar o 20', regra: '++ pós-fixo (nível 2) vence * unário (nível 3)',
      quis: '(*p)++', quisVal: 'incrementa o 10' },
  ];
  const ARV = { L: 800, H: 458, yTok: 48, y0: 158, dy: 120, meia: 32 };   // meia = metade da altura do nó
  function montarArvore(raiz, idx) {
    const d = ARVORES[idx];
    const n = d.tok.length;
    const passo = Math.min(150, 640 / Math.max(1, n - 1));
    const x0 = ARV.L / 2 - (passo * (n - 1)) / 2;
    const xTok = (i) => x0 + i * passo;
    const g = svg('g');
    const tokG = svg('g', { class: 'toks' });
    d.tok.forEach((t, i) => tokG.append(svg('text', { x: xTok(i), y: ARV.yTok, class: 'tok', 'text-anchor': 'middle', 'data-i': i, text: t })));
    const arestas = svg('g', { class: 'arestas' });
    const guias = svg('g', { class: 'guias' });
    const nos = svg('g', { class: 'nos' });
    const ordem = [];
    const visitar = (no, prof, pai) => {
      const x = xTok(no.t), y = ARV.y0 + prof * ARV.dy;
      no._x = x; no._y = y;
      no.f.forEach((f) => visitar(f, prof + 1, no));
      ordem.push(no);
      guias.append(svg('line', { x1: x, y1: ARV.yTok + 18, x2: x, y2: y - ARV.meia, class: 'guia' }));
      if (pai) {
        arestas.append(svg('line', { x1: pai._x, y1: pai._y + ARV.meia, x2: x, y2: y - ARV.meia, class: 'aresta', pathLength: 1, style: '--p:' + prof }));
      }
      const txt = d.tok[no.t];
      const larg = Math.max(74, txt.length * 22 + 36);
      const folha = !no.f.length;
      const grupo = svg('g', { class: 'no' + (folha ? ' folha' : ' op') + (pai ? '' : ' raiz'), style: '--p:' + prof + ';--dy:' + (ARV.yTok - y) + 'px' });
      grupo.append(svg('rect', { x: x - larg / 2, y: y - ARV.meia, width: larg, height: 2 * ARV.meia, rx: 16 }));
      grupo.append(svg('text', { x, y: y + 1, 'text-anchor': 'middle', 'dominant-baseline': 'central', class: 'rot', text: txt }));
      if (no.v) {
        // o valor fica à DIREITA do nó: as arestas saem por baixo e não o cruzam
        const lb = Math.max(50, no.v.length * 13.5 + 26);
        const bx = x + larg / 2 + 8, by = y - 17;
        const badge = svg('g', { class: 'val' });
        badge.append(svg('rect', { x: bx, y: by, width: lb, height: 34, rx: 17 }));
        badge.append(svg('text', { x: bx + lb / 2, y: by + 17.5, 'text-anchor': 'middle', 'dominant-baseline': 'central', text: no.v }));
        grupo.append(badge);
      }
      nos.append(grupo);
      no._g = grupo;
    };
    visitar(d.raiz, 0, null);
    g.append(guias, arestas, tokG, nos);
    raiz._svg.replaceChildren(g);
    raiz._ordem = ordem;
    // painel de leitura
    const P = raiz._painel;
    P._vars.replaceChildren(d.vars ? chip(d.vars) : el('span', { class: 'suave' }, 'só literais'));
    P._le.replaceChildren(chip(d.le, 'grande'));
    P._res.textContent = d.res;
    P._regra.textContent = d.regra;
    P._quis.replaceChildren(...(d.quis ? [chip(d.quis), el('span', { class: 'seta-txt' }, ' → '), el('b', null, d.quisVal)] : [el('span', { class: 'suave' }, '— é isso mesmo que se queria')]));
    P._quisBox.classList.toggle('vazio', !d.quis);
    P._aviso.replaceChildren(...(d.aviso ? comFlag('warning: ' + d.aviso) : ['nenhum aviso']));
    P._aviso.classList.toggle('nada', !d.aviso);
    raiz._botoes.forEach((b, j) => b.classList.toggle('selecionado', j === idx));
  }
  function estadoArvore(raiz, fase, nAval) {
    // fase 0: só tokens · 1: árvore montada · 2: avaliando (nAval nós) · 3: tudo
    raiz.dataset.fase = fase;
    (raiz._ordem || []).forEach((no, j) => no._g.classList.toggle('aval', fase >= 3 || (fase === 2 && j < nAval)));
  }
  async function tocarArvore(raiz, idx, ctx) {
    const tok = novoToken(raiz);
    const vivo = async (ms) => (await ctx.esperar(ms)) && raiz._tok === tok;
    montarArvore(raiz, idx);
    if (ctx.reduzido) { estadoArvore(raiz, 3); return; }
    estadoArvore(raiz, 0);
    void raiz.getBoundingClientRect();
    if (!(await vivo(380))) return;
    estadoArvore(raiz, 1);
    if (!(await vivo(900))) return;
    for (let j = 1; j <= raiz._ordem.length; j++) {
      estadoArvore(raiz, 2, j);
      if (!(await vivo(raiz._ordem[j - 1].f.length ? 520 : 200))) return;
    }
    estadoArvore(raiz, 3);
  }
  Deck.widget('expressoes/arvore', {
    iniciar(raiz, ctx) {
      raiz.classList.add('ex-arv');
      raiz._botoes = ARVORES.map((d, j) => el('button', {
        class: 'botao', type: 'button', onclick: () => { raiz._sel = j; tocarArvore(raiz, j, ctx); },
      }, d.txt));
      raiz._svg = svg('svg', { class: 'diagrama arv-svg', viewBox: '0 0 ' + ARV.L + ' ' + ARV.H, role: 'img', 'aria-label': 'árvore sintática da expressão' });
      const campo = (r, cls) => { const v = el('div', { class: 'v ' + (cls || '') }); return [el('div', { class: 'campo' }, rotulo(r), v), v]; };
      const [cVars, vVars] = campo('variáveis');
      const [cLe, vLe] = campo('o compilador lê');
      const [cRes, vRes] = campo('resultado', 'res');
      const [cRegra, vRegra] = campo('por quê', 'regra');
      const [cQuis, vQuis] = campo('você talvez quisesse', 'quis');
      const [cAv, vAv] = campo('g++ -Wall', 'aviso');
      raiz._painel = { _vars: vVars, _le: vLe, _res: vRes, _regra: vRegra, _quis: vQuis, _quisBox: cQuis, _aviso: vAv };
      raiz.append(
        el('div', { class: 'escolha botoes' }, raiz._botoes),
        el('div', { class: 'palco-arv' }, raiz._svg, el('div', { class: 'painel' }, cVars, cLe, cRes, cRegra, cQuis, cAv)));
      raiz._sel = 0;
    },
    passo(raiz, k, ctx, dir) {
      raiz._sel = k;
      if (animar(ctx, dir)) tocarArvore(raiz, k, ctx);
      else { novoToken(raiz); montarArvore(raiz, k); estadoArvore(raiz, 3); }
    },
    entrar(raiz, ctx) {
      if (!ctx.reduzido) tocarArvore(raiz, raiz._sel || 0, ctx);
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  categorias — lvalue / xvalue / prvalue
  // ════════════════════════════════════════════════════════════════════════════
  const CATS = [
    { e: 'x', cat: 'l', tipo: 'int&', pq: 'tem nome: dá para tomar &x' },
    { e: '42', cat: 'pr', tipo: 'int', pq: 'literal numérico: um valor puro, sem endereço' },
    { e: 'x + 1', cat: 'pr', tipo: 'int', pq: 'resultado novo, temporário' },
    { e: '"abc"', cat: 'l', tipo: 'const char(&)[4]', pq: 'literal de string é um array estático: tem endereço!' },
    { e: '++x', cat: 'l', tipo: 'int&', pq: 'pré-incremento devolve o próprio x' },
    { e: 'x++', cat: 'pr', tipo: 'int', pq: 'pós-incremento devolve uma cópia do antigo' },
    { e: 'devolve_referencia()', cat: 'l', tipo: 'int&', pq: 'função que devolve T&' },
    { e: 'devolve_valor()', cat: 'pr', tipo: 'int', pq: 'função que devolve T (por valor)' },
    { e: 'std::move(x)', cat: 'x', tipo: 'int&&', pq: 'cast para T&&: tem identidade E pode ser saqueado' },
    { e: 'devolve_rref()', decl: 'int&& devolve_rref();', cat: 'x', tipo: 'int&&', pq: 'função que devolve T&& — std::move é uma dessas' },
    { e: 'r', decl: 'int&& r = 5;', cat: 'l', tipo: 'int&', pq: 'TEM NOME → lvalue, mesmo declarado T&&' },
  ];
  const CAT_NOME = { l: 'lvalue', x: 'xvalue', pr: 'prvalue' };
  function desenharCats(raiz, feitos, atual) {
    CATS.forEach((c, j) => {
      const ok = feitos.has(j);
      c._pill.classList.toggle('feito', ok);
      c._pill.classList.toggle('atual', j === atual);
      c._chip.classList.toggle('dentro', ok);
      c._chip.classList.toggle('atual', j === atual);
    });
    const P = raiz._painel;
    raiz.dataset.cat = atual != null ? CATS[atual].cat : '';
    if (atual == null) {
      P._expr.replaceChildren(el('span', { class: 'suave' }, 'clique numa expressão (ou →)'));
      P._tipo.replaceChildren(el('span', { class: 'suave' }, 'T& → lvalue · T&& → xvalue · T → prvalue'));
      P._cat.textContent = '?';
      P._pq.textContent = 'O teste mecânico: decltype com parênteses duplos.';
      return;
    }
    const c = CATS[atual];
    P._expr.replaceChildren(...(c.decl ? [chip(c.decl, 'decl'), chip(c.e, 'grande')] : [chip(c.e, 'grande')]));
    P._tipo.replaceChildren(chip('decltype((' + c.e + '))'), el('span', { class: 'seta-txt' }, ' → '), chip(c.tipo));
    P._cat.textContent = CAT_NOME[c.cat];
    P._pq.textContent = c.pq;
  }
  Deck.widget('expressoes/categorias', {
    iniciar(raiz) {
      raiz.classList.add('ex-cats');
      const fila = el('div', { class: 'fila' });
      const regioes = {};
      ['l', 'x', 'pr'].forEach((k) => {
        regioes[k] = el('div', { class: 'regiao ' + k }, el('span', { class: 'nome-r' }, CAT_NOME[k]), el('div', { class: 'chips' }));
      });
      CATS.forEach((c, j) => {
        c._pill = el('button', { class: 'pill-e', type: 'button', onclick: () => { raiz._feitos.add(j); desenharCats(raiz, raiz._feitos, j); } }, chip(c.e));
        fila.append(c._pill);
        c._chip = el('div', { class: 'ch' }, chip(c.e));
        regioes[c.cat].querySelector('.chips').append(c._chip);
      });
      const mapa = el('div', { class: 'mapa' },
        el('div', { class: 'chave gl' }, el('span', null, 'glvalue · tem identidade'), el('i', { class: 'braco' })),
        regioes.l, regioes.x, regioes.pr,
        el('div', { class: 'chave rv' }, el('span', null, 'rvalue · pode ser saqueado'), el('i', { class: 'braco' })));
      const campo = (r, v) => el('div', { class: 'campo' }, rotulo(r), v);
      const P = { _expr: el('div', { class: 'v expr' }), _tipo: el('div', { class: 'v tipo' }), _cat: el('div', { class: 'v cat' }), _pq: el('p', { class: 'v pq' }) };
      raiz._painel = P;
      raiz.append(fila, el('div', { class: 'palco-cat' }, mapa,
        el('div', { class: 'painel' }, campo('expressão', P._expr), campo('o teste', P._tipo), campo('categoria', P._cat), P._pq)));
      raiz._feitos = new Set();
    },
    passo(raiz, k) {
      raiz._feitos = new Set(Array.from({ length: k }, (_, j) => j));
      desenharCats(raiz, raiz._feitos, k > 0 ? k - 1 : null);
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  comum — promoção + conversões aritméticas usuais ([expr.arith.conv])
  //  Algoritmo conferido contra o g++ 13.3 nos 196 pares dos 14 tipos (LP64).
  // ════════════════════════════════════════════════════════════════════════════
  const TIPOS = {
    'bool': { k: 'i', rank: 1, sinal: false, bits: 1 },
    'char': { k: 'i', rank: 2, sinal: true, bits: 8 },
    'unsigned char': { k: 'i', rank: 2, sinal: false, bits: 8 },
    'short': { k: 'i', rank: 3, sinal: true, bits: 16 },
    'unsigned short': { k: 'i', rank: 3, sinal: false, bits: 16 },
    'int': { k: 'i', rank: 4, sinal: true, bits: 32 },
    'unsigned': { k: 'i', rank: 4, sinal: false, bits: 32 },
    'long': { k: 'i', rank: 5, sinal: true, bits: 64 },
    'unsigned long': { k: 'i', rank: 5, sinal: false, bits: 64 },
    'long long': { k: 'i', rank: 6, sinal: true, bits: 64 },
    'unsigned long long': { k: 'i', rank: 6, sinal: false, bits: 64 },
    'float': { k: 'f', rank: 1 },
    'double': { k: 'f', rank: 2 },
    'long double': { k: 'f', rank: 3 },
  };
  const SEM_SINAL = { 'int': 'unsigned', 'long': 'unsigned long', 'long long': 'unsigned long long' };
  const promover = (n) => (TIPOS[n].k === 'i' && TIPOS[n].rank < 4 ? 'int' : n);
  function tipoComum(a, b) {
    const A = TIPOS[a], B = TIPOS[b];
    if (A.k === 'f' || B.k === 'f') {
      if (A.k === 'f' && B.k === 'f') return { r: A.rank >= B.rank ? a : b, regra: 1, pa: a, pb: b };
      return { r: A.k === 'f' ? a : b, regra: 1, pa: a, pb: b };
    }
    const pa = promover(a), pb = promover(b);
    if (pa === pb) return { r: pa, regra: 3, pa, pb };
    const PA = TIPOS[pa], PB = TIPOS[pb];
    if (PA.sinal === PB.sinal) return { r: PA.rank >= PB.rank ? pa : pb, regra: 4, pa, pb };
    const [u, s] = PA.sinal ? [pb, pa] : [pa, pb];
    if (TIPOS[u].rank >= TIPOS[s].rank) return { r: u, regra: 5, pa, pb };
    if (TIPOS[s].bits > TIPOS[u].bits) return { r: s, regra: 6, pa, pb };
    return { r: SEM_SINAL[s], regra: 7, pa, pb };
  }
  const ESCADA = [
    ['long double'], ['double'], ['float'],
    ['long long', 'unsigned long long'], ['long', 'unsigned long'], ['int', 'unsigned'],
    ['short', 'unsigned short'], ['char', 'unsigned char'], ['bool'],
  ];
  const REGRAS = [
    [1, 'Algum é ponto flutuante? Vence o maior ponto flutuante.'],
    [2, 'Promoção integral: bool, char e short sobem para int.'],
    [3, 'Ficaram iguais? Pronto.'],
    [4, 'Mesmo sinal? Vence o de maior rank.'],
    [5, 'O sem sinal tem rank ≥? Tudo vira SEM SINAL.'],
    [6, 'O com sinal comporta todos os valores do outro? Vira com sinal.'],
    [7, 'Senão: a versão sem sinal do tipo com sinal.'],
  ];
  const COMUM_PASSOS = [
    { a: 'char', b: 'int', ex: "'A' + 1", val: '66' },
    { a: 'short', b: 'short', ex: 's + s  // short s = 300', val: '600' },
    { a: 'int', b: 'double', ex: '1 + 2.5', val: '3.5' },
    { a: 'float', b: 'double', ex: '1.0f + 2.0', val: 'double' },
    { a: 'int', b: 'unsigned', ex: '-1 < 1u', val: 'false ⚠' },
    { a: 'long', b: 'unsigned', ex: '-1L < 1u', val: 'true' },
    { a: 'long long', b: 'unsigned long', ex: '-1LL < 1ul', val: 'false' },
  ];
  function desenharComum(raiz) {
    const { a, b, ex, val } = raiz._sel;
    const R = a && b ? tipoComum(a, b) : null;
    raiz._celulas.forEach((c, nome) => {
      c.classList.toggle('sel-a', nome === a);
      c.classList.toggle('sel-b', nome === b);
      c.classList.toggle('res', !!R && nome === R.r);
      c.classList.toggle('promovido', !!R && R.regra !== 1 && ((nome === a && R.pa !== a) || (nome === b && R.pb !== b)));
    });
    const P = raiz._painel;
    P._a.replaceChildren(a ? chip(a) : el('span', { class: 'suave' }, '—'));
    P._b.replaceChildren(b ? chip(b) : el('span', { class: 'suave' }, '—'));
    P._r.replaceChildren(R ? chip(R.r, 'grande') : el('span', { class: 'suave' }, 'clique em dois tipos'));
    P._ex.replaceChildren(...(ex ? [chip(ex), el('span', { class: 'seta-txt' }, ' → '), el('b', null, val)] : []));
    P._ex.classList.toggle('vazio', !ex);
    const promoveu = R && R.regra !== 1 && (R.pa !== a || R.pb !== b);
    raiz._regras.forEach((li, j) => {
      const n = REGRAS[j][0];
      li.classList.toggle('decide', !!R && n === R.regra);
      li.classList.toggle('aplicou', !!R && n === 2 && promoveu);
      li.classList.toggle('pulou', !!R && n !== R.regra && !(n === 2 && promoveu) && n < R.regra);
    });
  }
  Deck.widget('expressoes/comum', {
    iniciar(raiz) {
      raiz.classList.add('ex-comum');
      raiz._celulas = new Map();
      raiz._proximo = 'a';
      const esc = el('div', { class: 'escada' });
      ESCADA.forEach((linha, j) => {
        const deg = el('div', { class: 'degrau' + (linha.length === 1 ? ' unico' : '') + (j < 3 ? ' flut' : '') + (j >= 6 ? ' promo' : '') });
        linha.forEach((nome) => {
          const c = el('button', { class: 'tipo-c', type: 'button', onclick: () => {
            const s = raiz._sel;
            if (raiz._proximo === 'a') { s.a = nome; s.b = null; raiz._proximo = 'b'; } else { s.b = nome; raiz._proximo = 'a'; }
            s.ex = null; s.val = null;
            desenharComum(raiz);
          } }, nome, el('i', { class: 'mk-a' }, 'A'), el('i', { class: 'mk-b' }, 'B'));
          raiz._celulas.set(nome, c);
          deg.append(c);
        });
        esc.append(deg);
        if (j === 2) esc.append(el('div', { class: 'divisa' }, el('span', null, '↑ ponto flutuante vence qualquer inteiro')));
        if (j === 5) esc.append(el('div', { class: 'divisa promo-l' }, el('span', null, '↑ abaixo de int: promovidos a int')));
      });
      const P = { _a: el('span'), _b: el('span'), _r: el('span'), _ex: el('div', { class: 'ex' }) };
      raiz._painel = P;
      raiz._regras = REGRAS.map(([n, t]) => el('li', null, el('b', null, String(n)), el('span', null, t)));
      raiz.append(
        el('div', { class: 'col-esc' }, el('div', { class: 'eixo' }, el('span', null, 'com sinal'), el('span', null, 'sem sinal')), esc),
        el('div', { class: 'painel' },
          el('div', { class: 'conta' }, el('span', { class: 'op' }, 'A'), P._a, el('span', { class: 'op' }, '+'), el('span', { class: 'op' }, 'B'), P._b, el('span', { class: 'op' }, '→'), P._r),
          P._ex,
          el('ol', { class: 'regras limpa' }, raiz._regras),
          el('p', { class: 'mini suave' }, 'x86-64 Linux: int 32 bits; long e long long 64. Clique em dois tipos para testar.')));
      raiz._sel = { a: null, b: null };
    },
    passo(raiz, k) {
      const p = k > 0 ? COMUM_PASSOS[k - 1] : null;
      raiz._sel = p ? { ...p } : { a: null, b: null };
      raiz._proximo = 'a';
      desenharComum(raiz);
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  sinal — o odômetro de 32 bits (esquemático: perto do 0 há zoom)
  // ════════════════════════════════════════════════════════════════════════════
  const ODO = { cx: 270, cy: 236, r: 150, rU: 188, rS: 104 };
  const ODO_PONTOS = [
    { v: 0, ang: 0, u: '0', s: '0' },
    { v: 1, ang: 28, u: '1', s: '1' },
    { v: 2, ang: 50, u: '2', s: '2' },
    { v: 3, ang: 72, u: '3', s: '3' },
    { v: null, ang: 162, u: '2³¹−1', s: '2³¹−1' },
    { v: null, ang: 198, u: '2³¹', s: '−2³¹' },
    { v: -3, ang: 288, u: '2³²−3', s: '−3' },
    { v: -2, ang: 310, u: '2³²−2', s: '−2' },
    { v: -1, ang: 332, u: '4294967295', s: '−1' },
  ];
  const angDe = (v) => ODO_PONTOS.find((p) => p.v === v).ang;
  const polar = (r, ang) => [ODO.cx + r * Math.sin(ang * Math.PI / 180), ODO.cy - r * Math.cos(ang * Math.PI / 180)];
  function arco(r, a0, a1) {
    const [x0, y0] = polar(r, a0), [x1, y1] = polar(r, a1);
    const varre = ((a1 - a0) % 360 + 360) % 360;
    return `M ${x0.toFixed(1)} ${y0.toFixed(1)} A ${r} ${r} 0 ${varre > 180 ? 1 : 0} 1 ${x1.toFixed(1)} ${y1.toFixed(1)}`;
  }
  function montarOdometro(raiz) {
    const s = svg('svg', { class: 'diagrama odo', viewBox: '0 0 540 470', role: 'img', 'aria-label': 'odômetro de 32 bits: os mesmos bits lidos como int e como unsigned' });
    s.append(svg('circle', { cx: ODO.cx, cy: ODO.cy, r: ODO.r, class: 'anel' }));
    [117, 243].forEach((a) => {
      [-6, 0, 6].forEach((d) => { const [x, y] = polar(ODO.r, a + d); s.append(svg('circle', { cx: x, cy: y, r: 3, class: 'reticencia' })); });
    });
    raiz._arcoI = svg('path', { d: arco(ODO.r - 21, 200, 160), class: 'ordem ordem-i', pathLength: 1 });
    raiz._arcoU = svg('path', { d: arco(ODO.r + 18, 4, 352), class: 'ordem ordem-u', pathLength: 1 });
    // pontas de seta à parte (um marker-end apareceria antes de o arco terminar de ser desenhado)
    const ponta = (r, ang, cls) => {
      const [x, y] = polar(r, ang);
      const rad = ang * Math.PI / 180;
      const t = [Math.cos(rad), Math.sin(rad)], n = [Math.sin(rad), -Math.cos(rad)];
      const pt = (a, b) => `${(x + a * t[0] + b * n[0]).toFixed(1)},${(y + a * t[1] + b * n[1]).toFixed(1)}`;
      return svg('polygon', { points: [pt(10, 0), pt(-6, 8), pt(-6, -8)].join(' '), class: 'ponta ' + cls });
    };
    s.append(svg('text', { x: polar(ODO.r + 30, 8)[0] + 4, y: polar(ODO.r + 30, 8)[1] - 4, class: 'ini-t ini-u', 'text-anchor': 'start', text: 'unsigned começa no 0' }));
    s.append(raiz._arcoI, raiz._arcoU, ponta(ODO.r - 21, 160, 'ponta-i'), ponta(ODO.r + 18, 352, 'ponta-u'));
    ODO_PONTOS.forEach((p) => {
      const [x1, y1] = polar(ODO.r - 9, p.ang), [x2, y2] = polar(ODO.r + 9, p.ang);
      s.append(svg('line', { x1, y1, x2, y2, class: 'tique' }));
      const sin = Math.sin(p.ang * Math.PI / 180);
      const anc = (inv) => (Math.abs(sin) < 0.35 ? 'middle' : (sin > 0) !== inv ? 'start' : 'end');
      const [ux, uy] = polar(ODO.rU, p.ang);
      const [sx, sy] = polar(ODO.rS, p.ang);
      s.append(svg('text', { x: ux, y: uy, class: 'lb-u' + (p.v === -1 ? ' grande' : ''), 'text-anchor': anc(false), 'dominant-baseline': 'central', text: p.u }));
      s.append(svg('text', { x: sx, y: sy, class: 'lb-s', 'text-anchor': anc(true), 'dominant-baseline': 'central', text: p.s }));
    });
    s.append(svg('text', { x: ODO.cx, y: ODO.cy - 12, class: 'centro-t', 'text-anchor': 'middle', text: 'os MESMOS' }));
    s.append(svg('text', { x: ODO.cx, y: ODO.cy + 14, class: 'centro-t', 'text-anchor': 'middle', text: '32 bits' }));
    const marcador = (q) => {
      const g = svg('g', { class: 'mk mk-' + q });
      g.append(svg('circle', { r: 15 }));
      g.append(svg('text', { 'text-anchor': 'middle', 'dominant-baseline': 'central', text: q }));
      s.append(g);
      return g;
    };
    raiz._mkA = marcador('a');
    raiz._mkB = marcador('b');
    return s;
  }
  function posMarcador(g, ang) {
    const [x, y] = polar(ODO.r, ang);
    g.setAttribute('transform', `translate(${x.toFixed(1)} ${y.toFixed(1)})`);
  }
  function desenharSinal(raiz, k) {
    const a = raiz._a;
    posMarcador(raiz._mkA, angDe(a));
    posMarcador(raiz._mkB, angDe(1));
    raiz.dataset.k = k;
    const P = raiz._p;
    const comoInt = a < 1;
    const ua = (a >>> 0);
    const comoUns = ua < 1;
    const valor = (linha, v) => { const b = linha.querySelector('b'); b.textContent = String(v); b.className = v ? 'v-true' : 'v-false'; };
    valor(P._l1, comoInt);
    P._l1.querySelector('.conta').textContent = a + ' < 1';
    P._l1.classList.toggle('on', k >= 1);
    P._l2.querySelector('.conta').textContent = ua + ' < 1';
    valor(P._l2, comoUns);
    P._l2.classList.toggle('on', k >= 2);
    P._l2.classList.toggle('errado', k >= 2 && comoInt !== comoUns);
    valor(P._l3, comoInt);
    P._l3.classList.toggle('on', k >= 3);
    P._aviso.classList.toggle('on', k >= 2);
    P._botoes.forEach((b) => b.classList.toggle('selecionado', +b.dataset.v === a));
    P._decl.replaceChildren(chip('int a = ' + a + ';  unsigned b = 1;  a < b'));
  }
  Deck.widget('expressoes/sinal', {
    iniciar(raiz) {
      raiz.classList.add('ex-sinal');
      const term = raiz.querySelector('figure.terminal');
      raiz._a = -1;
      const odo = montarOdometro(raiz);
      const linha = (txt, cls) => el('div', { class: 'res-l ' + cls }, el('span', { class: 'q' }, txt), el('span', { class: 'conta mono' }), el('b'));
      const P = {
        _decl: el('div', { class: 'decl' }),
        _l1: linha('lido como int (de −2³¹ para cima):', 'l1'),
        _l2: linha('de verdade — a vira unsigned:', 'l2'),
        _l3: linha('std::cmp_less(a, b) · C++20:', 'l3'),
        _aviso: el('p', { class: 'aviso-g' }, ...comFlag('warning: comparison of integer expressions of different signedness: ‘int’ and ‘unsigned int’ [-Wsign-compare]')),
      };
      P._botoes = [-3, -2, -1, 0, 1, 2, 3].map((v) => el('button', {
        class: 'botao', type: 'button', 'data-v': v,
        onclick: () => { raiz._a = v; desenharSinal(raiz, Math.max(3, raiz._k || 0)); },
      }, String(v)));
      raiz._p = P;
      const painel = el('div', { class: 'painel' }, P._decl, P._l1, P._l2, P._l3, P._aviso);
      if (term) painel.append(term);
      raiz.append(el('div', { class: 'col-odo' }, odo,
        el('p', { class: 'legenda' }, el('span', { class: 'lg-s' }, '● lido como int (dentro)'), el('span', { class: 'lg-u' }, '● lido como unsigned (fora)')),
        el('div', { class: 'escolhe' }, rotulo('a ='), el('div', { class: 'botoes' }, P._botoes))), painel);
    },
    passo(raiz, k) {
      raiz._a = -1;
      raiz._k = k;
      desenharSinal(raiz, k);
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  float — perto de 2^24 só sobram os pares
  // ════════════════════════════════════════════════════════════════════════════
  const F_INI = 16777210, F_FIM = 16777224, F_2_24 = 16777216;
  const F_PASSOS = [16777215, 16777216, 16777217, 16777218, 16777219];
  const fbits = (x) => { const dv = new DataView(new ArrayBuffer(4)); dv.setFloat32(0, x); return dv.getUint32(0); };
  const sepMil = (n) => String(n).replace(/\B(?=(\d{3})+(?!\d))/g, "'");
  function desenharFloat(raiz) {
    const n = raiz._n;
    const f = Math.fround(n);
    const L = raiz._L;
    const xDe = (v) => L.x0 + (v - F_INI) * L.dx;
    // pontos representáveis
    raiz._tiques.forEach((t) => {
      const v = +t.dataset.v;
      t.classList.toggle('alvo', v === f);
      t.classList.toggle('n', v === n);
    });
    raiz._pino.setAttribute('transform', `translate(${xDe(n).toFixed(1)} 0)`);
    const desvia = f !== n;
    raiz._curva.setAttribute('d', `M ${xDe(n).toFixed(1)} 92 Q ${((xDe(n) + xDe(f)) / 2).toFixed(1)} 40 ${xDe(f).toFixed(1)} 92`);
    raiz._curva.classList.toggle('on', desvia);
    // bits do float
    const b = fbits(n);
    raiz._fb.forEach((c, j) => {
      const on = (b >>> (31 - j)) & 1;
      c.textContent = String(on);
      c.classList.toggle('on', !!on);
    });
    const exp = (b >>> 23) & 0xff;
    raiz._exp.textContent = 'expoente ' + exp + ' − 127 = ' + (exp - 127);
    raiz._nTxt.textContent = sepMil(n);
    const P = raiz._p;
    P._decl.replaceChildren(chip('float f = ' + sepMil(n) + ';'), el('span', { class: 'seta-txt' }, ' → '), el('b', { class: desvia ? 'erro-txt' : 'ok-txt' }, String(f)));
    let txt;
    if (n <= F_2_24) txt = n === F_2_24 ? '2²⁴ exato: o último ponto em que TODO inteiro cabe.' : 'exato: abaixo de 2²⁴ todo inteiro cabe nos 24 bits.';
    else if (!desvia) txt = 'par: cabe — o último bit da mantissa agora vale 2.';
    else txt = 'ímpar: fica no MEIO de dois floats → empate → vai para o PAR (mantissa terminando em 0).';
    P._txt.textContent = txt;
    P._erro.textContent = desvia ? 'erro: ' + (f - n > 0 ? '+' : '') + (f - n) : 'erro: 0';
    P._erro.classList.toggle('ruim', desvia);
  }
  Deck.widget('expressoes/float', {
    iniciar(raiz) {
      raiz.classList.add('ex-float');
      raiz._n = F_PASSOS[0];
      // bits: 1 sinal + 8 expoente + 23 mantissa
      const bitsBox = el('div', { class: 'fbits' });
      raiz._fb = [];
      for (let j = 0; j < 32; j++) {
        const c = el('b', { class: j === 0 ? 's' : j < 9 ? 'e' : 'm' }, '0');
        raiz._fb.push(c);
        bitsBox.append(c);
        if (j === 0 || j === 8) bitsBox.append(el('span', { class: 'sep' }));
      }
      raiz._exp = el('span', { class: 'mono' });
      const legenda = el('div', { class: 'leg' },
        el('span', { class: 's' }, '1 sinal'), el('span', { class: 'e' }, '8 expoente'), el('span', { class: 'm' }, '23 mantissa (+1 implícito = 24 bits)'), raiz._exp);
      // reta numérica
      const L = { x0: 46, dx: (1140 - 92) / (F_FIM - F_INI) };
      raiz._L = L;
      const s = svg('svg', { class: 'diagrama reta', viewBox: '0 0 1140 200', role: 'img', 'aria-label': 'inteiros perto de 2^24 e os floats que existem' });
      s.append(svg('line', { x1: 20, y1: 120, x2: 1120, y2: 120, class: 'eixo' }));
      const xz = L.x0 + (F_2_24 - F_INI) * L.dx;
      s.append(svg('rect', { x: xz, y: 70, width: 1120 - xz, height: 88, class: 'zona' }));
      // a fronteira 2^24: liga o rótulo lá de cima à borda da zona
      s.append(svg('line', { x1: xz, y1: 34, x2: xz, y2: 158, class: 'marco-l' }));
      s.append(svg('text', { x: xz + 10, y: 178, class: 'zona-t', text: 'acima de 2²⁴: só os pares' }));
      s.append(svg('text', { x: xz - 10, y: 178, class: 'zona-t', 'text-anchor': 'end', text: 'todo inteiro cabe' }));
      raiz._tiques = [];
      for (let v = F_INI; v <= F_FIM; v++) {
        const x = L.x0 + (v - F_INI) * L.dx;
        const existe = Math.fround(v) === v;
        const g = svg('g', { class: 'tq' + (existe ? ' existe' : ' falta'), 'data-v': v });
        g.append(svg('line', { x1: x, y1: 110, x2: x, y2: 130 }));
        g.append(svg('circle', { cx: x, cy: 92, r: existe ? 9 : 7 }));
        g.append(svg('text', { x, y: 152, 'text-anchor': 'middle', text: String(v).slice(-3) }));
        if (v === F_2_24) g.append(svg('text', { x, y: 24, 'text-anchor': 'middle', class: 'marco', text: '2²⁴' }));
        s.append(g);
        raiz._tiques.push(g);
      }
      raiz._curva = svg('path', { class: 'curva', d: 'M 0 0' });
      s.append(raiz._curva);
      raiz._pino = svg('g', { class: 'pino' });
      raiz._pino.append(svg('path', { d: 'M -10 54 L 10 54 L 0 70 z' }));
      s.append(raiz._pino);
      raiz._nTxt = el('b', { class: 'n-txt' });
      const P = { _decl: el('div', { class: 'decl' }), _txt: el('p', { class: 'txt' }), _erro: el('span', { class: 'erro-v' }) };
      raiz._p = P;
      const muda = (d) => () => { raiz._n = Math.min(F_FIM, Math.max(F_INI, raiz._n + d)); desenharFloat(raiz); };
      raiz.append(
        el('div', { class: 'cabeca' }, rotulo('os 32 bits de float(n)'), bitsBox, legenda),
        el('div', { class: 'reta-box' }, el('div', { class: 'prefixo-h' }, rotulo('n (int) — 16\'777\'…'), el('div', { class: 'botoes' },
          el('button', { class: 'botao', type: 'button', onclick: muda(-1) }, '− 1'), raiz._nTxt, el('button', { class: 'botao', type: 'button', onclick: muda(1) }, '+ 1'))), s),
        el('div', { class: 'rodape-f' }, P._decl, P._erro, P._txt));
    },
    passo(raiz, k) {
      raiz._n = F_PASSOS[k];
      desenharFloat(raiz);
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  castc — as 5 tentativas do cast estilo C ([expr.cast]/4)
  //  Cada caso conferido com concepts + static_assert no g++ 13.3.
  // ════════════════════════════════════════════════════════════════════════════
  const TENTATIVAS = ['const_cast', 'static_cast', 'static_cast + const_cast', 'reinterpret_cast', 'reinterpret_cast + const_cast'];
  const CASOS_C = [
    { decl: 'const int ci = 1;', expr: '(int*)&ci', vence: 0, motivos: [],
      eq: 'const_cast<int*>(&ci)', nota: 'só tira o const. Ler é ok; escrever é UB — ci nasceu const.' },
    { decl: 'double d = 3.7;', expr: '(int)d', vence: 1, motivos: ['não converte valores'],
      eq: 'static_cast<int>(d)  // 3', nota: 'o caso inofensivo: uma conversão numérica.' },
    { decl: 'Base* pb = &outra;', expr: '(Derivada*)pb', vence: 1, motivos: ['tipos diferentes'],
      eq: 'static_cast<Derivada*>(pb)', nota: '⚠ desce SEM conferir: o dynamic_cast daria nullptr; o cast C devolveu um ponteiro para o objeto errado (medido).' },
    { decl: 'const Base* cpb = &der;', expr: '(Derivada*)cpb', vence: 2, motivos: ['tipos diferentes', 'static_cast não tira const'],
      eq: 'static_cast<Derivada*>(\n    const_cast<Base*>(cpb))', nota: 'desce E tira o const — sem avisar nenhum dos dois.' },
    { decl: 'double d = 3.7;', expr: '(int*)&d', vence: 3, motivos: ['tipos diferentes', 'double* e int* não se relacionam', 'idem'],
      eq: 'reinterpret_cast<int*>(&d)', nota: 'compilou calado — e ler *p é UB (strict aliasing). Para ver os bits: std::bit_cast (C++20).' },
    { decl: 'const double cd = 2.5;', expr: '(int*)&cd', vence: 4, motivos: ['tipos diferentes', 'não se relacionam', 'idem', 'reinterpret não tira const'],
      eq: 'reinterpret_cast<int*>(\n    const_cast<double*>(&cd))', nota: 'o pior caso: reinterpreta os bits E tira o const.' },
  ];
  function desenharCastc(raiz, idx, ate) {
    // ate: quantas tentativas já foram "testadas" (null = todas até a vencedora)
    const c = CASOS_C[idx];
    const lim = ate == null ? c.vence + 1 : ate;
    raiz._exs.forEach((b, j) => b.classList.toggle('selecionado', j === idx));
    raiz._caixas.forEach((cx, j) => {
      const testada = j < lim && j <= c.vence;
      cx.classList.toggle('falhou', testada && j < c.vence);
      cx.classList.toggle('venceu', testada && j === c.vence);
      cx.classList.toggle('testando', ate != null && j === lim - 1);
      cx.classList.toggle('fora', j > c.vence);
      cx.querySelector('.motivo').textContent = testada ? (j < c.vence ? '✗ ' + c.motivos[j] : '✓ funciona — vence') : '';
    });
    const fim = ate == null || lim > c.vence;
    raiz._res.classList.toggle('on', fim);
    raiz._resExpr.replaceChildren(chip(c.decl), chip(c.expr, 'grande'));
    raiz._resEq.replaceChildren(fim ? Deck.util.codigo(c.eq, { classe: 'compacto' }) : el('span'));
    raiz._resNota.textContent = fim ? c.nota : '';
    raiz._resNota.classList.toggle('alerta', fim && /⚠|UB|pior/.test(c.nota));
  }
  async function tocarCastc(raiz, idx, ctx) {
    const tok = novoToken(raiz);
    if (ctx.reduzido) { desenharCastc(raiz, idx); return; }
    const c = CASOS_C[idx];
    for (let j = 1; j <= c.vence + 1; j++) {
      desenharCastc(raiz, idx, j);
      if (!(await ctx.esperar(j <= c.vence ? 560 : 420)) || raiz._tok !== tok) return;
    }
    desenharCastc(raiz, idx);
  }
  Deck.widget('expressoes/castc', {
    iniciar(raiz, ctx) {
      raiz.classList.add('ex-castc');
      raiz._exs = CASOS_C.map((c, j) => el('button', { class: 'ex-b', type: 'button', onclick: () => { raiz._sel = j; tocarCastc(raiz, j, ctx); } },
        el('span', { class: 'decl' }, c.decl), chip(c.expr)));
      raiz._caixas = TENTATIVAS.map((t, j) => el('div', { class: 'tent' }, el('b', { class: 'num' }, String(j + 1)), chip(t), el('span', { class: 'motivo' })));
      raiz._resExpr = el('div', { class: 'r-expr' });
      raiz._resEq = el('div', { class: 'r-eq' });
      raiz._resNota = el('p', { class: 'r-nota' });
      raiz._res = el('div', { class: 'resultado' }, rotulo('o cast C virou'), raiz._resExpr, el('div', { class: 'virou' }, '≡'), raiz._resEq, raiz._resNota);
      raiz.append(
        el('div', { class: 'exemplos' }, rotulo('exemplos'), raiz._exs),
        el('div', { class: 'funil' }, rotulo('o que (T)x tenta, em ordem'), raiz._caixas,
          el('div', { class: 'tent nunca' }, el('b', { class: 'num' }, '✗'), chip('dynamic_cast'), el('span', { class: 'motivo' }, 'nunca é tentado — sem checagem em runtime'))),
        raiz._res);
      raiz._sel = 0;
    },
    passo(raiz, k, ctx, dir) {
      raiz._sel = k;
      if (animar(ctx, dir)) { desenharCastc(raiz, k, 0); tocarCastc(raiz, k, ctx); } else { novoToken(raiz); desenharCastc(raiz, k); }
    },
    entrar(raiz, ctx) {
      if (!ctx.reduzido) tocarCastc(raiz, raiz._sel || 0, ctx);
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  padding — layout de struct no x86-64 (conferido com sizeof/offsetof do g++
  //  em 1364 structs de 1 a 5 membros)
  // ════════════════════════════════════════════════════════════════════════════
  const TAM = { char: 1, short: 2, int: 4, double: 8 };
  const PRESETS = [
    { rot: 'Alinhado', nome: 'Alinhado', m: [['char', 'c'], ['double', 'd']] },
    { rot: '{char, double, char}', nome: 'S', m: [['char', 'a'], ['double', 'd'], ['char', 'b']] },
    { rot: '{double, char, char}', nome: 'S', m: [['double', 'd'], ['char', 'a'], ['char', 'b']] },
    { rot: '5 membros', nome: 'S', m: [['char', 'a'], ['double', 'd'], ['char', 'b'], ['int', 'i'], ['short', 's']] },
    { rot: '5, ordenados', nome: 'S', m: [['double', 'd'], ['int', 'i'], ['short', 's'], ['char', 'a'], ['char', 'b']] },
  ];
  function layout(membros) {
    let off = 0, al = 1;
    const pos = membros.map(([t]) => {
      const a = TAM[t];
      off = Math.ceil(off / a) * a;
      const p = off;
      off += TAM[t];
      al = Math.max(al, a);
      return p;
    });
    return { pos, tam: Math.ceil(off / al) * al, al };
  }
  /** el() com uma custom property --cm (cor do membro) */
  function comCor(no, n, raiz) { no.style.setProperty('--cm', 'var(--ex-m' + raiz._cores.get(n) + ')'); return no; }
  /** reordenou à mão: a struct já não é nenhum preset (nem se chama Alinhado) */
  function editouPad(raiz) {
    raiz._nome = 'S';
    raiz._pres.forEach((b) => b.classList.remove('selecionado'));
    desenharPad(raiz);
  }
  function desenharPad(raiz) {
    const m = raiz._m;
    const L = layout(m);
    // editor
    raiz._lista.replaceChildren(...m.map(([t, n], j) => comCor(el('div', { class: 'membro' },
      el('i', { class: 'cor' }),
      chip(t + ' ' + n + ';'),
      el('span', { class: 'meta' }, 'offset ' + L.pos[j]),
      el('button', { class: 'mv', type: 'button', title: 'subir', disabled: j === 0, onclick: () => { [m[j - 1], m[j]] = [m[j], m[j - 1]]; editouPad(raiz); } }, '▲'),
      el('button', { class: 'mv', type: 'button', title: 'descer', disabled: j === m.length - 1, onclick: () => { [m[j + 1], m[j]] = [m[j], m[j + 1]]; editouPad(raiz); } }, '▼')), n, raiz)));
    raiz._cab.replaceChildren(chip('struct ' + raiz._nome + ' {'));
    // memória: linhas de 8 bytes
    const dono = new Array(L.tam).fill(-1);
    m.forEach(([t], j) => { for (let b = 0; b < TAM[t]; b++) dono[L.pos[j] + b] = j; });
    const linhas = [];
    for (let r = 0; r < L.tam; r += 8) {
      const cels = [];
      for (let b = r; b < r + 8; b++) {
        const j = dono[b];
        if (j < 0) cels.push(el('div', { class: 'cel pad' }, 'pad'));
        else {
          const [, n] = m[j];
          const ini = b === L.pos[j];
          cels.push(comCor(el('div', { class: 'cel dado' + (ini ? ' ini' : '') }, ini ? n : ''), n, raiz));
        }
      }
      linhas.push(el('div', { class: 'lm' }, el('span', { class: 'off' }, '+' + r), el('div', { class: 'cels' }, cels)));
    }
    raiz._mem.replaceChildren(...linhas);
    const pad = dono.filter((d) => d < 0).length;
    raiz._tot.replaceChildren(
      el('div', { class: 't' }, rotulo('sizeof'), el('b', null, String(L.tam))),
      el('div', { class: 't' }, rotulo('alignof'), el('b', null, String(L.al))),
      el('div', { class: 't pad-t' }, rotulo('padding'), el('b', null, pad + ' B'), el('small', null, Math.round((pad / L.tam) * 100) + '%')));
    raiz.style.setProperty('--linhas', L.tam / 8);
  }
  Deck.widget('expressoes/padding', {
    iniciar(raiz) {
      raiz.classList.add('ex-pad');
      raiz._cores = new Map([['c', 1], ['d', 2], ['a', 1], ['b', 3], ['i', 4], ['s', 5]]);
      raiz._lista = el('div', { class: 'lista' });
      raiz._cab = el('div', { class: 'cab' });
      raiz._mem = el('div', { class: 'memoria' });
      raiz._tot = el('div', { class: 'totais' });
      raiz._pres = PRESETS.map((p, j) => el('button', { class: 'botao', type: 'button', onclick: () => { raiz._m = p.m.map((x) => x.slice()); raiz._nome = p.nome; raiz._pres.forEach((b, i) => b.classList.toggle('selecionado', i === j)); desenharPad(raiz); } }, p.rot));
      raiz.append(
        el('div', { class: 'editor' }, raiz._cab, raiz._lista, chip('};'),
          el('button', { class: 'botao primario ordena', type: 'button', onclick: () => {
            raiz._m.sort((x, y) => TAM[y[0]] - TAM[x[0]]); editouPad(raiz);
          } }, 'ordenar: maior alinhamento primeiro')),
        // os totais ficam ACIMA da memória: não pulam quando a struct ganha ou perde linhas
        el('div', { class: 'direita' }, el('div', { class: 'botoes presets' }, raiz._pres), raiz._tot, el('div', { class: 'mem-box' }, rotulo('a memória, 8 bytes por linha'), raiz._mem)));
    },
    passo(raiz, k) {
      const p = PRESETS[k];
      raiz._m = p.m.map((x) => x.slice());
      raiz._nome = p.nome;
      raiz._pres.forEach((b, i) => b.classList.toggle('selecionado', i === k));
      desenharPad(raiz);
    },
  });

  // ════════════════════════════════════════════════════════════════════════════
  //  vazamento — new cru + exceção × make_unique
  // ════════════════════════════════════════════════════════════════════════════
  const VAZ_COD = [
    'struct T { int d[4]; };   // 16 bytes',
    'void cru() {',
    '    T* p = new T;',
    '    f();              // lança!',
    '    delete p;         // ✗ nunca roda',
    '}',
    'void moderno() {',
    '    auto p = std::make_unique<T>();',
    '    f();              // lança!',
    '}   // ✓ ~unique_ptr libera no unwinding',
  ].join('\n');
  const VAZ_FOCO = [null, [3], [4], [5], [8, 9], [10]];
  Deck.widget('expressoes/vazamento', {
    iniciar(raiz) {
      raiz.classList.add('ex-vaz');
      raiz._fig = Deck.util.codigo(VAZ_COD, { titulo: 'vazamento.cpp', classe: 'compacto' });
      const quadro = (cls, nome, ...filhos) => el('div', { class: 'quadro-v ' + cls }, el('span', { class: 'qn' }, nome), ...filhos);
      raiz._qMain = quadro('q-main', 'main()');
      raiz._pVar = el('span', { class: 'pvar' }, chip('T* p'));
      raiz._qFun = quadro('q-fun', 'cru()', raiz._pVar);
      raiz._qF = quadro('q-f', 'f()', el('span', { class: 'throw' }, 'throw'));
      // os 16 bytes de T = int d[4]; new T não inicializa: valores indeterminados (?)
      raiz._bloco = el('div', { class: 'bloco' },
        el('div', { class: 'cab-b' }, el('b', null, 'T'), el('span', { class: 'tam' }, '16 bytes'), el('span', { class: 'estado' })),
        el('div', { class: 'ints' }, [0, 1, 2, 3].map((j) => el('i', { title: 'd[' + j + ']' }, '?'))));
      const seta = svg('svg', { class: 'seta-p', viewBox: '0 0 200 60', preserveAspectRatio: 'none' });
      const defs = svg('defs');
      const mk = svg('marker', { id: 'expressoes-ponta-vaz', viewBox: '0 0 10 10', refX: 8, refY: 5, markerWidth: 7, markerHeight: 7, orient: 'auto' });
      mk.append(svg('path', { d: 'M0 0 L10 5 L0 10 z', class: 'ponta' }));
      defs.append(mk);
      seta.append(defs, svg('path', { d: 'M 4 30 C 70 30 120 30 192 30', class: 'linha', 'marker-end': 'url(#expressoes-ponta-vaz)' }));
      raiz._seta = seta;
      raiz._status = el('div', { class: 'status' });
      // grade da cena: os quadros da pilha têm linhas FIXAS (f, função, main), e a seta e o
      // bloco do heap moram na linha do quadro da função — alinhamento exato, sem pulos
      const lp = rotulo('pilha'), lh = rotulo('heap');
      lp.classList.add('lbl-p'); lh.classList.add('lbl-h');
      raiz.append(raiz._fig,
        el('div', { class: 'cena' }, lp, lh, raiz._qF, raiz._qFun, raiz._qMain,
          el('div', { class: 'col-seta' }, seta), raiz._bloco, raiz._status));
    },
    passo(raiz, k) {
      raiz.dataset.k = k;
      focar(raiz._fig, VAZ_FOCO[k]);
      const moderno = k >= 4;
      raiz._qFun.querySelector('.qn').textContent = moderno ? 'moderno()' : 'cru()';
      raiz._pVar.replaceChildren(chip(moderno ? 'unique_ptr<T> p' : 'T* p'));
      raiz._qFun.classList.toggle('on', (k >= 1 && k <= 2) || k === 4);
      raiz._qFun.classList.toggle('saindo', k === 3 || k === 5);
      raiz._qF.classList.toggle('on', k === 2 || k === 4);
      raiz._qF.classList.toggle('saindo', k === 3 || k === 5);
      raiz._bloco.classList.toggle('on', k >= 1);
      raiz._bloco.classList.toggle('orfao', k === 3);
      raiz._bloco.classList.toggle('livre', k === 5);
      raiz._bloco.querySelector('.estado').textContent = k === 3 ? 'órfão: ninguém aponta' : k === 5 ? 'liberado ✓' : '';
      raiz._seta.classList.toggle('on', (k >= 1 && k <= 2) || k === 4);
      raiz._status.className = 'status' + (k === 3 ? ' ruim' : k === 5 ? ' bom' : '');
      raiz._status.replaceChildren(...(k === 3
        ? [rotulo('g++ -fsanitize=address — saída medida'), el('span', { class: 'mono' }, 'ERROR: LeakSanitizer: detected memory leaks'), el('span', { class: 'mono' }, 'Direct leak of 16 byte(s) in 1 object(s)')]
        : k === 5 ? [rotulo('g++ -fsanitize=address — medido'), el('span', { class: 'ok-txt' }, 'o LeakSanitizer não relata nada: 0 vazamentos'), el('span', null, '~unique_ptr() roda durante o unwinding e chama delete (RAII)')]
          : k === 0 ? [el('span', { class: 'suave' }, 'avance para chamar cru()')]
            : k === 4 ? [el('span', null, 'mesma exceção, agora com make_unique…')]
              : [el('span', null, k === 1 ? 'new: aloca no heap; p guarda o endereço' : 'f() vai lançar…')]));
    },
  });
})();
