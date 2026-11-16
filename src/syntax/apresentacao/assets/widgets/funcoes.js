/* =============================================================================
   widgets/funcoes.js — Capítulo 3 · Funções (demo_funcoes.hpp)

   Widgets (todos com passo(k) idempotente: o estado k é desenhado do zero;
   as animações só rodam indo para a frente e param sozinhas):
     funcoes/anatomia  as seis peças de uma assinatura, uma por passo
     funcoes/linker    ODR: o que o nm mostra e o que o ld decide (4 cenários reais)
     funcoes/passagem  valor · & · const& · * · && em memória, chamador × função
     funcoes/padroes   argumentos padrão colados pelo compilador no chamador
     funcoes/virtual   padrão do tipo estático + corpo do tipo dinâmico
     funcoes/escada    resolução de sobrecarga: candidatas caindo nos degraus
     funcoes/pilha     referência pendurada: o quadro morre, a região é reusada
     funcoes/fold      fold expression se desdobrando e sendo reduzida
     funcoes/soquete   ponteiro × lambda × std::function (onde cada um cabe)
     funcoes/fib       árvore de chamadas de fib_recursivo, repetições e pilha

   Todos os números exibidos vêm da saída real do programa ou de programas de
   verificação compilados com g++ 13.3 (-std=c++20).
   ============================================================================= */
