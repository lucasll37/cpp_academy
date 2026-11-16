/* =============================================================================
   widgets/controle.js — Capítulo 2 · Controle de fluxo (demo_controle.hpp)

   Todos os widgets seguem o contrato do motor: passo(k) desenha o ESTADO k do
   zero (idempotente). Quando o passo chega para a frente, uma animação curta
   leva do estado k-1 ao k; voltar (←) ou ?estatico desenham o estado final.

     controle/cadeia      2.1  a cadeia else-if, avaliada de cima para baixo
     controle/gotofail    2.1  o goto fail; da Apple: o fluxo que escapa
     controle/escopo      2.1  a vida do nome: antes × init-statement (C++17)
     controle/constexpr   2.2  if constexpr: escolha T, veja o galho descartado
     controle/switch      2.3  o switch como salto para rótulo (queda + log)
     controle/lacos       2.4  while / do-while / for, cláusula por cláusula
     controle/aninhado    2.4  laços aninhados: continue e break numa grade
     controle/desacucar   2.5  a reescrita do range-for, peça por peça
     controle/busca       2.6  sair de dois laços: goto × break × return
     controle/curto       2.7  curto-circuito, operando por operando
   ============================================================================= */
(function () {
  'use strict';
  const { el, svg } = Deck.util;

  // ── utilitários ──────────────────────────────────────────────────────────
  const realcar = (txt) => Deck.util.realcar(txt, 'cpp');
  // trecho de linha: Deck.util.realcar remove a indentação comum (" : " viraria ": ");
  // aqui os espaços das pontas são preservados
  const realcarCru = (txt) => {
    const ini = /^\s*/.exec(txt)[0], fim = /\s*$/.exec(txt.slice(ini.length))[0];
    const meio = txt.slice(ini.length, txt.length - fim.length);
    return ini + (meio ? realcar(meio) : '') + fim;
  };

  // Animar só quando faz sentido: passo para a frente, slide visível, sem
  // movimento reduzido e fora da impressão (Ctrl+P aplica o último passo em
  // todos os slides de uma vez — ali o estado tem de ser o final, estático).
  let imprimindo = false;
  window.addEventListener('beforeprint', () => { imprimindo = true; });
  window.addEventListener('afterprint', () => { imprimindo = false; });
  const podeAnimar = (ctx) => !ctx.reduzido && ctx.ativo && !imprimindo;

  // posição de `no` relativa a `base` pelo layout (ignora transform/escala)
  function pos(no, base) {
    let x = 0, y = 0, n = no;
    while (n && n !== base) { x += n.offsetLeft; y += n.offsetTop; n = n.offsetParent; }
    return { x, y, w: no.offsetWidth, h: no.offsetHeight };
  }

  function botao(rotulo, acao, classe) {
    const b = el('button', { class: 'botao' + (classe ? ' ' + classe : ''), type: 'button', html: rotulo });
    b.addEventListener('click', (e) => { e.stopPropagation(); acao(e); b.blur(); });
    return b;
  }

  // marca da linha como o motor faz: ✗ vermelho · ✓ verde · ⚠ âmbar (no comentário)
  function marcaDe(txt) {
    const c = txt.indexOf('//');
    const com = c >= 0 ? txt.slice(c) : '';
    return /✗/.test(com) ? 'ruim' : /✓/.test(com) ? 'bom' : /⚠/.test(com) ? 'aviso' : '';
  }

  // linha de código realçada, com a indentação num <span> separado (anima o recuo)
  function linha(txt, classe) {
    const m = /^ */.exec(txt)[0].length;
    const ln = el('div', { class: ['lw', classe || '', marcaDe(txt)].join(' ').trim() });
    const ind = el('span', { class: 'ind' });
    ind.style.setProperty('--ind', m);
    const cod = el('span', { class: 'cod', html: realcar(txt.slice(m)) || '&#8203;' });
    ln.append(ind, cod);
    ln._cod = cod;
    return ln;
  }

  // lista que cresce durante a animação: mantém o prefixo que não mudou (sem
  // reanimar as linhas antigas), remove o resto e acrescenta só as novas
  function sincronizar(cont, itens, criar) {
    const filhos = Array.from(cont.children);
    let i = 0;
    while (i < itens.length && i < filhos.length && filhos[i].dataset.chave === itens[i].chave) i++;
    filhos.slice(i).forEach((f) => f.remove());
    for (let j = i; j < itens.length; j++) {
      const n = criar(itens[j], j);
      n.dataset.chave = itens[j].chave;
      cont.append(n);
    }
  }

  // flecha SVG com ponta (marker com fill explícito via currentColor)
  function camadaSvg(classe, idPonta) {
    const s = svg('svg', { class: classe, 'aria-hidden': 'true' });
    s.append(svg('defs', null,
      svg('marker', { id: idPonta, viewBox: '0 0 10 10', refX: '8', refY: '5', markerWidth: '6', markerHeight: '6', orient: 'auto-start-reverse' },
        svg('path', { d: 'M0 0 L10 5 L0 10 z', fill: 'currentColor' }))));
    const p = svg('path', { class: 'traco', pathLength: '1', 'marker-end': `url(#${idPonta})` });
    s.append(p);
    s._p = p;
    return s;
  }
  function ajustarSvg(s, base) {
    const W = base.offsetWidth, H = base.offsetHeight;
    s.setAttribute('viewBox', `0 0 ${W} ${H}`);
    s.setAttribute('width', W);
    s.setAttribute('height', H);
  }

  // ════════════════════════════════════════════════════════════════════════
  // 2.1  controle/cadeia — a primeira condição verdadeira vence
  // ════════════════════════════════════════════════════════════════════════
  const CD_ORDENS = {
    certa: [['A', 90], ['B', 80], ['C', 70], ['R', null]],
    errada: [['C', 70], ['B', 80], ['A', 90], ['R', null]],
  };
  const CD_LETRA = { A: 'A', B: 'B', C: 'C', R: 'reprovado' };
  const CD_ALT = 76;
  const letraCerta = (n) => (n >= 90 ? 'A' : n >= 80 ? 'B' : n >= 70 ? 'C' : 'reprovado');

  Deck.widget('controle/cadeia', {
    iniciar(raiz) {
      raiz.classList.add('w-cadeia');
      const w = raiz._w = { nota: 78, ordem: 'certa', aval: 0, L: {} };
      w.num = el('b', { class: 'cd-num' }, '78');
      w.range = el('input', { type: 'range', min: '0', max: '100', step: '1', value: '78', 'aria-label': 'nota' });
      w.range.addEventListener('input', () => { w.nota = +w.range.value; w.aval = 9; cdDesenhar(raiz); });
      w.range.addEventListener('change', () => w.range.blur());
      w.range.addEventListener('pointerup', () => w.range.blur());
      w.bOrdem = botao('', () => { w.ordem = w.ordem === 'certa' ? 'errada' : 'certa'; w.aval = 9; cdDesenhar(raiz); }, 'cd-ordem');
      w.lista = el('div', { class: 'cd-lista' });
      for (const id of ['A', 'B', 'C', 'R']) {
        const L = { kw: el('span', { class: 'cd-kw' }), cond: el('span', { class: 'cd-cond' }), corpo: el('span', { class: 'cd-corpo' }), selo: el('span', { class: 'cd-selo' }) };
        L.ln = el('div', { class: 'cd-ln' }, el('span', { class: 'cd-cod' }, L.kw, L.cond, L.corpo), L.selo);
        w.L[id] = L;
        w.lista.append(L.ln);
      }
      w.saida = el('div', { class: 'cd-saida' });
      w.alerta = el('div', { class: 'cd-alerta' });
      raiz.append(
        el('div', { class: 'cd-topo' }, el('span', { class: 'cd-decl', html: realcar('int nota =') }), w.num, w.range, w.bOrdem),
        w.lista,
        el('div', { class: 'cd-rodape' }, w.saida, w.alerta));
    },
    passo(raiz, k) {
      const w = raiz._w;
      w.nota = k >= 4 ? 95 : 78;
      w.ordem = k >= 4 ? 'errada' : 'certa';
      w.aval = k >= 3 ? 9 : k;
      cdDesenhar(raiz);
    },
  });

  function cdDesenhar(raiz) {
    const w = raiz._w;
    w.num.textContent = w.nota;
    w.range.value = String(w.nota);
    w.bOrdem.innerHTML = w.ordem === 'certa' ? '↕ ordem: 90 · 80 · 70' : '↕ ordem: 70 · 80 · 90';
    w.bOrdem.classList.toggle('selecionado', w.ordem === 'errada');
    let venc = null;
    CD_ORDENS[w.ordem].forEach(([id, lim], i) => {
      const L = w.L[id];
      let est;
      if (venc) est = 'pulado';
      else if (i >= w.aval) est = 'neutro';
      else if (lim == null) { est = 'executa'; venc = id; }
      else if (w.nota >= lim) { est = 'verdadeiro'; venc = id; }
      else est = 'falso';
      L.ln.style.transform = `translateY(${i * CD_ALT}px)`;
      L.ln.dataset.estado = est;
      L.kw.innerHTML = realcar(i === 0 ? 'if' : lim == null ? 'else' : 'else if');
      L.cond.innerHTML = lim == null ? '' : realcar(`(nota >= ${lim})`);
      L.corpo.innerHTML = realcar(`{ print("${CD_LETRA[id]}"); }`);
      L.selo.innerHTML = {
        neutro: '?', pulado: 'nem testado', executa: 'executa',
        falso: `${w.nota} >= ${lim} → <b>false</b>`, verdadeiro: `${w.nota} >= ${lim} → <b>true</b>`,
      }[est];
    });
    w.saida.innerHTML = venc
      ? `<span class="rot">saída</span>nota ${w.nota} → <b>${CD_LETRA[venc]}</b>`
      : '<span class="rot">saída</span><span class="vazio">…</span>';
    const certa = letraCerta(w.nota);
    const errado = venc && CD_LETRA[venc] !== certa;
    w.alerta.innerHTML = errado ? `⚠ ${w.nota} saiu como <b>${CD_LETRA[venc]}</b>: a cadeia para no <b>primeiro</b> verdadeiro` : '';
    w.alerta.classList.toggle('on', !!errado);
  }

  // ════════════════════════════════════════════════════════════════════════
  // 2.1  controle/gotofail — a linha duplicada que pula a verificação
  // ════════════════════════════════════════════════════════════════════════
  const GF = [
    ['static OSStatus SSLVerifySignedServerKeyExchange(…)'],
    ['{'],
    ['    OSStatus err;'],
    ['    …'],
    ['    if ((err = SSLHashSHA1.update(&hashCtx, &serverRandom)) != 0)', 'if1'],
    ['        goto fail;', 'g1'],
    ['    if ((err = SSLHashSHA1.update(&hashCtx, &signedParams)) != 0)', 'if2'],
    ['        goto fail;', 'g2'],
    ['        goto fail;', 'bug'],
    ['    if ((err = SSLHashSHA1.final(&hashCtx, &hashOut)) != 0)', 'if3'],
    ['        goto fail;', 'g4'],
    ['    err = sslRawVerify(ctx, …);   // confere a assinatura', 'ver'],
    ['    …', 'resto'],
    ['fail:', 'fail'],
    ['    SSLFreeBuffer(&hashCtx);', 'free'],
    ['    return err;', 'ret'],
    ['}'],
  ];
  const GF_ROTA = ['if1', 'if2', 'bug', 'fail', 'free', 'ret'];
  const GF_TXT = [
    ['Ache o bug', 'Esta função decide se a assinatura do servidor TLS é válida. Qualquer <code>err ≠ 0</code> vira falha.'],
    ['Uma linha duplicada', 'A indentação “diz” que as duas <code>goto fail;</code> estão dentro do <code>if</code>.'],
    ['O que o compilador vê', 'Sem chaves, o <code>if</code> governa só a <b>primeira</b>. A segunda é incondicional.'],
    ['Executando', 'Os dois <code>update</code> do hash dão certo, <code>err</code> fica 0 — e o salto vem mesmo assim.'],
    ['CVE-2014-1266', 'A assinatura do servidor <b>nunca</b> era conferida.'],
  ];

  Deck.widget('controle/gotofail', {
    iniciar(raiz) {
      raiz.classList.add('w-gotofail');
      const w = raiz._w = { ln: {}, ger: 0 };
      w.cod = el('div', { class: 'gf-codigo' });
      w.cursor = el('div', { class: 'gf-cursor' }, el('span', { class: 'gf-seta' }, '▶'));
      w.cod.append(w.cursor);
      GF.forEach(([t, id]) => {
        const ln = linha(t, 'gf-ln');
        if (id) { ln.dataset.id = id; w.ln[id] = ln; }
        if (/^if\d$/.test(id || '')) ln._cod.append(el('span', { class: 'gf-chave' }, ' {'));
        if (/^g\d$/.test(id || '')) ln._cod.append(el('span', { class: 'gf-chave' }, ' }'));
        if (id === 'bug') ln.append(el('span', { class: 'gf-tag' }, 'sempre executa'));
        if (id === 'fail') ln.append(el('span', { class: 'gf-tag gf-tag-salto' }, 'chega aqui com err = 0'));
        w.cod.append(ln);
      });
      w.arco = camadaSvg('gf-arco', 'controle-gf-ponta');
      w.cod.append(w.arco);

      w.titulo = el('h3', { class: 'gf-titulo' });
      w.texto = el('p', { class: 'gf-texto' });
      w.lista = el('ul', { class: 'gf-lista' });
      w.itens = {};
      [['h1', 'hash do serverRandom'], ['h2', 'hash do signedParams'], ['h3', 'hash final'], ['v', 'sslRawVerify — a assinatura']].forEach(([id, t]) => {
        const li = el('li', null, el('span', { class: 'ic' }), el('span', { class: 't' }, t), el('span', { class: 's' }));
        w.itens[id] = li;
        w.lista.append(li);
      });
      w.ret = el('div', { class: 'gf-ret' });
      w.cve = el('div', { class: 'gf-cve', html:
        '<p><b>Fev/2014</b> · iOS 7.0.6 e OS X 10.9.2 corrigem: o TLS aceitava um servidor falso → <b>man-in-the-middle</b>.</p>' +
        '<p class="gf-licao">Com <code>{ }</code>, a linha duplicada seria só código morto. E o <code>-Wall</code> do GCC avisa: <span class="mono">-Wmisleading-indentation</span>.</p>' });
      raiz.append(w.cod, el('div', { class: 'gf-painel' }, w.titulo, w.texto,
        el('span', { class: 'gf-rot' }, 'o que a função deveria fazer'), w.lista, w.ret, w.cve));
    },
    passo(raiz, k, ctx, dir) {
      const w = raiz._w;
      const animar = k === 3 && dir === 1 && podeAnimar(ctx);
      w.ger++;
      gfDesenhar(raiz, k, animar ? 0 : 99);
      if (animar) gfAnimar(raiz, ctx, w.ger);
    },
  });

  function gfDesenhar(raiz, k, prog) {
    const w = raiz._w;
    raiz.dataset.k = k;
    w.titulo.textContent = GF_TXT[k][0];
    w.texto.innerHTML = GF_TXT[k][1];
    w.ln.bug.classList.toggle('gf-bug', k >= 1);
    raiz.classList.toggle('gf-compilador', k >= 2);
    const p = k >= 3 ? Math.min(prog, GF_ROTA.length) : 0;
    const atual = p > 0 ? GF_ROTA[p - 1] : null;
    if (atual) {
      const q = pos(w.ln[atual], w.cod);
      w.cursor.style.transform = `translateY(${q.y}px)`;
      w.cursor.style.height = q.h + 'px';
    }
    w.cursor.classList.toggle('on', !!atual);
    GF_ROTA.forEach((id, i) => w.ln[id].classList.toggle('gf-visitada', i < p));
    const saltou = p >= 4;
    ['if3', 'g4', 'ver', 'resto'].forEach((id) => w.ln[id].classList.toggle('gf-pulada', saltou));
    w.ln.fail.classList.toggle('gf-alvo', saltou);
    // o arco do salto, da linha duplicada até o rótulo
    ajustarSvg(w.arco, w.cod);
    const a = pos(w.ln.bug, w.cod), b = pos(w.ln.fail, w.cod);
    const y1 = a.y + a.h / 2, y2 = b.y + b.h / 2;
    w.arco._p.setAttribute('d', `M 40 ${y1} C 2 ${y1 + 10}, 2 ${y2 - 10}, 36 ${y2}`);
    w.arco.classList.toggle('on', saltou);
    // checklist do que a função deveria fazer
    const est = { h1: p >= 1 ? 'ok' : '', h2: p >= 2 ? 'ok' : '', h3: saltou ? 'pulado' : '', v: saltou ? 'pulado' : '' };
    for (const [id, li] of Object.entries(w.itens)) {
      li.dataset.estado = est[id];
      li.querySelector('.s').textContent = est[id] === 'ok' ? 'err = 0' : est[id] === 'pulado' ? 'pulado' : '';
    }
    w.lista.classList.toggle('on', k >= 3);   // antes do passo 3: a lista aparece pendente
    w.ret.innerHTML = p >= 6 ? '<code>return err;</code> → <b>0</b> = <span class="ok-txt">“assinatura válida”</span>' : '';
    w.ret.classList.toggle('on', p >= 6);
    w.cve.classList.toggle('on', k >= 4);
  }

  async function gfAnimar(raiz, ctx, g) {
    const w = raiz._w;
    for (let p = 1; p <= GF_ROTA.length; p++) {
      if (!(await ctx.esperar(p === 1 ? 350 : p === 4 ? 900 : 700)) || g !== w.ger) return;
      gfDesenhar(raiz, 3, p);
    }
  }

  // ════════════════════════════════════════════════════════════════════════
  // 2.1  controle/escopo — onde o nome vive: antes × init-statement
  // ════════════════════════════════════════════════════════════════════════
  const ESC = {
    A: {
      linhas: [
        'auto it = estoque.find("porca");',
        'if (it != estoque.end()) {',
        '    usar(it->second);',
        '} else {',
        '    avisar("sem porca");',
        '}',
        '// … 40 linhas depois …',
        'it->second = 0;   // compila. E se for end()?',
      ],
      vida: [0, 7],
      rotulo: '<code>it</code> vive até o fim do bloco — aqui, a função inteira',
      titulo: '<span class="pill cpp98">antes do C++17</span> declarado fora do <code>if</code>',
    },
    B: {
      linhas: [
        'if (auto it = estoque.find("porca"); it != estoque.end()) {',
        '    usar(it->second);',
        '} else {',
        '    avisar("sem porca");   // it existe aqui também',
        '}',
        '',
        '// … 40 linhas depois …',
        'it->second = 0;   // e agora?',
      ],
      vida: [0, 4],
      rotulo: '<code>it</code> vive só no <code>if</code>/<code>else</code>',
      titulo: '<span class="pill cpp17">C++17</span> init-statement: <code>if (init; cond)</code>',
    },
  };

  Deck.widget('controle/escopo', {
    iniciar(raiz) {
      raiz.classList.add('w-escopo');
      const w = raiz._w = { cam: {} };
      w.titulo = el('div', { class: 'es-titulo' });
      w.cod = el('div', { class: 'es-codigo' });
      w.rotulo = el('span', { class: 'es-rotulo' });
      w.vida = el('div', { class: 'es-vida' });
      w.cod.append(w.vida);
      for (const v of ['A', 'B']) {
        const cam = el('div', { class: 'es-camada', dataset: { v } });
        ESC[v].linhas.forEach((t) => cam.append(linha(t)));
        w.cam[v] = cam;
        w.cod.append(cam);
      }
      w.diag = el('div', { class: 'es-diag' });
      raiz.append(w.titulo, w.cod, el('div', { class: 'es-legenda' }, el('i'), w.rotulo), w.diag);
    },
    passo(raiz, k) {
      const w = raiz._w;
      const v = k >= 2 ? 'B' : 'A';
      raiz.dataset.v = v;
      w.titulo.innerHTML = ESC[v].titulo;
      const lns = w.cam[v].children;
      const [i0, i1] = ESC[v].vida;
      const a = pos(lns[i0], w.cod), b = pos(lns[i1], w.cod);
      w.vida.style.top = a.y + 'px';
      w.vida.style.height = (b.y + b.h - a.y) + 'px';
      w.rotulo.innerHTML = ESC[v].rotulo;
      for (const c of Object.values(w.cam)) Array.from(c.children).forEach((ln) => ln.classList.remove('es-alvo'));
      if (k === 1 || k === 3) lns[7].classList.add('es-alvo');
      w.diag.dataset.tipo = k === 1 ? 'aviso' : k === 3 ? 'erro' : '';
      w.diag.innerHTML = k === 1
        ? '⚠ Compila: <code>it</code> ainda existe aqui — e, se for <code>end()</code>, <code>it-&gt;second</code> é UB.'
        : k === 3 ? '<span class="mono">error: ‘it’ was not declared in this scope</span> <span class="es-fonte">— GCC 13</span>' : '';
    },
  });

  // ════════════════════════════════════════════════════════════════════════
  // 2.2  controle/constexpr — escolha T: o galho falso é descartado
  // ════════════════════════════════════════════════════════════════════════
  const CX = [
    ['template <typename T>'],
    ['std::string descrever(const T& valor) {'],
    ['  if constexpr (std::is_integral_v<T>) {', 'c1'],
    ['    return fmt::format("inteiro de {} bytes, valor {}",', 'r1'],
    ['                       sizeof(T), valor);', 'r1'],
    ['  } else if constexpr (std::is_floating_point_v<T>) {', 'c2'],
    ['    return fmt::format("ponto flutuante, valor {:.2f}",', 'r2'],
    ['                       valor);', 'r2'],
    ['  } else if constexpr (std::is_same_v<T, std::string>) {', 'c3'],
    ['    return fmt::format("string de {} caracteres",', 'r3'],
    ['                       valor.size());   // só existe aqui', 'r3'],
    ['  } else {', 'c4'],
    ['    return "tipo desconhecido";', 'r4'],
    ['  }'],
    ['}'],
  ];
  const CX_T = [
    { b: 'int', call: 'descrever(42)', T: 'int', c: [1, 0, 0], r: 'inteiro de 4 bytes, valor 42',
      nota: 'Só o 1º galho é instanciado; os outros três nem são compilados para <code>int</code>.' },
    { b: 'double', call: 'descrever(3.14159)', T: 'double', c: [0, 1, 0], r: 'ponto flutuante, valor 3.14',
      nota: '<code>{:.2f}</code> → duas casas decimais.' },
    { b: 'std::string', call: 'descrever("abc"s)', T: 'std::string', c: [0, 0, 1], r: 'string de 3 caracteres',
      nota: 'O único T em que <code>valor.size()</code> chega a ser compilado.' },
    { b: 'char', call: "descrever('x')", T: 'char', c: [1, 0, 0], r: 'inteiro de 1 bytes, valor x', surpresa: true,
      nota: '<b>char é integral</b>: <code>is_integral_v&lt;char&gt;</code> é <code>true</code>.' },
    { b: 'bool', call: 'descrever(true)', T: 'bool', c: [1, 0, 0], r: 'inteiro de 1 bytes, valor true', surpresa: true,
      nota: '<b>bool também é integral</b>.' },
    { b: '"abc"', call: 'descrever("abc")', T: 'char[4]', c: [0, 0, 0], r: 'tipo desconhecido', surpresa: true,
      nota: '<code>"abc"</code> é um array de <code>char</code>, <b>não</b> <code>std::string</code>: cai no <code>else</code>.' },
  ];
  const CX_COND = [
    (T) => `std::is_integral_v<${T}>`,
    (T) => `std::is_floating_point_v<${T}>`,
    (T) => `std::is_same_v<${T}, std::string>`,
  ];

  Deck.widget('controle/constexpr', {
    iniciar(raiz) {
      raiz.classList.add('w-constexpr');
      const w = raiz._w = { sel: null };
      w.cod = el('div', { class: 'cx-codigo' });
      w.lns = CX.map(([t, tag], i) => {
        const ln = linha(t, 'cx-ln');
        if (tag) ln.dataset.tag = tag;
        if (/^r/.test(tag || '') && (CX[i - 1] || [])[1] !== tag) ln.append(el('span', { class: 'cx-etq' }));
        w.cod.append(ln);
        return ln;
      });
      w.tipos = el('div', { class: 'cx-tipos' });
      w.bts = CX_T.map((t, i) => {
        const b = botao(Deck.util.escapar(t.b), () => { w.sel = i; cxDesenhar(raiz); }, 'mono');
        w.tipos.append(b);
        return b;
      });
      w.chamada = el('div', { class: 'cx-chamada' });
      w.conds = el('div', { class: 'cx-conds' });
      w.saida = el('div', { class: 'cx-saida' });
      w.nota = el('div', { class: 'cx-nota' });
      raiz.append(w.cod, el('div', { class: 'cx-painel' },
        el('span', { class: 'cx-rot' }, 'escolha o T'), w.tipos, w.chamada,
        el('span', { class: 'cx-rot' }, 'avaliado em compile-time'), w.conds, w.saida, w.nota));
    },
    passo(raiz, k) { raiz._w.sel = k > 0 ? k - 1 : null; cxDesenhar(raiz); },
  });

  function cxDesenhar(raiz) {
    const w = raiz._w;
    const t = w.sel == null ? null : CX_T[w.sel];
    raiz.classList.toggle('cx-vazio', !t);
    w.bts.forEach((b, i) => b.classList.toggle('selecionado', i === w.sel));
    const ramo = t ? (t.c[0] ? 1 : t.c[1] ? 2 : t.c[2] ? 3 : 4) : 0;
    w.lns.forEach((ln) => {
      const tag = ln.dataset.tag || '';
      const r = tag ? +tag.slice(1) : 0;
      let est = '';
      if (t && tag[0] === 'r') est = r === ramo ? 'mantido' : 'descartado';
      if (t && tag[0] === 'c') est = r <= ramo ? (r === ramo ? 'verdadeira' : 'falsa') : 'descartado';
      ln.dataset.est = est;
      const etq = ln.querySelector('.cx-etq');
      if (etq) etq.textContent = est === 'mantido' ? '✓ instanciado' : est === 'descartado' ? 'descartado' : '';
    });
    w.chamada.innerHTML = t
      ? `<span class="mono">${realcar(t.call)}</span><span class="cx-T">T = <b class="mono">${Deck.util.escapar(t.T)}</b></span>`
      : '<span class="apagado">escolha um tipo acima — ou avance com →</span>';
    w.conds.replaceChildren(...CX_COND.map((f, i) => {
      const avaliada = t && i + 1 <= ramo;
      const v = !t ? '' : avaliada ? (t.c[i] ? 'true' : 'false') : '—';
      const linhaC = el('div', { class: 'cx-cond', dataset: { v: v === '—' ? 'nao' : v } },
        el('span', { class: 'mono', html: realcar(f(t ? t.T : 'T')) }), el('b', { class: 'mono' }, v === '—' ? 'nem avaliada' : v));
      return linhaC;
    }));
    w.saida.innerHTML = t ? `<span class="rot">devolve</span><span class="mono">"${Deck.util.escapar(t.r)}"</span>` : '';
    w.nota.innerHTML = t ? t.nota : '';
    w.nota.classList.toggle('surpresa', !!(t && t.surpresa));
  }

  // ════════════════════════════════════════════════════════════════════════
  // 2.3  controle/switch — salto para o rótulo, queda até o return
  // ════════════════════════════════════════════════════════════════════════
  const SW = [
    'switch (c) {',
    '  case Comando::Iniciar:',
    '    return "motor ligado";',
    '  case Comando::Retomar:',
    '    fmt::print("(log: retomando de uma pausa)\\n");',
    '    [[fallthrough]];',
    '  case Comando::Pausar:',
    '    return "estado alterado";',
    '  case Comando::Parar: {',
    '    const char* motivo = "pedido do usuário";',
    '    fmt::print("(log: parando — {})\\n", motivo);',
    '    return "motor desligado";',
    '  }',
    '  case Comando::Reiniciar:',
    '    return "reiniciando";',
    '}',
  ];
  const SW_ROTULOS = new Set([1, 3, 6, 8, 13]);
  const SW_CMD = [
    { nome: 'Iniciar', rota: [1, 2], ret: 'motor ligado', saida: [{ t: 'executar(...) → motor ligado', em: 2 }] },
    { nome: 'Pausar', rota: [6, 7], ret: 'estado alterado', saida: [{ t: 'executar(...) → estado alterado', em: 7 }] },
    { nome: 'Retomar', rota: [3, 4, 5, 6, 7], ret: 'estado alterado',
      saida: [{ t: '  (log: retomando de uma pausa)', em: 4, log: true }, { t: 'executar(...) → estado alterado', em: 7 }] },
    { nome: 'Parar', rota: [8, 9, 10, 11], ret: 'motor desligado',
      saida: [{ t: '  (log: parando — pedido do usuário)', em: 10, log: true }, { t: 'executar(...) → motor desligado', em: 11 }] },
    { nome: 'Reiniciar', rota: [13, 14], ret: 'reiniciando', saida: [{ t: 'executar(...) → reiniciando', em: 14 }] },
  ];

  Deck.widget('controle/switch', {
    iniciar(raiz, ctx) {
      raiz.classList.add('w-switch');
      const w = raiz._w = { ger: 0, ctx };
      w.cod = el('div', { class: 'sw-codigo' });
      w.cursor = el('div', { class: 'sw-cursor' });
      w.cod.append(w.cursor);
      w.lns = SW.map((t, i) => {
        const ln = linha(t, 'sw-ln' + (SW_ROTULOS.has(i) ? ' sw-rotulo' : ''));
        ln.append(el('span', { class: 'sw-tag' }));
        w.cod.append(ln);
        return ln;
      });
      w.tabela = el('div', { class: 'sw-tabela' });
      w.linhasT = SW_CMD.map((c, i) => {
        const b = el('button', { type: 'button', class: 'sw-entrada' },
          el('span', { class: 'mono', html: realcar('Comando::' + c.nome) }), el('span', { class: 'sw-dest' }, 'linha ' + (c.rota[0] + 1)));
        b.addEventListener('click', (e) => { e.stopPropagation(); b.blur(); swExecutar(raiz, i, [], true); });
        w.tabela.append(b);
        return b;
      });
      w.console = el('div', { class: 'sw-console' });
      w.seta = camadaSvg('sw-seta', 'controle-sw-ponta');
      raiz.append(
        w.cod,
        el('div', { class: 'sw-painel' },
          el('span', { class: 'sw-rot' }, 'tabela de saltos: valor → linha (montada na compilação)'), w.tabela,
          el('span', { class: 'sw-rot' }, 'saída'), w.console,
          el('p', { class: 'sw-citacao', html: '<b>[stmt.switch]</b> <code>case</code> e <code>default</code>, por si sós, não alteram o fluxo: o controle passa direto por eles. Para sair, <code>break</code>.' })),
        w.seta);
    },
    passo(raiz, k, ctx, dir) {
      const w = raiz._w;
      w.ctx = ctx;
      const feitos = [];
      for (let i = 0; i < k - 1; i++) feitos.push(i);
      if (k === 0) { w.ger++; swDesenhar(raiz, null, 0, []); return; }
      swExecutar(raiz, k - 1, feitos, dir === 1 && podeAnimar(ctx));
    },
  });

  function swExecutar(raiz, idx, feitos, animar) {
    const w = raiz._w;
    const g = ++w.ger;
    const c = SW_CMD[idx];
    if (!animar || !podeAnimar(w.ctx)) { swDesenhar(raiz, idx, 99, feitos); return; }
    swDesenhar(raiz, idx, 0, feitos);
    (async () => {
      for (let p = 1; p <= c.rota.length; p++) {
        if (!(await w.ctx.esperar(p === 1 ? 650 : 520)) || g !== w.ger) return;
        swDesenhar(raiz, idx, p, feitos);
      }
    })();
  }

  function swDesenhar(raiz, idx, prog, feitos) {
    const w = raiz._w;
    const c = idx == null ? null : SW_CMD[idx];
    const p = c ? Math.min(prog, c.rota.length) : 0;
    const visitadas = c ? c.rota.slice(0, p) : [];
    const atual = visitadas.length ? visitadas[visitadas.length - 1] : null;
    w.lns.forEach((ln, i) => {
      const vis = visitadas.includes(i);
      ln.classList.toggle('sw-visitada', vis);
      const passa = vis && SW_ROTULOS.has(i) && i !== c.rota[0];
      const tag = ln.querySelector('.sw-tag');
      tag.textContent = passa ? 'passa direto ↓' : (c && vis && i === c.rota[c.rota.length - 1]) ? '→ "' + c.ret + '"' : '';
      ln.classList.toggle('sw-passa', passa);
      ln.classList.toggle('sw-alvo', !!c && i === c.rota[0]);
    });
    if (atual != null) {
      const q = pos(w.lns[atual], w.cod);
      w.cursor.style.transform = `translateY(${q.y}px)`;
      w.cursor.style.height = q.h + 'px';
    }
    w.cursor.classList.toggle('on', atual != null);
    w.linhasT.forEach((b, i) => { b.classList.toggle('on', i === idx); b.classList.toggle('feito', feitos.includes(i)); });
    // a seta: da entrada da tabela até o rótulo do case
    ajustarSvg(w.seta, raiz);
    if (c && w.setaCmd !== idx) {
      const novo = w.seta._p.cloneNode(false);
      w.seta._p.replaceWith(novo);
      w.seta._p = novo;
    }
    w.setaCmd = c ? idx : null;
    if (c) {
      const a = pos(w.linhasT[idx], raiz), alvo = w.lns[c.rota[0]];
      const b = pos(alvo, raiz), cp = pos(w.cod, raiz);
      const x1 = a.x - 6, y1 = a.y + a.h / 2;
      const x2 = cp.x + cp.w - 14, y2 = b.y + b.h / 2;
      w.seta._p.setAttribute('d', `M ${x1} ${y1} C ${x1 - 70} ${y1}, ${x2 + 80} ${y2}, ${x2} ${y2}`);
    }
    w.seta.classList.toggle('on', !!c);
    w.seta.dataset.cmd = idx == null ? '' : String(idx);
    // console: saídas dos comandos já feitos + o atual até onde chegou
    const linhas = [];
    feitos.forEach((i) => linhas.push(...SW_CMD[i].saida));
    if (c) c.saida.forEach((s) => { if (visitadas.includes(s.em)) linhas.push(s); });
    sincronizar(w.console, linhas.length ? linhas.map((x, j) => ({ chave: j + x.t, x })) : [{ chave: 'vazio' }],
      (it) => (it.x ? el('div', { class: 'sw-out' + (it.x.log ? ' log' : '') }, it.x.t) : el('div', { class: 'sw-out vazio' }, '…')));
  }

  // ════════════════════════════════════════════════════════════════════════
  // 2.4  controle/lacos — onde fica o teste: while × do-while × for
  // ════════════════════════════════════════════════════════════════════════
  function evWhile(n0) {
    const ev = [], out = [];
    let n = n0;
    for (;;) {
      const antes = n--;
      const ok = antes > 0;
      ev.push({ p: 'teste', ok, msg: `n-- &gt; 0 → ${antes} &gt; 0 → <b>${ok}</b> · n vira ${n}`, v: { n }, out: out.slice() });
      if (!ok) break;
      out.push(String(n));
      ev.push({ p: 'corpo', msg: `corpo: imprime <b>${n}</b>`, v: { n }, out: out.slice() });
    }
    ev.push({ p: 'fim', msg: `saiu do laço — e agora <b>n = ${n}</b>`, v: { n }, out: out.slice() });
    return ev;
  }
  function evDo() {
    const o = ['corpo(m=0)'];
    return [
      { p: 'corpo', msg: 'o corpo vem <b>primeiro</b>: imprime corpo(m=0)', v: { m: 0 }, out: o },
      { p: 'teste', ok: false, msg: 'só agora testa: 0 &gt; 0 → <b>false</b>', v: { m: 0 }, out: o },
      { p: 'fim', msg: 'rodou <b>1×</b> com a condição falsa', v: { m: 0 }, out: o },
    ];
  }
  function evFor() {
    const ev = [], out = [];
    let i = 0, j = 9;
    ev.push({ p: 'init', msg: 'init roda <b>uma vez</b>: i = 0, j = 9', v: { i, j }, out: [] });
    for (;;) {
      const ok = i < j;
      ev.push({ p: 'teste', ok, msg: `${i} &lt; ${j} → <b>${ok}</b>`, v: { i, j }, out: out.slice() });
      if (!ok) break;
      out.push(`(${i},${j})`);
      ev.push({ p: 'corpo', msg: `corpo: imprime <b>(${i},${j})</b>`, v: { i, j }, out: out.slice() });
      i++; j--;
      ev.push({ p: 'inc', msg: `++i, --j → i = ${i}, j = ${j}`, v: { i, j }, out: out.slice() });
    }
    ev.push({ p: 'fim', msg: `${i} &lt; ${j} falhou: fim do laço`, v: { i, j }, out: out.slice() });
    return ev;
  }
  const LC = {
    while: {
      aba: 'while', pre: (w) => `int n = ${w.n0};`,
      partes: [['while (', ''], ['n-- > 0', 'teste'], [') ', ''], ['fmt::print(" {}", n);', 'corpo']],
      ev: (w) => evWhile(w.n0), v0: (w) => ({ n: w.n0 }),
    },
    dowhile: {
      aba: 'do-while', pre: () => 'int m = 0;',
      partes: [['do { ', ''], ['fmt::print(" corpo(m={})", m);', 'corpo'], [' } while (', ''], ['m > 0', 'teste'], [');', '']],
      ev: evDo, v0: () => ({ m: 0 }),
    },
    for: {
      aba: 'for', pre: () => '// duas variáveis, em direções opostas',
      partes: [['for (', ''], ['int i = 0, j = 9', 'init'], ['; ', ''], ['i < j', 'teste'], ['; ', ''], ['++i, --j', 'inc'], [') ', ''], ['fmt::print("({},{}) ", i, j);', 'corpo']],
      ev: evFor, v0: () => ({ i: '—', j: '—' }),
    },
  };
  const LC_CHIP = { init: 'init', teste: 'teste', corpo: 'corpo', inc: 'inc', fim: 'fim' };

  // o desenho do laço — ONDE fica o teste. Geometria fixa (o palco é 1600×900):
  // nós em linha, a seta de entrada no 1º nó, a volta por cima e a saída pelo teste.
  const LF = { NW: 112, NH: 46, G: 60, X0: 56, Y: 46 };
  const LF_MODOS = {
    while: { nos: ['teste', 'corpo'], volta: 'teste',
      lado: 'O teste vem <b>antes</b> do corpo: com <code>n = 0</code>, o corpo roda <b>0 vezes</b>.' },
    dowhile: { nos: ['corpo', 'teste'], volta: 'corpo',
      lado: 'O teste vem <b>depois</b> do corpo: o corpo roda <b>pelo menos 1 vez</b>.' },
    for: { nos: ['init', 'teste', 'corpo', 'inc'], volta: 'teste', lado: '' },
  };

  function lfMontar(w) {
    const M = LF_MODOS[w.modo];
    const { NW, NH, G, X0, Y } = LF;
    const cy = Y + NH / 2;
    const xs = M.nos.map((_, i) => X0 + i * (NW + G));
    const cx = (p) => xs[M.nos.indexOf(p)] + NW / 2;
    const xt = cx('teste');
    const larg = Math.max(xs[xs.length - 1] + NW + 16, xt + 130);
    const alt = Y + NH + 40;
    w.fluxo.setAttribute('viewBox', `0 0 ${larg} ${alt}`);
    w.fluxo.setAttribute('width', larg);
    w.fluxo.setAttribute('height', alt);
    const g = svg('g', { class: 'lf-planta' });
    const aresta = (d, de, para) => svg('path', { class: 'lf-aresta', d, 'data-de': de, 'data-para': para, 'marker-end': 'url(#controle-lf-ponta)' });
    // entrada: onde a execução chega no laço
    g.append(svg('circle', { class: 'lf-entra', cx: 16, cy, r: 6 }), aresta(`M 24 ${cy} H ${X0 - 4}`, '', M.nos[0]));
    // as arestas em sequência
    M.nos.slice(1).forEach((p, i) => {
      const x1 = xs[i] + NW, x2 = xs[i + 1];
      g.append(aresta(`M ${x1} ${cy} H ${x2 - 4}`, M.nos[i], p));
      if (M.nos[i] === 'teste') g.append(svg('text', { class: 'lf-rot', x: (x1 + x2) / 2, y: cy - 10 }, 'true'));
    });
    // a volta: do último nó de volta à cabeça do laço, por cima
    const ult = M.nos[M.nos.length - 1];
    const xa = cx(ult), xb = cx(M.volta);
    g.append(aresta(`M ${xa} ${Y} C ${xa} ${Y - 40}, ${xb} ${Y - 40}, ${xb} ${Y - 4}`, ult, M.volta));
    if (ult === 'teste') g.append(svg('text', { class: 'lf-rot', x: (xa + xb) / 2, y: Y - 10 }, 'true'));
    // a saída: teste falso deixa o laço
    g.append(aresta(`M ${xt} ${Y + NH} V ${Y + NH + 24}`, 'teste', 'fim'));
    g.append(svg('text', { class: 'lf-rot lf-rot-sai', x: xt + 12, y: Y + NH + 24 }, 'false → sai'));
    // os nós por cima das arestas
    M.nos.forEach((p, i) => g.append(svg('g', { class: 'lf-no', 'data-p': p },
      svg('rect', { x: xs[i], y: Y, width: NW, height: NH, rx: 10 }),
      svg('text', { x: xs[i] + NW / 2, y: cy }, p))));
    const velha = w.fluxo.querySelector('.lf-planta');
    if (velha) velha.replaceWith(g); else w.fluxo.append(g);
    w.lado.innerHTML = M.lado;
  }

  function lfDesenhar(w, evs) {
    if (w.lfModo !== w.modo) { lfMontar(w); w.lfModo = w.modo; }
    const e = w.idx > 0 ? evs[w.idx - 1] : null;
    const ant = w.idx > 1 ? evs[w.idx - 2] : null;
    w.fluxo.querySelectorAll('.lf-no').forEach((n) => {
      const on = !!e && e.p === n.dataset.p;
      n.classList.toggle('on', on);
      n.classList.toggle('falso', on && e.ok === false);
    });
    // a aresta recém-percorrida acende (de ant.p para e.p)
    w.fluxo.querySelectorAll('.lf-aresta').forEach((a) => {
      const on = !!e && a.dataset.para === e.p && a.dataset.de === (ant ? ant.p : '');
      a.classList.toggle('on', on);
      a.setAttribute('marker-end', !on ? 'url(#controle-lf-ponta)' : e.p === 'fim' ? 'url(#controle-lf-ponta-sai)' : 'url(#controle-lf-ponta-on)');
    });
    w.fluxo.classList.toggle('saiu', !!e && e.p === 'fim');
  }

  Deck.widget('controle/lacos', {
    iniciar(raiz, ctx) {
      raiz.classList.add('w-lacos');
      const w = raiz._w = { modo: 'while', idx: 0, n0: 3, ger: 0, ctx };
      w.abas = el('div', { class: 'lc-abas' });
      w.bAbas = Object.keys(LC).map((m) => {
        const b = botao(LC[m].aba, () => { w.modo = m; w.n0 = 3; w.idx = 0; w.ger++; lcDesenhar(raiz); }, 'mono');
        b.dataset.m = m;
        w.abas.append(b);
        return b;
      });
      w.nMenos = botao('−', () => { w.n0 = Math.max(0, w.n0 - 1); w.idx = 0; w.ger++; lcDesenhar(raiz); }, 'lc-n');
      w.nMais = botao('+', () => { w.n0 = Math.min(6, w.n0 + 1); w.idx = 0; w.ger++; lcDesenhar(raiz); }, 'lc-n');
      w.ctrlN = el('span', { class: 'lc-ctrl-n' }, el('span', { class: 'mini apagado' }, 'n ='), w.nMenos, w.nMais);
      const bRodar = botao('▶ rodar', () => { w.idx = 0; lcRodar(raiz); }, 'primario');
      const bPasso = botao('⏭ passo', () => { w.ger++; const n = LC[w.modo].ev(w).length; w.idx = w.idx >= n ? 0 : w.idx + 1; lcDesenhar(raiz); });
      w.pre = el('div', { class: 'lc-pre lw' });
      w.laco = el('div', { class: 'lc-laco lw' });
      w.vars = el('div', { class: 'lc-vars' });
      w.msg = el('div', { class: 'lc-msg' });
      w.trilha = el('div', { class: 'lc-trilha' });
      w.saida = el('div', { class: 'lc-saida' });
      w.reta = el('div', { class: 'lc-reta' });
      for (let x = 0; x <= 9; x++) w.reta.append(el('span', { class: 'lc-pos' }, String(x)));
      w.mi = el('b', { class: 'lc-marca mi' }, 'i');
      w.mj = el('b', { class: 'lc-marca mj' }, 'j');
      w.reta.append(w.mi, w.mj);
      const ponta = (id, classe) => svg('marker', { id, viewBox: '0 0 10 10', refX: '8', refY: '5', markerWidth: '12', markerHeight: '12', markerUnits: 'userSpaceOnUse', orient: 'auto-start-reverse' },
        svg('path', { d: 'M0 0 L10 5 L0 10 z', class: classe }));
      w.fluxo = svg('svg', { class: 'lc-fluxo', 'aria-hidden': 'true' },
        svg('defs', null, ponta('controle-lf-ponta', 'lf-ponta'), ponta('controle-lf-ponta-on', 'lf-ponta on'), ponta('controle-lf-ponta-sai', 'lf-ponta sai')));
      w.lado = el('div', { class: 'lc-lado' });
      raiz.append(
        el('div', { class: 'lc-topo' }, w.abas, el('div', { class: 'botoes' }, w.ctrlN, bRodar, bPasso)),
        el('div', { class: 'lc-codigo' }, w.pre, w.laco),
        el('div', { class: 'lc-meio' }, w.vars, w.msg),
        el('div', { class: 'lc-viz' }, w.fluxo, w.lado, w.reta),
        w.trilha, w.saida);
    },
    passo(raiz, k, ctx, dir) {
      const w = raiz._w;
      w.ctx = ctx;
      w.ger++;
      w.modo = ['while', 'while', 'dowhile', 'for'][k];
      w.n0 = 3;
      const animar = k > 0 && dir === 1 && podeAnimar(ctx);
      w.idx = k > 0 && !animar ? LC[w.modo].ev(w).length : 0;
      lcDesenhar(raiz);
      if (animar) lcRodar(raiz);
    },
  });

  async function lcRodar(raiz) {
    const w = raiz._w;
    const g = ++w.ger;
    const n = LC[w.modo].ev(w).length;
    if (!podeAnimar(w.ctx)) { w.idx = n; lcDesenhar(raiz); return; }
    lcDesenhar(raiz);
    while (w.idx < n) {
      if (!(await w.ctx.esperar(w.idx === 0 ? 380 : 560)) || g !== w.ger) return;
      w.idx++;
      lcDesenhar(raiz);
    }
  }

  function lcDesenhar(raiz) {
    const w = raiz._w;
    const M = LC[w.modo];
    raiz.dataset.modo = w.modo;
    w.bAbas.forEach((b) => b.classList.toggle('selecionado', b.dataset.m === w.modo));
    w.ctrlN.classList.toggle('on', w.modo === 'while');
    const evs = M.ev(w);
    const e = w.idx > 0 ? evs[w.idx - 1] : null;
    w.pre.innerHTML = realcar(M.pre(w));
    w.laco.replaceChildren(...M.partes.map(([t, p]) => {
      const s = el('span', { class: 'lc-parte', html: realcarCru(t) });
      if (p) { s.dataset.p = p; s.classList.toggle('on', !!e && e.p === p); }
      if (p && e && e.p === p && p === 'teste') s.dataset.ok = String(e.ok);
      return s;
    }));
    const v = e ? e.v : M.v0(w);
    w.vars.replaceChildren(...Object.entries(v).map(([nome, val]) =>
      el('div', { class: 'var ativa' }, el('span', { class: 'nome' }, nome), el('span', { class: 'valor' }, String(val)))));
    w.msg.innerHTML = e ? e.msg : '<span class="apagado">▶ rodar — ou avance com →</span>';
    w.msg.dataset.p = e ? e.p : '';
    sincronizar(w.trilha, evs.slice(0, w.idx).map((x, i) => ({ chave: `${w.modo}${w.n0}:${i}`, x })),
      (it) => el('span', { class: 'lc-chip', dataset: { p: it.x.p } }, LC_CHIP[it.x.p] + (it.x.p === 'teste' ? (it.x.ok ? ' ✓' : ' ✗') : '')));
    Array.from(w.trilha.children).forEach((c, i) => c.classList.toggle('atual', i === w.idx - 1));
    const out = e ? e.out : [];
    w.saida.innerHTML = '<span class="rot">saída</span>' + (out.length ? out.map((o) => `<span>${o}</span>`).join(' ') : '<span class="apagado">—</span>');
    // reta numérica (só no for)
    const vi = typeof v.i === 'number' ? v.i : null, vj = typeof v.j === 'number' ? v.j : null;
    w.mi.style.setProperty('--x', vi == null ? 0 : Math.min(vi, 9));
    w.mj.style.setProperty('--x', vj == null ? 9 : Math.max(vj, 0));
    w.reta.classList.toggle('vivo', vi != null);
    lfDesenhar(w, evs);
  }

  // ════════════════════════════════════════════════════════════════════════
  // grades (2.4 e 2.6): utilitário comum
  // ════════════════════════════════════════════════════════════════════════
  function criarGrade(linhasN, colunasN, rotL, rotC, conteudo) {
    const g = el('div', { class: 'grade-w' });
    g.style.setProperty('--cols', colunasN);
    g.append(el('span', { class: 'gw-canto' }));
    for (let c = 0; c < colunasN; c++) g.append(el('span', { class: 'gw-rot' }, rotC(c)));
    const cel = [];
    for (let l = 0; l < linhasN; l++) {
      g.append(el('span', { class: 'gw-rot gw-rot-l' }, rotL(l)));
      const lin = [];
      for (let c = 0; c < colunasN; c++) {
        const x = el('span', { class: 'gw-cel' }, conteudo ? conteudo(l, c) : '');
        g.append(x);
        lin.push(x);
      }
      cel.push(lin);
    }
    g._cel = cel;
    return g;
  }

  // ════════════════════════════════════════════════════════════════════════
  // 2.4  controle/aninhado — continue pula, break sai de UM nível
  // ════════════════════════════════════════════════════════════════════════
  const AN_COD = [
    'for (int linha = 1; linha <= 3; ++linha) {',
    '    fmt::print("linha {}:", linha);',
    '    for (int col = 1; col <= 5; ++col) {',
    '        if (col == 2) continue;  // pula ESTA volta',
    '        if (col == 4) break;     // sai do INTERNO',
    '        fmt::print(" {}x{}", linha, col);',
    '    }',
    '    fmt::print("\\n");',
    '}',
  ];
  function anEventos(r) {   // r = 1..3
    return [
      { t: 'linha', r, ln: 1 },
      { t: 'imprime', r, c: 1, ln: 5 },
      { t: 'continue', r, c: 2, ln: 3 },
      { t: 'imprime', r, c: 3, ln: 5 },
      { t: 'break', r, c: 4, ln: 4 },
      { t: 'fim', r, ln: 7 },
    ];
  }

  Deck.widget('controle/aninhado', {
    iniciar(raiz) {
      raiz.classList.add('w-aninhado');
      const w = raiz._w = { ger: 0 };
      w.cod = el('div', { class: 'an-codigo' });
      w.lns = AN_COD.map((t) => { const ln = linha(t, 'an-ln'); w.cod.append(ln); return ln; });
      w.grade = criarGrade(3, 5, (l) => 'linha ' + (l + 1), (c) => 'col ' + (c + 1));
      w.saida = el('div', { class: 'an-saida' });
      w.legenda = el('div', { class: 'an-legenda', html:
        '<span class="lg imprime">imprime</span><span class="lg continue">↷ continue</span><span class="lg break">break</span><span class="lg nunca">nunca visitada</span>' });
      raiz.append(w.cod, el('div', { class: 'an-dir' }, w.grade, w.legenda,
        el('div', { class: 'an-term' }, el('span', { class: 'an-rot' }, 'saída'), w.saida)));
    },
    passo(raiz, k, ctx, dir) {
      const w = raiz._w;
      const g = ++w.ger;
      const animar = k > 0 && dir === 1 && podeAnimar(ctx);
      anDesenhar(raiz, k, animar ? 0 : 99);
      if (!animar) return;
      (async () => {
        for (let p = 1; p <= 6; p++) {
          if (!(await ctx.esperar(p === 1 ? 300 : 620)) || g !== w.ger) return;
          anDesenhar(raiz, k, p);
        }
        if (!(await ctx.esperar(800)) || g !== w.ger) return;
        anDesenhar(raiz, k, 99);
      })();
    },
  });

  function anDesenhar(raiz, k, prog) {
    const w = raiz._w;
    const evs = [];
    for (let r = 1; r <= k; r++) evs.push(...anEventos(r).slice(0, r < k ? 99 : prog));
    const atual = k > 0 && prog > 0 && prog !== 99 ? anEventos(k)[prog - 1] : null;
    w.grade._cel.forEach((lin) => lin.forEach((x) => { x.className = 'gw-cel'; x.textContent = ''; }));
    const textos = {};
    for (const e of evs) {
      if (e.c) {
        const x = w.grade._cel[e.r - 1][e.c - 1];
        x.classList.add(e.t);
        x.textContent = e.t === 'imprime' ? `${e.r}x${e.c}` : e.t === 'continue' ? '↷' : 'break';
      }
      if (e.t === 'linha') textos[e.r] = `linha ${e.r}:`;
      if (e.t === 'imprime') textos[e.r] += ` ${e.r}x${e.c}`;
      if (e.t === 'break') { const x = w.grade._cel[e.r - 1][4]; x.classList.add('nunca'); x.textContent = '—'; }
    }
    if (atual && atual.c) w.grade._cel[atual.r - 1][atual.c - 1].classList.add('atual');
    const lnAtual = atual ? atual.ln : -1;
    w.lns.forEach((ln, i) => ln.classList.toggle('on', i === lnAtual));
    const rs = Object.keys(textos);
    sincronizar(w.saida, rs.length ? rs.map((r) => ({ chave: 'r' + r })) : [{ chave: 'vazio' }],
      (it) => el('div', { class: 'an-out' + (it.chave === 'vazio' ? ' vazio' : '') }, it.chave === 'vazio' ? '…' : ''));
    rs.forEach((r, j) => { w.saida.children[j].textContent = textos[r]; });
  }

  // ════════════════════════════════════════════════════════════════════════
  // 2.5  controle/desacucar — o range-for reescrito pelo compilador
  // ════════════════════════════════════════════════════════════════════════
  const DS_DEST = [   // [passo em que aparece, texto antes, token, texto depois]
    [1, '{'],
    [1, '    auto&& __r = ', 'range', ';'],
    [2, '    for (auto __b = begin(__r), __e = end(__r);'],
    [3, '         __b != __e; ++__b) {'],
    [4, '        ', 'decl', ' = *__b;'],
    [5, '        ', 'corpo', ''],
    [3, '    }'],
    [1, '}'],
  ];
  const DS_NOTAS = [
    'O range é avaliado <b>uma vez</b> e preso numa referência — <code>auto&amp;&amp;</code> aceita qualquer coisa.',
    '<code>begin</code>/<code>end</code>: o membro <code>v.begin()</code> se existir; senão <code>begin(v)</code> por ADL; num array, <code>__r</code> e <code>__r + N</code>. <b>Qualquer tipo com begin/end serve.</b>',
    'O fim é calculado <b>uma vez</b>: um <code>push_back</code> no meio invalida <code>__e</code> (e, se realocar, <code>__b</code>) → <span class="pill ub">UB</span>',
    '<code>x</code> nasce <b>a cada volta</b>, ligado a <code>*__b</code> — por isso <code>auto&amp;</code> modifica e <code>auto</code> copia.',
    'C++17: na forma exata, <code>__b</code> e <code>__e</code> são declarações separadas e podem ter <b>tipos diferentes</b> (sentinela — a base dos ranges).',
  ];
  const DS_TOK = { range: 1, decl: 4, corpo: 5 };   // passo em que cada peça voa

  Deck.widget('controle/desacucar', {
    iniciar(raiz) {
      raiz.classList.add('w-desacucar');
      const w = raiz._w = { ger: 0, src: {}, alvo: {} };
      const tok = (id, html) => { const s = el('span', { class: 'ds-tok', dataset: { tok: id }, html }); return s; };
      w.src.decl = tok('decl', realcar('auto& x'));
      w.src.range = tok('range', realcar('v'));
      w.src.corpo = tok('corpo', '<span class="ds-corpo">corpo</span>');
      const txt = (t) => el('span', { html: realcarCru(t) });
      w.origem = el('div', { class: 'ds-origem' }, txt('for ('), w.src.decl, txt(' : '), w.src.range, txt(') { '), w.src.corpo, txt(' }'));
      w.dest = el('div', { class: 'ds-dest' });
      w.lns = DS_DEST.map(([passo, antes, id, depois]) => {
        const ln = el('div', { class: 'lw ds-ln', dataset: { passo } });
        ln.append(el('span', { html: realcarCru(antes) }));
        if (id) {
          const a = tok(id, id === 'corpo' ? '<span class="ds-corpo">corpo</span>' : realcar(id === 'range' ? 'v' : 'auto& x'));
          a.classList.add('ds-alvo');
          w.alvo[id] = a;
          ln.append(a);
          if (depois) ln.append(el('span', { html: realcarCru(depois) }));
        }
        w.dest.append(ln);
        return ln;
      });
      w.notas = el('ol', { class: 'ds-notas' });
      w.itens = DS_NOTAS.map((n) => { const li = el('li', { html: n }); w.notas.append(li); return li; });
      w.palco = el('div', { class: 'ds-palco' },
        el('span', { class: 'ds-rot' }, 'você escreve'), w.origem,
        el('span', { class: 'ds-rot ds-rot-seta' }, '↓ o compilador reescreve (aproximadamente — a forma que o demo imprime)'), w.dest);
      raiz.append(w.palco, w.notas);
    },
    passo(raiz, k, ctx, dir) {
      const w = raiz._w;
      const g = ++w.ger;
      dsDesenhar(raiz, k);
      const voa = Object.keys(DS_TOK).find((id) => DS_TOK[id] === k);
      if (voa && dir === 1 && podeAnimar(ctx)) dsVoar(raiz, voa, ctx, g);
    },
  });

  function dsDesenhar(raiz, k) {
    const w = raiz._w;
    w.palco.querySelectorAll('.ds-voando').forEach((n) => n.remove());
    w.lns.forEach((ln) => ln.classList.toggle('on', +ln.dataset.passo <= k));
    for (const id of Object.keys(DS_TOK)) {
      w.alvo[id].classList.toggle('oculto', DS_TOK[id] > k);
      w.src[id].classList.toggle('voou', DS_TOK[id] <= k);
    }
    w.itens.forEach((li, i) => { li.classList.toggle('on', i < k); li.classList.toggle('atual', i === k - 1); });
  }

  async function dsVoar(raiz, id, ctx, g) {
    const w = raiz._w;
    const de = w.src[id], para = w.alvo[id];
    const a = pos(de, w.palco), b = pos(para, w.palco);
    const clone = de.cloneNode(true);
    clone.classList.remove('voou');
    clone.classList.add('ds-voando');
    const fa = parseFloat(getComputedStyle(de).fontSize) || 1;
    const fb = parseFloat(getComputedStyle(para).fontSize) || fa;
    clone.style.fontSize = fa + 'px';
    clone.style.left = a.x + 'px';
    clone.style.top = a.y + 'px';
    para.classList.add('oculto');
    w.palco.append(clone);
    void clone.offsetWidth;
    const esc = fb / fa;
    clone.style.transform = `translate(${b.x - a.x}px, ${b.y - a.y}px) scale(${esc})`;
    const ok = await ctx.esperar(820);
    clone.remove();
    if (ok && g === w.ger) para.classList.remove('oculto');
  }

  // ════════════════════════════════════════════════════════════════════════
  // 2.6  controle/busca — goto × só break × return
  // ════════════════════════════════════════════════════════════════════════
  const BS_COD = {
    goto: [
      'for (int i = 0; i < 5; ++i)',
      '    for (int j = 0; j < 5; ++j)',
      '        if (i * j == alvo - 1) {   // 6',
      '            achou_i = i; achou_j = j;',
      '            goto encontrado;       // sai dos DOIS',
      '        }',
      'encontrado:',
      'fmt::print("i={} j={}", achou_i, achou_j);',
    ],
    break: [
      'for (int i = 0; i < 5; ++i)',
      '    for (int j = 0; j < 5; ++j)',
      '        if (i * j == alvo - 1) {',
      '            achou_i = i; achou_j = j;',
      '            break;        // ⚠ sai só do interno',
      '        }',
      '',
      'fmt::print("i={} j={}", achou_i, achou_j);',
    ],
    return: [
      'auto busca = [](int a) -> std::pair<int, int> {',
      '    for (int i = 0; i < 5; ++i)',
      '        for (int j = 0; j < 5; ++j)',
      '            if (i * j == a - 1)',
      '                return {i, j};   // ✓ sai de tudo',
      '    return {-1, -1};',
      '};',
      'auto [bi, bj] = busca(alvo);',
    ],
  };
  const BS_SAIDA = { goto: 4, break: 4, return: 4 };   // linha do salto em cada versão
  const BS_ABA = { goto: 'goto', break: 'só break', return: 'função + return' };
  function bsSeq(modo) {
    const seq = [];
    for (let i = 0; i < 5; i++) {
      for (let j = 0; j < 5; j++) {
        const achou = i * j === 6;
        seq.push({ i, j, achou });
        if (achou && modo !== 'break') return seq;
        if (achou) break;
      }
    }
    return seq;
  }

  Deck.widget('controle/busca', {
    iniciar(raiz, ctx) {
      raiz.classList.add('w-busca');
      const w = raiz._w = { modo: 'goto', prog: 0, ger: 0, ctx };
      w.abas = el('div', { class: 'bs-abas' });
      w.bAbas = Object.keys(BS_COD).map((m) => {
        const b = botao(BS_ABA[m], () => { w.modo = m; bsRodar(raiz, true); }, 'mono');
        b.dataset.m = m;
        w.abas.append(b);
        return b;
      });
      w.cod = el('div', { class: 'bs-codigo' });
      w.grade = criarGrade(5, 5, (l) => 'i=' + l, (c) => 'j=' + c, (l, c) => String(l * c));
      w.seta = el('div', { class: 'bs-seta' });
      w.grade.append(w.seta);
      w.voltas = el('b', { class: 'bs-num' }, '0');
      w.res = el('div', { class: 'bs-res' });
      w.obs = el('div', { class: 'bs-obs' });
      w.porOnde = el('div', { class: 'bs-poronde' });
      raiz.append(
        el('div', { class: 'bs-topo' }, el('span', { class: 'bs-alvo', html: 'procura o 1º <code>(i, j)</code> com <code>i * j == 6</code>' }), w.abas),
        el('div', { class: 'bs-corpo' }, w.cod, w.grade,
          el('div', { class: 'bs-placar' }, el('span', { class: 'bs-rot' }, 'voltas do laço interno'), w.voltas, el('span', { class: 'bs-rot' }, 'resultado'), w.res, w.porOnde, w.obs)));
    },
    passo(raiz, k, ctx, dir) {
      const w = raiz._w;
      w.ctx = ctx;
      w.modo = ['goto', 'goto', 'break', 'return'][k];
      if (k === 0) { w.ger++; w.prog = 0; bsDesenhar(raiz); return; }
      bsRodar(raiz, dir === 1 && podeAnimar(ctx));
    },
  });

  function bsRodar(raiz, animar) {
    const w = raiz._w;
    const g = ++w.ger;
    const n = bsSeq(w.modo).length;
    if (!animar || !podeAnimar(w.ctx)) { w.prog = n + 1; bsDesenhar(raiz); return; }
    w.prog = 0;
    bsDesenhar(raiz);
    (async () => {
      while (w.prog <= n) {
        if (!(await w.ctx.esperar(w.prog === 0 ? 400 : w.prog === n ? 700 : 150)) || g !== w.ger) return;
        w.prog++;
        bsDesenhar(raiz);
      }
    })();
  }

  function bsDesenhar(raiz) {
    const w = raiz._w;
    const seq = bsSeq(w.modo);
    const n = seq.length;
    const p = Math.min(w.prog, n);
    const fim = w.prog > n;
    raiz.dataset.modo = w.modo;
    w.bAbas.forEach((b) => b.classList.toggle('selecionado', b.dataset.m === w.modo));
    // código da versão atual
    if (w.codModo !== w.modo) {
      w.codModo = w.modo;
      w.cod.replaceChildren(...BS_COD[w.modo].map((t, i) => {
        const ln = linha(t, 'bs-ln');
        if (i === BS_SAIDA[w.modo]) ln.classList.add('bs-salto');
        return ln;
      }));
    }
    const ultimo = p ? seq[p - 1] : null;
    w.cod.children[BS_SAIDA[w.modo]].classList.toggle('on', !!(ultimo && ultimo.achou));
    // grade
    const visit = new Set(seq.slice(0, p).map((s) => s.i * 5 + s.j));
    const achados = seq.slice(0, p).filter((s) => s.achou);
    w.grade._cel.forEach((lin, i) => lin.forEach((x, j) => {
      x.className = 'gw-cel';
      if (visit.has(i * 5 + j)) x.classList.add('visitada');
      else if (fim) x.classList.add('nunca');
      if (i * j === 6) x.classList.add('seis');
    }));
    achados.forEach((s, idx) => w.grade._cel[s.i][s.j].classList.add(idx === achados.length - 1 ? 'achou' : 'sobrescrito'));
    if (ultimo && !fim) w.grade._cel[ultimo.i][ultimo.j].classList.add('atual');
    // seta de saída (goto / return) a partir da célula achada
    const sai = fim && w.modo !== 'break' && achados.length;
    if (sai) {
      const c = w.grade._cel[achados[0].i][achados[0].j];
      const q = pos(c, w.grade);
      w.seta.style.top = (q.y + q.h / 2) + 'px';
      w.seta.style.left = (q.x + q.w) + 'px';
      w.seta.style.width = Math.max(30, w.grade.offsetWidth - (q.x + q.w) + 26) + 'px';
    }
    w.seta.classList.toggle('on', !!sai);
    // placar
    w.voltas.textContent = String(p);
    const r = achados.length ? achados[achados.length - 1] : null;
    w.res.innerHTML = fim && r ? `<span class="mono">i=${r.i} j=${r.j}</span>` : '<span class="apagado">…</span>';
    w.res.classList.toggle('errado', fim && w.modo === 'break');
    w.obs.innerHTML = !fim ? '' : '<span>' + (w.modo === 'break'
      ? '⚠ <b>Errado</b>: o <code>break</code> só saiu do laço interno; a busca seguiu e <b>sobrescreveu</b> (2, 3) com (3, 2).'
      : w.modo === 'goto' ? '✓ Saiu dos dois laços na primeira ocorrência.' : '✓ O mesmo (2, 3) — sem <code>goto</code>, sem flag.') + '</span>';
    w.obs.dataset.tipo = !fim ? '' : w.modo === 'break' ? 'erro' : 'ok';
    w.porOnde.innerHTML = !fim ? '' : w.modo === 'goto' ? 'saiu por <code>goto encontrado;</code>'
      : w.modo === 'return' ? 'saiu por <code>return {2, 3};</code>' : 'saiu quando o laço <b>externo</b> acabou';
  }

  // ════════════════════════════════════════════════════════════════════════
  // 2.7  controle/curto — curto-circuito, operando por operando
  // ════════════════════════════════════════════════════════════════════════
  const CT = [
    { esq: ['false', 'true'], op: '&&', dir: 'caro("B")',
      aval: (a) => (a === 'false' ? { curto: true, res: 'false' } : { log: '(avaliou B)', vDir: 'true', res: 'true' }),
      nota: '<code>&amp;&amp;</code> com o 1º <b>falso</b>: o resultado já é <code>false</code> — <code>caro("B")</code> nunca roda.',
      notaSem: 'Com o 1º <b>verdadeiro</b>, o resultado depende do 2º: agora <code>caro("B")</code> roda.' },
    { esq: ['true', 'false'], op: '||', dir: 'caro("C")',
      aval: (a) => (a === 'true' ? { curto: true, res: 'true' } : { log: '(avaliou C)', vDir: 'true', res: 'true' }),
      nota: '<code>||</code> com o 1º <b>verdadeiro</b>: o resultado já é <code>true</code> — <code>caro("C")</code> nunca roda.',
      notaSem: 'Com o 1º <b>falso</b>, o <code>||</code> precisa do 2º: agora <code>caro("C")</code> roda.' },
    { esq: ['p', 'p'], vEsq: ['nullptr → false', '&amp;no → true'], op: '&&', dir: 'p->campo > 0',
      aval: (a, i) => (i === 0 ? { curto: true, res: 'false' } : { log: '(leu p->campo: 5)', vDir: '5 &gt; 0 → true', res: 'true' }),
      vazio: 'nada — p->campo nem foi lido',
      nota: 'O idioma <code>if (p &amp;&amp; p-&gt;campo)</code>: com <code>p</code> nulo, a desreferência <b>nunca acontece</b>.',
      notaSem: 'Com <code>p</code> válido, aí sim <code>p-&gt;campo</code> é lido — com segurança.' },
    { esq: ['false', 'true'], op: '&', dir: 'caro("D")',
      aval: (a) => ({ log: '(avaliou D)', vDir: 'true', res: a === 'true' ? 'true' : 'false' }),
      nota: '⚠ <code>&amp;</code> é bit a bit: <b>sem</b> curto-circuito — os dois lados rodam. Um <code>operator&amp;&amp;</code> sobrecarregado também perde o curto-circuito.' },
  ];

  Deck.widget('controle/curto', {
    iniciar(raiz, ctx) {
      raiz.classList.add('w-curto');
      const w = raiz._w = { modo: 0, esq: 0, fase: 0, ger: 0, ctx };
      w.abas = el('div', { class: 'ct-abas' });
      w.bAbas = CT.map((m, i) => {
        const b = botao(Deck.util.escapar(`${m.esq[0]} ${m.op} ${m.dir}`), () => { w.modo = i; w.esq = 0; ctRodar(raiz, true); }, 'mono');
        w.abas.append(b);
        return b;
      });
      w.chipE = el('button', { type: 'button', class: 'ct-op ct-esq', title: 'clique para trocar o valor' });
      w.chipE.addEventListener('click', (e) => { e.stopPropagation(); w.chipE.blur(); w.esq = 1 - w.esq; ctRodar(raiz, true); });
      w.chipD = el('span', { class: 'ct-op ct-dir' });
      w.op = el('span', { class: 'ct-operador' });
      w.vE = el('span', { class: 'ct-val' });
      w.vD = el('span', { class: 'ct-val' });
      w.console = el('div', { class: 'ct-console' });
      w.res = el('div', { class: 'ct-res' });
      w.nota = el('div', { class: 'ct-nota' });
      raiz.append(
        w.abas,
        el('div', { class: 'ct-expr' },
          el('span', { class: 'ct-decl', html: realcar('bool r =') }),
          el('span', { class: 'ct-col' }, w.chipE, w.vE), w.op, el('span', { class: 'ct-col' }, w.chipD, w.vD),
          el('span', { class: 'ct-decl', html: realcar(';') }),
          el('span', { class: 'ct-dica' }, '↻ clique no 1º operando para trocar o valor')),
        el('div', { class: 'ct-baixo' },
          el('div', { class: 'ct-caixa' }, el('span', { class: 'ct-rot' }, 'efeitos do 2º operando'), w.console),
          el('div', { class: 'ct-caixa' }, el('span', { class: 'ct-rot' }, 'resultado'), w.res),
          w.nota));
    },
    passo(raiz, k, ctx, dir) {
      const w = raiz._w;
      w.ctx = ctx;
      w.modo = Math.max(0, k - 1);
      w.esq = 0;
      if (k === 0) { w.ger++; w.fase = 0; ctDesenhar(raiz); return; }
      ctRodar(raiz, dir === 1 && podeAnimar(ctx));
    },
  });

  function ctRodar(raiz, animar) {
    const w = raiz._w;
    const g = ++w.ger;
    if (!animar || !podeAnimar(w.ctx)) { w.fase = 3; ctDesenhar(raiz); return; }
    w.fase = 0;
    ctDesenhar(raiz);
    (async () => {
      for (let f = 1; f <= 3; f++) {
        if (!(await w.ctx.esperar(f === 1 ? 350 : 750)) || g !== w.ger) return;
        w.fase = f;
        ctDesenhar(raiz);
      }
    })();
  }

  function ctDesenhar(raiz) {
    const w = raiz._w;
    const m = CT[w.modo];
    const a = m.esq[w.esq];
    const r = m.aval(a, w.esq);
    const f = w.fase;
    w.bAbas.forEach((b, i) => b.classList.toggle('selecionado', i === w.modo));
    w.chipE.innerHTML = realcar(a);
    w.op.innerHTML = realcar(m.op);
    w.chipD.innerHTML = realcar(m.dir);
    w.chipE.classList.toggle('ativo', f === 1);
    w.chipE.classList.toggle('feito', f >= 1);
    w.vE.innerHTML = f >= 1 ? (m.vEsq ? m.vEsq[w.esq] : '1º: ' + a) : '';
    const cortado = !!(f >= 2 && r.curto);
    w.chipD.classList.toggle('cortado', cortado);
    w.chipD.classList.toggle('ativo', f === 2 && !r.curto);
    w.chipD.classList.toggle('feito', f >= 2 && !r.curto);
    w.vD.innerHTML = f < 2 ? '' : r.curto ? '✂ nunca avaliado' : (r.vDir || '');
    w.vD.classList.toggle('corte', cortado);
    const logs = f >= 2 && r.log ? [r.log] : [];
    const nada = m.vazio || 'nada — caro() não rodou';
    const itens = logs.length ? logs.map((t) => ({ chave: 'log' + t, t }))
      : [{ chave: f >= 2 ? 'nada:' + nada : 'espera', t: f >= 2 ? nada : '…', vazio: true }];   // a chave leva o texto: troca de aba recria a linha
    sincronizar(w.console, itens, (it) => el('div', { class: 'ct-log' + (it.vazio ? ' vazio' : '') }, it.t));
    w.res.innerHTML = f >= 3 ? `r = <b>${r.res}</b>` : '<span class="apagado">…</span>';
    w.nota.innerHTML = f < 3 ? '' : (r.curto || !m.notaSem) ? m.nota : m.notaSem;
    w.nota.classList.toggle('aviso', w.modo === 3);
  }
})();
