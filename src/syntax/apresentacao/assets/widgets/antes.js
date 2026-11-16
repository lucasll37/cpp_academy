/* =============================================================================
   widgets/antes.js — Capítulo 0 · Antes de começar

   Todos os números, símbolos e mensagens abaixo foram MEDIDOS com o GCC 13.3
   deste repositório (g++ -E, g++ -c, nm -C, objdump -d, ld) — nada de memória.

     antes/pipeline   texto → pré-processador → TU → compilador → .o → linker → exe
     antes/tu         a explosão da TU e o custo de um header incluído em N .cpp
     antes/linker     linkage e ODR do ponto de vista do linker (nm -C + ld reais)
     antes/glossario  as palavras que aparecem o tempo todo, e onde cada uma volta
     antes/vida       escopo (do nome, no texto) × tempo de vida (do objeto, no tempo)
     antes/programa   o menor programa completo, peça por peça, com o capítulo de cada uma
     antes/aposta     as 11 armadilhas: a turma aposta, depois revela
     antes/flags      que flag pega que armadilha (as 32 combinações, compiladas)
   ============================================================================= */
(function () {
  'use strict';
  const { el, svg } = Deck.util;

  // ── utilitários ──────────────────────────────────────────────────────────
  // posição de `no` relativa a `base` (que precisa ser posicionada), em px do
  // palco: offsetLeft/Top ignoram a escala do palco, então o cálculo é exato.
  function pos(no, base) {
    let x = 0, y = 0, n = no;
    while (n && n !== base) { x += n.offsetLeft; y += n.offsetTop; n = n.offsetParent; }
    return { x, y, w: no.offsetWidth, h: no.offsetHeight };
  }

  // código realçado, uma <span class="l"> por linha (para animar linha a linha)
  function cod(texto, classe, lang) {
    const linhas = Deck.util.realcar(texto, lang || 'cpp').split('\n');
    return el('pre', { class: 'a-cod' + (classe ? ' ' + classe : '') },
      linhas.map((h) => el('span', { class: 'l', html: h || '&#8203;' })));
  }

  const fmt = (n) => Math.round(n).toLocaleString('pt-BR');
  // replaceChildren converteria null em "null": filtre antes
  const filhos = (...xs) => xs.flat().filter((x) => x != null && x !== false);
  // a palavra das mensagens reais do ld ("undefined reference") e do UBSan
  const UNDEF = 'undefined';

  // contador animado e cancelável: uma nova chamada cancela a anterior
  function contar(no, de, ate, ms, semAnimar) {
    const tok = (no._tok = (no._tok || 0) + 1);
    if (semAnimar) { no.textContent = fmt(ate); return; }
    const t0 = performance.now();
    const quadro = (t) => {
      if (no._tok !== tok) return;
      const p = Math.min(1, (t - t0) / ms);
      no.textContent = fmt(de + (ate - de) * (1 - Math.pow(1 - p, 3)));
      if (p < 1) requestAnimationFrame(quadro);
    };
    requestAnimationFrame(quadro);
  }
  function fixar(no, texto) { no._tok = (no._tok || 0) + 1; no.textContent = texto; }

  // as cores dos capítulos são as do motor (deck.css)
  const CAPS = {
    antes: '#38bdf8', declaracoes: '#7c8cff', controle: '#a78bfa', funcoes: '#e879f9', tipos: '#f472b6',
    classes: '#fb7185', organizacao: '#fb923c', expressoes: '#facc15', generico: '#a3e635', encerramento: '#40e0d0',
  };
  // [slug, rótulo] → link para o capítulo; [slug, rótulo, '#/…'] → link interno;
  // [null, rótulo, href] → link para fora (README de outro subprojeto)
  function chip([slug, texto, href]) {
    const ext = !!href && !href.startsWith('#');
    const n = el('a', { class: 'a-chip' + (ext ? ' repo' : ''), href: href || `#/${slug}/1`, target: ext ? '_blank' : null }, texto);
    n.style.setProperty('--c', slug ? CAPS[slug] : 'var(--cor)');
    return n;
  }

  const dots = () => el('span', { class: 'dots' }, el('i'), el('i'), el('i'));

  // O motor não realça <code class="cpp"> inline: realçamos os deste capítulo
  // antes de ele montar os slides (o texto continua o mesmo para a busca).
  document.querySelectorAll('#cap-antes code.cpp').forEach((c) => {
    c.innerHTML = Deck.util.realcar(c.textContent, 'cpp');
  });

  /* ═════════════════════════════════════════════════════════════════════════
     1. antes/pipeline — o caminho do código até o programa
     ═════════════════════════════════════════════════════════════════════════ */
  const face = (f, ...filhos) => el('div', { class: 'face', dataset: { f } }, ...filhos);
  const linhaHex = (hex, asm, nota) => el('div', { class: 'hexl' },
    el('span', { class: 'hex' }, hex), el('span', { class: 'asm' }, asm), nota ? el('span', { class: 'nota' }, nota) : null);
  const linhaSim = (l, nome, nota, cls) => el('div', { class: 'siml' + (cls ? ' ' + cls : ''), dataset: { l, n: nome } },
    el('b', { class: 'letra' }, l), el('span', { class: 'nome' }, nome), nota ? el('span', { class: 'nota' }, nota) : null);

  function cartao(cls, nomes, faces, ...extra) {
    const tit = el('span', { class: 'nome' }, nomes.fonte);
    const c = el('div', { class: 'arq ' + cls }, el('div', { class: 'tit' }, dots(), tit), el('div', { class: 'faces' }, faces), ...extra);
    c._tit = tit; c._nomes = nomes; c._faces = faces;
    return c;
  }

  const LEG_PIPE = [
    'Três arquivos de <b>texto</b>. Nada foi compilado ainda — e o header nunca é compilado sozinho.',
    '<b>① Pré-processador</b>: cola o texto de <code>dobro.hpp</code> no lugar de cada <code>#include</code> e troca <code>VEZES</code> por <code>3</code>. Cada .cpp vira uma <b>TU</b>.',
    '<b>② Compilador</b>: traduz cada TU <b>sozinha</b>, sem ver as outras. A chamada a <code>dobro</code> vira um buraco literal — <code>e8 00 00 00 00</code> — e um <code>U</code> na tabela.',
    '<b>③ Linker</b>: casa o <code>U dobro(int)</code> de main.o com o <code>T dobro(int)</code> de dobro.o e, no executável, preenche o buraco com a distância: <code>03 00 00 00</code>.',
    '<b>Executável</b>: <code>dobro(3)</code> devolve 6, e <code>main</code> entrega o 6 ao shell como código de saída.',
  ];

  function fioPipe(raiz) {
    const b = raiz._banc;
    if (!b.offsetWidth) return;
    const pu = pos(raiz._u, b), pt = pos(raiz._t, b);
    const x1 = pu.x + pu.w + 4, y1 = pu.y + pu.h / 2, x2 = pt.x - 4, y2 = pt.y + pt.h / 2;
    const dx = Math.max(60, (x2 - x1) * 0.5);
    raiz._svg.setAttribute('viewBox', `0 0 ${b.offsetWidth} ${b.offsetHeight}`);
    raiz._path.setAttribute('d', `M ${x1} ${y1} C ${x1 + dx} ${y1} ${x2 - dx} ${y2} ${x2} ${y2}`);
    raiz._p1.setAttribute('cx', x1); raiz._p1.setAttribute('cy', y1);
    raiz._p2.setAttribute('cx', x2); raiz._p2.setAttribute('cy', y2);
    raiz._ld.style.top = `${(y1 + y2) / 2 - raiz._ld.offsetHeight / 2}px`;
  }

  function limparFantasmas(raiz) { raiz._fantasmas.forEach((f) => f.remove()); raiz._fantasmas = []; }

  // a linha do header "voa" para dentro das duas TUs
  function colar(raiz) {
    const b = raiz._banc;
    const src = raiz._H.querySelector('.l:nth-child(2)');
    const ps = pos(src, b);
    [raiz._A, raiz._C].forEach((c, i) => {
      const dst = c.querySelector('.face[data-f="tu"] .l');
      const pd = pos(dst, b);
      const g = el('div', { class: 'fantasma', html: src.innerHTML });
      g.style.left = ps.x + 'px'; g.style.top = ps.y + 'px';
      b.append(g); raiz._fantasmas.push(g);
      const dx = pd.x - ps.x, dy = pd.y - ps.y;
      const anim = g.animate([
        { transform: 'translate(0, 0)', opacity: 0 },
        { transform: 'translate(0, -16px) scale(1.06)', opacity: 1, offset: 0.22 },
        { transform: `translate(${dx}px, ${dy}px)`, opacity: 1, offset: 0.86 },
        { transform: `translate(${dx}px, ${dy}px)`, opacity: 0 },
      ], { duration: 1200, delay: i * 160, easing: 'cubic-bezier(.22, 1, .36, 1)', fill: 'both' });
      anim.onfinish = () => g.remove();
    });
  }

  Deck.widget('antes/pipeline', {
    iniciar(raiz) {
      raiz.classList.add('w-pipe');
      const trilho = el('div', { class: 'trilho' });
      raiz._nos = []; raiz._maqs = [];
      [['.cpp', 'texto'], ['TU', 'unidade de tradução'], ['.o', 'arquivo objeto'], ['exe', 'executável']].forEach(([s, n], i) => {
        const no = el('div', { class: 'no' }, el('b', null, s), el('span', null, n));
        raiz._nos.push(no); trilho.append(no);
        if (i < 3) {
          const m = el('div', { class: 'maq' },
            el('span', { class: 'rot' }, el('i', null, String(i + 1)), ['pré-processador', 'compilador', 'linker'][i]),
            el('span', { class: 'fio' }));
          raiz._maqs.push(m); trilho.append(m);
        }
      });

      const troca = (a, b) => el('span', { class: 'troca' }, el('span', { class: 'v0' }, a), el('span', { class: 'v1' }, b));
      // no passo do linker a nota do U troca: o buraco foi ligado à definição de dobro.o
      raiz._u = linhaSim('U', 'dobro(int)', troca('buraco: está em outra TU', 'ligado ao T de dobro.o'), 'u');
      raiz._t = linhaSim('T', 'dobro(int)', 'definido aqui', 't');

      // `link`: o linker não reescreve os .o — o código deles é copiado para o prog,
      // e é lá que o buraco é preenchido; o título do cartão diz isso no passo 3
      raiz._A = cartao('a', { fonte: 'main.cpp', tu: 'main.cpp → TU', obj: 'main.o', link: 'main.o → prog' }, [
        face('fonte', cod('#include "dobro.hpp"\n#define VEZES 3\nint main() { return dobro(VEZES); }')),
        face('tu', cod('⟦int dobro(int x);⟧\nint main() { return dobro(⟦3⟧); }'),
          el('div', { class: 'marcas-tu' }, el('span', { class: 'm1' }, '▲ colado de dobro.hpp'), el('span', { class: 'm2' }, '▲ era VEZES'))),
        face('obj', el('div', { class: 'obj' },
          el('div', { class: 'sec' }, 'código de máquina · trecho do objdump -d'),
          linhaHex('bf 03 00 00 00', 'mov  $0x3,%edi'),
          el('div', { class: 'hexl chamada' }, el('span', { class: 'hex' }, 'e8 ', troca('00 00 00 00', '03 00 00 00')),
            el('span', { class: 'asm' }, 'call ', troca('???', '<dobro(int)>'))),
          el('div', { class: 'sec' }, 'símbolos · nm -C main.o'),
          raiz._u,
          linhaSim('T', 'main', 'definido aqui'))),
      ]);
      raiz._H = cartao('h', { fonte: 'dobro.hpp' }, [face('fonte', cod('#pragma once\nint dobro(int x);'))],
        el('div', { class: 'consumido' }, 'colado — não vira .o'));
      raiz._C = cartao('c', { fonte: 'dobro.cpp', tu: 'dobro.cpp → TU', obj: 'dobro.o', link: 'dobro.o → prog' }, [
        face('fonte', cod('#include "dobro.hpp"\nint dobro(int x) { return x * 2; }')),
        face('tu', cod('⟦int dobro(int x);⟧\nint dobro(int x) { return x * 2; }'),
          el('div', { class: 'marcas-tu' }, el('span', { class: 'm1' }, '▲ colado de dobro.hpp'))),
        face('obj', el('div', { class: 'obj' },
          el('div', { class: 'sec' }, 'código de máquina · trecho do objdump -d'),
          // trecho contíguo do objdump -d (offsets b, e, 10, 11): sem pular o pop
          linhaHex('8b 45 fc', 'mov  -0x4(%rbp),%eax'),
          linhaHex('01 c0', 'add  %eax,%eax', '← x*2 = x+x'),
          linhaHex('5d', 'pop  %rbp'),
          linhaHex('c3', 'ret'),
          el('div', { class: 'sec' }, 'símbolos · nm -C dobro.o'),
          raiz._t)),
      ]);
      raiz._E = el('div', { class: 'arq exe' },
        el('div', { class: 'tit' }, dots(), el('span', { class: 'nome' }, 'prog')),
        el('div', { class: 'exe-corpo' },
          el('div', { class: 'cmd' }, el('span', { class: 'pr' }, '$ '), './prog; echo $?'),
          el('div', { class: 'res' }, '6')));
      raiz._ld = el('div', { class: 'ld' }, el('b', null, 'ld'), el('span', null, 'linker'));
      raiz._path = svg('path', { class: 'fio-lig', pathLength: '1' });
      raiz._p1 = svg('circle', { class: 'ponta', r: '6' });
      raiz._p2 = svg('circle', { class: 'ponta', r: '6' });
      raiz._svg = svg('svg', { class: 'fios', 'aria-hidden': 'true', preserveAspectRatio: 'none' }, raiz._path, raiz._p1, raiz._p2);
      raiz._banc = el('div', { class: 'bancada' }, raiz._A, raiz._H, raiz._C, raiz._svg, raiz._ld, raiz._E);
      raiz._legs = LEG_PIPE.map((h) => el('p', { html: h }));
      raiz._fantasmas = [];
      raiz.append(trilho, raiz._banc, el('div', { class: 'legenda' }, raiz._legs));
    },
    passo(raiz, k, ctx, dir) {
      ['k1', 'k2', 'k3', 'k4'].forEach((c, i) => raiz.classList.toggle(c, k >= i + 1));
      const noAtivo = [0, 1, 2, 2, 3][k], maqAtiva = [-1, 0, 1, 2, -1][k];
      raiz._nos.forEach((n, i) => { n.classList.toggle('ativo', i === noAtivo); n.classList.toggle('feito', i < noAtivo); });
      raiz._maqs.forEach((m, i) => { m.classList.toggle('ativo', i === maqAtiva); m.classList.toggle('feito', k === 4 || i < maqAtiva); });
      const fase = k === 0 ? 'fonte' : k === 1 ? 'tu' : 'obj';
      [raiz._A, raiz._C].forEach((c) => {
        c._faces.forEach((f) => f.classList.toggle('on', f.dataset.f === fase));
        c._tit.textContent = c._nomes[k >= 3 ? 'link' : fase];
      });
      raiz._H._faces[0].classList.add('on');
      raiz._legs.forEach((p, i) => p.classList.toggle('on', i === k));
      limparFantasmas(raiz);
      fioPipe(raiz);
      if (k === 1 && dir > 0 && !ctx.reduzido) colar(raiz);
    },
    entrar(raiz) { fioPipe(raiz); },
    sair(raiz) { limparFantasmas(raiz); },
  });

  /* ═════════════════════════════════════════════════════════════════════════
     2. antes/tu — a TU é o .cpp com tudo colado (medido: g++ -std=c++20 -E | wc -l;
        sem o -std=c++20, o padrão gnu++17 dá menos: 42.246 para o minimo.cpp)
     ═════════════════════════════════════════════════════════════════════════ */
  const HEADERS = [['<cstdio>', 1049], ['<vector>', 26258], ['<algorithm>', 28270], ['<string>', 31825], ['<iostream>', 43165], ['<fmt/core.h>', 58209]];
  const MAIOR = 58209;

  function desenharTu(raiz, animar) {
    const [nome, linhas] = HEADERS[raiz._h];
    raiz._chips.forEach((c, i) => c.setAttribute('aria-pressed', String(i === raiz._h)));
    raiz._tiles.forEach((t, i) => {
      t.classList.toggle('on', i < raiz._n);
      t.style.setProperty('--enche', String(Math.max(0.03, linhas / MAIOR)));
    });
    raiz._qtdTxt.textContent = String(raiz._n);
    raiz._hTxt.textContent = nome;
    raiz._lTxt.textContent = fmt(linhas);
    const total = raiz._n * linhas;
    if (raiz.classList.contains('k2')) contar(raiz._total, animar ? 0 : total, total, 1500, !animar);
    else fixar(raiz._total, '—');
  }

  Deck.widget('antes/tu', {
    iniciar(raiz) {
      raiz.classList.add('w-tu');
      raiz._numTu = el('span', { class: 'num' }, '?');
      const explosao = el('div', { class: 'explosao' },
        el('div', { class: 'doc fonte' }, el('span', { class: 'rot' }, 'minimo.cpp'), el('span', { class: 'num' }, '18'), el('span', { class: 'un' }, 'linhas que você escreveu')),
        el('div', { class: 'seta-tu' }, el('code', null, 'g++ -std=c++20 -E | wc -l'), el('i')),
        el('div', { class: 'doc tu' }, el('span', { class: 'rot' }, 'a TU de minimo.cpp'), raiz._numTu, el('span', { class: 'un' }, 'linhas que o compilador lê')),
        el('div', { class: 'fatia' },
          el('div', { class: 'barra' }, el('i', { class: 'seu' }), el('span', { class: 'cola' }, '<fmt/core.h> + <string>, colados')),
          el('p', { html: 'o seu código: <b>0,03%</b> da TU' })));

      raiz._tiles = Array.from({ length: 50 }, (_, i) => { const t = el('i', { class: 'cpp' }, el('b')); t.style.setProperty('--i', i); return t; });
      raiz._chips = HEADERS.map(([h], i) => el('button', { class: 'botao chip-h', type: 'button',
        onclick: () => { raiz._h = i; desenharTu(raiz, true); } }, h));
      raiz._qtd = el('input', { type: 'range', min: '1', max: '50', value: '50', 'aria-label': 'quantos .cpp incluem o header' });
      raiz._qtd.addEventListener('input', () => { raiz._n = +raiz._qtd.value; desenharTu(raiz, false); });
      raiz._qtdTxt = el('b', null, '50');
      raiz._hTxt = el('code', null, '<fmt/core.h>');
      raiz._lTxt = el('b', null, '58.209');
      raiz._total = el('span', { class: 'total' }, '—');
      const custo = el('div', { class: 'custo' },
        el('div', { class: 'grade-cpp' }, raiz._tiles),
        el('div', { class: 'conta' },
          el('p', { class: 'eq' }, raiz._qtdTxt, ' .cpp × ', raiz._lTxt, ' linhas de ', raiz._hTxt),
          el('div', { class: 'res' }, raiz._total, el('small', null, 'linhas para compilar')),
          el('div', { class: 'ctrl' }, el('div', { class: 'chips' }, raiz._chips),
            el('label', { class: 'qtd' }, el('span', null, 'nº de .cpp'), raiz._qtd))));
      raiz.append(explosao, custo);
      raiz._h = 5; raiz._n = 50;
    },
    passo(raiz, k, ctx, dir) {
      raiz.classList.toggle('k1', k >= 1);
      raiz.classList.toggle('k2', k >= 2);
      if (k >= 1) contar(raiz._numTu, 18, 58226, 1500, !(k === 1 && dir > 0 && !ctx.reduzido));
      else fixar(raiz._numTu, '?');
      raiz._h = 5; raiz._n = 50; raiz._qtd.value = '50';
      desenharTu(raiz, k === 2 && dir > 0 && !ctx.reduzido);
    },
  });

  /* ═════════════════════════════════════════════════════════════════════════
     3. antes/linker — linkage e ODR: o que o linker enxerga (nm -C e ld reais)
     ═════════════════════════════════════════════════════════════════════════ */
  const B_USA = '#include <cstdio>\nextern int contador;\nvoid incrementa();\nint main() {\n    incrementa(); incrementa();\n    std::printf("contador = %d\\n", contador);\n}';
  const SIM_B_USA = [['U', 'incrementa()'], ['U', 'contador'], ['T', 'main'], ['U', 'printf']];
  const A_DEF = 'int contador = 0;\nvoid incrementa() { ++contador; }';
  // trecho (marcado com "… trecho" na linha do comando): a saída real do ld também traz
  // dois avisos (relocation against `contador' / DT_TEXTREL) e a linha do collect2
  const ERRO_UNDEF = ['/usr/bin/ld: b.o: in function `main\':', `b.cpp:(.text+0x14): ${UNDEF} reference to \`contador'`];
  const INL_A = 'inline int dobro(int x) { return x * 2; }\nint via_a() { return dobro(10); }';

  const CENARIOS = {
    externa: {
      aba: 'externa',
      a: { cod: A_DEF, sim: [['T', 'incrementa()'], ['B', 'contador']] },
      b: { cod: B_USA, sim: SIM_B_USA },
      fios: [['incrementa()', 'incrementa()', 'ok'], ['contador', 'contador', 'ok']],
      cmd: '$ g++ a.o b.o -o prog && ./prog', saida: ['contador = 2'], tipo: 'ok',
      licao: 'Linkage <b>externa</b>: <code>B contador</code> e <code>T incrementa()</code> valem para o programa inteiro. O linker liga a eles os <code>U</code> de mesmo nome em b.o.',
    },
    interna: {
      aba: 'static',
      a: { cod: 'static int contador = 0;\nvoid incrementa() { ++contador; }', sim: [['T', 'incrementa()'], ['b', 'contador']] },
      b: { cod: B_USA, sim: SIM_B_USA },
      fios: [['incrementa()', 'incrementa()', 'ok'], ['contador', 'contador', 'bloq']],
      cmd: '$ g++ a.o b.o -o prog', trecho: true, saida: ERRO_UNDEF, tipo: 'erro',
      licao: '<code>static</code> dá linkage <b>interna</b>: o símbolo existe, mas em minúscula (<code>b</code>) — só a.o o enxerga. Para b.o, <code>contador</code> não existe.',
    },
    anonimo: {
      aba: 'namespace { }',
      a: { cod: 'namespace { int contador = 0; }\nvoid incrementa() { ++contador; }', sim: [['T', 'incrementa()'], ['b', '(anonymous namespace)::contador']] },
      b: { cod: B_USA, sim: SIM_B_USA },
      fios: [['incrementa()', 'incrementa()', 'ok'], ['contador', '(anonymous namespace)::contador', 'bloq']],
      cmd: '$ g++ a.o b.o -o prog', trecho: true, saida: ERRO_UNDEF, tipo: 'erro',
      licao: 'Namespace anônimo: o mesmo efeito do <code>static</code> — linkage <b>interna</b> para tudo o que está dentro. É o jeito moderno (Cap.&nbsp;6).',
    },
    duas: {
      aba: '2 definições',
      a: { cod: A_DEF, sim: [['T', 'incrementa()'], ['B', 'contador']] },
      b: { cod: '#include <cstdio>\nint contador = 0;\nvoid incrementa();\nint main() {\n    incrementa();\n    std::printf("contador = %d\\n", contador);\n}',
        sim: [['U', 'incrementa()'], ['B', 'contador'], ['T', 'main'], ['U', 'printf']] },
      fios: [['incrementa()', 'incrementa()', 'ok'], ['contador', 'contador', 'conflito']],
      cmd: '$ g++ a.o b.o -o prog', trecho: true, saida: ['/usr/bin/ld: b.o:(.bss+0x0): multiple definition of `contador\'; a.o:(.bss+0x0): first defined here'], tipo: 'erro',
      licao: 'Duas <b>definições</b> do mesmo nome externo: a ODR foi violada — e este caso o linker <b>pega</b>.',
    },
    inline: {
      aba: 'inline idêntica',
      a: { cod: INL_A, sim: [['W', 'dobro(int)'], ['T', 'via_a()']] },
      b: { cod: 'inline int dobro(int x) { return x * 2; }\nint via_b() { return dobro(10); }', sim: [['W', 'dobro(int)'], ['T', 'via_b()']] },
      fios: [['dobro(int)', 'dobro(int)', 'funde']],
      cmd: '$ g++ main.o a.o b.o -o prog && ./prog', saida: ['via_a = 20   via_b = 20'], tipo: 'ok',
      licao: '<code>inline</code> pode se repetir: o símbolo é <b>fraco</b> (<code>W</code>) e o linker guarda <b>uma</b> cópia. Vale porque as duas são idênticas, token a token.',
    },
    'inline-dif': {
      aba: 'inline diferente',
      a: { cod: INL_A, sim: [['W', 'dobro(int)'], ['T', 'via_a()']] },
      b: { cod: 'inline int dobro(int x) { return x + x + 1; }\nint via_b() { return dobro(10); }', sim: [['W', 'dobro(int)'], ['T', 'via_b()']] },
      fios: [['dobro(int)', 'dobro(int)', 'silencio']],
      cmd: null,
      tabela: [['g++ main.o a.o b.o', 'via_a = 20   via_b = 20'], ['g++ main.o b.o a.o', 'via_a = 21   via_b = 21'], ['g++ -O2 (cada TU expande a sua)', 'via_a = 20   via_b = 21']],
      tipo: 'aviso',
      licao: 'Corpos <b>diferentes</b>, o mesmo <code>W dobro(int)</code>: o linker não tem como notar. <i>Ill-formed, no diagnostic required</i> — o resultado muda com a ordem do link e com o <code>-O</code>.',
    },
  };
  const NOTA_LETRA = { T: 'código global', B: 'dado global', b: 'LOCAL: não exportado', U: 'buraco: vem de fora', W: 'fraco: pode repetir' };
  const ROT_FIO = { bloq: '✗ não enxerga', conflito: '✗ 2 definições', funde: '✓ guarda 1 cópia', silencio: '? guarda 1 — qual?' };

  // `linhas` = altura comum do código nas duas colunas: assim as tabelas do nm
  // começam na mesma altura e cada fio do linker sai na horizontal.
  function montarTu(col, arq, obj, d, linhas) {
    const pre = cod(d.cod, 'mini');
    pre.style.minHeight = `calc(${linhas} * 1.42em + 23px)`;
    col.replaceChildren(
      el('div', { class: 'tit' }, dots(), el('span', { class: 'nome' }, arq)),
      pre,
      el('div', { class: 'tab-sim' },
        el('div', { class: 'cab' }, el('span', null, '$ nm -C ' + obj)),
        d.sim.map(([l, n]) => linhaSim(l, n, n === 'printf' ? 'buraco: vem da libc' : NOTA_LETRA[l]))));
  }

  function fiosLig(raiz) {
    const m = raiz._mesa;
    if (!m.offsetWidth) return;
    const c = CENARIOS[raiz._ids[raiz._i]];
    raiz._svg.setAttribute('viewBox', `0 0 ${m.offsetWidth} ${m.offsetHeight}`);
    raiz._svg.replaceChildren();
    raiz._rots.replaceChildren();
    const linha = (col, nome) => Array.from(col.querySelectorAll('.siml')).find((r) => r.dataset.n === nome);
    c.fios.forEach(([nb, na, tipo], j) => {
      const rb = linha(raiz._colB, nb), ra = linha(raiz._colA, na);
      if (!rb || !ra) return;
      const pb = pos(rb, m), pa = pos(ra, m);
      const x1 = pb.x - 4, y1 = pb.y + pb.h / 2, x2 = pa.x + pa.w + 4, y2 = pa.y + pa.h / 2;
      const xm = (x1 + x2) / 2, ym = (y1 + y2) / 2;
      const xf = xm - 6;                                   // onde o fio bloqueado para
      const d = tipo === 'bloq'
        ? `M ${x1} ${y1} C ${x1 - 50} ${y1} ${xf + 50} ${ym} ${xf} ${ym}`
        : `M ${x1} ${y1} C ${x1 - 90} ${y1} ${x2 + 90} ${y2} ${x2} ${y2}`;
      const p = svg('path', { d, class: 'fio ' + tipo, pathLength: '1' });
      p.style.setProperty('--atraso', (0.15 + j * 0.2) + 's');
      raiz._svg.append(p, svg('circle', { class: 'ponta ' + tipo, cx: x1, cy: y1, r: '5' }));
      if (tipo === 'bloq') {
        raiz._svg.append(svg('path', { class: 'barreira', d: `M ${xf - 9} ${ym - 9} l 18 18 m 0 -18 l -18 18` }));
      } else raiz._svg.append(svg('circle', { class: 'ponta ' + tipo, cx: x2, cy: y2, r: '5' }));
      if (ROT_FIO[tipo]) {
        const r = el('span', { class: 'rot-fio ' + tipo }, ROT_FIO[tipo]);
        r.style.left = (tipo === 'bloq' ? xf : xm) + 'px';
        r.style.top = ym + 'px';
        r.style.setProperty('--atraso', (0.5 + j * 0.2) + 's');
        raiz._rots.append(r);
      }
    });
  }

  function cenario(raiz, i) {
    const c = CENARIOS[raiz._ids[i]];
    raiz._i = i;
    raiz._abas.forEach((a, j) => a.setAttribute('aria-pressed', String(j === i)));
    raiz.dataset.tipo = c.tipo;
    const linhas = Math.max(c.a.cod.split('\n').length, c.b.cod.split('\n').length);
    montarTu(raiz._colA, 'a.cpp', 'a.o', c.a, linhas);
    montarTu(raiz._colB, 'b.cpp', 'b.o', c.b, linhas);
    const term = c.tabela
      ? el('div', { class: 'term tab' }, c.tabela.map(([cmd, out]) => el('div', { class: 'par' }, el('span', { class: 'c' }, cmd), el('span', { class: 's' }, out))))
      : el('div', { class: 'term' }, el('div', { class: 'cmdl' }, el('span', null, c.cmd), c.trecho ? el('span', { class: 'trecho' }, '… trecho') : null),
        c.saida.map((s) => el('div', { class: 'out' }, s)));
    raiz._ver.replaceChildren(
      el('span', { class: 'ic' }, { ok: '✓', erro: '✗', aviso: '⚠' }[c.tipo]),
      term,
      el('p', { class: 'licao', html: c.licao }));
    raiz._ver.classList.remove('surge'); void raiz._ver.offsetWidth; raiz._ver.classList.add('surge');
    fiosLig(raiz);
  }

  Deck.widget('antes/linker', {
    iniciar(raiz) {
      raiz.classList.add('w-lig');
      raiz._ids = (raiz.dataset.cenarios || 'externa').split(',').map((s) => s.trim()).filter((s) => CENARIOS[s]);
      raiz._abas = raiz._ids.map((id, i) => el('button', { class: 'botao aba', type: 'button', onclick: () => cenario(raiz, i) }, CENARIOS[id].aba));
      raiz._colA = el('div', { class: 'tu-col a' });
      raiz._colB = el('div', { class: 'tu-col b' });
      raiz._svg = svg('svg', { class: 'fios', 'aria-hidden': 'true', preserveAspectRatio: 'none' });
      raiz._rots = el('div', { class: 'rots' });
      raiz._mesa = el('div', { class: 'mesa' }, raiz._colA,
        el('div', { class: 'meio' }, el('div', { class: 'ld' }, el('b', null, 'ld'), el('span', null, 'linker'))),
        raiz._colB, raiz._svg, raiz._rots);
      raiz._ver = el('div', { class: 'veredito' });
      const LEG = { T: 'código', B: 'dado', b: 'local (só a TU)', U: 'buraco', W: 'fraco' };
      const legenda = el('div', { class: 'leg-letras' }, el('span', { class: 'r' }, 'nm:'), ['T', 'B', 'b', 'U', 'W'].map((l) =>
        el('span', { dataset: { l } }, el('b', null, l), LEG[l])));
      // data-regra: no lugar da legenda das letras, a regra da ODR em três pílulas
      const esquerda = 'regra' in raiz.dataset
        ? el('div', { class: 'regra' }, el('span', { class: 'pill ok' }, 'declarar: N vezes'), el('span', { class: 'pill info' }, 'definir: UMA vez'),
          el('span', { class: 'pill aviso' }, 'class · template · inline: repetem se idênticos'))
        : legenda;
      raiz.append(el('div', { class: 'topo-lig' }, esquerda, el('div', { class: 'abas' }, raiz._abas)), raiz._mesa, raiz._ver);
    },
    passo(raiz, k) { cenario(raiz, Math.min(k, raiz._ids.length - 1)); },
    entrar(raiz) { fiosLig(raiz); },
  });

  /* ═════════════════════════════════════════════════════════════════════════
     4. antes/glossario — as palavras que aparecem o tempo todo
     ═════════════════════════════════════════════════════════════════════════ */
  const FAMILIAS = { contrato: 'o contrato da linguagem', nomes: 'nomes e objetos', valores: 'valores e tipos', recursos: 'recursos e erros', ferramenta: 'ferramenta' };
  const TERMOS = [
    { fam: 'contrato', nome: 'UB', ext: `${UNDEF} behavior · comportamento indefinido`,
      def: 'Situação em que o padrão não define <b>nada</b>: o programa pode funcionar, travar ou piorar depois de otimizado. Não é “valor aleatório”: é <b>ausência total de contrato</b>.',
      ex: 'int x;\nreturn x;   // ler x: UB', onde: [['declaracoes', 'Cap. 1 · 1.2'], ['expressoes', 'Cap. 7 · 7.7'], [null, 'src/undefined_behavior/', '../../undefined_behavior/README.md']] },
    { fam: 'nomes', nome: 'Escopo', ext: 'onde o NOME é visível',
      def: 'O trecho de <b>texto</b> em que um nome pode ser usado: o bloco <code>{ }</code>, a classe, o namespace.',
      ex: '{\n    int x = 1;   // x visível daqui…\n}                // …até aqui', onde: [['declaracoes', 'Cap. 1 · 1.5'], ['controle', 'Cap. 2 · 2.1']] },
    { fam: 'nomes', nome: 'Tempo de vida', ext: 'quando o OBJETO existe',
      def: 'O intervalo de <b>tempo</b> em que o objeto existe na memória. Escopo é sobre <i>o nome</i>; tempo de vida, sobre <i>o objeto</i> — e os dois podem divergir.',
      ex: 'static int n = 0;  // nome: só o bloco\n                   // objeto: o programa todo', onde: [['declaracoes', 'Cap. 1 · 1.5'], ['funcoes', 'Cap. 3 · 3.5']] },
    { fam: 'contrato', nome: 'Narrowing', ext: 'estreitamento',
      def: 'Conversão que pode perder informação: <code>double → int</code>, <code>int → char</code>, <code>long → short</code>. Com <code>=</code> acontece em silêncio; com <code>{ }</code> vira erro.',
      ex: 'int t = 3.99;   // 3, em silêncio\nint u{3.99};    // ✗ erro: narrowing', onde: [['declaracoes', 'Cap. 1 · 1.2'], ['expressoes', 'Cap. 7 · 7.4']] },
    { fam: 'valores', nome: 'Categoria de valor', ext: 'lvalue · prvalue · xvalue',
      def: 'Toda expressão tem um <b>tipo</b> e uma <b>categoria</b>. A categoria responde: tem identidade (dá para tomar o endereço)? Pode ser saqueada (roubar as entranhas)?',
      ex: 'x              // lvalue: tem nome\nx + 1          // prvalue: temporário\nstd::move(x)   // xvalue: pode saquear', onde: [['expressoes', 'Cap. 7 · 7.3'], [null, 'src/value_semantics/', '../../value_semantics/README.md']] },
    { fam: 'nomes', nome: 'ADL', ext: 'argument-dependent lookup',
      def: 'Ao resolver <code>f(x)</code>, o compilador procura <code>f</code> também nos namespaces <b>dos argumentos</b> — não só no escopo atual.',
      ex: 'std::string s = "oi";\nstd::cout << s;   // operator<< é função livre de std:\n                  // achada pela ADL, sem escrever std::', onde: [['organizacao', 'Cap. 6 · 6.1']] },
    { fam: 'recursos', nome: 'Stack unwinding', ext: 'desempilhamento',
      def: 'Entre o <code>throw</code> e o <code>catch</code>, o C++ desmonta a pilha quadro a quadro e roda o <b>destrutor</b> de cada objeto local já construído. É o mecanismo que faz o RAII funcionar.',
      ex: '{ Sentinela s; throw Erro{}; }\n// ~Sentinela() roda ANTES do catch', onde: [['generico', 'Cap. 8 · 8.7']] },
    { fam: 'recursos', nome: 'RAII', ext: 'resource acquisition is initialization',
      def: 'O construtor adquire o recurso; o destrutor o devolve. Como o destrutor roda em <b>qualquer</b> saída de escopo — inclusive por exceção —, o recurso nunca vaza.',
      ex: '{\n    std::lock_guard trava{m};  // trava\n}   // destrutor: destrava, aconteça o que for', onde: [['controle', 'Cap. 2 · 2.6'], ['generico', 'Cap. 8 · 8.7']] },
    // `rot` (HTML): o botão quebra a linha no "·", nunca no hífen de standard-layout
    { fam: 'valores', nome: 'Trivial · standard-layout', rot: 'Trivial · <span class="nw">standard-layout</span>', ext: 'duas propriedades independentes de um tipo',
      def: '<b>Trivial</b>: construir, copiar e destruir só mexem bytes. <b class="nw">Standard-layout</b>: arranjo previsível, compatível com C. Só <i>trivially copyable</i> vai para <code>memcpy</code>, arquivo ou socket.',
      ex: 'struct P { int a; double b; };\nstd::is_trivial_v<P>           // true\nstd::is_standard_layout_v<P>   // true', onde: [['tipos', 'Cap. 4 · 4.7'], [null, 'src/layout/', '../../layout/README.md']] },
    { fam: 'valores', nome: 'CTAD', ext: 'class template argument deduction · C++17',
      def: 'Deduzir os argumentos de um template de classe a partir do construtor — sem escrever os <code>&lt;&gt;</code>.',
      ex: 'std::pair p{1, 2.0};      // pair<int, double>\nstd::vector v{1, 2, 3};   // vector<int>', onde: [['generico', 'Cap. 8 · 8.2']] },
    { fam: 'valores', nome: 'Forwarding', ext: 'repasse perfeito',
      def: 'Passar um argumento adiante <b>preservando</b> se ele era temporário ou não. Precisa de <code>T&amp;&amp;</code> deduzido + <code>std::forward&lt;T&gt;</code>.',
      ex: 'template <class T>\nvoid f(T&& a) { g(std::forward<T>(a)); }', onde: [['funcoes', 'Cap. 3 · 3.2'], [null, 'src/value_semantics/', '../../value_semantics/README.md']] },
    { fam: 'ferramenta', nome: '-Wall -Wextra', ext: 'conjuntos de avisos do compilador',
      def: 'Os avisos que importam <b>não vêm ligados</b> — só uns poucos são padrão. Este subprojeto compila sem nenhum aviso com <code>-Wall -Wextra -Wpedantic</code>.',
      ex: 'g++ -std=c++20 -Wall -Wextra -Wpedantic …', lang: 'shell', onde: [['antes', '0.8 · neste capítulo', '#/antes-avisos'], ['encerramento', 'Encerramento']] },
  ];

  function mostrarTermo(raiz, i) {
    raiz._i = i;
    raiz._botoes.forEach((b, j) => b.setAttribute('aria-pressed', String(j === i)));
    const p = raiz._painel;
    if (i < 0) {
      p.dataset.fam = 'intro';
      p.replaceChildren(
        el('span', { class: 'fam' }, 'glossário'),
        el('h3', { class: 'nome' }, '12 palavras'),
        el('p', { class: 'def', html: 'Cada termo é usado adiante — e nenhum aparece antes de estar definido aqui. <b>Clique</b> num termo, ou avance com <kbd>→</kbd>.' }),
        el('div', { class: 'familias' }, Object.entries(FAMILIAS).map(([f, n]) => el('span', { dataset: { fam: f } }, n))));
    } else {
      const t = TERMOS[i];
      p.dataset.fam = t.fam;
      p.replaceChildren(
        el('span', { class: 'fam' }, FAMILIAS[t.fam]),
        el('h3', { class: 'nome' }, t.nome),
        el('p', { class: 'ext' }, t.ext),
        el('p', { class: 'def', html: t.def }),
        cod(t.ex, 'ex', t.lang),
        el('div', { class: 'onde' }, el('span', { class: 'rot' }, 'volta em'), t.onde.map(chip)));
    }
    p.classList.remove('surge'); void p.offsetWidth; p.classList.add('surge');
  }

  Deck.widget('antes/glossario', {
    iniciar(raiz) {
      raiz.classList.add('w-glos');
      raiz._botoes = TERMOS.map((t, i) => el('button', { class: 'termo', type: 'button', dataset: { fam: t.fam }, onclick: () => mostrarTermo(raiz, i) },
        el('span', { class: 'n' }, String(i + 1).padStart(2, '0')), el('span', t.rot ? { class: 't', html: t.rot } : { class: 't' }, t.rot ? null : t.nome)));
      raiz._painel = el('div', { class: 'painel' });
      raiz.append(el('div', { class: 'termos' }, raiz._botoes), raiz._painel);
    },
    passo(raiz, k) { mostrarTermo(raiz, k - 1); },
  });

  /* ═════════════════════════════════════════════════════════════════════════
     5. antes/vida — escopo (texto) × tempo de vida (execução)
     ═════════════════════════════════════════════════════════════════════════ */
  const VIDA_COD = ['int main() {', '    int* p = nullptr;', '    {', '        int x = 42;', '        static int n = 0;',
    '        ++n;', '        p = &x;', '    }', '    *p = 7;        // ✗ UB', '}'];
  const ESCOPO = { p: [2, 10], x: [4, 8], n: [5, 8] };
  // [nome visível, objeto vivo, valor]
  const VIDA = [
    { pc: 1, p: [0, 0, '—'], x: [0, 0, '—'], n: [0, 1, '0'], seta: 0,
      leg: 'Começo de <code>main</code>: nenhum nome visível — mas o objeto <code>n</code> <b>já existe</b>. <code>static</code> vive o programa inteiro (inicializado antes de <code>main</code>).' },
    { pc: 2, p: [1, 1, 'nullptr'], x: [0, 0, '—'], n: [0, 1, '0'], seta: 0,
      leg: '<code>p</code> nasce valendo <code>nullptr</code>. O <b>nome</b> <code>p</code> vale daqui até a <code>}</code> final de <code>main</code>.' },
    { pc: 4, p: [1, 1, 'nullptr'], x: [1, 1, '42'], n: [0, 1, '0'], seta: 0,
      leg: 'Bloco interno: <code>x</code> nasce com 42. Nomes visíveis agora: <code>p</code> e <code>x</code>.' },
    { pc: 6, p: [1, 1, 'nullptr'], x: [1, 1, '42'], n: [1, 1, '1'], seta: 0,
      leg: 'A linha 5 não executa nada: <code>n</code> já estava pronto. Aqui o <b>nome</b> <code>n</code> fica visível, e <code>++n</code> o leva a 1.' },
    { pc: 7, p: [1, 1, '&x'], x: [1, 1, '42'], n: [1, 1, '1'], seta: 1,
      leg: '<code>p = &amp;x;</code> — <code>p</code> passa a apontar para o objeto <code>x</code>.' },
    { pc: 8, p: [1, 1, '&x'], x: [0, 0, '42'], n: [0, 1, '1'], seta: 2, morto: 1,
      leg: 'Fim do bloco: o objeto <code>x</code> <b>morre</b>, e os nomes <code>x</code> e <code>n</code> somem. O objeto <code>n</code> segue <b>vivo</b> — só ficou sem nome visível.' },
    { pc: 9, p: [1, 1, '&x'], x: [0, 0, '42'], n: [0, 1, '1'], seta: 3, morto: 1,
      leg: '<code>*p = 7</code>: nome <code>p</code> em escopo, objeto <code>p</code> vivo — o apontado, morto. Escrever ali é <b>UB</b>. O GCC 13 avisa com <code>-Wall</code> (<code>-Wdangling-pointer</code>) — quando enxerga.' },
  ];

  // linha do tempo: [passo em que o objeto nasce, passo em que morre (null = ainda vivo
  // no último passo), vive além de main?]. p morre na } da linha 10, depois do último
  // passo (linha 9); x morre na } da linha 8 (passo 5); n vive até o fim do programa.
  const VIDA_INTERVALO = { p: [1, null, false], x: [2, 5, false], n: [0, null, true] };
  const VIDA_TICKS = VIDA.map((s) => 'linha ' + s.pc);

  function setaVida(raiz) {
    const z = raiz._mem;
    if (!z.offsetWidth) return;
    const vp = pos(raiz._ent.p.valor, z), vx = pos(raiz._ent.x.valor, z);
    const x1 = vp.x + vp.w, y1 = vp.y + vp.h / 2, x2 = vx.x - 8, y2 = vx.y + vx.h / 2;
    raiz._svg.setAttribute('viewBox', `0 0 ${z.offsetWidth} ${z.offsetHeight}`);
    raiz._seta.setAttribute('d', `M ${x1} ${y1} L ${x2} ${y2}`);
    raiz._ponta.setAttribute('d', `M ${x2} ${y2} l -14 -8 l 0 16 z`);
  }

  Deck.widget('antes/vida', {
    iniciar(raiz) {
      raiz.classList.add('w-vida');
      raiz._linhas = VIDA_COD.map((txt, i) => {
        const n = i + 1;
        const gut = ['p', 'x', 'n'].map((nm) => el('i', { class: 'g ' + nm + (n >= ESCOPO[nm][0] && n <= ESCOPO[nm][1] ? ' dentro' : '') +
          (n === ESCOPO[nm][0] ? ' ini' : '') + (n === ESCOPO[nm][1] ? ' fim' : '') }));
        return el('div', { class: 'lv' + (/✗/.test(txt) ? ' ruim' : '') }, el('span', { class: 'pc' }, '▶'), el('span', { class: 'num' }, String(n)), gut,
          el('span', { class: 'txt', html: Deck.util.realcar(txt.trim() ? txt : ' ', 'cpp') }));
      });
      // o realce normaliza a indentação; recoloca-a (1ch = uma coluna da fonte mono)
      raiz._linhas.forEach((ln, i) => { ln.querySelector('.txt').style.paddingLeft = VIDA_COD[i].match(/^ */)[0].length + 'ch'; });
      const cab = el('div', { class: 'lv cab' }, el('span', { class: 'pc' }), el('span', { class: 'num' }),
        ['p', 'x', 'n'].map((nm) => el('i', { class: 'g ' + nm }, nm)), el('span', { class: 'txt' }, 'escopo de cada nome →'));
      const codigo = el('div', { class: 'codv' }, cab, raiz._linhas);

      raiz._ent = {};
      const ent = (nm, onde) => {
        const valor = el('div', { class: 'valor' }, '—');
        const e = el('div', { class: 'ent ' + nm },
          el('div', { class: 'cab-ent' }, el('b', null, nm), el('span', { class: 'onde' }, onde)),
          valor,
          el('div', { class: 'luz esc' }, el('i'), 'nome visível'),
          el('div', { class: 'luz vid' }, el('i'), 'objeto vivo'));
        raiz._ent[nm] = { el: e, valor };
        return e;
      };
      raiz._seta = svg('path', { class: 'seta' });
      raiz._ponta = svg('path', { class: 'ponta' });
      raiz._svg = svg('svg', { class: 'fios', 'aria-hidden': 'true', preserveAspectRatio: 'none' }, raiz._seta, raiz._ponta);
      raiz._mem = el('div', { class: 'mem-vida' },
        el('div', { class: 'zona pilha-z' }, el('span', { class: 'rz' }, 'pilha · automático'), el('div', { class: 'ents' }, ent('p', 'main'), ent('x', 'bloco interno'))),
        el('div', { class: 'zona estat-z' }, el('span', { class: 'rz' }, 'estático · vida toda'), el('div', { class: 'ents' }, ent('n', 'static local'))),
        raiz._svg);
      raiz._leg = el('p', { class: 'leg-vida' });
      // tempo de vida no TEMPO: uma barra por objeto, que cresce com a execução
      raiz._barras = {};
      raiz._cursor = el('i', { class: 'cursor-t' });
      const trilhas = ['n', 'p', 'x'].map((nm) => {
        // quem vive além de main ganha o rótulo na ponta da barra (um elemento, não ::after,
        // para obedecer ao modo estático/movimento reduzido como o resto)
        const b = el('i', { class: 'barra-v ' + nm }, VIDA_INTERVALO[nm][2] ? el('span', { class: 'alem-rot' }, '→ fim do programa') : null);
        raiz._barras[nm] = b;
        return el('div', { class: 'trilha-t' }, el('b', { class: nm }, nm), el('div', { class: 'pista' }, b));
      });
      raiz._ticks = VIDA_TICKS.map((t, i) => { const s = el('span', null, t); s.style.left = (i / 6 * 100) + '%'; return s; });
      const ticks = el('div', { class: 'ticks' }, raiz._ticks);
      raiz._tempo = el('div', { class: 'tempo' }, el('span', { class: 'rot-t' }, 'tempo de vida, no tempo →'),
        el('div', { class: 'trilhas' }, trilhas, el('div', { class: 'pista-cursor' }, raiz._cursor)), el('div', { class: 'ticks-l' }, el('span'), ticks));
      raiz.append(codigo, el('div', { class: 'dir' }, raiz._mem, raiz._leg, raiz._tempo));
    },
    passo(raiz, k) {
      const s = VIDA[k];
      raiz._linhas.forEach((ln, i) => {
        const n = i + 1;
        ln.classList.toggle('atual', n === s.pc);
        ln.querySelectorAll('.g').forEach((g, j) => {
          const nm = ['p', 'x', 'n'][j];
          g.classList.toggle('agora', !!s[nm][0] && n >= ESCOPO[nm][0] && n <= ESCOPO[nm][1]);
        });
      });
      for (const nm of ['p', 'x', 'n']) {
        const [vis, viv, val] = s[nm];
        const e = raiz._ent[nm];
        e.el.classList.toggle('visivel', !!vis);
        e.el.classList.toggle('vivo', !!viv);
        e.el.classList.toggle('morto', nm === 'x' && !!s.morto);
        e.el.classList.toggle('nasceu', viv || (nm === 'x' && !!s.morto));
        e.valor.textContent = val;
      }
      raiz.dataset.seta = String(s.seta);
      for (const nm of ['p', 'x', 'n']) {
        const [a, b, alem] = VIDA_INTERVALO[nm];
        const fim = b == null ? k : Math.min(b, k);
        const bar = raiz._barras[nm];
        bar.style.left = (a / 6 * 100) + '%';
        bar.style.width = (k >= a ? Math.max(0, (fim - a) / 6 * 100) : 0) + '%';
        bar.classList.toggle('nasceu', k >= a);
        bar.classList.toggle('morreu', b != null && k >= b);
        bar.classList.toggle('alem', alem);
        // "→ fim do programa" só cabe na barra a partir de 2 passos de largura
        bar.classList.toggle('rot', alem && k >= a && fim - a >= 2);
      }
      raiz._ticks.forEach((t, i) => t.classList.toggle('agora', i === k));
      raiz._cursor.style.left = (k / 6 * 100) + '%';
      raiz._leg.innerHTML = s.leg;
      raiz._leg.classList.remove('surge'); void raiz._leg.offsetWidth; raiz._leg.classList.add('surge');
      setaVida(raiz);
    },
    entrar(raiz) { setaVida(raiz); },
  });

  /* ═════════════════════════════════════════════════════════════════════════
     6. antes/programa — o menor programa completo, peça por peça
     ═════════════════════════════════════════════════════════════════════════ */
  const PROGRAMA = `#include <fmt/core.h>
#include <string>

struct Saudacao {
    std::string nome;
    void falar() const { fmt::print("Olá, {}!\\n", nome); }
};

int dobro(int x) { return x * 2; }

int main() {
    int n{3};
    if (n > 0) {
        for (int i = 0; i < n; ++i)
            fmt::print("{} dobrado = {}\\n", i, dobro(i));
    }
    Saudacao{"mundo"}.falar();
}`;
  const PECAS = [
    { l: [1, 2], slug: 'organizacao', num: '06', rot: '6 · organização', tit: 'Trazer código de fora',
      txt: '<code>#include</code> cola o texto do header aqui. Namespaces, macros, linkage: tudo no Cap.&nbsp;6.' },
    { l: [4, 7], slug: 'tipos', num: '04', rot: '4/5 · tipos e classes', tit: 'Um tipo seu',
      txt: '<code>struct</code> junta dado (<code>nome</code>) e comportamento (<code>falar</code>, um método <code>const</code>). Caps.&nbsp;4 e 5.' },
    { l: [9, 9], slug: 'funcoes', num: '03', rot: '3 · funções', tit: 'Entrada → saída',
      txt: 'Parâmetro, corpo, retorno. Passagem, sobrecarga, <code>constexpr</code>…: Cap.&nbsp;3.' },
    { l: [11, 11], slug: 'antes', num: '▶', rot: 'ponto de entrada', tit: 'Tudo começa em main',
      txt: '<code>main</code> devolve <code>int</code> — e é a única função que pode omitir o <code>return</code>.' },
    { l: [12, 12], slug: 'declaracoes', num: '01', rot: '1 · declarações', tit: 'Declarar e inicializar',
      txt: '<code>int n{3};</code> — declaração com <code>{}</code>, a forma que proíbe narrowing. Cap.&nbsp;1.' },
    { l: [13, 16], slug: 'controle', num: '02', rot: '2 · controle de fluxo', tit: 'Por onde a execução anda',
      txt: '<code>if</code> decide, <code>for</code> repete. <code>switch</code>, range-for, <code>goto</code>…: Cap.&nbsp;2.' },
    // linhas 14–15: o i < n e o ++i citados estão na 14; o README anota a 15
    { l: [14, 15], nivel: 1, slug: 'expressoes', num: '07', rot: '7 · expressões', tit: 'Operadores e conversões',
      txt: '<code>i &lt; n</code>, <code>++i</code>, <code>dobro(i)</code>: Cap.&nbsp;7. E <code>fmt::print</code> é um template variádico — Cap.&nbsp;8.' },
    { l: [17, 17], slug: 'antes', num: '★', rot: 'objeto temporário', tit: 'Nasce e morre na linha',
      txt: '<code>Saudacao{"mundo"}</code> cria um temporário, chama <code>falar()</code> e o destrói no <code>;</code> — sem <code>new</code>, sem <code>delete</code>.' },
  ];

  function tagsPrograma(raiz) {
    if (!raiz._trilho.offsetWidth) return;
    const y0 = pos(raiz._trilho, raiz).y;      // linhas e trilho medidos contra a raiz
    PECAS.forEach((p, i) => {
      const a = raiz._lns[p.l[0] - 1], b = raiz._lns[p.l[1] - 1];
      const ya = pos(a, raiz).y - y0, yb = pos(b, raiz).y - y0 + b.offsetHeight;
      const t = raiz._tags[i];
      t.style.top = ya + 'px';
      t.style.setProperty('--alt', (yb - ya) + 'px');
    });
  }

  Deck.widget('antes/programa', {
    iniciar(raiz) {
      raiz.classList.add('w-prog');
      raiz._fig = Deck.util.codigo(PROGRAMA, { classe: 'compacto', numeros: true });
      raiz._lns = Array.from(raiz._fig.querySelectorAll('.ln'));
      raiz._tags = PECAS.map((p) => {
        const t = el('div', { class: 'tag' + (p.l[0] !== p.l[1] ? ' faixa' : '') }, el('span', null, p.rot));
        t.style.setProperty('--c', CAPS[p.slug]);
        t.style.setProperty('--nivel', String(p.nivel || 0));
        return t;
      });
      raiz._trilho = el('div', { class: 'trilho-tags' }, raiz._tags);
      raiz._painel = el('div', { class: 'painel-prog' });
      raiz.append(raiz._fig, raiz._trilho, raiz._painel);
    },
    passo(raiz, k) {
      const atual = k > 0 ? PECAS[k - 1] : null;
      raiz._fig.classList.toggle('focando', !!atual);
      if (atual) raiz._fig.style.setProperty('--cor', CAPS[atual.slug]); else raiz._fig.style.removeProperty('--cor');
      raiz._lns.forEach((ln, i) => ln.classList.toggle('foco', !!atual && i + 1 >= atual.l[0] && i + 1 <= atual.l[1]));
      raiz._tags.forEach((t, i) => { t.classList.toggle('vista', i < k); t.classList.toggle('atual', i === k - 1); });
      const P = raiz._painel;
      if (!atual) {
        P.style.setProperty('--c', 'var(--cor)');
        P.replaceChildren(el('span', { class: 'num' }, '18'), el('h3', null, 'linhas, o programa inteiro'),
          el('p', { html: 'Cada peça tem um capítulo que a aprofunda. Avance com <kbd>→</kbd>: a peça acende e ganha a sua etiqueta.' }));
      } else {
        P.style.setProperty('--c', CAPS[atual.slug]);
        P.replaceChildren(...filhos(el('span', { class: 'num' }, atual.num), el('span', { class: 'rot' }, atual.rot), el('h3', null, atual.tit),
          el('p', { html: atual.txt }),
          atual.slug !== 'antes' ? chip([atual.slug, 'ir para o Cap. ' + atual.num.replace(/^0/, '')]) : null));
      }
      P.classList.remove('surge'); void P.offsetWidth; P.classList.add('surge');
      tagsPrograma(raiz);
    },
    entrar(raiz) { tagsPrograma(raiz); },
  });

  /* ═════════════════════════════════════════════════════════════════════════
     7. antes/aposta — as 11 armadilhas: aposte, depois revele
     ═════════════════════════════════════════════════════════════════════════ */
  const AVISA = { sim: ['ok', 'avisa'], nao: ['erro', 'não avisa'], asvezes: ['aviso', 'às vezes'], conv: ['aviso', 'só com -Wconversion'] };
  const ARMADILHAS = [
    { cod: 'Widget w();', q: 'O que é <code>w</code>?',
      ops: ['um <code>Widget</code> criado pelo construtor padrão', 'uma <b>função</b> sem parâmetros que devolve <code>Widget</code>', 'erro de compilação'], certa: 1,
      resp: 'Declara uma <b>função</b> <code>w()</code> que devolve <code>Widget</code> — a <i>most vexing parse</i>. Objeto: <code>Widget w;</code> ou <code>Widget w{};</code>',
      avisa: 'sim', flag: '-Wvexing-parse', via: 'ligado por padrão', readme: 'não', onde: [['declaracoes', 'Cap. 1 · 1.2']] },
    { cod: 'int x;        // local\nreturn x;', q: 'O que é devolvido?',
      ops: ['0: variável nova começa zerada', 'lixo — mas sempre o mesmo lixo', 'nada garantido: ler <code>x</code> é UB'], certa: 2,
      resp: 'Lixo <b>indeterminado</b> — e <b>ler</b> é UB: o padrão não promete nem “um número qualquer”. <code>int x{};</code> garante zero.',
      avisa: 'asvezes', flag: '-Wuninitialized', via: '-Wall ou -Wextra', nota: 'escrito só num galho (<code>if (c) x = 1;</code>) e lido na linha de baixo (<code>return x;</code>): calado, nem com -O2', readme: 'não', onde: [['declaracoes', 'Cap. 1 · 1.2']] },
    { cod: 'if (a) f(); g();', q: 'Quando <code>g()</code> roda?',
      ops: ['só quando <code>a</code> é verdadeiro', '<b>sempre</b>', 'nunca: é erro de sintaxe'], certa: 1,
      resp: 'O <code>if</code> guarda <b>uma</b> instrução: <code>f();</code>. <code>g()</code> roda <b>sempre</b>. Chaves sempre, mesmo em uma linha.',
      avisa: 'asvezes', flag: '-Wmisleading-indentation', via: '-Wall', nota: 'tudo numa linha, o GCC só avisa se o <code>if</code> abre a linha: <code>int y = a; if (y) f(); g();</code> passa calado', readme: 'às vezes', onde: [['controle', 'Cap. 2 · 2.1']] },
    { cod: 'int o = 052;', q: 'Quanto vale <code>o</code>?', ops: ['52', '42', 'erro: zero à esquerda'], certa: 1,
      resp: 'Zero à esquerda é <b>octal</b>: 5×8 + 2 = <b>42</b>. Nenhuma flag reclama.',
      avisa: 'nao', readme: 'não', onde: [['declaracoes', 'Cap. 1 · 1.8']] },
    { cod: '#define Q(x) x*x\nint r = Q(1+2);', q: 'Quanto vale <code>r</code>?', ops: ['9', '5', '7'], certa: 1,
      resp: 'O pré-processador cola <b>texto</b>: vira <code>1+2*1+2</code> = <b>5</b>. Parênteses em tudo — <code>((x)*(x))</code> — ou, melhor, uma função.',
      avisa: 'nao', readme: 'não', onde: [['organizacao', 'Cap. 6 · 6.2']] },
    { cod: 'int a = -1; unsigned b = 1;\nbool menor = a < b;', q: '<code>menor</code> vale…', ops: ['<code>true</code>', '<code>false</code>', 'não compila'], certa: 1,
      resp: '<code>a</code> é convertido para <code>unsigned</code>: −1 vira <b>4294967295</b>, e 4294967295 &lt; 1 é <b>false</b>.',
      avisa: 'sim', flag: '-Wsign-compare', via: '-Wall ou -Wextra', readme: '-Wsign-compare', onde: [['expressoes', 'Cap. 7 · 7.4']] },
    { cod: 'int t = 3.99;', q: 'Quanto vale <code>t</code>?', ops: ['4: arredonda', '3: trunca', 'erro de compilação'], certa: 1,
      resp: '<i>Narrowing</i> com <code>=</code>: <b>trunca em silêncio</b> → 3. Com <code>int t{3.99};</code> vira <b>erro</b> de compilação.',
      avisa: 'conv', flag: '-Wfloat-conversion', via: '-Wconversion', nota: 'nem <code>-Wall</code> nem <code>-Wextra</code> avisam', readme: 'não (com {}, sim)', onde: [['declaracoes', 'Cap. 1 · 1.2']] },
    { cod: 'int* p = new int[5];\ndelete p;', q: 'O que acontece?', ops: ['libera os 5 <code>int</code>', 'libera só o primeiro', 'UB'], certa: 2,
      resp: '<code>new[]</code> exige <code>delete[]</code>: misturar é <b>UB</b> — o ASan acusa <code>alloc-dealloc-mismatch</code>. Melhor ainda: <code>std::vector</code>.',
      avisa: 'asvezes', flag: '-Wmismatched-new-delete', via: '-Wall', nota: 'só quando enxerga as duas pontas: o <code>new</code> em outra TU passa calado', readme: 'não', onde: [['expressoes', 'Cap. 7 · 7.7']] },
    { cod: 'struct S {\n    int m = 1;\n    auto f() { return [=] { return m; }; }\n};', q: 'O que o <code>[=]</code> captura?',
      ops: ['uma cópia de <code>m</code>', 'o ponteiro <code>this</code>', 'uma cópia de <code>*this</code>'], certa: 1,
      resp: 'Captura <code>this</code> <b>por ponteiro</b> e lê <code>m</code> através dele: mudou <code>s.m</code> para 99, a lambda devolve 99. Se o objeto morrer: <i>dangling</i>.',
      avisa: 'sim', flag: '-Wdeprecated', via: 'C++20, ligado por padrão', readme: 'não', onde: [[null, 'src/lambdas/', '../../lambdas/README.md']] },
    { cod: 'int r = 0;\nswitch (1) {\ncase 1: r = 1;      // sem break\ncase 2: r += 2; break;\n}', q: 'Quanto vale <code>r</code>?', ops: ['1', '3', '2'], certa: 1,
      resp: 'Sem <code>break</code>, a execução <b>cai</b> no <code>case</code> seguinte: 1, depois += 2 → <b>3</b>. Queda de propósito? <code>[[fallthrough]];</code>',
      avisa: 'sim', flag: '-Wimplicit-fallthrough', via: '-Wextra', readme: '-Wimplicit-fallthrough', onde: [['controle', 'Cap. 2 · 2.3']] },
    { cod: 'struct R { unsigned codigo : 4; };\nR r{};\nr.codigo = 20;', q: 'Quanto vale <code>r.codigo</code>?', ops: ['20', '15: satura no máximo', '4'], certa: 2,
      resp: '4 bits guardam 0..15. 20 = 1 0100₂ → sobram os 4 bits de baixo: 0100₂ = <b>4</b>. Trunca, não satura.',
      avisa: 'asvezes', flag: '-Woverflow', via: 'padrão, com constante', nota: 'valor vindo de runtime: só <code>-Wconversion</code> avisa (“may change value”)', readme: 'não', onde: [['tipos', 'Cap. 4 · 4.5']] },
  ];

  function desenharAposta(raiz) {
    const n = raiz._t, a = raiz._lista[n], num = raiz._de + n;
    const aposta = raiz._apostas[n];
    raiz.classList.toggle('revelado', raiz._rev);
    raiz._pontos.forEach((p, j) => { p.classList.toggle('atual', j === n); p.classList.toggle('feito', j < n || (j === n && raiz._rev)); });
    raiz._cont.innerHTML = `armadilha <b>${num}</b> de 11`;
    const longa = Math.max(...a.cod.split('\n').map((s) => s.length));
    raiz._escrito.replaceChildren(el('span', { class: 'rotulo' }, 'escrito assim'), cod(a.cod, longa > 30 ? 'm' : longa > 18 ? 'g' : 'gg'));
    raiz._perg.innerHTML = a.q + ' <span class="apostem">— apostem:</span>';
    raiz._ops.replaceChildren(...a.ops.map((o, j) => el('button', {
      class: 'op' + (aposta === j ? ' escolhida' : '') + (raiz._rev && j === a.certa ? ' certa' : '') + (raiz._rev && aposta === j && j !== a.certa ? ' errou' : ''),
      type: 'button', onclick: () => { raiz._apostas[n] = j; desenharAposta(raiz); },
    }, el('b', null, 'ABC'[j]), el('span', { html: o }))));
    const [cl, txt] = AVISA[a.avisa];
    raiz._verso.replaceChildren(...filhos(
      el('span', { class: 'rotulo' }, 'o que realmente acontece'),
      el('p', { class: 'resp', html: a.resp }),
      el('div', { class: 'avisa' },
        el('span', { class: 'rot' }, 'avisa? · GCC 13.3'),
        el('span', { class: 'pill grande ' + cl }, txt),
        a.flag ? el('code', { class: 'flag' }, a.flag) : null,
        a.via ? el('span', { class: 'via' }, a.via) : null),
      a.nota ? el('p', { class: 'nota', html: '⚠ ' + a.nota }) : null,
      el('div', { class: 'rodape-ap' },
        el('span', { class: 'readme', html: `tabela do README: <i>${a.readme}</i>` }),
        el('span', { class: 'onde' }, el('span', { class: 'rot' }, 'onde ver'), a.onde.map(chip)))));
  }

  Deck.widget('antes/aposta', {
    iniciar(raiz) {
      raiz.classList.add('w-ap');
      raiz._de = +(raiz.dataset.de || 1);
      raiz._lista = ARMADILHAS.slice(raiz._de - 1, +(raiz.dataset.ate || 11));
      raiz._apostas = {};
      raiz._cont = el('span', { class: 'cont' });
      raiz._pontos = raiz._lista.map((_, j) => el('button', { class: 'ponto', type: 'button', title: 'armadilha ' + (raiz._de + j),
        onclick: () => { raiz._t = j; raiz._rev = false; desenharAposta(raiz); } }, String(raiz._de + j)));
      raiz._escrito = el('div', { class: 'escrito' });
      raiz._perg = el('p', { class: 'perg' });
      raiz._ops = el('div', { class: 'ops' });
      raiz._verso = el('div', { class: 'verso' });
      const frente = el('div', { class: 'frente' }, el('span', { class: 'q' }, '?'), el('p', null, 'votem antes de revelar'),
        el('button', { class: 'botao', type: 'button', onclick: () => { raiz._rev = true; desenharAposta(raiz); } }, 'revelar'));
      raiz.append(
        el('div', { class: 'topo-ap' }, raiz._cont, el('div', { class: 'pontos' }, raiz._pontos)),
        el('div', { class: 'mesa-ap' },
          el('div', { class: 'esq' }, raiz._escrito, raiz._perg, raiz._ops),
          el('div', { class: 'revela' }, frente, raiz._verso)));
      raiz._t = 0; raiz._rev = false;
    },
    passo(raiz, k) {
      raiz._t = Math.min(raiz._lista.length - 1, Math.floor(k / 2));
      raiz._rev = k % 2 === 1;
      desenharAposta(raiz);
    },
  });

  /* ═════════════════════════════════════════════════════════════════════════
     8. antes/flags — que flag pega que armadilha
        MATRIZ: para cada arquivo de teste, aviso → máscara das flags que o
        ligam (bit 0 -Wall, 1 -Wextra, 2 -Wpedantic, 3 -Wshadow, 4 -Wconversion;
        -1 = ligado por padrão). Obtida compilando as 32 combinações de flags
        (em -O0 e -O2: idênticas) — o aviso aparece se QUALQUER flag da máscara
        estiver ligada, e isso foi conferido nas 32.
     ═════════════════════════════════════════════════════════════════════════ */
  const FLAGS = ['-Wall', '-Wextra', '-Wpedantic', '-Wshadow', '-Wconversion'];
  const MATRIZ = {
    t01: { '-Wvexing-parse': -1 }, t02a: { '-Wuninitialized': 3 }, t02b: {}, t03a: { '-Wmisleading-indentation': 1 }, t03d: {},
    t04: {}, t05: {}, t06: { '-Wsign-compare': 3 }, t07: { '-Wfloat-conversion': 16 }, t08a: { '-Wmismatched-new-delete': 1 }, t08b: {},
    t09: { '-Wdeprecated': -1 }, t10: { '-Wimplicit-fallthrough': 2 }, t11a: { '-Woverflow': -1 }, t11b: { '-Wconversion': 16 },
    t12: { '-Wshadow': 8 }, t13: { '-Wvla': 4 },
  };
  const MENSAGEM = {
    '-Wvexing-parse': 'empty parentheses were disambiguated as a function declaration',
    '-Wuninitialized': '‘x’ is used uninitialized',
    '-Wmisleading-indentation': 'this ‘if’ clause does not guard...',
    '-Wsign-compare': 'comparison of integer expressions of different signedness: ‘int’ and ‘unsigned int’',
    '-Wfloat-conversion': 'conversion from ‘double’ to ‘int’ changes value from ‘3.9900000000000002e+0’ to ‘3’',
    '-Wmismatched-new-delete': '‘void operator delete(void*, long unsigned int)’ called on pointer returned from a mismatched allocation function',
    '-Wdeprecated': 'implicit capture of ‘this’ via ‘[=]’ is deprecated in C++20',
    '-Wimplicit-fallthrough': 'this statement may fall through',
    '-Woverflow': 'conversion from ‘unsigned int’ to ‘unsigned char:4’ changes value from ‘20’ to ‘4’',
    '-Wconversion': 'conversion from ‘unsigned int’ to ‘unsigned char:4’ may change value',
    '-Wshadow': 'declaration of ‘x’ shadows a previous local',
    '-Wvla': 'ISO C++ forbids variable length array ‘arr’',
  };
  const CASOS = [
    { tit: 'vexing parse', cod: 'Widget w();', v: ['t01'] },
    // `calado`: a variante que nenhuma flag pega (t02b, t03d, t08b) ou só -Wconversion (t11b)
    { tit: 'não inicializada', cod: 'int x; return x;', v: ['t02a', 't02b'], calado: 'escrito só num galho (<code>if (c) x = 1;</code>) e lido depois, passa calado.' },
    { tit: 'if sem chaves', cod: 'if (a) f(); g();', v: ['t03a', 't03d'], calado: 'tudo numa linha, só avisa se o <code>if</code> abre a linha — <code>int y = a; if (y) f(); g();</code> passa calado.' },
    { tit: 'octal', cod: 'int o = 052;', v: ['t04'] },
    { tit: 'macro', cod: '#define Q(x) x*x', v: ['t05'] },
    { tit: 'signed × unsigned', cod: 'a < b  // int, unsigned', v: ['t06'] },
    { tit: 'narrowing', cod: 'int t = 3.99;', v: ['t07'] },
    { tit: 'new[] × delete', cod: 'delete p;  // new int[5]', v: ['t08a', 't08b'], calado: 'com o <code>new</code> em outra TU, passa calado.' },
    { tit: '[=] num método', cod: '[=] { return m; }', v: ['t09'] },
    { tit: 'case sem break', cod: 'case 1: r = 1;  // cai', v: ['t10'] },
    { tit: 'bitfield', cod: 'r.codigo = 20;  // :4', v: ['t11a', 't11b'], calado: 'com valor de runtime em vez de constante, só <code>-Wconversion</code> avisa.' },
    { tit: 'sombreamento', cod: 'int x; { int x; }', v: ['t12'], extra: 1 },
    { tit: 'VLA (extensão GCC)', cod: 'int arr[n];', v: ['t13'], extra: 1 },
  ];
  const PASSOS_FLAGS = [0, 1, 3, 7, 15, 31];
  const TIT_FLAGS = ['sem flag nenhuma', '+ -Wall', '+ -Wextra', '+ -Wpedantic', '+ -Wshadow', '+ -Wconversion'];

  function avaliar(caso, mask) {
    const pegos = caso.v.map((id) => Object.entries(MATRIZ[id]).filter(([, B]) => B === -1 || (mask & B)).map(([t]) => t));
    const n = pegos.filter((p) => p.length).length;
    return { st: n === 0 ? 'nao' : n === caso.v.length ? 'sim' : 'asvezes', tags: [...new Set(pegos.flat())] };
  }
  const viaDe = (tag, mask) => {
    for (const t of Object.values(MATRIZ)) {
      if (!(tag in t)) continue;
      const B = t[tag];
      return B === -1 ? 'padrão' : FLAGS.filter((_, i) => (B >> i) & 1 && (mask >> i) & 1).join(' · ');
    }
    return '';
  };

  function desenharFlags(raiz, novos) {
    const mask = raiz._mask;
    raiz._tog.forEach((b, i) => b.setAttribute('aria-pressed', String(!!((mask >> i) & 1))));
    // cada token sem quebra: o navegador quebraria "-Wshadow" no hífen
    raiz._cmd.replaceChildren(...['g++', '-std=c++20', ...FLAGS.filter((_, i) => (mask >> i) & 1), 'a.cpp']
      .flatMap((t, i) => [i ? ' ' : '', el('span', { class: 'tok' }, t)]).filter(Boolean));
    let sim = 0, as = 0;
    raiz._linhas.forEach((ln, j) => {
      const r = avaliar(CASOS[j], mask);
      if (r.st === 'sim') sim++; else if (r.st === 'asvezes') as++;
      ln.dataset.st = r.st;
      ln._st.textContent = { sim: 'avisa', asvezes: 'às vezes', nao: 'silêncio' }[r.st];
      ln._tag.replaceChildren(...r.tags.map((t) => { const via = viaDe(t, mask); return el('span', null, el('code', null, t), via !== t ? el('small', null, via) : null); }));
      ln.classList.toggle('novo', novos.has(j));
    });
    raiz._num.textContent = String(sim);
    raiz._as.textContent = as ? `+ ${as} às vezes` : '';
  }

  // `semAviso`: o texto quando a lista sai vazia ("novo" só faz sentido ao ligar uma flag).
  // (a classe é .nada, e não .vazio: o motor já tem .vazio { flex: 1 } como espaçador)
  function logFlags(raiz, titulo, tags, rodape, semAviso = 'nenhum aviso novo') {
    raiz._logTit.textContent = titulo;
    raiz._log.replaceChildren(...filhos(tags.length ? tags.map((t) => el('div', { class: 'msg' }, el('span', { class: 'w' }, 'warning: '),
      MENSAGEM[t] + ' ', el('b', null, `[${t}${t === '-Wimplicit-fallthrough' ? '=' : ''}]`))) : el('div', { class: 'msg nada' }, semAviso),
      rodape ? el('p', { class: 'rod', html: rodape }) : null));
  }

  function aplicarMask(raiz, mask, titulo, rodape) {
    const antes = raiz._mask;
    const tagsAntes = new Set(CASOS.flatMap((c) => avaliar(c, antes).tags));
    raiz._mask = mask;
    const novos = new Set(), tagsNovas = [];
    CASOS.forEach((c, j) => {
      const r = avaliar(c, mask);
      const ra = avaliar(c, antes);
      if (r.tags.some((t) => !ra.tags.includes(t))) novos.add(j);
      r.tags.forEach((t) => { if (!tagsAntes.has(t) && !tagsNovas.includes(t)) tagsNovas.push(t); });
    });
    desenharFlags(raiz, novos);
    logFlags(raiz, titulo, tagsNovas, rodape);
  }

  Deck.widget('antes/flags', {
    iniciar(raiz) {
      raiz.classList.add('w-flags');
      raiz._mask = 0;
      raiz._linhas = CASOS.map((c, j) => {
        const st = el('span', { class: 'st' });
        const tag = el('span', { class: 'tags' });
        const ln = el('button', { class: 'caso' + (c.extra ? ' extra' : ''), type: 'button', onclick: () => {
          const r = avaliar(c, raiz._mask);
          logFlags(raiz, `${j + 1} · ${c.tit}`, r.tags, r.st === 'nao' ? 'Com estas flags: <b>silêncio</b>.' :
            r.st === 'asvezes' ? `<b>Às vezes</b>: ${c.calado}` : '', 'nenhum aviso');
        } }, el('span', { class: 'n' }, String(j + 1)), el('code', { class: 'c', html: Deck.util.realcar(c.cod, 'cpp') }), st, tag);
        ln._st = st; ln._tag = tag;
        return ln;
      });
      raiz._tog = FLAGS.map((f, i) => el('button', { class: 'botao tog', type: 'button', onclick: () => {
        aplicarMask(raiz, raiz._mask ^ (1 << i), ((raiz._mask >> i) & 1 ? '− ' : '+ ') + f, '');
      } }, f));
      raiz._cmd = el('code', { class: 'cmd' });
      raiz._num = el('b', { class: 'num' }, '0');
      raiz._as = el('span', { class: 'as' });
      raiz._logTit = el('span', { class: 'tit' });
      raiz._log = el('div', { class: 'log' });
      const lista = el('div', { class: 'lista' }, raiz._linhas.slice(0, 11),
        el('div', { class: 'sep' }, 'extras: o que -Wpedantic e -Wshadow pegam'), raiz._linhas.slice(11));
      raiz.append(lista, el('div', { class: 'lado' },
        el('div', { class: 'linha-cmd' }, el('span', { class: 'pr' }, '$ '), raiz._cmd),
        el('div', { class: 'togs' }, raiz._tog),
        el('p', { class: 'dica-w', html: 'Clique nas <b>flags</b> para combinar; numa <b>linha</b>, para ler o aviso.' }),
        el('div', { class: 'placar' }, raiz._num, el('span', null, 'de 13 avisam'), raiz._as),
        el('div', { class: 'console' }, el('div', { class: 'cab' }, dots(), raiz._logTit), raiz._log)));
    },
    passo(raiz, k) {
      raiz._mask = k > 0 ? PASSOS_FLAGS[k - 1] : 0;
      const rod = k === 0 ? 'Sem flag nenhuma o GCC 13.3 já avisa estes três.'
        : k === 5 ? '<code>052</code> e <code>Q(1+2)</code>: nenhuma flag pega — só o olho treinado.' : '';
      aplicarMask(raiz, PASSOS_FLAGS[k], TIT_FLAGS[k], rod);
      if (k === 0) {
        // no estado inicial, "novos" = tudo o que já avisa por padrão
        const novos = new Set(CASOS.map((c, j) => (avaliar(c, 0).tags.length ? j : -1)).filter((j) => j >= 0));
        desenharFlags(raiz, novos);
        logFlags(raiz, TIT_FLAGS[0], ['-Wvexing-parse', '-Wdeprecated', '-Woverflow'], rod);
      }
    },
  });
})();