(function () {
  'use strict';
  const { el, svg } = Deck.util;
  const realcar = (txt) => Deck.util.realcar(txt, 'cpp');
  const cod = (txt, classe) => el('code', { class: 'f-cod' + (classe ? ' ' + classe : ''), html: realcar(txt) });

  // Ficha de animação: cada novo passo/clique invalida a animação anterior.
  function ficha(raiz, ctx) {
    raiz._ficha = (raiz._ficha || 0) + 1;
    const f = raiz._ficha;
    return async (ms) => (await ctx.esperar(ms)) && f === raiz._ficha;
  }
  function invalidar(raiz) { raiz._ficha = (raiz._ficha || 0) + 1; }

  // FLIP: anima os nós da posição antiga para a nova depois de mudar o DOM.
  function flip(raiz, nos, mudar, animar) {
    if (!animar) { mudar(); return; }
    const r = raiz.getBoundingClientRect();
    const esc = r.width / (raiz.offsetWidth || 1) || 1;
    const antes = new Map(nos.map((n) => [n, n.getBoundingClientRect()]));
    mudar();
    nos.forEach((n) => {
      const a = antes.get(n);
      const d = n.getBoundingClientRect();
      if (!a || (!a.width && !a.height) || (!d.width && !d.height)) return;
      const dx = (a.left - d.left) / esc, dy = (a.top - d.top) / esc;
      if (Math.abs(dx) < 1 && Math.abs(dy) < 1) return;
      n.style.transition = 'none';
      n.style.transform = `translate(${dx}px, ${dy}px)`;
      void n.offsetWidth;
      n.style.transition = '';
      n.style.transform = '';
    });
  }

  function botoes(lista, aoClicar, classe) {
    const box = el('div', { class: 'botoes ' + (classe || '') });
    lista.forEach((b) => {
      const bt = el('button', { type: 'button', class: 'botao', dataset: { id: b.id }, html: b.html || '', text: b.html ? null : b.txt });
      bt.addEventListener('click', (e) => { e.stopPropagation(); bt.blur(); aoClicar(b.id); });
      box.append(bt);
    });
    return box;
  }
  function marcarBotao(box, id) {
    box.querySelectorAll('button').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.id === String(id))));
  }

  async function digitar(no, de, ate, espera, ms = 26) {
    if (!espera) { no.textContent = ate; return true; }
    no.textContent = de;
    for (let c = de.length + 1; c <= ate.length; c++) {
      if (!(await espera(ms))) { no.textContent = ate; return false; }
      no.textContent = ate.slice(0, c);
    }
    return true;
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  1. Anatomia da assinatura
  // ───────────────────────────────────────────────────────────────────────────
  const ANAT_TOK = [
    { t: '[[nodiscard]]', p: 'atr', k: 'atr', c: 'tk-atr' }, { t: ' ' },
    { t: 'constexpr', p: 'esp', k: 'esp', c: 'tk-kw' }, { t: ' ' },
    { t: 'auto', p: 'ret', k: 'auto', c: 'tk-kw' }, { t: ' ' },
    { t: 'soma', p: 'nome', k: 'nome', c: 'tk-fn', id: 1 },
    { t: '(', p: 'par', k: 'par', c: 'tk-op', id: 1 },
    { t: 'int', p: 'par', k: 'par', c: 'tk-tipo', id: 1 },
    { t: ' a', p: 'par', k: 'par' },
    { t: ', ', p: 'par', k: 'par', c: 'tk-op', id: 1 },
    { t: 'int', p: 'par', k: 'par', c: 'tk-tipo', id: 1 },
    { t: ' b', p: 'par', k: 'par' },
    { t: ' = 0', p: 'par', k: 'par', c: 'tk-num', sub: 'padrao' },
    { t: ')', p: 'par', k: 'par', c: 'tk-op', id: 1 }, { t: ' ' },
    { t: 'noexcept', p: 'exc', k: 'exc', c: 'tk-kw' }, { t: ' ' },
    { t: '-> int', p: 'ret', k: 'seta', c: 'tk-tipo' },
    { t: ';', c: 'tk-op' },
  ];
  const ANAT_CHAVES = [
    { k: 'atr', p: 'atr', rot: 'atributo', cor: 'var(--tk-atr)' },
    { k: 'esp', p: 'esp', rot: 'especificador', cor: 'var(--tk-kw)' },
    { k: 'auto', p: 'ret', rot: 'marcador', cor: 'var(--tk-tipo)' },
    { k: 'nome', p: 'nome', rot: 'nome', cor: 'var(--tk-fn)' },
    { k: 'par', p: 'par', rot: 'parâmetros', cor: 'var(--tk-str)' },
    { k: 'exc', p: 'exc', rot: 'exceção', cor: 'var(--tk-ctl)' },
    { k: 'seta', p: 'ret', rot: 'retorno', cor: 'var(--tk-tipo)' },
  ];
  const ANAT_INFO = {
    '': { n: '', tit: 'Seis peças, três obrigatórias', cod: 'int soma(int a, int b);',
      txt: 'Retorno, nome e parâmetros são o mínimo. Cada peça a mais acrescenta uma <b>promessa</b> ao contrato — e o compilador cobra cada uma.', pills: [['obrigatórias: retorno · nome · parâmetros', 'ok']] },
    atr: { n: '1', tit: 'Atributo', cod: '[[nodiscard]]',
      txt: 'Instrução extra ao compilador, entre <code>[[ ]]</code>. Esta pede um <b>aviso</b> se quem chama ignorar o retorno.', pills: [['opcional', 'info'], ['[[ ]]: C++11 · nodiscard: C++17', 'cpp17'], ['→ 3.6', '']] },
    esp: { n: '2', tit: 'Especificador', cod: 'constexpr',
      txt: 'Diz <b>como</b> a função pode ser usada: <code>constexpr</code> = pode ser avaliada em compilação. Moram aqui também <code>inline</code>, <code>static</code>, <code>virtual</code>…', pills: [['opcional', 'info'], ['constexpr: C++11', 'cpp11'], ['→ 3.6', '']] },
    nome: { n: '3', tit: 'Nome', cod: 'soma',
      txt: 'Junto com os tipos dos parâmetros, é o que <b>identifica</b> a função quando várias têm o mesmo nome.', pills: [['obrigatório', 'ok'], ['→ 3.4', '']] },
    par: { n: '4', tit: 'Parâmetros', cod: '(int a, int b = 0)',
      txt: 'O que entra. Os nomes são opcionais numa declaração. <code>= 0</code> é um <b>argumento padrão</b>: <code>soma(5)</code> vira <code>soma(5, 0)</code> no chamador.', pills: [['obrigatório', 'ok'], ['→ 3.2 · 3.3', '']] },
    exc: { n: '5', tit: 'Especificação de exceção', cod: 'noexcept',
      txt: 'Promete que nenhuma exceção <b>sai</b> dela. Saiu mesmo assim? <code>std::terminate</code>. Desde C++17 faz parte do <b>tipo</b> da função.', pills: [['opcional', 'info'], ['noexcept: C++11', 'cpp11'], ['→ 3.6', '']] },
    ret: { n: '6', tit: 'Retorno', cod: 'auto … -> int',
      txt: '<code>auto</code> na frente só guarda o lugar; o tipo vem depois da seta (<i>trailing return type</i>). Aqui equivale a <code>int soma(…)</code>.', pills: [['obrigatório', 'ok'], ['trailing: C++11', 'cpp11'], ['→ 3.5', '']] },
    ident: { n: '★', tit: 'A identidade para a sobrecarga', cod: 'soma(int, int)',
      txt: 'Só <b>nome + tipos dos parâmetros</b> distinguem sobrecargas (num método, também <code>const</code>/<code>&amp;</code>). Retorno, nomes, padrão, <code>noexcept</code> e atributos não entram.', pills: [['→ 3.4', '']] },
  };
  const ANAT_PASSOS = ['', 'atr', 'esp', 'nome', 'par', 'exc', 'ret', 'ident'];

  Deck.widget('funcoes/anatomia', {
    iniciar(raiz, ctx) {
      raiz.classList.add('f-anat');
      const sig = el('div', { class: 'anat-sig', 'aria-label': '[[nodiscard]] constexpr auto soma(int a, int b = 0) noexcept -> int;' });
      const pos = {};
      let x = 0;
      ANAT_TOK.forEach((tk) => {
        const s = el('span', { class: ['tok', tk.c || '', tk.id ? 'ident' : '', tk.sub ? 'sub-' + tk.sub : ''].join(' ').trim(), text: tk.t });
        if (tk.p) { s.dataset.p = tk.p; s.addEventListener('click', () => mostrar(raiz, tk.p)); }
        sig.append(s);
        if (tk.k) { pos[tk.k] = pos[tk.k] || { ini: x, fim: x }; pos[tk.k].fim = x + tk.t.length; }
        x += tk.t.length;
      });
      const chaves = el('div', { class: 'anat-chaves', 'aria-hidden': 'true' });
      ANAT_CHAVES.forEach((c) => {
        const p = pos[c.k];
        const ch = el('div', { class: 'chave', dataset: { p: c.p }, style: { left: (p.ini + 0.12) + 'ch', width: (p.fim - p.ini - 0.24) + 'ch' } },
          el('span', { class: 'rot', text: c.rot }));
        ch.style.setProperty('--c', c.cor);
        ch.addEventListener('click', () => mostrar(raiz, c.p));
        chaves.append(ch);
      });
      const painel = el('div', { class: 'anat-painel' }, el('div', { class: 'anat-grade' }, sig, chaves));
      raiz._info = el('div', { class: 'anat-info' });
      // cada uso se liga às peças que ele demonstra (acende quando a peça está em foco)
      const uso = (codigo, res, cls, liga) => el('div', { class: 'anat-uso ' + (cls || ''), dataset: { liga } }, cod(codigo), el('span', { class: 'anat-res', html: res }));
      raiz.append(painel, raiz._info,
        el('div', { class: 'anat-usos' },
          el('span', { class: 'anat-usos-rot', text: 'em uso' }),
          uso('soma(2, 3)', '→ <b>5</b>', '', 'nome ret'),
          uso('soma(7)', '→ <b>7</b> <span>(b = 0)</span>', '', 'par'),
          uso('static_assert(soma(2, 3) == 5);', '✓ compila', 'ok', 'esp'),
          uso('soma(1, 2);', '⚠ warning: ignoring return value', 'aviso', 'atr')),
        el('p', { class: 'mini apagado anat-dica', text: 'Clique numa peça para rever a explicação.' }));
      raiz._ctx = ctx;
    },
    passo(raiz, k) { mostrar(raiz, ANAT_PASSOS[k] ?? ''); },
  });

  function mostrar(raiz, foco) {
    raiz.dataset.foco = foco;
    raiz.querySelectorAll('.tok').forEach((s) => {
      const ativo = foco === 'ident' ? s.classList.contains('ident') : (!!foco && s.dataset.p === foco);
      s.classList.toggle('ativo', ativo);
      s.classList.toggle('apagado', !!foco && !ativo);
    });
    raiz.querySelectorAll('.chave').forEach((c) => {
      const ativo = !!foco && c.dataset.p === foco;
      c.classList.toggle('ativo', ativo);
      c.classList.toggle('apagado', !!foco && !ativo);
    });
    // a faixa "em uso" mostra o efeito da peça em foco (se houver um uso ligado a ela)
    const usos = [...raiz.querySelectorAll('.anat-uso')];
    const ligado = (u) => !!foco && (u.dataset.liga || '').split(' ').includes(foco);
    const algum = usos.some(ligado);
    usos.forEach((u) => { u.classList.toggle('liga', ligado(u)); u.classList.toggle('longe', algum && !ligado(u)); });
    const inf = ANAT_INFO[foco] || ANAT_INFO[''];
    const pills = inf.pills.map(([t, c]) => `<span class="pill ${c}">${t}</span>`).join('');
    raiz._info.innerHTML = `
      <div class="anat-num">${inf.n || '6'}</div>
      <div class="anat-txt">
        <div class="anat-tit"><h3>${inf.tit}</h3><code class="cpp anat-cod">${realcar(inf.cod)}</code></div>
        <p>${inf.txt}</p>
        <div class="linha gap-s">${pills}</div>
      </div>`;
    raiz._info.classList.remove('troca');
    void raiz._info.offsetWidth;
    raiz._info.classList.add('troca');
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  2. ODR e o linker — quatro cenários reais (nm -C / ld do g++ 13.3)
  // ───────────────────────────────────────────────────────────────────────────
  const LNK = [
    { aba: '① .hpp declara, .cpp define',
      hpp: ['int multiplicar(int, int);', '// só a declaração'],
      a: ['#include "util.hpp"', 'int multiplicar(int a, int b)', '{ return a * b; }'],
      b: ['#include "util.hpp"', 'int main() {', '  … multiplicar(6, 7) … }'],
      ao: [['T', 'multiplicar(int, int)', 'casa']], bo: [['U', 'multiplicar(int, int)', 'casa'], ['T', 'main']],
      ok: true, cmd: 'g++ a.o b.o -o prog && ./prog', saida: ['42'],
      licao: 'b.o <b>usa</b> (U) e a.o <b>define</b> (T): o linker casa as pontas. N declarações, <b>uma</b> definição.' },
    { aba: '② definição no .hpp',
      hpp: ['int multiplicar(int a, int b)', '{ return a * b; }', '// definição no header!'],
      a: ['#include "util.hpp"', 'int usa_a() {', '  … multiplicar(2, 3) … }'],
      b: ['#include "util.hpp"', 'int main() {', '  … multiplicar(6, 7) … }'],
      ao: [['T', 'multiplicar(int, int)', 'conflito'], ['T', 'usa_a()']], bo: [['T', 'multiplicar(int, int)', 'conflito'], ['T', 'main']],
      ok: false, cmd: 'g++ a.o b.o -o prog',
      saida: ["/usr/bin/ld: b.o: in function `multiplicar(int, int)':",
        "b.cpp:(.text+0x0): ⟦multiple definition⟧ of `multiplicar(int, int)'; a.o:a.cpp:(.text+0x0): first defined here",
        'collect2: error: ld returned 1 exit status'],
      licao: 'O header foi <b>colado</b> nas duas .cpp: cada uma compilou a sua definição. Dois T para o mesmo nome → violou a ODR.' },
    { aba: '③ … com inline',
      hpp: ['inline int', 'multiplicar(int a, int b)', '{ return a * b; }'],
      a: ['#include "util.hpp"', 'int usa_a() {', '  … multiplicar(2, 3) … }'],
      b: ['#include "util.hpp"', 'int main() {', '  … multiplicar(6, 7) … }'],
      ao: [['W', 'multiplicar(int, int)', 'fica'], ['T', 'usa_a()']], bo: [['W', 'multiplicar(int, int)', 'descarta'], ['T', 'main']],
      ok: true, cmd: 'g++ a.o b.o -o prog && ./prog', saida: ['42'],
      licao: '<code>inline</code> = “definições idênticas repetidas são OK”: o símbolo vira <b>fraco</b> (W) e o linker fica com uma cópia.' },
    { aba: '④ ninguém define',
      hpp: ['int multiplicar(int, int);', '// só a declaração'],
      a: null,
      b: ['#include "util.hpp"', 'int main() {', '  … multiplicar(6, 7) … }'],
      ao: null, bo: [['U', 'multiplicar(int, int)', 'orfa'], ['T', 'main']],
      ok: false, cmd: 'g++ b.o -o prog',
      saida: ["/usr/bin/ld: b.o: in function `main':",
        "b.cpp:(.text+0x13): ⟦un\u2060defined reference⟧ to `multiplicar(int, int)'",
        'collect2: error: ld returned 1 exit status'],
      licao: 'Todo mundo prometeu, ninguém cumpriu: o U de b.o não encontra nenhum T. O compilador aceitou; o <b>linker</b> recusa.' },
  ];

  Deck.widget('funcoes/linker', {
    iniciar(raiz, ctx) {
      raiz.classList.add('f-lnk');
      raiz._ctx = ctx;
      raiz._abas = botoes(LNK.map((c, i) => ({ id: i, txt: c.aba })), (i) => cenario(raiz, +i, true), 'lnk-abas');
      const caixa = (cls, titulo) => {
        const corpo = el('div', { class: 'lnk-corpo' });
        const b = el('div', { class: 'lnk-caixa ' + cls }, el('div', { class: 'lnk-tit', html: titulo }), corpo);
        b._corpo = corpo;
        return b;
      };
      raiz._hpp = caixa('hpp', '<span class="lnk-ico">h</span>util.hpp');
      raiz._a = caixa('cpp a', '<span class="lnk-ico">c</span>a.cpp');
      raiz._b = caixa('cpp b', '<span class="lnk-ico">c</span>b.cpp');
      raiz._ao = caixa('obj ao', '<span class="lnk-ico">o</span>a.o <span class="lnk-nm">nm -C</span>');
      raiz._bo = caixa('obj bo', '<span class="lnk-ico">o</span>b.o <span class="lnk-nm">nm -C</span>');
      raiz._ld = el('div', { class: 'lnk-ld' }, el('div', { class: 'lnk-ld-tit', text: 'ld' }), el('div', { class: 'lnk-ld-sub', text: 'o linker' }), el('div', { class: 'lnk-ld-res' }));
      // geometria do palco (1408 × 294): caixas em CSS, setas aqui — mantenha os dois em sincronia
      const s = svg('svg', { class: 'diagrama lnk-setas', viewBox: '0 0 1408 294', width: 1408, height: 294, 'aria-hidden': 'true' },
        svg('defs', null, svg('marker', { id: 'funcoes-lnk-seta', viewBox: '0 0 10 10', refX: 9, refY: 5, markerWidth: 7, markerHeight: 7, orient: 'auto-start-reverse' },
          svg('path', { d: 'M0,0 L10,5 L0,10 z', style: 'fill: var(--texto-3)' }))),
        svg('path', { class: 'lnk-s s-inc-a', d: 'M312,126 C342,126 342,66 366,66', 'marker-end': 'url(#funcoes-lnk-seta)' }),
        svg('path', { class: 'lnk-s s-inc-b', d: 'M312,168 C342,168 342,228 366,228', 'marker-end': 'url(#funcoes-lnk-seta)' }),
        svg('path', { class: 'lnk-s s-cc-a', d: 'M732,66 L786,66', 'marker-end': 'url(#funcoes-lnk-seta)' }),
        svg('path', { class: 'lnk-s s-cc-b', d: 'M732,228 L786,228', 'marker-end': 'url(#funcoes-lnk-seta)' }),
        svg('path', { class: 'lnk-s s-ld-a', d: 'M1112,66 C1146,66 1146,126 1176,126', 'marker-end': 'url(#funcoes-lnk-seta)' }),
        svg('path', { class: 'lnk-s s-ld-b', d: 'M1112,228 C1146,228 1146,168 1176,168', 'marker-end': 'url(#funcoes-lnk-seta)' }),
        svg('text', { x: 735, y: 54, class: 'lnk-rot' }, 'g++ -c'),
        svg('text', { x: 735, y: 216, class: 'lnk-rot' }, 'g++ -c'));
      const legenda = el('div', { class: 'lnk-legenda', html: '<span><b class="nm T">T</b> definida aqui</span><span><b class="nm U">U</b> usada, definida fora</span><span><b class="nm W">W</b> fraca (inline)</span>' });
      const palco = el('div', { class: 'lnk-palco' }, s, legenda, raiz._hpp, el('span', { class: 'lnk-inc', html: '#include cola este texto<br>nas duas .cpp' }), raiz._a, raiz._b, raiz._ao, raiz._bo, raiz._ld);
      raiz._licao = el('p', { class: 'lnk-licao' });
      raiz._term = el('div', { class: 'lnk-term' });
      raiz.append(raiz._abas, palco, raiz._term, raiz._licao);
    },
    passo(raiz, k, ctx, dir) { cenario(raiz, k, dir > 0 && k > 0); },
  });

  function linhasCod(lista) { return lista.map((l) => el('code', { class: 'f-cod', html: realcar(l) })); }

  async function cenario(raiz, i, animar) {
    const c = LNK[i];
    const ctx = raiz._ctx;
    const espera = ficha(raiz, ctx);
    animar = animar && !ctx.reduzido;
    marcarBotao(raiz._abas, i);
    raiz.dataset.cen = String(i + 1);
    raiz.dataset.ok = c.ok ? '1' : '0';
    raiz._hpp._corpo.replaceChildren(...linhasCod(c.hpp));
    raiz._a.classList.toggle('ausente', !c.a);
    raiz._a._corpo.replaceChildren(...(c.a ? linhasCod(c.a) : [el('span', { class: 'lnk-vazio', text: 'nenhuma .cpp define multiplicar' })]));
    raiz._b._corpo.replaceChildren(...linhasCod(c.b));
    const simbolos = (lista) => (lista || []).map(([t, nome, est]) => el('div', { class: 'lnk-sim' + (est ? ' ' + est : '') }, el('b', { class: 'nm ' + t, text: t }), el('span', { text: nome })));
    raiz._ao.classList.toggle('ausente', !c.ao);
    raiz._ao._corpo.replaceChildren(...(c.ao ? simbolos(c.ao) : [el('span', { class: 'lnk-vazio', text: '(não existe)' })]));
    raiz._bo._corpo.replaceChildren(...simbolos(c.bo));
    const res = raiz._ld.querySelector('.lnk-ld-res');
    res.innerHTML = c.ok ? '<span class="ok-txt">✓</span> executável' : '<span class="erro-txt">✗</span> erro de link';
    raiz._licao.innerHTML = c.licao;
    const term = Deck.util.terminal(c.saida.join('\n'), { titulo: 'bash — o linker', comando: c.cmd, classe: 'compacto' });
    raiz._term.replaceChildren(term);
    raiz.classList.remove('etapa-1', 'etapa-2', 'etapa-3');
    if (!animar) { raiz.classList.add('etapa-1', 'etapa-2', 'etapa-3'); return; }
    if (!(await espera(120))) return void raiz.classList.add('etapa-1', 'etapa-2', 'etapa-3');
    raiz.classList.add('etapa-1');
    if (!(await espera(520))) return void raiz.classList.add('etapa-2', 'etapa-3');
    raiz.classList.add('etapa-2');
    if (!(await espera(620))) return void raiz.classList.add('etapa-3');
    raiz.classList.add('etapa-3');
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  3. Passagem de argumentos — chamador × função
  // ───────────────────────────────────────────────────────────────────────────
  const PAS = [
    { id: 'valor', aba: 'T', v: 'a', fn: 'por_valor(std::string s)', fases: 3,
      cod: 'void por_valor(std::string s) { s += " (mexido)"; }', cham: 'std::string a = original; por_valor(a);',
      saida: 'por valor   `void f(std::string s)`  → "texto"  ← CÓPIA, original intacto', custo: '1 cópia de std::string',
      meio: 'viaja uma <b>CÓPIA</b>', fim: 'texto', sFim: 'texto (mexido)' },
    { id: 'ref', aba: 'T&amp;', v: 'b', fn: 'por_referencia(std::string&amp; s)', fases: 2,
      cod: 'void por_referencia(std::string& s) { s += " (mexido)"; }', cham: 'std::string b = original; por_referencia(b);',
      saida: 'por ref     `void f(std::string& s)` → "texto (mexido)"  ← modificou o original', custo: '0 cópias',
      meio: 'nada viaja: <b>s</b> é outro nome para <b>b</b>', fim: 'texto (mexido)' },
    { id: 'cref', aba: 'const T&amp;', v: 'c', fn: 'por_const_ref(const std::string&amp; s)', fases: 2,
      cod: 'void por_const_ref(const std::string& s) { (void)s; /* só leitura */ }', cham: 'std::string c = original; por_const_ref(c);',
      saida: 'por const&  `void f(const std::string&)` → "texto"  ← sem cópia, sem escrita', custo: '0 cópias',
      meio: 'nada viaja; <b>s</b> só pode <b>ler</b>', fim: 'texto' },
    { id: 'ptr', aba: 'T*', v: 'd', fn: 'por_ponteiro(std::string* s)', fases: 3,
      cod: 'void por_ponteiro(std::string* s) { if (s) *s += " (mexido)"; }', cham: 'std::string d = original; por_ponteiro(&d);',
      saida: 'por ptr     `void f(std::string* s)` → "texto (mexido)"  ← pode ser nullptr', custo: 'copia só o endereço: 8 bytes',
      meio: 'viaja um <b>ENDEREÇO</b>', fim: 'texto (mexido)' },
    { id: 'rref', aba: 'T&amp;&amp;', v: 'e', fn: 'por_rvalue_ref(std::string&amp;&amp; s)', fases: 3,
      cod: 'void por_rvalue_ref(std::string&& s) { std::string roubado = std::move(s); (void)roubado; }', cham: 'std::string e = original; por_rvalue_ref(std::move(e));',
      saida: 'por &&      `void f(std::string&& s)` → recebe temporários / move', custo: '0 cópias · 1 move',
      meio: '<b>std::move(e)</b> só converte; quem rouba é <b>roubado = std::move(s)</b>', fim: '?' },
  ];

  Deck.widget('funcoes/passagem', {
    iniciar(raiz, ctx) {
      raiz.classList.add('f-pas');
      raiz._ctx = ctx;
      raiz._abas = el('div', { class: 'botoes pas-abas' });
      PAS.forEach((f, i) => {
        const b = el('button', { type: 'button', class: 'botao', dataset: { id: String(i + 1) } },
          el('span', { class: 'pas-aba-t', html: f.aba }), el('span', { class: 'pas-aba-f', html: f.fn.replace(/\(.*$/, '') }));
        b.addEventListener('click', (e) => { e.stopPropagation(); b.blur(); forma(raiz, i + 1, true); });
        raiz._abas.append(b);
      });
      const caixaVar = (cls) => {
        const v = el('div', { class: 'pas-val' });
        const n = el('span', { class: 'pas-nome' });
        const b = el('div', { class: 'pas-var ' + cls }, n, v);
        b._v = v; b._n = n;
        return b;
      };
      raiz._orig = caixaVar('orig');
      raiz._s = caixaVar('param');
      raiz._roub = caixaVar('roubado');
      raiz._roub._n.textContent = 'roubado';
      raiz._fant = el('div', { class: 'pas-fantasma', text: '"texto"' });
      raiz._fnTit = el('span', { class: 'pas-fn' });
      raiz._meio = el('div', { class: 'pas-meio' });
      raiz._extra = el('div', { class: 'pas-extra' });
      const cadeado = el('div', { class: 'pas-cadeado', 'aria-hidden': 'true', html: '<svg viewBox="0 0 24 24"><rect x="5" y="11" width="14" height="10" rx="2.5"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/></svg>' });
      const s = svg('svg', { class: 'diagrama pas-setas', viewBox: '0 0 1408 300', width: 1408, height: 300, 'aria-hidden': 'true' },
        svg('defs', null,
          svg('marker', { id: 'funcoes-pas-seta', viewBox: '0 0 10 10', refX: 8, refY: 5, markerWidth: 6, markerHeight: 6, orient: 'auto-start-reverse' },
            svg('path', { d: 'M0,0 L10,5 L0,10 z', style: 'fill: var(--cor)' })),
          svg('marker', { id: 'funcoes-pas-seta-e', viewBox: '0 0 10 10', refX: 8, refY: 5, markerWidth: 6, markerHeight: 6, orient: 'auto-start-reverse' },
            svg('path', { d: 'M0,0 L10,5 L0,10 z', style: 'fill: var(--aviso)' })),
          svg('marker', { id: 'funcoes-pas-seta-i', viewBox: '0 0 10 10', refX: 8, refY: 5, markerWidth: 6, markerHeight: 6, orient: 'auto-start-reverse' },
            svg('path', { d: 'M0,0 L10,5 L0,10 z', style: 'fill: var(--info)' })),
          svg('marker', { id: 'funcoes-pas-seta-p', viewBox: '0 0 10 10', refX: 8, refY: 5, markerWidth: 6, markerHeight: 6, orient: 'auto-start-reverse' },
            svg('path', { d: 'M0,0 L10,5 L0,10 z', style: 'fill: var(--tk-num)' }))),
        svg('path', { class: 'pas-s s-copia', d: 'M438,132 L964,132', pathLength: 1, 'marker-end': 'url(#funcoes-pas-seta)' }),
        svg('path', { class: 'pas-s s-ref', d: 'M968,132 L440,132', pathLength: 1, 'marker-end': 'url(#funcoes-pas-seta)' }),
        svg('path', { class: 'pas-s s-move', d: 'M438,150 C700,150 700,232 964,232', pathLength: 1, 'marker-end': 'url(#funcoes-pas-seta-e)' }));
      const palco = el('div', { class: 'pas-palco' },
        el('div', { class: 'pas-quadro chamador' }, el('div', { class: 'pas-q-tit', html: '<span class="rotulo">chamador</span><span class="pas-fn">demo_passagem()</span>' }), raiz._orig),
        el('div', { class: 'pas-quadro funcao' }, el('div', { class: 'pas-q-tit' }, el('span', { class: 'rotulo', text: 'função' }), raiz._fnTit), raiz._s, raiz._roub),
        s, raiz._fant, raiz._meio, cadeado, raiz._extra);
      raiz._cod = el('div', { class: 'pas-cod' });
      raiz._saida = el('div', { class: 'pas-saida' });
      raiz._custo = el('span', { class: 'pill grande pas-custo' });
      raiz.append(raiz._abas, palco, el('div', { class: 'pas-rodape' }, raiz._cod, el('div', { class: 'linha gap-s pas-linha' }, raiz._saida, raiz._custo)));
    },
    passo(raiz, k, ctx, dir) { forma(raiz, k, dir > 0 && k > 0); },
  });

  function pasFase(raiz, f, fase) {
    raiz.dataset.fase = String(fase);
    const o = raiz._orig._v, s = raiz._s._v, r = raiz._roub._v;
    if (!f) return;
    // valores de cada fase (o estado final de cada forma é o da última fase)
    if (f.id === 'valor') { o.textContent = '"texto"'; s.textContent = fase >= 3 ? '"texto (mexido)"' : '"texto"'; }
    if (f.id === 'ref') { o.textContent = fase >= 2 ? '"texto (mexido)"' : '"texto"'; }
    if (f.id === 'cref') { o.textContent = '"texto"'; }
    if (f.id === 'ref' || f.id === 'cref' || f.id === 'rref') s.textContent = '≡ ' + f.v;
    if (f.id === 'ptr') { o.textContent = fase >= 3 ? '"texto (mexido)"' : '"texto"'; s.textContent = '&d'; }
    if (f.id === 'rref') { o.textContent = fase >= 3 ? '?' : '"texto"'; r.textContent = '"texto"'; }
  }

  async function forma(raiz, k, animar) {
    const ctx = raiz._ctx;
    const espera = ficha(raiz, ctx);
    animar = animar && !ctx.reduzido;
    const f = PAS[k - 1] || null;
    marcarBotao(raiz._abas, k);
    raiz.dataset.forma = f ? f.id : '';
    raiz._orig._n.textContent = f ? f.v : 'original';
    raiz._s._n.textContent = 's';
    raiz._fnTit.innerHTML = f ? f.fn : 'escolha uma forma ↑';
    raiz._meio.innerHTML = f ? f.meio : '';
    raiz._extra.innerHTML = !f ? '' : f.id === 'cref'
      ? '<span class="pas-balao erro">s += "…" <b>✗ não compila</b>: s é const</span>'
      : f.id === 'ptr' ? '<span class="pas-balao ok"><b>if (s)</b> ✓ não é nulo</span>'
        : f.id === 'rref' ? '<span class="pas-balao aviso"><b>e</b>: válido, mas não especificado</span>'
          : f.id === 'valor' ? '<span class="pas-balao ok"><b>a</b> intacto ✓</span>' : '';
    raiz._cod.replaceChildren(cod(f ? f.cod : 'std::string original = "texto";'));
    raiz._saida.innerHTML = f ? Deck.util.realcar(f.saida, 'saida') : '<span class="pas-dica">↑ cinco formas, a mesma string</span>';
    raiz._custo.textContent = f ? f.custo : 'custo';
    raiz._custo.style.visibility = f ? '' : 'hidden';
    raiz._orig._v.textContent = '"texto"';
    if (!f) { pasFase(raiz, null, 0); raiz.dataset.fase = '0'; return; }
    if (!animar) { pasFase(raiz, f, f.fases); return; }
    pasFase(raiz, f, 0);
    if (!(await espera(250))) return void pasFase(raiz, f, f.fases);
    if (f.id === 'valor') {
      pasFase(raiz, f, 1);                                   // a cópia voa
      if (!(await espera(900))) return void pasFase(raiz, f, 3);
      pasFase(raiz, f, 2);
      if (!(await espera(350))) return void pasFase(raiz, f, 3);
      raiz.dataset.fase = '3';
      await digitar(raiz._s._v, '"texto', '"texto (mexido)"', espera);
      return;
    }
    if (f.id === 'ref') {
      pasFase(raiz, f, 1);
      if (!(await espera(800))) return void pasFase(raiz, f, 2);
      raiz.dataset.fase = '2';
      await digitar(raiz._orig._v, '"texto', '"texto (mexido)"', espera);
      return;
    }
    if (f.id === 'cref') {
      pasFase(raiz, f, 1);
      if (!(await espera(900))) return void pasFase(raiz, f, 2);
      pasFase(raiz, f, 2);
      return;
    }
    if (f.id === 'ptr') {
      pasFase(raiz, f, 1);
      if (!(await espera(800))) return void pasFase(raiz, f, 3);
      pasFase(raiz, f, 2);
      if (!(await espera(650))) return void pasFase(raiz, f, 3);
      raiz.dataset.fase = '3';
      await digitar(raiz._orig._v, '"texto', '"texto (mexido)"', espera);
      return;
    }
    if (f.id === 'rref') {
      pasFase(raiz, f, 1);
      if (!(await espera(850))) return void pasFase(raiz, f, 3);
      pasFase(raiz, f, 2);                                   // o conteúdo voa para roubado
      if (!(await espera(900))) return void pasFase(raiz, f, 3);
      pasFase(raiz, f, 3);
    }
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  4. Argumentos padrão — o compilador completa a chamada
  // ───────────────────────────────────────────────────────────────────────────
  const PAD_PARAMS = [
    { tipo: 'const std::string&', nome: 'texto', padrao: null },
    { tipo: 'char', nome: 'preenchimento', padrao: "'.'" },
    { tipo: 'int', nome: 'largura', padrao: '20' },
  ];
  const PAD_CHAM = [
    { args: ['"abc"'], texto: 'abc', ch: '.', larg: 20 },
    { args: ['"abc"', "'-'"], texto: 'abc', ch: '-', larg: 20 },
    { args: ['"abc"', "'-'", '10'], texto: 'abc', ch: '-', larg: 10 },
  ];

  Deck.widget('funcoes/padroes', {
    iniciar(raiz, ctx) {
      raiz.classList.add('f-pad');
      raiz._ctx = ctx;
      const decl = el('div', { class: 'pad-decl' }, el('code', { class: 'f-cod', html:
        realcar('std::string formatar(const std::string& texto, char preenchimento = ⟦\'.\'⟧, int largura = ⟦20⟧);') }));
      raiz._cham = el('div', { class: 'botoes pad-chamadas' });
      PAD_CHAM.forEach((c, i) => {
        const b = el('button', { type: 'button', class: 'botao', dataset: { id: String(i + 1) } }, cod(`formatar(${c.args.join(', ')})`));
        b.addEventListener('click', (e) => { e.stopPropagation(); b.blur(); chamar(raiz, i + 1, true); });
        raiz._cham.append(b);
      });
      raiz._slots = PAD_PARAMS.map((p) => {
        const val = el('div', { class: 'pad-val' });
        const ori = el('div', { class: 'pad-ori' });
        const s = el('div', { class: 'pad-slot' }, el('div', { class: 'pad-slot-tit', html: `<span class="tk-tipo">${Deck.util.escapar(p.tipo)}</span> <b>${p.nome}</b>` }), val, ori);
        s._val = val; s._ori = ori;
        return s;
      });
      raiz._reescrita = el('div', { class: 'pad-reescrita' });
      raiz._celulas = el('div', { class: 'pad-celulas' });
      raiz.append(decl,
        el('div', { class: 'pad-meio' }, raiz._cham, el('div', { class: 'pad-slots' }, ...raiz._slots)),
        el('div', { class: 'pad-baixo' },
          el('div', { class: 'pad-bloco' }, el('span', { class: 'pad-rot', text: 'o compilador escreve' }), raiz._reescrita),
          el('div', { class: 'pad-bloco' }, el('span', { class: 'pad-rot', text: 'resultado' }), raiz._celulas)));
    },
    passo(raiz, k, ctx, dir) { chamar(raiz, k, dir > 0 && k > 0); },
  });

  async function chamar(raiz, k, animar) {
    const ctx = raiz._ctx;
    const espera = ficha(raiz, ctx);
    animar = animar && !ctx.reduzido;
    const c = PAD_CHAM[k - 1] || null;
    marcarBotao(raiz._cham, k);
    raiz.dataset.k = String(k);
    raiz.classList.toggle('animando', animar);
    raiz._slots.forEach((s, i) => {
      const p = PAD_PARAMS[i];
      s.classList.remove('do-chamador', 'do-padrao', 'vazio', 'entra');
      if (!c) {
        s._val.innerHTML = p.padrao ? realcar(p.padrao) : '—';
        s._ori.textContent = p.padrao ? 'padrão disponível' : 'sem padrão: obrigatório';
        s.classList.add('vazio');
        return;
      }
      const doCham = i < c.args.length;
      s._val.innerHTML = realcar(doCham ? c.args[i] : p.padrao);
      s._ori.textContent = doCham ? 'veio do chamador' : 'padrão — colado pelo compilador';
      s.classList.add(doCham ? 'do-chamador' : 'do-padrao');
    });
    if (!c) {
      raiz._reescrita.innerHTML = '<span class="pad-dica">escolha uma chamada ←</span>';
      raiz._celulas.replaceChildren();
      return;
    }
    const partes = PAD_PARAMS.map((p, i) => (i < c.args.length ? c.args[i] : `⟦${p.padrao}⟧`));
    raiz._reescrita.replaceChildren(cod(`formatar(${partes.join(', ')});`));
    let out = c.texto;
    while (out.length < c.larg) out += c.ch;
    const cels = [el('span', { class: 'pad-colchete', text: '[' })];
    [...out].forEach((ch, i) => cels.push(el('span', { class: 'pad-cel' + (i >= c.texto.length ? ' enchido' : ''), style: { '--i': i }, text: ch })));
    cels.push(el('span', { class: 'pad-colchete', text: ']' }), el('span', { class: 'pad-conta', text: `${out.length} posições` }));
    raiz._celulas.replaceChildren(...cels);
    if (!animar) { raiz.classList.add('pronto'); return; }
    raiz.classList.remove('pronto');
    raiz._slots.forEach((s, i) => s.style.setProperty('--d', i < c.args.length ? i * 160 + 'ms' : 520 + (i - c.args.length) * 180 + 'ms'));
    void raiz.offsetWidth;
    if (!(await espera(40))) return void raiz.classList.add('pronto');
    raiz.classList.add('pronto');
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  5. Padrão + virtual — duas metades de duas fases diferentes
  // ───────────────────────────────────────────────────────────────────────────
  Deck.widget('funcoes/virtual', {
    iniciar(raiz, ctx) {
      raiz.classList.add('f-vir');
      raiz._ctx = ctx;
      const elo = (txt, cls) => el('div', { class: 'vir-elo ' + (cls || ''), html: txt });
      const seta = () => el('span', { class: 'vir-seta', text: '→' });
      raiz.append(
        el('div', { class: 'vir-faixa comp' },
          el('div', { class: 'vir-faixa-tit', html: '<b>1 · compilação</b> — olha o tipo <b>estático</b>' }),
          el('div', { class: 'vir-cadeia' },
            elo(`<code class="cpp">${realcar('b.f()')}</code>`), seta(),
            elo('<span class="vir-mini">b é</span> <code class="cpp">' + realcar('Base&') + '</code>'), seta(),
            elo(`<code class="cpp">${realcar('Base::f(int x = 1)')}</code>`), seta(),
            elo(`<code class="cpp">${realcar('b.f(⟦1⟧)')}</code>`, 'chave'))),
        el('div', { class: 'vir-faixa exec' },
          el('div', { class: 'vir-faixa-tit', html: '<b>2 · execução</b> — olha o tipo <b>dinâmico</b>' }),
          el('div', { class: 'vir-cadeia' },
            elo('<span class="vir-mini">o objeto é</span> <code class="cpp">' + realcar('Derivada') + '</code>'), seta(),
            elo('<span class="vir-mini">vptr → vtable de</span> <code class="cpp">' + realcar('Derivada') + '</code>'), seta(),
            elo(`<code class="cpp">${realcar('Derivada::f(1)')}</code>`, 'chave'))),
        el('div', { class: 'vir-saida' },
          el('div', { class: 'vir-saida-tit', text: 'saída real' }),
          // antes do passo 3 as linhas mostram só a pergunta ("?"), como os comentários do código
          el('div', { class: 'vir-linha l1', html: '<code class="cpp">b.f();</code> <span class="vir-q">?</span><span class="vir-out">Derivada::f(x = <b class="destaque">1</b>)</span> <span class="vir-nota">corpo da <b>Derivada</b> + padrão da <b>Base</b></span>' }),
          el('div', { class: 'vir-linha l2', html: '<code class="cpp">d.f();</code> <span class="vir-q">?</span><span class="vir-out">Derivada::f(x = <b>2</b>)</span> <span class="vir-nota">tipo estático = Derivada</span>' })));
    },
    passo(raiz, k, ctx, dir) {
      invalidar(raiz);
      raiz.dataset.k = String(k);
      raiz.classList.toggle('anima', dir > 0 && !ctx.reduzido);
    },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  6. A escada da resolução de sobrecarga (resultados verificados no g++)
  // ───────────────────────────────────────────────────────────────────────────
  const ESC_CAND = [
    { id: 'int', sig: '(int)' }, { id: 'dbl', sig: '(double)' }, { id: 'cch', sig: '(const char*)' },
    { id: 'str', sig: '(const std::string&)' }, { id: 'bool', sig: '(bool)' }, { id: 'ii', sig: '(int, int)' },
    { id: 'ell', sig: '(...)' },
  ];
  const ESC_DEG = [
    { r: 1, nome: 'exata', ex: 'identidade · array→ptr' },
    { r: 2, nome: 'promoção', ex: 'char→int · float→double' },
    { r: 3, nome: 'conversão padrão', ex: 'int→double · →bool' },
    { r: 4, nome: 'do usuário', ex: 'construtor · operator T' },
    { r: 5, nome: 'reticências', ex: '(...)' },
  ];
  const ESC_CHAM = [
    { id: '42', txt: 'tipo_de(42)', tipo: 'int', r: { int: [1, 'identidade'], dbl: [3, 'int → double'], bool: [3, 'int → bool'] } },
    { id: '314', txt: 'tipo_de(3.14)', tipo: 'double', r: { dbl: [1, 'identidade'], int: [3, 'double → int'], bool: [3, 'double → bool'] } },
    { id: 'a', txt: "tipo_de('a')", tipo: 'char', r: { int: [2, 'char → int'], dbl: [3, 'char → double'], bool: [3, 'char → bool'] } },
    { id: 'f', txt: 'tipo_de(3.14f)', tipo: 'float', r: { dbl: [2, 'float → double'], int: [3, 'float → int'], bool: [3, 'float → bool'] } },
    { id: 'true', txt: 'tipo_de(true)', tipo: 'bool', r: { bool: [1, 'identidade'], int: [2, 'bool → int'], dbl: [3, 'bool → double'] } },
    { id: 'lit', txt: 'tipo_de("literal")', tipo: 'const char[8]', r: { cch: [1, 'array → ponteiro'], bool: [3, 'ponteiro → bool'], str: [4, 'construtor std::string'] } },
    { id: 'str', txt: 'tipo_de(std::string{"s"})', tipo: 'std::string', r: { str: [1, 'liga a referência'] } },
    { id: '42u', txt: 'tipo_de(42u)', tipo: 'unsigned int', r: { int: [3, 'unsigned → int'], dbl: [3, 'unsigned → double'], bool: [3, 'unsigned → bool'] } },
    { id: '12', txt: 'tipo_de(1, 2)', tipo: 'int, int', dois: true, r: { ii: [1, 'identidade'] } },
    { id: 'ponto', txt: 'tipo_de(Ponto{1, 2})', tipo: 'Ponto', r: {} },
  ];
  const ESC_PASSOS = [['42', false], ['a', false], ['f', false], ['lit', false], ['42u', false], ['lit', true]];

  Deck.widget('funcoes/escada', {
    iniciar(raiz, ctx) {
      raiz.classList.add('f-esc');
      raiz._ctx = ctx;
      raiz._estado = { cham: '42', semCch: false, comEll: false };
      raiz._lista = el('div', { class: 'esc-chamadas' }, el('div', { class: 'esc-rot', text: 'a chamada' }));
      ESC_CHAM.forEach((c) => {
        const b = el('button', { type: 'button', class: 'botao', dataset: { id: c.id } }, cod(c.txt));
        b.addEventListener('click', (e) => { e.stopPropagation(); b.blur(); raiz._estado.cham = c.id; resolver(raiz, true); });
        raiz._lista.append(b);
      });
      raiz._chips = {};
      ESC_CAND.forEach((c) => {
        const ch = el('div', { class: 'esc-chip', dataset: { id: c.id } },
          el('span', { class: 'esc-sig', html: realcar(c.sig) }),
          el('span', { class: 'esc-conv' }));
        raiz._chips[c.id] = ch;
      });
      raiz._faixas = {};
      const escada = el('div', { class: 'esc-escada' });
      ESC_DEG.forEach((d) => {
        const faixa = el('div', { class: 'esc-faixa' });
        raiz._faixas[d.r] = faixa;
        escada.append(el('div', { class: 'esc-degrau', dataset: { r: d.r } },
          el('div', { class: 'esc-deg-rot' }, el('b', { class: 'esc-num', text: d.r }), el('span', null, el('span', { class: 'esc-nome', text: d.nome }), el('span', { class: 'esc-ex', text: d.ex }))),
          faixa));
      });
      const bandeja = el('div', { class: 'esc-faixa' });
      raiz._faixas[0] = bandeja;
      escada.append(el('div', { class: 'esc-degrau inviavel', dataset: { r: 0 } },
        el('div', { class: 'esc-deg-rot' }, el('b', { class: 'esc-num', text: '✗' }), el('span', null, el('span', { class: 'esc-nome', text: 'inviáveis' }), el('span', { class: 'esc-ex', text: 'sem conversão possível' }))),
        bandeja));
      Object.values(raiz._chips).forEach((ch) => bandeja.append(ch));
      const tog = (id, txt) => {
        const b = el('button', { type: 'button', class: 'botao esc-tog', dataset: { id }, html: txt });
        b.addEventListener('click', (e) => { e.stopPropagation(); b.blur(); raiz._estado[id] = !raiz._estado[id]; resolver(raiz, true); });
        return b;
      };
      raiz._togCch = tog('semCch', '<span class="esc-menos">−</span> <code class="cpp">tipo_de(const char*)</code>');
      raiz._togEll = tog('comEll', '<span class="esc-mais">+</span> <code class="cpp">tipo_de(...)</code>');
      raiz._veredito = el('div', { class: 'esc-veredito' });
      raiz.append(raiz._lista, el('div', { class: 'esc-dir' }, escada,
        el('div', { class: 'linha gap-s esc-opcoes' }, el('span', { class: 'esc-rot', text: 'mexer nas sobrecargas' }), raiz._togCch, raiz._togEll),
        raiz._veredito));
    },
    passo(raiz, k, ctx, dir) {
      const [cham, semCch] = ESC_PASSOS[k] || ESC_PASSOS[0];
      raiz._estado = { cham, semCch, comEll: false };
      resolver(raiz, dir > 0 && k > 0);
    },
  });

  function resolver(raiz, animar) {
    invalidar(raiz);
    const ctx = raiz._ctx;
    const st = raiz._estado;
    const c = ESC_CHAM.find((x) => x.id === st.cham);
    marcarBotao(raiz._lista, st.cham);
    raiz._togCch.setAttribute('aria-pressed', String(st.semCch));
    raiz._togEll.setAttribute('aria-pressed', String(st.comEll));
    // degrau de cada candidata para esta chamada
    const lugar = {};
    ESC_CAND.forEach((cand) => {
      if (cand.id === 'ell') { lugar.ell = st.comEll ? [5, 'aceita qualquer argumento'] : null; return; }
      if (cand.id === 'cch' && st.semCch) { lugar.cch = 'removida'; return; }
      lugar[cand.id] = c.r[cand.id] || [0, cand.id === 'ii' || c.dois ? 'aridade diferente' : 'sem conversão'];
    });
    const viaveis = Object.entries(lugar).filter(([, v]) => Array.isArray(v) && v[0] > 0);
    const melhor = viaveis.length ? Math.min(...viaveis.map(([, v]) => v[0])) : 0;
    const topo = viaveis.filter(([, v]) => v[0] === melhor).map(([id]) => id);
    const chips = Object.values(raiz._chips);
    flip(raiz, chips, () => {
      ESC_CAND.forEach((cand) => {
        const ch = raiz._chips[cand.id];
        const v = lugar[cand.id];
        ch.classList.remove('vence', 'empate', 'removida', 'oculta', 'perde');
        if (v === null) { ch.classList.add('oculta'); raiz._faixas[0].append(ch); return; }
        if (v === 'removida') { ch.classList.add('removida'); ch.querySelector('.esc-conv').textContent = 'removida'; raiz._faixas[0].append(ch); return; }
        ch.querySelector('.esc-conv').textContent = v[1];
        raiz._faixas[v[0]].append(ch);
        if (v[0] > 0) ch.classList.add(topo.includes(cand.id) ? (topo.length > 1 ? 'empate' : 'vence') : 'perde');
      });
    }, animar && !ctx.reduzido);
    const sig = (id) => ESC_CAND.find((x) => x.id === id).sig;
    let html;
    if (!topo.length) {
      html = `<span class="esc-v erro">✗</span><div><div class="gcc"><span class="e">error:</span> no matching function for call to ‘tipo_de(${c.tipo})’</div><span class="esc-v-sub">nenhuma candidata aceita um <code>${c.tipo}</code> — experimente <b>+ tipo_de(...)</b></span></div>`;
    } else if (topo.length > 1) {
      const nomes = topo.map((id) => sig(id).slice(1, -1));
      html = `<span class="esc-v erro">✗</span><div><div class="gcc"><span class="e">error:</span> call of overloaded ‘tipo_de(${c.tipo})’ is ambiguous</div><span class="esc-v-sub"><b>${nomes.slice(0, -1).join(', ')}</b> e <b>${nomes[nomes.length - 1]}</b> empatam no degrau ${melhor} (${ESC_DEG[melhor - 1].nome})</span></div>`;
    } else {
      const id = topo[0];
      const conv = lugar[id][1];
      // as derrotadas do degrau logo abaixo da vencedora (todas, não só a primeira)
      const outras = viaveis.filter(([x]) => x !== id);
      const r2 = outras.length ? Math.min(...outras.map(([, v]) => v[0])) : 0;
      const vices = outras.filter(([, v]) => v[0] === r2).map(([x]) => `<code>tipo_de${Deck.util.escapar(sig(x))}</code>`);
      const extra = vices.length
        ? `vence ${vices.length > 1 ? vices.slice(0, -1).join(', ') + ' e ' : ''}${vices[vices.length - 1]} (${ESC_DEG[r2 - 1].nome})`
        : 'a única viável';
      html = `<span class="esc-v ok">✓</span><div><div class="esc-v-tit"><code class="cpp">${realcar('tipo_de' + sig(id))}</code> <span class="esc-v-deg">degrau ${melhor} · ${ESC_DEG[melhor - 1].nome} (${conv})</span></div><span class="esc-v-sub">${extra}</span></div>`;
    }
    raiz._veredito.innerHTML = html;
    raiz._veredito.classList.toggle('erro', topo.length !== 1);
    raiz._veredito.classList.remove('troca');
    void raiz._veredito.offsetWidth;
    raiz._veredito.classList.add('troca');
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  7. Referência pendurada — a pilha por dentro
  // ───────────────────────────────────────────────────────────────────────────
  const PIL_COD_RUIM = [
    'int& f() { int x = 1; return x; }   // ✗',
    'void g() { int y = 99; (void)y; }',
    'int main() {',
    '    int& r = f();',
    '    g();',
    '    return r;   // ✗ lê memória morta: UB',
    '}',
  ];
  const PIL_COD_BOM = [
    'int f() { int x = 1; return x; }    // ✓ por valor',
    'void g() { int y = 99; (void)y; }',
    'int main() {',
    '    int r = f();   // r tem a SUA cópia',
    '    g();',
    '    return r;      // ✓ 1',
    '}',
  ];
  const PIL_EST = [
    { linhas: [4], txt: '<b>main</b> vai chamar <code>f()</code> e guardar o resultado numa <b>referência</b>.' },
    { linhas: [1, 4], txt: '<code>f()</code> <b>empilha</b> um quadro com a sua variável local <code>x = 1</code>.' },
    { linhas: [1, 4], txt: '<code>return x</code> devolve um <b>apelido</b> para <code>x</code> — e o quadro de <code>f</code> é desempilhado. <code>r</code> ficou <b>pendurada</b>.' },
    { linhas: [5, 6], txt: '<code>g()</code> <b>reaproveita</b> a região: <code>r</code> aponta para memória que agora é de <code>g</code>. Ler <code>r</code> é <span class="pill ub">UB</span>' },
    { linhas: [1, 4, 6], txt: 'A correção: devolva <b>por valor</b>. <code>r</code> guarda a <b>sua</b> cópia — barata (e, para objetos, a elisão evita até a cópia).', bom: true },
  ];

  Deck.widget('funcoes/pilha', {
    iniciar(raiz, ctx) {
      raiz.classList.add('f-pil');
      raiz._ctx = ctx;
      raiz._cod = el('pre', { class: 'f-mini-codigo' });
      const quadro = (cls, rot, linhas) => el('div', { class: 'pil-quadro ' + cls }, el('span', { class: 'rotulo', text: rot }), ...linhas);
      raiz._slotR = el('div', { class: 'pil-slot r' }, el('span', { class: 'pil-n', text: 'r' }), el('span', { class: 'pil-v', text: '?' }));
      raiz._main = quadro('main', 'main', [raiz._slotR]);
      raiz._f = quadro('f', 'f()', [el('div', { class: 'pil-slot x' }, el('span', { class: 'pil-n', text: 'x' }), el('span', { class: 'pil-v', text: '1' }))]);
      raiz._g = quadro('g', 'g()', [el('div', { class: 'pil-slot y' }, el('span', { class: 'pil-n', text: 'y' }), el('span', { class: 'pil-v', text: '99' }))]);
      const s = svg('svg', { class: 'diagrama pil-seta', viewBox: '0 0 420 330', width: 420, height: 330, 'aria-hidden': 'true' },
        svg('defs', null, svg('marker', { id: 'funcoes-pil-seta', viewBox: '0 0 10 10', refX: 8, refY: 5, markerWidth: 6, markerHeight: 6, orient: 'auto-start-reverse' },
          svg('path', { d: 'M0,0 L10,5 L0,10 z', style: 'fill: var(--erro)' }))),
        svg('path', { class: 'pil-ref', d: 'M338,270 C408,270 408,128 344,128', pathLength: 1, 'marker-end': 'url(#funcoes-pil-seta)' }));
      raiz._txt = el('div', { class: 'pil-txt' });
      const regiao = el('div', { class: 'pil-regiao' }, el('span', { class: 'pil-topo', text: 'topo da pilha ↑' }), raiz._f, raiz._g, raiz._main, s);
      raiz.append(raiz._cod, el('div', { class: 'pil-baixo' }, regiao, raiz._txt));
    },
    passo(raiz, k) {
      const e = PIL_EST[k] || PIL_EST[0];
      raiz.dataset.k = String(k);
      const linhas = e.bom ? PIL_COD_BOM : PIL_COD_RUIM;
      const html = realcar(linhas.join('\n')).split('\n');
      raiz._cod.replaceChildren(...linhas.map((l, i) => {
        const marca = /\/\/.*✗/.test(l) ? ' ln-ruim' : /\/\/.*✓/.test(l) ? ' ln-bom' : '';
        return el('span', { class: 'ln' + marca + (e.linhas.includes(i + 1) ? ' foco' : ''), html: html[i] || ' ' });
      }));
      raiz._cod.classList.toggle('focando', true);
      raiz._slotR.querySelector('.pil-v').textContent = k === 0 ? '?' : k === 4 ? '1' : k === 1 ? '…' : '→';
      raiz._txt.innerHTML = `<span class="pil-k">${k === 4 ? '✓' : k}</span><p>${e.txt}</p>`;
    },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  8. Fold expression — o pacote se desdobra e é reduzido
  // ───────────────────────────────────────────────────────────────────────────
  const FOLD_PACOTES = {
    n1234: { rot: '1, 2, 3, 4', itens: [1, 2, 3, 4].map((v) => ({ t: 'num', v })) },
    n1032: { rot: '10, 3, 2', itens: [10, 3, 2].map((v) => ({ t: 'num', v })) },
    nd: { rot: '1.5, 2.5', itens: [1.5, 2.5].map((v) => ({ t: 'num', v })) },
    s: { rot: '"x"s, "y", "z"', itens: [{ t: 'str', v: 'x' }, { t: 'ptr', v: 'y' }, { t: 'ptr', v: 'z' }] },
  };
  const FOLD_ERRO = 'invalid operands of types ‘const char*’ and ‘const char*’ to binary ‘operator+’';

  function foldArvore(itens, op, dir) {
    const f = itens.map((x) => ({ ...x }));
    if (dir === 'd') { let r = f[f.length - 1]; for (let i = f.length - 2; i >= 0; i--) r = { op, l: f[i], r }; return r; }
    let l = f[0]; for (let i = 1; i < f.length; i++) l = { op, l, r: f[i] }; return l;
  }
  const folha = (n) => !n.op;
  // número negativo: sinal tipográfico (−) e, como operando, entre parênteses: 2 − (−1)
  const foldNum = (v, topo) => (v < 0 ? (topo ? `−${-v}` : `(−${-v})`) : String(v));
  function foldTxt(n, alvo, topo = true) {
    if (folha(n)) return `<span class="fx-${n.t}">${n.t === 'num' ? foldNum(n.v, topo) : n.t === 'str' ? `"${n.v}"s` : `"${n.v}"`}</span>`;
    const s = `${foldTxt(n.l, alvo, false)} <span class="fx-op">${n.op === '-' ? '−' : n.op}</span> ${foldTxt(n.r, alvo, false)}`;
    const dentro = topo ? s : `(${s})`;
    return n === alvo ? `<mark class="fx-alvo">${dentro}</mark>` : dentro;
  }
  function foldReduzivel(n) {
    if (folha(n)) return null;
    if (folha(n.l) && folha(n.r)) return n;
    return foldReduzivel(n.l) || foldReduzivel(n.r);
  }
  function foldAplica(n) {
    const a = n.l, b = n.r;
    if (a.t === 'num' && b.t === 'num') return { t: 'num', v: n.op === '+' ? a.v + b.v : a.v - b.v };
    if (a.t === 'ptr' && b.t === 'ptr') return { erro: true };
    return { t: 'str', v: a.v + b.v };
  }
  function foldSubst(n, alvo, novo) {
    if (n === alvo) return novo;
    if (folha(n)) return n;
    return { op: n.op, l: foldSubst(n.l, alvo, novo), r: foldSubst(n.r, alvo, novo) };
  }
  function foldLinhas(itens, op, dir) {
    let a = foldArvore(itens, op, dir);
    const linhas = [];
    for (let guarda = 0; guarda < 12; guarda++) {
      const alvo = foldReduzivel(a);
      linhas.push({ html: foldTxt(a, alvo) });
      if (!alvo) break;
      const novo = foldAplica(alvo);
      if (novo.erro) { linhas.push({ erro: true }); break; }
      a = foldSubst(a, alvo, novo);
    }
    return linhas;
  }

  Deck.widget('funcoes/fold', {
    iniciar(raiz, ctx) {
      raiz.classList.add('f-fold');
      raiz._ctx = ctx;
      raiz._st = { pac: 'n1234', op: '+', dir: 'd' };
      const grupo = (rot, lista, chave) => {
        const g = botoes(lista, (id) => { raiz._st[chave] = id; if (raiz._st.pac === 's') raiz._st.op = '+'; explorar(raiz, true); }, 'fold-grupo');
        g.dataset.chave = chave;
        return el('div', { class: 'fold-ctrl-g' }, el('span', { class: 'fold-rot', text: rot }), g);
      };
      raiz._ctrl = el('div', { class: 'fold-ctrl' },
        grupo('pacote', Object.entries(FOLD_PACOTES).map(([id, p]) => ({ id, html: `<code class="cpp">${Deck.util.escapar(p.rot)}</code>` })), 'pac'),
        grupo('direção', [{ id: 'd', html: '<code class="cpp">(args op ...)</code>' }, { id: 'e', html: '<code class="cpp">(... op args)</code>' }], 'dir'),
        grupo('op', [{ id: '+', html: '<code class="cpp">+</code>' }, { id: '-', html: '<code class="cpp">−</code>' }], 'op'));
      raiz._palco = el('div', { class: 'fold-palco' });
      raiz.append(raiz._ctrl, raiz._palco);
    },
    passo(raiz, k, ctx, dir) {
      const animar = dir > 0 && k > 0 && !ctx.reduzido;
      raiz.dataset.k = String(k);
      if (k <= 2) { raiz._st = { pac: 'n1234', op: '+', dir: 'd' }; desenharFold(raiz, k === 0 ? 0 : k === 1 ? 1 : 99, animar && k === 2, animar); }
      else if (k === 3) { raiz._st = { pac: 'n1032', op: '-', dir: 'd' }; comparar(raiz, animar); }
      else virgula(raiz, animar);
    },
  });

  function foldCtrlSync(raiz) {
    raiz._ctrl.querySelectorAll('.fold-grupo').forEach((g) => marcarBotao(g, raiz._st[g.dataset.chave]));
    const menos = raiz._ctrl.querySelector('[data-chave="op"] [data-id="-"]');
    if (menos) menos.disabled = raiz._st.pac === 's';
  }
  function foldTmpl(op, dir) {
    const o = op === '-' ? '-' : '+';
    return dir === 'd' ? `(args ${o} ...)` : `(... ${o} args)`;
  }
  function foldPack(p) {
    return el('div', { class: 'fold-pack' }, el('span', { class: 'fold-rot', text: 'args =' }),
      el('span', { class: 'fold-chaves', text: '{' }),
      ...p.itens.map((x, i) => el('span', { class: 'fold-chip fx-' + x.t, style: { '--i': i }, text: x.t === 'num' ? String(x.v) : x.t === 'str' ? `"${x.v}"s` : `"${x.v}"` })),
      el('span', { class: 'fold-chaves', text: '}' }),
      el('span', { class: 'fold-n', html: `<code class="cpp">sizeof...(args)</code> = ${p.itens.length}` }));
  }
  function foldColuna(st, maxLinhas, titulo) {
    const p = FOLD_PACOTES[st.pac];
    const linhas = foldLinhas(p.itens, st.op, st.dir);
    const col = el('div', { class: 'fold-col' });
    if (titulo) col.append(el('div', { class: 'fold-col-tit', html: titulo }));
    col.append(el('div', { class: 'fold-tmpl' }, el('span', { class: 'fold-rot', text: st.dir === 'd' ? 'fold à direita' : 'fold à esquerda' }), cod(foldTmpl(st.op, st.dir))));
    const lista = el('div', { class: 'fold-linhas' });
    linhas.slice(0, maxLinhas).forEach((l, i) => {
      const ultima = i === linhas.length - 1;
      if (l.erro) {
        lista.append(el('div', { class: 'fold-linha erro', style: { '--i': i }, html: `<span class="fold-pre">✗</span><div class="gcc"><span class="e">error:</span> ${FOLD_ERRO}</div>` }));
        return;
      }
      lista.append(el('div', { class: 'fold-linha' + (ultima && i > 0 ? ' final' : '') + (i === 0 ? ' expande' : ''), style: { '--i': i },
        html: `<span class="fold-pre">${i === 0 ? '⇒' : '='}</span><span class="fold-expr">${l.html}</span>` }));
    });
    col.append(lista);
    return { col, total: linhas.length };
  }
  function desenharFold(raiz, maxLinhas, animarReducao, animar) {
    const st = raiz._st;
    foldCtrlSync(raiz);
    raiz._ctrl.classList.remove('oculto');
    const p = FOLD_PACOTES[st.pac];
    const { col } = foldColuna(st, maxLinhas);
    raiz._palco.replaceChildren(foldPack(p), col);
    raiz._palco.className = 'fold-palco' + (animar ? ' anima' : '') + (animarReducao ? ' reduz' : '');
  }
  function explorar(raiz, animar) { invalidar(raiz); desenharFold(raiz, 99, false, animar && !raiz._ctx.reduzido); }
  function comparar(raiz, animar) {
    const st = raiz._st;
    foldCtrlSync(raiz);
    marcarBotao(raiz._ctrl.querySelector('[data-chave="dir"]'), '');
    raiz._ctrl.classList.remove('oculto');
    const p = FOLD_PACOTES[st.pac];
    const a = foldColuna({ ...st, dir: 'd' }, 99, 'à direita: <code class="cpp">10 − (3 − 2)</code>');
    const b = foldColuna({ ...st, dir: 'e' }, 99, 'à esquerda: <code class="cpp">(10 − 3) − 2</code>');
    raiz._palco.replaceChildren(foldPack(p), el('div', { class: 'fold-dupla' }, a.col, b.col));
    raiz._palco.className = 'fold-palco' + (animar ? ' anima' : '');
  }
  function virgula(raiz, animar) {
    raiz._ctrl.classList.add('oculto');
    const exec = ['print(1)', 'print("dois")', 'print(3.0)'];
    const saidas = ['[1] ', '[dois] ', '[3] '];
    raiz._palco.replaceChildren(
      el('div', { class: 'fold-pack' }, el('span', { class: 'fold-rot', text: 'args =' }), el('span', { class: 'fold-chaves', text: '{' }),
        ...['1', '"dois"', '3.0'].map((t, i) => el('span', { class: 'fold-chip fx-' + (i === 1 ? 'ptr' : 'num'), style: { '--i': i }, text: t })),
        el('span', { class: 'fold-chaves', text: '}' }),
        el('span', { class: 'fold-n', html: '<code class="cpp">sizeof...(args)</code> = 3' })),
      el('div', { class: 'fold-col' },
        el('div', { class: 'fold-tmpl' }, el('span', { class: 'fold-rot', text: 'fold sobre a vírgula' }), cod('((fmt::print("[{}] ", args)), ...);')),
        el('div', { class: 'fold-linhas' },
          el('div', { class: 'fold-linha expande', style: { '--i': 0 }, html: `<span class="fold-pre">⇒</span><span class="fold-expr">${realcar('print(1), (print("dois"), print(3.0))')}</span>` }),
          el('div', { class: 'fold-ordem', style: { '--i': 1 } }, ...exec.map((t, i) => el('span', { class: 'fold-passo', style: { '--j': i } }, el('b', { text: String(i + 1) }), cod(t), el('span', { class: 'fold-sai', text: saidas[i].trim() }))))),
        el('p', { class: 'mini apagado fold-obs', html: '<code>print</code> = <code>fmt::print("[{}] ", …)</code> · a vírgula avalia da <b>esquerda para a direita</b> · saída real: última linha do terminal ↓' })));
    raiz._palco.className = 'fold-palco virgula' + (animar ? ' anima' : '');
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  9. O soquete — onde cada chamável cabe (sizeof medido no g++ 13.3)
  // ───────────────────────────────────────────────────────────────────────────
  const SOQ_PLUGS = [
    { id: 'som', tipo: 'função', cod: 'somar_int', estado: 'sem estado', tam: 'código', res: 13 },
    { id: 'sub', tipo: 'função', cod: 'subtrair_int', estado: 'sem estado', tam: 'código', res: 7 },
    { id: 'lam', tipo: 'lambda sem captura', cod: '[](int a, int b)\n{ return a * b; }', estado: 'sem estado', tam: 'sizeof 1', res: 30 },
    { id: 'cap', tipo: 'lambda COM captura', cod: '[base](int a, int b)\n{ return base + a + b; }', estado: 'base = 100', tam: 'sizeof 4', res: 113, captura: true },
  ];
  const SOQ_TOMADAS = [
    { id: 'ptr', tipo: 'int (*op)(int, int)', rot: 'ponteiro cru', tam: '8 bytes', desc: 'só o endereço do código', cham: 'op(10, 3)' },
    { id: 'fn', tipo: 'std::function<int(int, int)> f', rot: 'std::function', tam: '32 bytes', desc: 'guarda qualquer chamável', cham: 'f(10, 3)' },
  ];
  const SOQ_PASSOS = [
    { ptr: null, fn: null, msg: 'Escolha um chamável e depois uma tomada — ou avance (→).' },
    { ptr: 'som', fn: null, msg: 'Uma função vira ponteiro direto: <code>op = &amp;somar_int;</code> (o <code>&amp;</code> é opcional).' },
    { ptr: 'lam', fn: null, msg: 'Toda lambda é um objeto de uma <b>classe anônima</b> com <code>operator()</code>. <b>Sem captura</b>, ela se converte sozinha em ponteiro de função: não há estado para guardar.' },
    { ptr: 'lam', fn: null, recusa: 'cap', msg: '' },
    { ptr: 'lam', fn: 'cap', msg: '<code>std::function</code> guarda o objeto inteiro — inclusive <code>base = 100</code>. Ele apaga o tipo; o preço é o tamanho e uma indireção.' },
  ];
  const SOQ_ERRO = '<div class="gcc"><span class="e">error:</span> cannot convert ‘main()::&lt;lambda(int, int)&gt;’ to ‘int (*)(int, int)’ in initialization</div>';

  Deck.widget('funcoes/soquete', {
    iniciar(raiz, ctx) {
      raiz.classList.add('f-soq');
      raiz._ctx = ctx;
      raiz._sel = null;
      raiz._chips = {};
      raiz._casas = {};
      const plugs = el('div', { class: 'soq-plugs' });
      SOQ_PLUGS.forEach((p) => {
        const chip = el('div', { class: 'soq-chip' + (p.captura ? ' captura' : ''), dataset: { id: p.id } },
          el('code', { class: 'f-cod', html: realcar(p.cod) }),
          p.captura ? el('span', { class: 'soq-mochila', text: 'base = 100' }) : null);
        raiz._chips[p.id] = chip;
        const casa = el('div', { class: 'soq-casa' }, chip);
        raiz._casas[p.id] = casa;
        const card = el('button', { type: 'button', class: 'soq-plug', dataset: { id: p.id } },
          el('span', { class: 'soq-tipo', text: p.tipo }), casa,
          el('span', { class: 'soq-meta', html: `<span>${p.estado}</span><span>${p.tam}</span>` }));
        card.addEventListener('click', (e) => { e.stopPropagation(); card.blur(); raiz._sel = p.id; raiz.querySelectorAll('.soq-plug').forEach((x) => x.classList.toggle('sel', x.dataset.id === p.id)); });
        plugs.append(card);
      });
      raiz._tom = {};
      const tomadas = el('div', { class: 'soq-tomadas' });
      SOQ_TOMADAS.forEach((t) => {
        const slot = el('div', { class: 'soq-slot' }, el('span', { class: 'soq-vazio', text: 'vazio' }));
        const res = el('div', { class: 'soq-res' });
        const box = el('button', { type: 'button', class: 'soq-tomada ' + t.id, dataset: { id: t.id } },
          el('div', { class: 'soq-t-cab' }, el('span', { class: 'soq-t-rot', text: t.rot }), el('span', { class: 'pill', text: t.tam })),
          el('code', { class: 'f-cod soq-t-tipo', html: realcar(t.tipo) }),
          el('span', { class: 'soq-t-desc', text: t.desc }), slot, res);
        box._slot = slot; box._res = res; box._t = t;
        box.addEventListener('click', (e) => {
          e.stopPropagation(); box.blur();
          if (!raiz._sel) return;
          const st = { ...(raiz._est || SOQ_PASSOS[0]) };
          st.recusa = null;
          if (t.id === 'ptr' && raiz._sel === 'cap') { st.recusa = 'cap'; st.msg = ''; }
          else { if (st.ptr === raiz._sel) st.ptr = null; if (st.fn === raiz._sel) st.fn = null; st[t.id] = raiz._sel; st.msg = SOQ_PLUGS.find((p) => p.id === raiz._sel).captura ? SOQ_PASSOS[4].msg : 'Encaixou: ' + (t.id === 'ptr' ? 'o ponteiro guarda o endereço do código.' : 'std::function aceita qualquer chamável com essa assinatura.'); }
          soquete(raiz, st, true);
        });
        raiz._tom[t.id] = box;
        tomadas.append(box);
      });
      raiz._msg = el('div', { class: 'soq-msg' });
      raiz.append(plugs, tomadas, raiz._msg);
    },
    passo(raiz, k, ctx, dir) { soquete(raiz, SOQ_PASSOS[k] || SOQ_PASSOS[0], dir > 0 && k > 0); },
  });

  async function soquete(raiz, st, animar) {
    const ctx = raiz._ctx;
    const espera = ficha(raiz, ctx);
    animar = animar && !ctx.reduzido;
    raiz._est = st;
    const chips = Object.values(raiz._chips);
    flip(raiz, chips, () => {
      SOQ_PLUGS.forEach((p) => {
        const onde = st.ptr === p.id ? raiz._tom.ptr._slot : st.fn === p.id ? raiz._tom.fn._slot : raiz._casas[p.id];
        if (raiz._chips[p.id].parentElement !== onde) onde.append(raiz._chips[p.id]);
      });
    }, animar);
    SOQ_TOMADAS.forEach((t) => {
      const box = raiz._tom[t.id];
      const id = st[t.id];
      const p = SOQ_PLUGS.find((x) => x.id === id);
      box.classList.toggle('cheia', !!p);
      box.classList.toggle('recusou', t.id === 'ptr' && !!st.recusa);
      box._slot.querySelector('.soq-vazio').style.display = p ? 'none' : '';
      box._res.innerHTML = p ? `<code class="cpp">${realcar(t.cham)}</code> <span class="soq-seta">→</span> <b class="soq-num">${p.res}</b>` : '';
    });
    Object.entries(raiz._casas).forEach(([id, casa]) => casa.parentElement.classList.toggle('usado', st.ptr === id || st.fn === id));
    raiz.querySelectorAll('.soq-plug').forEach((x) => x.classList.toggle('recusado', x.dataset.id === st.recusa));
    raiz._msg.classList.toggle('erro', !!st.recusa);
    raiz._msg.innerHTML = st.recusa
      ? `<span class="soq-msg-ico">✗</span><div><p><b>Não cabe:</b> o ponteiro cru não tem onde guardar <code>base</code>.</p>${SOQ_ERRO}</div>`
      : `<span class="soq-msg-ico">→</span><div><p>${st.msg}</p></div>`;
    const chip = st.recusa ? raiz._chips[st.recusa] : null;
    if (chip) {
      chip.classList.remove('bate');
      if (animar) {
        // o chip vai até a tomada do PONTEIRO (onde quer que ela esteja) e é rejeitado lá
        const alvo = raiz._tom.ptr._slot.getBoundingClientRect();
        const c = chip.getBoundingClientRect();
        const esc = raiz.getBoundingClientRect().width / (raiz.offsetWidth || 1) || 1;
        chip.style.setProperty('--dx', Math.round((alvo.left + alvo.width / 2 - (c.left + c.width / 2)) / esc) + 'px');
        chip.style.setProperty('--dy', Math.round((alvo.top + alvo.height / 2 - (c.top + c.height / 2)) / esc) + 'px');
        void chip.offsetWidth;
        chip.classList.add('bate');
        if (!(await espera(900))) return;
        chip.classList.remove('bate');
      }
    }
  }

  // ───────────────────────────────────────────────────────────────────────────
  //  10. A árvore de fib_recursivo (contagens conferidas com um contador real)
  // ───────────────────────────────────────────────────────────────────────────
  const FIB_ESCALA = [[5, '15'], [10, '177'], [20, '21 891'], [30, '2 692 537'], [40, '331 160 281']];

  function fibArvore(n) {
    const nos = [];
    let folhas = 0;
    const vistos = new Set();
    function rec(v, prof, pai) {
      const no = { v, prof, ordem: nos.length, pai, filhos: [], repete: vistos.has(v) };
      vistos.add(v);
      nos.push(no);
      if (v >= 2) {
        no.filhos.push(rec(v - 1, prof + 1, no), rec(v - 2, prof + 1, no));
        no.x = (no.filhos[0].x + no.filhos[1].x) / 2;
      } else no.x = folhas++;
      return no;
    }
    rec(n, 0, null);
    return { nos, folhas, prof: Math.max(...nos.map((x) => x.prof)) + 1, distintos: vistos.size };
  }

  Deck.widget('funcoes/fib', {
    iniciar(raiz, ctx) {
      raiz.classList.add('f-fib');
      raiz._ctx = ctx;
      raiz._n = 5;
      raiz._ns = botoes([1, 2, 3, 4, 5, 6, 7].map((n) => ({ id: n, txt: String(n) })), (n) => { raiz._n = +n; fibDesenhar(raiz, Math.max(1, raiz._fase || 1), !raiz._ctx.reduzido); }, 'fib-ns');
      raiz._cont = el('div', { class: 'fib-cont' });
      raiz._svg = svg('svg', { class: 'diagrama fib-svg', viewBox: '0 0 820 400', width: 820, height: 400, role: 'img', 'aria-label': 'Árvore de chamadas de fib_recursivo' });
      raiz._tab = el('div', { class: 'fib-tab' });
      raiz.append(el('div', { class: 'fib-topo' }, el('span', { class: 'fib-rot', html: '<code class="cpp">fib_recursivo(n)</code>, n =' }), raiz._ns, raiz._cont),
        el('div', { class: 'fib-palco' }, raiz._svg, raiz._tab));
    },
    passo(raiz, k, ctx, dir) {
      raiz._n = 5;
      fibDesenhar(raiz, k, dir > 0 && k === 1 && !ctx.reduzido);
    },
  });

  function fibDesenhar(raiz, fase, crescer) {
    invalidar(raiz);
    raiz._fase = fase;
    const n = raiz._n;
    marcarBotao(raiz._ns, n);
    raiz.dataset.fase = String(fase);
    const A = fibArvore(n);
    const W = 820, H = 400, topo = 30;
    const passoX = Math.min(104, (W - 60) / Math.max(1, A.folhas));
    const R = Math.max(12, Math.min(19, passoX / 2 - 3));   // folhas não se tocam em n = 7
    const fonte = Math.round(Math.min(19, R * 1.05));
    const x0 = (W - passoX * (A.folhas - 1)) / 2;
    const passoY = A.prof > 1 ? Math.min(78, (H - topo - 30) / (A.prof - 1)) : 0;
    const X = (no) => x0 + no.x * passoX;
    const Y = (no) => topo + no.prof * passoY;
    const caminho = new Set();
    for (let no = A.nos[0]; no; no = no.filhos[0]) caminho.add(no);
    const arestas = svg('g', { class: 'fib-arestas' });
    const nos = svg('g', { class: 'fib-nos' });
    const visivel = fase >= 1;
    A.nos.forEach((no) => {
      const atraso = crescer ? `${no.ordem * Math.min(90, 2200 / A.nos.length)}ms` : '0ms';
      if (no.pai) {
        arestas.append(svg('line', { x1: X(no.pai), y1: Y(no.pai), x2: X(no), y2: Y(no),
          class: 'fib-aresta' + (visivel ? ' on' : '') + (fase === 3 && caminho.has(no) ? ' cam' : ''), style: `transition-delay:${atraso}` }));
      }
      const cls = ['fib-no', no.ordem === 0 || visivel ? 'on' : '', fase >= 2 && no.repete ? 'repete' : '', fase === 3 && caminho.has(no) ? 'cam' : '', no.v < 2 ? 'base' : ''].join(' ');
      nos.append(svg('g', { class: cls, transform: `translate(${X(no)} ${Y(no)})`, style: `transition-delay:${atraso}` },
        svg('circle', { r: R }), svg('text', { y: Math.round(fonte * 0.36), 'text-anchor': 'middle', style: `font-size:${fonte}px` }, String(no.v))));
    });
    raiz._svg.replaceChildren(arestas, nos);
    const chamadas = fase >= 1 ? A.nos.length : 1;
    const rep = A.nos.length - A.distintos;
    raiz._cont.innerHTML =
      `<span class="fib-c"><b>${chamadas}</b> chamada${chamadas === 1 ? '' : 's'}</span>` +
      `<span class="fib-c it"><b>${n}</b> volta${n === 1 ? '' : 's'} no iterativo</span>` +
      (fase >= 2 ? `<span class="fib-c rep"><b>${rep}</b> repetidas</span>` : '') +
      (fase >= 3 ? `<span class="fib-c cam"><b>${A.prof}</b> quadros na pilha</span>` : '');
    raiz._tab.innerHTML = fase >= 4
      ? `<table class="tabela compacta"><thead><tr><th>n</th><th>recursivo · chamadas</th><th>iterativo · voltas</th></tr></thead><tbody>${
        FIB_ESCALA.map(([m, c]) => `<tr${m === 20 ? ' class="realce"' : ''}><td>${m}</td><td class="fib-num">${c}</td><td>${m}</td></tr>`).join('')}</tbody></table>
         <p class="mini apagado">contado com um contador no programa (g++ 13.3) · chamadas = 2·fib(n+1) − 1</p>`
      : fase >= 2 ? `<div class="fib-leg"><span><i class="fib-bola"></i>primeira vez</span><span><i class="fib-bola rep"></i>recalculada</span>${fase >= 3 ? '<span><i class="fib-bola cam"></i>pilha mais funda</span>' : ''}</div>` : '';
  }
})();
