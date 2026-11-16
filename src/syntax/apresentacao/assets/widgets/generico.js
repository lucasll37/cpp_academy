/* =============================================================================
   widgets/generico.js — Cap. 8 · Genérico e erros (demo_generico.hpp)

   generico/gcc         colore diagnósticos reais do GCC dentro de um terminal
   generico/fabrica     o molde maior<T>, a dedução e as instâncias geradas
   generico/pilha       Pilha<T> LIFO e o método template convertida<double>()
   generico/ctad        CTAD: os argumentos do construtor/guia viram os <…>
   generico/nttp        NTTP: o N entra no tipo e no tamanho (BufferFixo<int, N>)
   generico/resolvedor  qual especialização casa + a composição recursiva do nome
   generico/formas      as quatro formas de restringir com concepts (a pílula migra)
   generico/unwinding   a pilha desmontando: destrutor ANTES do catch
   generico/catch       a hierarquia de exceções e "qual catch pega?"

   Todo passo(k) desenha o ESTADO k do zero (idempotente). Animações só quando
   o passo avança de um em um e o usuário não pediu movimento reduzido.
   Todos os textos de compilador e todos os valores vêm de execuções reais
   (GCC 13.3, -std=c++20) ou da saída real do ./dist/bin/syntax.
   ============================================================================= */
(function () {
  'use strict';
  const { el } = Deck.util;
  const realcar = (t) => Deck.util.realcar(t, 'cpp');
  const esc = (s) => Deck.util.escapar(s);

  // ── utilitários ────────────────────────────────────────────────────────────
  function escalaDe(raiz) {
    const r = raiz.getBoundingClientRect();
    return raiz.offsetWidth ? r.width / raiz.offsetWidth : 1;
  }
  function rel(raiz, no) {
    const s = escalaDe(raiz) || 1;
    const a = raiz.getBoundingClientRect();
    const b = no.getBoundingClientRect();
    return { x: (b.left - a.left) / s, y: (b.top - a.top) / s, w: b.width / s, h: b.height / s };
  }
  function limparVoos(raiz) { raiz.querySelectorAll('.g-voo').forEach((v) => v.remove()); }
  // um "fantasma" voa do centro de `de` ao centro de `para`; devolve false se cancelado
  async function voar(raiz, ctx, de, para, html, classe, ms = 650) {
    if (ctx.reduzido || !de || !para) return true;
    const a = rel(raiz, de), b = rel(raiz, para);
    const v = el('div', { class: 'g-voo' + (classe ? ' ' + classe : ''), html });
    v.style.left = (a.x + a.w / 2) + 'px';
    v.style.top = (a.y + a.h / 2) + 'px';
    v.style.transitionDuration = ms + 'ms';
    raiz.append(v);
    void v.offsetWidth;
    v.style.transform = `translate(-50%, -50%) translate(${(b.x + b.w / 2) - (a.x + a.w / 2)}px, ${(b.y + b.h / 2) - (a.y + a.h / 2)}px)`;
    const ok = await ctx.esperar(ms);
    v.remove();
    return ok;
  }
  // foco de linhas num bloco de código criado pelo widget
  function focar(fig, linhas) {
    const alvo = new Set(linhas || []);
    fig.classList.toggle('focando', alvo.size > 0);
    fig.querySelectorAll('.ln').forEach((ln, j) => ln.classList.toggle('foco', alvo.has(j + 1)));
  }
  function reanimar(no, classe) { no.classList.remove(classe); void no.offsetWidth; no.classList.add(classe); }
  // contexto para animar um CLIQUE: um clique novo (ou um passo do clicker, que
  // chama ctx._cancelar no motor) interrompe a animação do clique anterior
  function ctxDoClique(raiz, ctx) {
    const meu = raiz._clique = (raiz._clique || 0) + 1;
    return { get reduzido() { return ctx.reduzido; }, esperar: (ms) => ctx.esperar(ms).then((ok) => ok && raiz._clique === meu) };
  }
  // decide se o passo k merece animação (avançou exatamente um)
  function avancouUm(raiz, k, ctx, dir) {
    const ant = raiz._kAnt;
    raiz._kAnt = k;
    return !ctx.reduzido && dir > 0 && ant != null && k === ant + 1;
  }
  const codigoInline = (txt) => el('code', { class: 'g-cod', html: realcar(txt) });

  // ═══════════════════════════════════════════════════════════════════════════
  // generico/gcc — realça "error:", "warning:", "note:", a localização e o
  // trecho de código citado. Só decoração: o texto é o do compilador.
  // ═══════════════════════════════════════════════════════════════════════════
  Deck.widget('generico/gcc', {
    iniciar(raiz) {
      raiz.classList.add('g-gcc');
      raiz.querySelectorAll('.ln-saida').forEach((ln) => {
        let h = ln.innerHTML;
        const gut = /^(\s*\d*\s*\|)(.*)$/.exec(h);
        if (gut) {
          const resto = /^[\s~^]+$/.test(gut[2]) ? `<span class="g-caret">${gut[2]}</span>` : gut[2];
          ln.innerHTML = `<span class="g-gut">${gut[1]}</span>${resto}`;
          return;
        }
        h = h.replace(/^((?:\/[\w.+-]+)+\.\w+:\d+:\d+:|[\w.+-]+\.(?:cpp|hpp|o):(?:\d+:\d+:|\(\.text\+0x[0-9a-f]+\):)?|\/usr\/bin\/ld:|collect2:|cc1plus:)/,
          '<span class="g-loc">$1</span>');
        h = h.replace(/\b(error|warning|note)(:)/g, (m, w, c) => `<span class="g-${w[0]}">${w}${c}</span>`);
        h = h.replace(/(un\u2060?defined reference to)/g, '<span class="g-e">$1</span>');
        h = h.replace(/(\[-W[\w=+-]+\])/g, '<span class="g-flag">$1</span>');
        h = h.replace(/^(terminate called after throwing)/, '<span class="g-e">$1</span>');
        ln.innerHTML = h;
      });
    },
  });

  // ═══════════════════════════════════════════════════════════════════════════
  // generico/fabrica — o molde maior<T>, a dedução, as instâncias
  // ═══════════════════════════════════════════════════════════════════════════
  const FAB = [
    { cod: 'maior(3, 7)', args: [['3', 'T = int'], ['7', 'T = int']], T: 'int', res: '7',
      veredito: 'T = int', leg: '<b>maior(3, 7)</b>: os dois argumentos são <code>int</code> → T = int → o compilador escreve <b>maior&lt;int&gt;</b>.' },
    { cod: 'maior(2.5, 1.5)', args: [['2.5', 'T = double'], ['1.5', 'T = double']], T: 'double', res: '2.5',
      veredito: 'T = double', leg: 'Outro tipo, outra função: <b>maior&lt;double&gt;</b> nasce agora.' },
    { cod: 'maior<double>(3, 7.5)', explicito: true, args: [['3', 'int → 3.0'], ['7.5', 'double']], T: 'double', res: '7.5',
      veredito: 'T dado: double', leg: 'T dado à mão: nada a deduzir. O 3 é convertido para 3.0 e <b>maior&lt;double&gt; é reaproveitada</b>.' },
    { cod: 'maior(std::string{"abacaxi"},\n      std::string{"banana"})', args: [['"abacaxi"', 'T = std::string'], ['"banana"', 'T = std::string']], T: 'std::string', res: 'banana',
      veredito: 'T = std::string', leg: 'Para std::string, <code>&gt;</code> compara char a char (ordem lexicográfica): <b>"banana"</b> vence "abacaxi".' },
    { cod: 'maior(3, 7.5)', conflito: true, args: [['3', 'T = int'], ['7.5', 'T = double']], T: null, res: '✗',
      veredito: 'int ≠ double', leg: 'O 3 diz T = int, o 7.5 diz T = double, e a dedução não faz int → double: <b class="erro-txt">não compila</b>.' },
  ];
  const FAB_INST = [
    { T: 'int', nome: 'maior<int>', sig: '(int, int) → int' },
    { T: 'double', nome: 'maior<double>', sig: '(double, double) → double' },
    { T: 'std::string', nome: 'maior<std::string>', sig: '(std::string, std::string)' },
  ];
  const FAB_LEG0 = 'Um template não é código pronto: é um <b>molde</b>. Nada é gerado até alguém chamar. Clique numa chamada ou avance com →.';
  const FAB_MOLDE = 'template <typename T>\nT maior(T a, T b) { return a > b ? a : b; }';
  const gerado = (T) => `${T} maior<${T}>(\n    ${T} a, ${T} b)\n{ return a > b ? a : b; }`;

  function fabEstado(k) {
    // instâncias existentes depois das chamadas 1..k, e qual foi nova/reusada no passo k
    const vistos = [];
    let nova = null, reuso = null;
    for (let i = 0; i < k; i++) {
      const c = FAB[i];
      if (c.conflito) continue;
      if (!vistos.includes(c.T)) { vistos.push(c.T); if (i === k - 1) nova = c.T; } else if (i === k - 1) reuso = c.T;
    }
    return { vistos, nova, reuso };
  }

  function fabDesenhar(raiz, k, op = {}) {
    const atual = k > 0 ? FAB[k - 1] : null;
    const st = fabEstado(k);
    raiz.dataset.k = k;
    raiz._ch.forEach((b, i) => {
      b.classList.toggle('feita', i < k && !(op.pendente && i === k - 1));
      b.classList.toggle('atual', i === k - 1);
      b.classList.toggle('falhou', i < k && !!FAB[i].conflito);
    });
    // molde
    raiz._molde.classList.toggle('conflito', !!(atual && atual.conflito));
    raiz._molde.classList.toggle('ativo', !!atual);
    const temT = !!(atual && atual.T && !op.semT);
    raiz._slots.forEach((s) => s.classList.toggle('cheio', temT));
    raiz._tSelo.textContent = temT ? 'T = ' + atual.T : atual && atual.conflito && !op.semVeredito ? 'T = ?' : 'T = …';
    raiz._tSelo.classList.toggle('cheio', temT);
    raiz._tSelo.classList.toggle('ruim', !!(atual && atual.conflito && !op.semVeredito));
    // dedução: as propostas de cada argumento (esquerda) e o veredito (direita)
    const ded = raiz._ded;
    if (!atual) ded.replaceChildren(el('p', { class: 'f-vazio' }, 'cada argumento propõe um T'));
    else {
      const nArgs = op.args != null ? op.args : atual.args.length;
      const args = el('div', { class: 'f-args' }, ...atual.args.map(([v, t], j) => el('div',
        { class: 'f-linha' + (j >= nArgs ? ' espera' : '') + (atual.conflito ? ' ruim' : '') },
        el('code', { class: 'f-arg', text: v }), el('span', { class: 'f-seta' }, '→'), el('span', { class: 'f-tipo', text: t }))));
      const ver = op.semVeredito ? el('div', { class: 'f-veredito vazio' })
        : el('div', { class: 'f-veredito ' + (atual.conflito ? 'ruim' : 'bom') },
          atual.conflito ? '✗ ' + atual.veredito + ': conflito' : '✓ ' + atual.veredito);
      ded.replaceChildren(args, ver);
    }
    // as setas do fluxo: dedução → molde → código gerado
    const falhou = !!(atual && atual.conflito && !op.semVeredito);
    raiz._fluxo1.classList.toggle('on', temT);
    raiz._fluxo1.classList.toggle('ruim', falhou);
    raiz._fluxo1.lastChild.textContent = falhou ? 'nenhum T serve' : temT ? 'T = ' + atual.T : 'deduz T';
    const temCodigo = temT && !op.semCodigo;
    raiz._fluxo2.classList.toggle('on', temCodigo);
    raiz._fluxo2.classList.toggle('apagado', falhou);
    // código gerado
    const ger = raiz._ger;
    if (temCodigo) {
      ger.classList.remove('vazio');
      ger._cod.innerHTML = realcar(gerado(atual.T));
      ger._rot.textContent = st.reuso ? 'já existia — reaproveitada' : 'escrito pelo compilador';
    } else {
      ger.classList.add('vazio');
      ger._cod.innerHTML = atual && atual.conflito
        ? '<span class="f-erro">// ✗ nenhuma instância:\n//   a dedução falhou</span>'
        : temT ? '<span class="tk-com">// …</span>'
          : '<span class="tk-com">// nenhuma função\n// gerada ainda</span>';
      ger._rot.textContent = atual && atual.conflito ? 'nada é gerado' : 'código gerado';
    }
    // instâncias
    const visiveis = op.instancias || st.vistos;
    raiz._inst.forEach((c) => {
      const on = visiveis.includes(c._T);
      c.classList.toggle('oculta', !on);
      c.classList.toggle('nova', on && st.nova === c._T && !op.semMarcas);
      c.classList.toggle('reuso', on && st.reuso === c._T && !op.semMarcas);
    });
    raiz._nada.classList.toggle('oculta', visiveis.length > 0);
    raiz._cont.textContent = String(visiveis.length);
    raiz._leg.innerHTML = atual ? atual.leg : FAB_LEG0;
  }

  // a sequência, em batidas: cada argumento propõe um T → veredito → T preenche
  // o molde → o compilador escreve a função → ela voa para o .o
  async function fabAnimar(raiz, k, ctx) {
    const atual = FAB[k - 1];
    const base = { instancias: fabEstado(k - 1).vistos, semMarcas: true, pendente: true };
    fabDesenhar(raiz, k, { ...base, args: 0, semVeredito: true, semT: true });
    for (let j = 1; j <= atual.args.length; j++) {
      if (!(await ctx.esperar(420))) return;
      fabDesenhar(raiz, k, { ...base, args: j, semVeredito: true, semT: true });
    }
    if (!(await ctx.esperar(450))) return;
    if (atual.conflito) {
      fabDesenhar(raiz, k);
      reanimar(raiz._molde, 'treme');
      return;
    }
    fabDesenhar(raiz, k, { ...base, semT: true });
    if (!(await ctx.esperar(450))) return;
    fabDesenhar(raiz, k, { ...base, semCodigo: true });
    raiz._slots.forEach((s) => reanimar(s, 'pisca'));
    reanimar(raiz._tSelo, 'pisca');
    if (!(await ctx.esperar(600))) return;
    fabDesenhar(raiz, k, base);
    reanimar(raiz._ger, 'escreve');
    if (!(await ctx.esperar(500))) return;
    const st = fabEstado(k);
    const alvo = raiz._inst.find((c) => c._T === atual.T);
    if (st.nova) {
      alvo.classList.remove('oculta');
      alvo.classList.add('chegando');
      const ok = await voar(raiz, ctx, raiz._ger, alvo, esc('maior<' + atual.T + '>'), 'f-voo');
      alvo.classList.remove('chegando');
      if (!ok) return;
    } else {
      const ok = await voar(raiz, ctx, raiz._ger, alvo, esc('maior<' + atual.T + '>'), 'f-voo');
      if (!ok) return;
    }
    fabDesenhar(raiz, k);
  }

  Deck.widget('generico/fabrica', {
    iniciar(raiz, ctx) {
      raiz.classList.add('w-fabrica');
      // coluna 1: as chamadas
      const col1 = el('div', { class: 'f-col f-chamadas' }, el('span', { class: 'g-rot' }, 'chamadas no código'));
      raiz._ch = FAB.map((c, i) => {
        const b = el('button', { type: 'button', class: 'f-chamada', title: 'executar esta chamada' },
          el('span', { class: 'f-num' }, String(i + 1)),
          el('code', { class: 'f-cod', html: realcar(c.cod) }),
          el('span', { class: 'f-res' }, c.conflito ? '✗' : '= ' + c.res));
        b.addEventListener('click', () => { limparVoos(raiz); fabAnimar(raiz, i + 1, ctxDoClique(raiz, ctx)); });
        col1.append(b);
        return b;
      });
      // coluna 2, de cima para baixo: dedução → molde → código gerado
      const tpl = el('pre', { class: 'f-tpl', html: realcar(FAB_MOLDE).replace(/<span class="tk-tipo">T<\/span>/g, '<span class="tk-tipo f-slot">T</span>') });
      raiz._tSelo = el('span', { class: 'f-t-selo' }, 'T = …');
      raiz._molde = el('div', { class: 'f-molde' }, el('div', { class: 'f-molde-cab' }, el('span', { class: 'g-rot' }, 'o molde'), raiz._tSelo), tpl);
      raiz._slots = Array.from(tpl.querySelectorAll('.f-slot'));
      raiz._ded = el('div', { class: 'f-ded' });
      const ger = el('div', { class: 'f-ger vazio' });
      ger._rot = el('span', { class: 'g-rot' }, 'código gerado');
      ger._cod = el('pre', { class: 'f-ger-cod' });
      ger.append(ger._rot, ger._cod);
      raiz._ger = ger;
      const fluxo = (txt) => el('div', { class: 'f-fluxo' }, el('span', { class: 'f-fluxo-seta' }, '↓'), el('span', null, txt));
      raiz._fluxo1 = fluxo('deduz T');
      raiz._fluxo2 = fluxo('instancia');
      const col2 = el('div', { class: 'f-col f-centro' },
        el('div', { class: 'f-ded-caixa' }, el('span', { class: 'g-rot' }, 'dedução'), raiz._ded),
        raiz._fluxo1, raiz._molde, raiz._fluxo2, ger);
      // coluna 3: as instâncias no .o
      raiz._cont = el('b', null, '0');
      raiz._nada = el('p', { class: 'f-nada' }, 'vazio: ninguém chamou');
      raiz._inst = FAB_INST.map((d) => {
        const c = el('div', { class: 'f-inst oculta' },
          el('code', { class: 'f-inst-nome', html: realcar(d.nome) }),
          el('span', { class: 'f-inst-sig' }, d.sig),
          el('span', { class: 'f-selo-nova' }, 'nova'),
          el('span', { class: 'f-selo-reuso' }, 'reaproveitada'));
        c._T = d.T;
        return c;
      });
      const col3 = el('div', { class: 'f-col f-insts' },
        el('span', { class: 'g-rot' }, 'instâncias no .o · ', raiz._cont), raiz._nada, ...raiz._inst);
      raiz._leg = el('p', { class: 'g-legenda' });
      raiz.append(el('div', { class: 'f-grade' }, col1, col2, col3), raiz._leg);
    },
    passo(raiz, k, ctx, dir) {
      limparVoos(raiz);
      raiz._clique = (raiz._clique || 0) + 1;
      const animar = avancouUm(raiz, k, ctx, dir);
      if (animar && k > 0) fabAnimar(raiz, k, ctx); else fabDesenhar(raiz, k);
    },
    sair(raiz) { limparVoos(raiz); },
  });

  // ═══════════════════════════════════════════════════════════════════════════
  // generico/pilha — Pilha<int> LIFO + convertida<double>()
  // ═══════════════════════════════════════════════════════════════════════════
  const PILHA_SRC = [
    'template <typename T>',
    'class Pilha {',
    'public:',
    '    void empilhar(T v)  { itens_.push_back(std::move(v)); }',
    '    T    desempilhar()  { T v = std::move(itens_.back());',
    '                          itens_.pop_back(); return v; }',
    '    template <typename U>',
    '    Pilha<U> convertida() const {',
    '        Pilha<U> out;',
    '        for (const auto& x : itens_)',
    '            out.empilhar(static_cast<U>(x));',
    '        return out;',
    '    }',
    'private:',
    '    std::vector<T> itens_;',
    '};',
  ].join('\n');
  const PILHA = [
    { cham: 'Pilha<int> p;', nota: 'o tipo é obrigatório: sem argumentos, nada a deduzir', foco: [1, 2, 15], p: [], pd: null, ret: [] },
    { cham: 'p.empilhar(1);', nota: 'tamanho() = 1', foco: [4], p: [1], pd: null, ret: [] },
    { cham: 'p.empilhar(2);', nota: 'tamanho() = 2', foco: [4], p: [1, 2], pd: null, ret: [] },
    { cham: 'p.empilhar(3);', nota: 'tamanho() = 3', foco: [4], p: [1, 2, 3], pd: null, ret: [] },
    { cham: 'auto pd = p.convertida<double>();', nota: 'Pilha<double>: OUTRO tipo, tamanho 3', foco: [7, 8, 9, 10, 11, 12, 13], p: [1, 2, 3], pd: [1, 2, 3], ret: [] },
    { cham: 'const int topo = p.desempilhar();', nota: '→ 3 (o último que entrou)', foco: [5, 6], p: [1, 2], pd: [1, 2, 3], ret: [['topo', 3]] },
    { cham: 'const int seguinte = p.desempilhar();', nota: '→ 2 · "3 depois 2 (LIFO)"', foco: [5, 6], p: [1], pd: [1, 2, 3], ret: [['topo', 3], ['seguinte', 2]] },
  ];

  function pilhaTubo(raiz, tubo, itens, dbl, anim) {
    const caixa = tubo._itens;
    caixa.replaceChildren();
    (itens || []).forEach((v, i) => {
      const it = el('div', { class: 'p-item' + (dbl ? ' dbl' : '') },
        el('b', null, dbl ? v.toFixed(1) : String(v)),
        el('span', null, dbl ? 'double · 8 B' : 'int · 4 B'));
      it.style.setProperty('--i', i);
      if (anim === 'cai' && i === itens.length - 1) it.classList.add('cai');
      if (anim === 'copia') it.classList.add('copia');
      caixa.append(it);
    });
    tubo._topo.style.setProperty('--n', (itens || []).length);
    tubo._topo.classList.toggle('oculta', !(itens && itens.length));
    tubo._tam.textContent = `tamanho() = ${(itens || []).length}`;
  }

  Deck.widget('generico/pilha', {
    iniciar(raiz) {
      raiz.classList.add('w-pilha');
      raiz._fig = Deck.util.codigo(PILHA_SRC, { titulo: 'Pilha<T> — demo_generico.hpp (condensado)', classe: 'minimo' });
      raiz._cham = el('div', { class: 'p-cham' });
      const esquerda = el('div', { class: 'p-esq' }, raiz._fig, raiz._cham);
      const tubo = (rot, cls) => {
        const t = el('div', { class: 'p-tubo ' + cls });
        t._itens = el('div', { class: 'p-itens' });
        t._topo = el('span', { class: 'p-topo' }, 'topo');
        t._tam = el('span', { class: 'p-tam' });
        t.append(el('div', { class: 'p-vidro' }, t._itens, t._topo), el('code', { class: 'p-rotulo', html: realcar(rot) }), t._tam);
        return t;
      };
      raiz._p = tubo('Pilha<int> p', 'p-int');
      raiz._pd = tubo('Pilha<double> pd', 'p-dbl');
      raiz._ret = el('div', { class: 'p-ret' });
      raiz._nota = el('p', { class: 'p-nota' });
      raiz._palco = el('div', { class: 'p-palco' }, raiz._nota, el('div', { class: 'p-tubos' }, raiz._p, raiz._pd), raiz._ret);
      raiz.append(esquerda, raiz._palco);
    },
    passo(raiz, k, ctx, dir) {
      raiz.querySelectorAll('.p-fantasma').forEach((f) => f.remove());
      const animar = avancouUm(raiz, k, ctx, dir);
      const e = PILHA[k];
      const ant = PILHA[Math.max(0, k - 1)];
      focar(raiz._fig, e.foco);
      raiz._cham.replaceChildren(el('span', { class: 'g-rot' }, k === 0 ? 'declaração' : `passo ${k} de 6`), codigoInline(e.cham));
      raiz._nota.textContent = e.nota;
      if (animar) reanimar(raiz._nota, 'muda');
      const empurrou = animar && e.p.length > ant.p.length;
      const tirou = animar && e.p.length < ant.p.length;
      pilhaTubo(raiz, raiz._p, e.p, false, empurrou ? 'cai' : null);
      pilhaTubo(raiz, raiz._pd, e.pd, true, animar && k === 4 ? 'copia' : null);
      raiz._pd.classList.toggle('oculta', !e.pd);
      if (animar && k === 4) reanimar(raiz._pd, 'surge');
      if (tirou) {
        const v = ant.p[ant.p.length - 1];
        const f = el('div', { class: 'p-item p-fantasma' }, el('b', null, String(v)), el('span', null, 'int · 4 B'));
        f.style.setProperty('--i', ant.p.length - 1);
        raiz._p._itens.append(f);
      }
      const filhos = [el('span', { class: 'g-rot' }, 'valores devolvidos'),
        ...(e.ret.length ? e.ret.map(([n, v], j) => el('div', { class: 'p-ret-item' + (animar && j === e.ret.length - 1 ? ' novo' : '') },
          el('code', { html: realcar(`const int ${n}`) }), el('b', null, String(v))))
          : [el('p', { class: 'p-ret-vazio' }, 'nada ainda')])];
      if (e.ret.length === 2) filhos.push(el('p', { class: 'p-lifo' }, '3 depois 2: LIFO'));
      raiz._ret.replaceChildren(...filhos);
    },
  });

  // ═══════════════════════════════════════════════════════════════════════════
  // generico/ctad — os argumentos viram os <…>
  // ═══════════════════════════════════════════════════════════════════════════
  const CTAD = [
    { decl: 'std::pair p{1, 2.0};', args: [['1', 'int'], ['2.0', 'double']],
      viaRot: 'deduction guide da biblioteca', via: 'template <class T1, class T2>\npair(T1, T2) -> pair<T1, T2>;',
      res: ['std::pair<', 0, ', ', 1, '>'], nota: 'Antes do C++17: <code>std::pair&lt;int, double&gt; p{1, 2.0};</code> ou <code>std::make_pair(1, 2.0)</code>.' },
    { decl: 'std::vector v{1, 2, 3};', args: [['{1, 2, 3}', 'std::initializer_list<int>']], slots: ['int'],
      viaRot: 'construtor (simplificado)', via: 'vector(std::initializer_list<T>);',
      res: ['std::vector<', 0, '>'], nota: 'O construtor de lista revela o T: <code>T = int</code>.' },
    { decl: 'std::lock_guard lk{mtx};', args: [['mtx', 'std::mutex']],
      viaRot: 'construtor (simplificado)', via: 'explicit lock_guard(Mutex& m);',
      res: ['std::lock_guard<', 0, '>'], nota: 'A trava sem repetir o tipo do mutex.' },
    { decl: 'Dupla d{42, "resposta"s};', args: [['42', 'int'], ['"resposta"s', 'std::string']],
      viaRot: 'o nosso deduction guide', via: 'template <typename A, typename B>\nDupla(A, B) -> Dupla<A, B>;',
      res: ['Dupla<', 0, ', ', 1, '>'], nota: 'Saída real: <code>Dupla d{42, "resposta"s};  → (42, resposta)</code>' },
    { decl: 'Dupla d2{42, "resposta"};', aviso: true, args: [['42', 'int'], ['"resposta"', 'const char[9] → const char*']], slots: ['int', 'const char*'],
      viaRot: 'o nosso deduction guide', via: 'template <typename A, typename B>\nDupla(A, B) -> Dupla<A, B>;',
      res: ['Dupla<', 0, ', ', 1, '>'], nota: '⚠ Sem o sufixo <code>s</code>, o literal é um array de char e <b>decai para ponteiro</b>: não é std::string.' },
    { decl: 'std::vector v2{v};', aviso: true, args: [['v', 'std::vector<int>']], slots: ['int'],
      viaRot: 'candidato de cópia (tem preferência; simplificado)', via: 'template <class T>\nvector(vector<T>) -> vector<T>;',
      res: ['std::vector<', 0, '>'], nota: '⚠ É uma <b>cópia</b> de v: <code>std::vector&lt;int&gt;</code>, não <code>std::vector&lt;std::vector&lt;int&gt;&gt;</code>.' },
  ];
  const CTAD_LEG0 = 'Escreva o nome do template de classe <b>sem</b> os &lt;…&gt;: o compilador deduz os argumentos a partir do que você passou ao construtor.';

  function ctadDesenhar(raiz, k, op = {}) {
    const c = k > 0 ? CTAD[k - 1] : null;
    raiz._itens.forEach((b, i) => { b.classList.toggle('atual', i === k - 1); b.classList.toggle('feita', i < k - 1); });
    const q = raiz._q;
    q.classList.toggle('vazio', !c);
    q.classList.toggle('aviso', !!(c && c.aviso));
    if (!c) {
      q._decl.innerHTML = realcar('std::pair p{1, 2.0};   // sem <int, double>');
      q._args.replaceChildren(el('p', { class: 'c-dica' }, 'escolha uma declaração'));
      q._via.replaceChildren();
      q._res.replaceChildren();
      q._nota.innerHTML = CTAD_LEG0;
      return;
    }
    q._decl.innerHTML = realcar(c.decl);
    q._args.replaceChildren(...c.args.map(([v, t], j) => el('div', { class: 'c-arg' + (op.tipos === false ? ' sem-tipo' : '') },
      el('code', { class: 'c-valor', text: v }), el('span', { class: 'c-desce' }, '↓'),
      el('span', { class: 'c-tipo', dataset: { j } }, t))));
    q._via.replaceChildren(el('span', { class: 'g-rot' }, 'via ' + c.viaRot), el('pre', { class: 'c-via-cod', html: realcar(c.via) }));
    const slots = c.slots || c.args.map((a) => a[1]);
    q._res.replaceChildren(el('span', { class: 'c-igual' }, '⇒'), el('code', { class: 'c-tipo-res' + (op.res === false ? ' pendente' : '') },
      ...c.res.map((p) => (typeof p === 'number'
        ? el('span', { class: 'c-slot' + (op.res === false ? ' vazio' : ''), dataset: { j: p } }, op.res === false ? '?' : slots[p])
        : el('span', { html: realcar(p) })))));
    q._nota.innerHTML = c.nota;
  }

  async function ctadAnimar(raiz, k, ctx) {
    ctadDesenhar(raiz, k, { tipos: false, res: false });
    if (!(await ctx.esperar(250))) return;
    ctadDesenhar(raiz, k, { res: false });
    raiz._q._args.querySelectorAll('.c-arg').forEach((a) => a.classList.add('aparece'));
    if (!(await ctx.esperar(550))) return;
    const c = CTAD[k - 1];
    const slots = c.slots || c.args.map((a) => a[1]);
    const tipos = Array.from(raiz._q._args.querySelectorAll('.c-tipo'));
    const alvos = Array.from(raiz._q._res.querySelectorAll('.c-slot'));
    const voos = alvos.map((alvo, j) => voar(raiz, ctx, tipos[Math.min(j, tipos.length - 1)], alvo, esc(slots[j]), 'c-voo', 700));
    const oks = await Promise.all(voos);
    if (oks.some((x) => !x)) return;
    ctadDesenhar(raiz, k);
    raiz._q._res.querySelectorAll('.c-slot').forEach((s) => reanimar(s, 'pisca'));
  }

  Deck.widget('generico/ctad', {
    iniciar(raiz, ctx) {
      raiz.classList.add('w-ctad');
      const lista = el('div', { class: 'c-lista' }, el('span', { class: 'g-rot' }, 'declarações sem <…>'));
      raiz._itens = CTAD.map((c, i) => {
        const b = el('button', { type: 'button', class: 'c-item' + (c.aviso ? ' aviso' : '') },
          el('span', { class: 'c-num' }, c.aviso ? '⚠' : String(i + 1)), el('code', { class: 'g-cod', html: realcar(c.decl) }));
        b.addEventListener('click', () => { limparVoos(raiz); ctadAnimar(raiz, i + 1, ctxDoClique(raiz, ctx)); });
        lista.append(b);
        return b;
      });
      const q = el('div', { class: 'c-quadro' });
      q._decl = el('pre', { class: 'c-decl' });
      q._args = el('div', { class: 'c-args' });
      q._via = el('div', { class: 'c-via' });
      q._res = el('div', { class: 'c-res' });
      q._nota = el('p', { class: 'c-nota' });
      q.append(el('span', { class: 'g-rot' }, 'o que o compilador faz'), q._decl, q._args, q._via, q._res, q._nota);
      raiz._q = q;
      raiz.append(lista, q);
    },
    passo(raiz, k, ctx, dir) {
      limparVoos(raiz);
      raiz._clique = (raiz._clique || 0) + 1;
      if (avancouUm(raiz, k, ctx, dir) && k > 0) ctadAnimar(raiz, k, ctx); else ctadDesenhar(raiz, k);
    },
    sair(raiz) { limparVoos(raiz); },
  });

  // ═══════════════════════════════════════════════════════════════════════════
  // generico/nttp — BufferFixo<int, N>: o N está no tipo
  // ═══════════════════════════════════════════════════════════════════════════
  // mensagem real do GCC 13.3 para `b = c` (conferida com N = 1, 8, 12; com N = 16 compila)
  const nttpErro1 = (N) => `error: no match for ‘operator=’ (operand types are ‘BufferFixo<int, ${N}>’ and ‘BufferFixo<int, 16>’)`;
  const NTTP_OK1 = '✓ compila: com N = 16, b e c são do MESMO tipo';
  const NTTP_ERRO2 = 'error: the value of ‘n’ is not usable in a constant expression';

  function nttpBuffer(rotulo) {
    const b = el('div', { class: 'n-buf' });
    b._tipo = el('code', { class: 'g-cod n-tipo' });
    b._meta = el('span', { class: 'n-meta' });
    b._cels = el('div', { class: 'n-cels' });
    // o rótulo é o NOME da variável (b, c): sem a caixa-alta dos rótulos
    b.append(el('div', { class: 'n-cab' }, el('code', { class: 'n-var' }, rotulo), b._tipo, b._meta), b._cels);
    return b;
  }
  function nttpPintar(b, N, primeiro) {
    b._tipo.innerHTML = realcar(`BufferFixo<int, ${N}>`);
    b._meta.innerHTML = `sizeof = <b>${4 * N}</b> bytes · capacidade() = <b>${N}</b>`;
    const atual = b._cels.children.length;
    for (let i = atual; i < N; i++) b._cels.append(el('i', { class: 'nova' }, '0'));
    while (b._cels.children.length > N) b._cels.lastChild.remove();
    Array.from(b._cels.children).forEach((c, i) => { c.textContent = i === 0 && primeiro != null ? String(primeiro) : '0'; c.classList.toggle('usado', i === 0 && primeiro != null); });
  }
  function nttpComparar(raiz) {
    const N = raiz._N;
    const igual = N === 16;
    raiz._cmp.classList.toggle('igual', igual);
    raiz._cmp.innerHTML = `<code class="g-cod">${realcar(`std::is_same_v<BufferFixo<int, ${N}>, BufferFixo<int, 16>>`)}</code>`
      + (igual ? '<span>→ <b class="ok-txt">true</b> · mesmo N, mesmo tipo</span>' : '<span>→ <b class="erro-txt">false</b> · tipos DIFERENTES</span>');
    raiz._e1.classList.toggle('compila', igual);
    raiz._e1Msg.textContent = igual ? NTTP_OK1 : nttpErro1(N);
    nttpPintar(raiz._a, N, 5);
  }

  Deck.widget('generico/nttp', {
    iniciar(raiz) {
      raiz.classList.add('w-nttp');
      raiz._N = 8;
      raiz._range = el('input', { type: 'range', min: '1', max: '16', step: '1', value: '8', 'aria-label': 'valor de N' });
      raiz._valor = el('b', { class: 'n-valor' }, '8');
      raiz._range.addEventListener('input', () => {
        raiz._N = +raiz._range.value;
        raiz._valor.textContent = String(raiz._N);
        nttpComparar(raiz);
      });
      raiz._a = nttpBuffer('b');
      raiz._b = nttpBuffer('c');
      raiz._cmp = el('div', { class: 'n-cmp' });
      raiz._e1Msg = el('p', { class: 'g-msg-gcc' }, nttpErro1(8));
      raiz._e1 = el('div', { class: 'n-erro' },
        el('code', { class: 'g-cod', html: realcar('b = c;   // atribuir um ao outro') }), raiz._e1Msg);
      raiz._e2 = el('div', { class: 'n-erro' },
        el('code', { class: 'g-cod', html: realcar('std::size_t n = 8;  BufferFixo<int, n> r;') }),
        el('p', { class: 'g-msg-gcc' }, NTTP_ERRO2));
      raiz.append(
        el('div', { class: 'n-controle' }, el('span', { class: 'g-rot' }, 'arraste: N ='), raiz._range, raiz._valor),
        raiz._a, raiz._b, raiz._cmp, raiz._e1, raiz._e2);
    },
    passo(raiz, k) {
      raiz._N = 8;
      raiz._range.value = '8';
      raiz._valor.textContent = '8';
      nttpPintar(raiz._b, 16, null);
      nttpComparar(raiz);
      raiz._b.classList.toggle('oculta', k < 1);
      raiz._cmp.classList.toggle('oculta', k < 1);
      raiz._e1.classList.toggle('oculta', k < 2);
      raiz._e2.classList.toggle('oculta', k < 3);
    },
  });

  // ═══════════════════════════════════════════════════════════════════════════
  // generico/resolvedor — NomeDoTipo<X>: qual especialização, que nome
  // ═══════════════════════════════════════════════════════════════════════════
  const T_ = {
    int: { k: 'int' }, double: { k: 'double' }, char: { k: 'char' }, string: { k: 'string' },
    ptr: (de) => ({ k: 'ptr', de }), vec: (de) => ({ k: 'vec', de }), cst: (de) => ({ k: 'const', de }), cptr: (de) => ({ k: 'cptr', de }),
  };
  function nomeCpp(t) {
    switch (t.k) {
      case 'string': return 'std::string';
      case 'ptr': return nomeCpp(t.de) + '*';
      case 'vec': return `std::vector<${nomeCpp(t.de)}>`;
      case 'const': return 'const ' + nomeCpp(t.de);
      case 'cptr': return nomeCpp(t.de) + ' const';
      default: return t.k;
    }
  }
  // devolve a cadeia de níveis: [{tipo, cand, T, peca}]
  function resolver(t) {
    const niveis = [];
    for (let atual = t; atual;) {
      const n = { tipo: nomeCpp(atual) };
      if (atual.k === 'int') { n.cand = 'int'; n.peca = '"int"'; atual = null; }
      else if (atual.k === 'string') { n.cand = 'string'; n.peca = '"std::string"'; atual = null; }
      else if (atual.k === 'ptr') { n.cand = 'ptr'; n.T = nomeCpp(atual.de); n.peca = `"ponteiro para " + NomeDoTipo<${n.T}>::nome()`; atual = atual.de; }
      else if (atual.k === 'vec') { n.cand = 'vec'; n.T = nomeCpp(atual.de); n.peca = `"vector de " + NomeDoTipo<${n.T}>::nome()`; atual = atual.de; }
      else { n.cand = 'prim'; n.peca = '"desconhecido"'; atual = null; }
      niveis.push(n);
    }
    return niveis;
  }
  function nomeFinal(niveis) {
    return niveis.map((n) => ({ int: 'int', string: 'std::string', ptr: 'ponteiro para ', vec: 'vector de ', prim: 'desconhecido' })[n.cand]).join('');
  }
  const RES_DEMO = [T_.double, T_.int, T_.string, T_.ptr(T_.int), T_.vec(T_.int), T_.vec(T_.ptr(T_.int))];
  const RES_EXTRA = [T_.ptr(T_.ptr(T_.int)), T_.ptr(T_.cst(T_.int)), T_.cptr(T_.ptr(T_.int)), T_.vec(T_.vec(T_.int)), T_.char];
  const RES_CANDS = [
    { id: 'prim', tipo: 'primário', arg: '<T>', ret: '"desconhecido"' },
    { id: 'int', tipo: 'total', arg: '<int>', ret: '"int"' },
    { id: 'string', tipo: 'total', arg: '<std::string>', ret: '"std::string"' },
    { id: 'ptr', tipo: 'parcial', arg: '<T*>', ret: '"ponteiro para " + …' },
    { id: 'vec', tipo: 'parcial', arg: '<std::vector<T>>', ret: '"vector de " + …' },
  ];
  const CIRC = ['①', '②', '③', '④'];
  const NOTAS_EXTRA = {
    'const int*': '⚠ <code>const int</code> não é <code>int</code>: o nível 2 cai no primário.',
    'int* const': '⚠ Um ponteiro <b>const</b> não tem a forma <code>T*</code>: cai direto no primário.',
  };

  function resDesenhar(raiz, t, animar) {
    if (!t) {
      raiz._botoes.forEach((b) => b.classList.remove('atual'));
      raiz._cands.forEach((c) => { c.classList.remove('casou', 'acende'); c._selos.replaceChildren(); });
      raiz._cadeia.replaceChildren(el('p', { class: 'r-vazio' }, 'Para cada tipo, o compilador procura a especialização mais específica que casa. Avance com → ou clique num tipo.'));
      raiz._final.replaceChildren();
      raiz._nota.innerHTML = '';
      return;
    }
    const niveis = resolver(t);
    const nome = nomeCpp(t);
    raiz._botoes.forEach((b) => b.classList.toggle('atual', b._nome === nome));
    raiz._cands.forEach((c) => {
      const usos = niveis.map((n, i) => (n.cand === c._id ? i : -1)).filter((i) => i >= 0);
      c.classList.toggle('casou', usos.length > 0);
      c._selos.replaceChildren(...usos.map((i) => el('span', { class: 'r-selo' }, CIRC[i])));
      if (animar && usos.length) { c.style.setProperty('--d', usos[0] * 420 + 'ms'); reanimar(c, 'acende'); }
    });
    raiz._cadeia.replaceChildren(...niveis.map((n, i) => {
      const cand = RES_CANDS.find((c) => c.id === n.cand);
      const linha = el('div', { class: 'r-nivel' + (animar ? ' entra' : '') },
        el('span', { class: 'r-num' }, CIRC[i]),
        el('code', { class: 'g-cod', html: realcar(`NomeDoTipo<${n.tipo}>`) }),
        el('span', { class: 'r-casa' }, el('span', { class: 'r-tipo-cand ' + n.cand }, cand.tipo), ' ', el('code', { class: 'g-cod', html: realcar(cand.arg) }), n.T ? el('span', { class: 'r-t' }, ` T = ${n.T}`) : null),
        el('code', { class: 'g-cod r-peca', html: realcar(n.peca) }));
      linha.style.setProperty('--d', i * 420 + 'ms');
      return linha;
    }));
    const final = nomeFinal(niveis);
    raiz._final.replaceChildren(el('code', { class: 'g-cod', html: realcar(`NomeDoTipo<${nome}>::nome()`) }), el('span', { class: 'r-igual' }, '→'),
      el('b', { class: 'r-str' + (animar ? ' entra' : '') }, `"${final}"`));
    raiz._final.style.setProperty('--d', niveis.length * 420 + 'ms');
    raiz._nota.innerHTML = NOTAS_EXTRA[nome] || '';
  }

  Deck.widget('generico/resolvedor', {
    iniciar(raiz) {
      raiz.classList.add('w-resolv');
      const botao = (t, extra) => {
        const nome = nomeCpp(t);
        const b = el('button', { type: 'button', class: 'r-bt' + (extra ? ' extra' : '') }, el('code', { html: realcar(nome) }));
        b._nome = nome;
        b.addEventListener('click', () => resDesenhar(raiz, t, !Deck.util.reduzido()));
        return b;
      };
      raiz._botoes = [...RES_DEMO.map((t) => botao(t, false)), ...RES_EXTRA.map((t) => botao(t, true))];
      // duas linhas alinhadas: os seis casos do demo (os passos) e os extras (só clique)
      const linha = (rot, bts) => el('div', { class: 'r-linha' }, el('span', { class: 'g-rot r-linha-rot' }, rot), ...bts);
      raiz.append(el('div', { class: 'r-tipos' },
        linha('do demo', raiz._botoes.slice(0, RES_DEMO.length)),
        linha('extras', raiz._botoes.slice(RES_DEMO.length))));
      raiz._cands = RES_CANDS.map((c) => {
        const card = el('div', { class: 'r-cand ' + c.id },
          el('span', { class: 'r-cand-tipo' }, c.tipo),
          el('span', { class: 'r-cand-nome' }, 'NomeDoTipo'),
          el('code', { class: 'g-cod r-cand-arg', html: realcar(c.arg) }),
          el('code', { class: 'r-cand-ret', text: c.ret }));
        card._id = c.id;
        card._selos = el('span', { class: 'r-selos' });
        card.append(card._selos);
        return card;
      });
      raiz._cadeia = el('div', { class: 'r-cadeia' });
      raiz._final = el('div', { class: 'r-final' });
      raiz._nota = el('p', { class: 'r-nota' });
      raiz.append(el('div', { class: 'r-cands' }, ...raiz._cands), raiz._cadeia, el('div', { class: 'r-rodape' }, raiz._final, raiz._nota));
    },
    passo(raiz, k, ctx, dir) {
      const animar = avancouUm(raiz, k, ctx, dir);
      resDesenhar(raiz, k > 0 ? RES_DEMO[k - 1] : null, animar);
    },
  });

  // ═══════════════════════════════════════════════════════════════════════════
  // generico/formas — a mesma restrição em quatro lugares (a pílula migra)
  // ═══════════════════════════════════════════════════════════════════════════
  // cada forma: linhas; cada linha: pedaços (string = código; {c:'id'} = a pílula)
  const FORMAS = [
    { id: 'a', rot: '(a) no lugar de typename', linhas: [['template <', { c: 'c1' }, ' T>'], ['T soma_a(T a, T b) { return a + b; }']],
      txt: '<code>template &lt;Somavel T&gt;</code> lê-se "T, desde que Somavel". A forma mais curta para um parâmetro.',
      saida: ['soma_a(2, 3)', '5'] },
    { id: 'b', rot: '(b) cláusula requires', linhas: [['template <typename T> requires ', { c: 'c1' }, '<T>'], ['T soma_b(T a, T b) { return a + b; }']],
      txt: 'Depois da lista de parâmetros. Aceita combinações: <code>requires Somavel&lt;T&gt; &amp;&amp; std::copyable&lt;T&gt;</code>.',
      saida: ['soma_b(2.5, 1.5)', '4'] },
    { id: 'c', rot: '(c) requires no fim', linhas: [['template <typename T>'], ['T soma_c(T a, T b) requires ', { c: 'c1' }, '<T>'], ['{ return a + b; }']],
      txt: 'No fim da assinatura. É a forma que serve até para um método <b>não-template</b> de uma classe template.',
      saida: ['soma_c(10, 20)', '30'] },
    { id: 'd', rot: '(d) abreviada', linhas: [['auto soma_d(', { c: 'c1' }, ' auto a, ', { c: 'c2' }, ' auto b) { return a + b; }']],
      txt: 'Sintaxe abreviada (C++20): sem <code>template &lt;…&gt;</code> escrito. O compilador inventa os parâmetros.',
      saida: ['soma_d(1, 2)', '3'] },
  ];

  // realcar() normaliza a indentação e engole os espaços do começo de um pedaço
  // (" auto a, " virava "auto a,", colado na pílula): devolvemos esses espaços
  const realcarPedaco = (p) => (/^ */.exec(p)[0]) + realcar(p);
  function formasLinhas(f) {
    return f.linhas.map((pedacos) => el('div', { class: 'q-linha' }, ...pedacos.map((p) => (typeof p === 'string'
      ? el('span', { html: realcarPedaco(p) })
      : el('span', { class: 'q-chip', dataset: { id: p.c } }, 'Somavel')))));
  }

  function formasDesenhar(raiz, k, animar) {
    const f = FORMAS[Math.min(k, 3)];
    const antes = new Map();
    if (animar) raiz._cod.querySelectorAll('.q-chip').forEach((c) => antes.set(c.dataset.id, rel(raiz, c)));
    raiz._cod.replaceChildren(...formasLinhas(f));
    raiz._abas.forEach((a, i) => { a.classList.toggle('atual', i === Math.min(k, 4)); a.classList.toggle('feita', i < k); });
    raiz._txt.innerHTML = f.txt;
    raiz._saida.replaceChildren(el('span', { class: 'g-rot' }, 'saída real'), el('code', { class: 'g-cod', html: realcar(f.saida[0]) }),
      el('span', { class: 'q-igual' }, '='), el('b', null, f.saida[1]));
    raiz._cmp.classList.toggle('oculta', k < 4);
    raiz._saida.classList.toggle('oculta', k >= 4);
    raiz._txt.classList.toggle('oculta', k >= 4);
    if (animar) {
      raiz._cod.querySelectorAll('.q-linha').forEach((l, i) => { l.style.setProperty('--i', i); l.classList.add('entra'); });
      raiz._cod.querySelectorAll('.q-chip').forEach((c) => {
        const de = antes.get(c.dataset.id) || antes.get('c1');
        if (!de) return;
        const para = rel(raiz, c);
        c.style.transition = 'none';
        c.style.transform = `translate(${de.x - para.x}px, ${de.y - para.y}px)`;
        void c.offsetWidth;
        c.style.transition = '';
        c.style.transform = '';
        c.classList.add('voando');
      });
    }
  }

  Deck.widget('generico/formas', {
    iniciar(raiz) {
      raiz.classList.add('w-formas');
      raiz._abas = [...FORMAS.map((f) => f.rot), '(d) ≠ (a)'].map((r, i) => {
        const b = el('button', { type: 'button', class: 'q-aba' + (i === 4 ? ' pegadinha' : '') }, r);
        b.addEventListener('click', () => formasDesenhar(raiz, i, !Deck.util.reduzido()));
        return b;
      });
      raiz._cod = el('div', { class: 'q-cod' });
      raiz._txt = el('p', { class: 'q-txt' });
      raiz._saida = el('div', { class: 'q-saida' });
      raiz._cmp = el('div', { class: 'q-cmp oculta' },
        el('p', { class: 'q-txt' }, 'Cada ', el('code', null, 'Somavel auto'), ' é um parâmetro de template ', el('b', null, 'novo e independente'), ': (d) equivale a ',
          el('code', { class: 'g-cod', html: realcar('template <Somavel A, Somavel B>') }), '.'),
        el('div', { class: 'q-cmp-linhas' },
          el('div', { class: 'q-cmp-l ok' }, el('code', { class: 'g-cod', html: realcar('soma_d(1, 2.5)') }), el('span', null, '→'), el('b', null, '3.5'), el('span', { class: 'q-cmp-nota' }, 'A = int, B = double')),
          el('div', { class: 'q-cmp-l ruim' }, el('code', { class: 'g-cod', html: realcar('soma_a(1, 2.5)') }), el('span', null, '→'), el('b', null, '✗'), el('span', { class: 'q-cmp-nota' }, 'deduced conflicting types for parameter ‘T’ (‘int’ and ‘double’)'))));
      raiz.append(el('div', { class: 'q-abas' }, ...raiz._abas), el('div', { class: 'q-palco' }, raiz._cod), raiz._txt, raiz._saida, raiz._cmp);
    },
    passo(raiz, k, ctx, dir) {
      const animar = avancouUm(raiz, k, ctx, dir);
      formasDesenhar(raiz, k, animar && k <= 3);
    },
  });

  // ═══════════════════════════════════════════════════════════════════════════
  // generico/unwinding — main → run → demo_excecoes → lanca_apos_adquirir
  // ═══════════════════════════════════════════════════════════════════════════
  const UNW_SRC = [
    'void lanca_apos_adquirir() {',
    '    Sentinela s{"recurso"};        // ctor: "adquirido"',
    '    throw ErroDeDominio("algo deu errado depois de adquirir");',
    '}                                  // ~Sentinela(): "liberado"',
    '// em demo_excecoes():',
    'try {',
    '    lanca_apos_adquirir();',
    '} catch (const ErroDeDominio& e) {',
    '    fmt::print("capturado ErroDeDominio: …", e.what());',
    '}',
  ].join('\n');
  // linhas reais da saída (8.7), na ordem em que aparecem
  const UNW_SAIDA = [
    '          [recurso] adquirido',
    '          [recurso] liberado (destrutor rodou)',
    '        capturado ErroDeDominio: "domínio: algo deu errado depois de adquirir"',
    '        ↑ repare: o destrutor da Sentinela rodou ANTES do catch.',
    '          É o stack unwinding — a base do RAII.',
  ].join('\n');
  const UNW = [
    { foco: [6], linhas: 0, leg: '<code>demo_excecoes()</code> entra no <code>try</code>. A pilha tem três quadros.' },
    { foco: [7, 1], linhas: 0, leg: 'Chamada: o quadro de <code>lanca_apos_adquirir()</code> vai para o topo da pilha.' },
    { foco: [2], linhas: 1, leg: '<code>Sentinela s</code> é construída: o recurso foi adquirido.' },
    { foco: [3], linhas: 1, leg: '<code>throw</code>: a função para aqui. O objeto da exceção fica <b>fora da pilha</b>, e o runtime sobe procurando um <code>catch</code> compatível.' },
    { foco: [4], linhas: 2, leg: '<b>Unwinding</b>: antes de descartar o quadro, o destrutor de <code>s</code> roda e libera o recurso.' },
    { foco: [8, 9], linhas: 5, leg: 'O <code>catch</code> recebe <code>e</code> com o recurso <b>já liberado</b>: o quadro de <code>lanca_apos_adquirir()</code> não existe mais. É a base do RAII.' },
  ];

  function unwLimparFlip(raiz) {
    [raiz._qDemo, raiz._qRun, raiz._qMain, raiz._exVaga, raiz._leg].forEach((n) => { n.style.transition = ''; n.style.transform = ''; });
  }
  // o quadro desmontado desliza para fora; os de baixo e a legenda sobem suavemente (FLIP)
  async function unwDesempilhar(raiz, ctx) {
    const q = raiz._qLanca;
    q.classList.add('desmontando');
    void q.offsetWidth;
    q.classList.add('saindo');
    if (!(await ctx.esperar(420))) return;
    const vizinhos = [raiz._qDemo, raiz._qRun, raiz._qMain, raiz._exVaga, raiz._leg];
    const antes = vizinhos.map((n) => rel(raiz, n).y);
    q.classList.remove('saindo', 'desmontando');
    q.classList.add('oculta');
    vizinhos.forEach((n, i) => {
      const d = antes[i] - rel(raiz, n).y;
      if (!d) return;
      n.style.transition = 'none';
      n.style.transform = `translateY(${d}px)`;
      void n.offsetWidth;
      n.style.transition = 'transform .45s var(--ease)';
      n.style.transform = '';
    });
    if (!(await ctx.esperar(480))) return;
    vizinhos.forEach((n) => { n.style.transition = ''; });
  }

  function unwPosicionarExcecao(raiz, k) {
    const ex = raiz._ex;
    if (k < 3) { ex.classList.add('oculta'); raiz._exVaga.append(ex); return; }
    ex.classList.remove('oculta');
    const destino = k === 3 ? raiz._qLanca._vaga : raiz._qDemo._vaga;
    destino.append(ex);
    ex.classList.toggle('pego', k >= 5);
    ex.classList.toggle('subindo', k === 4);
    raiz._exRot.textContent = k === 3 ? 'objeto da exceção' : k === 4 ? 'a caminho do catch…' : 'pego pelo catch';
  }

  Deck.widget('generico/unwinding', {
    iniciar(raiz) {
      raiz.classList.add('w-unwind');
      const quadro = (rot, ...filhos) => {
        const q = el('div', { class: 'quadro u-quadro' });
        q._vaga = el('div', { class: 'u-vaga' });
        q.append(el('div', { class: 'u-q-corpo' }, el('span', { class: 'rotulo' }, 'quadro'), el('code', { class: 'u-fn', html: realcar(rot) }), ...filhos), q._vaga);
        return q;
      };
      raiz._try = el('code', { class: 'u-chip u-try', html: realcar('try { … }') });
      raiz._catchChip = el('code', { class: 'u-chip u-catch', html: realcar('catch (const ErroDeDominio& e)') });
      raiz._sent = el('code', { class: 'u-chip u-sent', html: realcar('Sentinela s') });
      raiz._sentEstado = el('span', { class: 'u-sent-estado' });
      raiz._qMain = quadro('main()');
      raiz._qRun = quadro('demo_generico::run()');
      raiz._qDemo = quadro('demo_excecoes()', el('div', { class: 'u-chips' }, raiz._try, raiz._catchChip));
      raiz._qLanca = quadro('lanca_apos_adquirir()', el('div', { class: 'u-chips' }, raiz._sent, raiz._sentEstado));
      raiz._exRot = el('span', { class: 'u-ex-rot' }, 'objeto da exceção');
      raiz._ex = el('div', { class: 'u-ex oculta' }, raiz._exRot, el('code', { class: 'g-cod', html: realcar('ErroDeDominio') }));
      raiz._exVaga = el('div', { class: 'u-ex-guarda' });
      const pilha = el('div', { class: 'pilha-quadros u-pilha' }, raiz._qMain, raiz._qRun, raiz._qDemo, raiz._qLanca);
      raiz._leg = el('p', { class: 'g-legenda u-leg' });
      raiz._fig = Deck.util.codigo(UNW_SRC, { titulo: 'demo_generico.hpp (trechos)', classe: 'minimo' });
      raiz._term = Deck.util.terminal(UNW_SAIDA, { titulo: 'saída real — 8.7', classe: 'compacto' });
      raiz._linhas = Array.from(raiz._term.querySelectorAll('.ln-saida'));
      raiz.append(el('div', { class: 'u-esq' }, el('span', { class: 'g-rot' }, 'pilha de chamadas (topo em cima)'), pilha, raiz._exVaga, raiz._leg),
        el('div', { class: 'u-dir' }, raiz._fig, raiz._term));
    },
    passo(raiz, k, ctx, dir) {
      const animar = avancouUm(raiz, k, ctx, dir);
      unwLimparFlip(raiz);
      const e = UNW[k];
      focar(raiz._fig, e.foco);
      raiz._linhas.forEach((ln, j) => ln.classList.toggle('escondida', j >= e.linhas));
      if (animar) raiz._linhas.slice(UNW[k - 1].linhas, e.linhas).forEach((ln, j) => { ln.style.transitionDelay = (k === 4 ? 350 : 500) + j * 160 + 'ms'; });
      else raiz._linhas.forEach((ln) => { ln.style.transitionDelay = ''; });
      raiz._qMain.classList.remove('ativo');
      raiz._qRun.classList.remove('ativo');
      raiz._qDemo.classList.toggle('ativo', k === 0 || k >= 5);
      raiz._qLanca.classList.toggle('ativo', k >= 1 && k <= 3);
      raiz._qLanca.classList.toggle('lancou', k === 3);
      // passo 4: o quadro continua VISÍVEL enquanto é desmontado (o destrutor roda nele);
      // passo 5: ele sai da pilha (animado só quando se avança de 4 para 5)
      raiz._qLanca.classList.toggle('desmontando', k === 4);
      raiz._qLanca.classList.remove('saindo');
      raiz._qLanca.classList.toggle('oculta', k < 1 || (k >= 5 && !(animar && k === 5)));
      if (animar && k === 5) unwDesempilhar(raiz, ctx);
      raiz._try.classList.toggle('aceso', k <= 4);
      raiz._catchChip.classList.toggle('aceso', k >= 5);
      raiz._sent.classList.toggle('oculta', k < 2);
      raiz._sent.classList.toggle('morto', k >= 4);
      raiz._sentEstado.textContent = k < 2 ? '' : k < 4 ? 'adquirido' : '~Sentinela() rodou';
      raiz._sentEstado.classList.toggle('morto', k >= 4);
      // a exceção: FLIP entre as vagas quando anima
      const antes = animar && !raiz._ex.classList.contains('oculta') ? rel(raiz, raiz._ex) : null;
      unwPosicionarExcecao(raiz, k);
      if (antes && k >= 4) {
        const depois = rel(raiz, raiz._ex);
        raiz._ex.style.transition = 'none';
        raiz._ex.style.transform = `translate(${antes.x - depois.x}px, ${antes.y - depois.y}px)`;
        void raiz._ex.offsetWidth;
        raiz._ex.style.transition = '';
        raiz._ex.style.transform = '';
      }
      if (animar && k === 3) reanimar(raiz._ex, 'nasce');
      raiz._leg.innerHTML = e.leg;
    },
    sair(raiz) { unwLimparFlip(raiz); raiz._qLanca.classList.remove('saindo'); },
  });

  // ═══════════════════════════════════════════════════════════════════════════
  // generico/catch — a hierarquia e a varredura dos catch
  // ═══════════════════════════════════════════════════════════════════════════
  const ARVORE = { id: 'exception', nome: 'std::exception', filhos: [
    { id: 'logic_error', nome: 'std::logic_error', filhos: [
      { id: 'invalid_argument', nome: 'std::invalid_argument' }, { id: 'domain_error', nome: 'std::domain_error' },
      { id: 'length_error', nome: 'std::length_error' }, { id: 'out_of_range', nome: 'std::out_of_range' }] },
    { id: 'runtime_error', nome: 'std::runtime_error', filhos: [
      { id: 'range_error', nome: 'std::range_error' }, { id: 'overflow_error', nome: 'std::overflow_error' },
      { id: 'underflow_error', nome: 'std::underflow_error' }, { id: 'system_error', nome: 'std::system_error' },
      { id: 'ErroDeDominio', nome: 'ErroDeDominio', nosso: true }] },
    { id: 'bad_alloc', nome: 'std::bad_alloc' },
    { id: 'bad_cast', nome: 'std::bad_cast' },
  ] };
  const PAI = {};
  (function indexar(n, pai) { PAI[n.id] = pai; (n.filhos || []).forEach((f) => indexar(f, n.id)); })(ARVORE, null);
  const ancestrais = (id) => { const out = []; for (let x = id; x; x = PAI[x]) out.push(x); return out; };
  const NOMES = {};
  (function nomes(n) { NOMES[n.id] = n.nome; (n.filhos || []).forEach(nomes); })(ARVORE);
  NOMES.int = 'int';
  // std::exception, std::bad_alloc e std::bad_cast não têm construtor que receba mensagem;
  // std::system_error precisa de um código de erro. Todas as linhas compiladas no GCC 13.3.
  const LANCAR = {
    int: 'throw 42;', ErroDeDominio: 'throw ErroDeDominio("…");', exception: 'throw std::exception();',
    bad_alloc: 'throw std::bad_alloc();', bad_cast: 'throw std::bad_cast();',
    system_error: 'throw std::system_error(EIO, std::generic_category());',
  };
  const lancarTxt = (id) => LANCAR[id] || `throw ${NOMES[id]}("…");`;
  const ORDEM_CERTA = ['invalid_argument', 'logic_error', 'runtime_error', 'exception', '...'];
  const ORDEM_ERRADA = ['exception', 'invalid_argument', 'logic_error', 'runtime_error', '...'];
  const CATCH_PASSOS = [
    { lanca: 'invalid_argument', errada: false }, { lanca: 'out_of_range', errada: false }, { lanca: 'ErroDeDominio', errada: false },
    { lanca: 'bad_alloc', errada: false }, { lanca: 'int', errada: false }, { lanca: 'invalid_argument', errada: true },
  ];
  const clausulaTxt = (c) => (c === '...' ? 'catch (...)' : `catch (const ${NOMES[c]}& e)`);
  const casa = (c, lancado) => c === '...' || ancestrais(lancado).includes(c);

  // (re)cria as linhas da lista de catch só quando a ordem muda; recém-criadas,
  // nascem neutras e o estilo é calculado antes de receberem o estado (assim a
  // mudança de estado também vira transição)
  function catchLinhas(raiz, ordem) {
    if (raiz._ordemAtual === ordem) return raiz._linhasK;
    raiz._ordemAtual = ordem;
    raiz._linhasK = ordem.map((c, i) => {
      const linha = el('div', { class: 'k-clausula k-nada' },
        el('span', { class: 'k-idx' }, String(i + 1)),
        el('code', { class: 'g-cod', html: realcar(clausulaTxt(c) + ' { … }') }));
      linha._estado = el('span', { class: 'k-estado' });
      linha.append(linha._estado);
      return linha;
    });
    raiz._lista.replaceChildren(...raiz._linhasK);
    void raiz._lista.offsetWidth;
    return raiz._linhasK;
  }

  function catchDesenhar(raiz, st, animar) {
    raiz._st = st;
    const caminho = st.lanca ? ancestrais(st.lanca) : [];
    raiz._nos.forEach((n) => {
      n.classList.toggle('lancado', n._id === st.lanca);
      n.classList.toggle('caminho', caminho.includes(n._id) && n._id !== st.lanca);
    });
    raiz._throw.innerHTML = st.lanca ? realcar(lancarTxt(st.lanca)) : '<span class="tk-com">// clique num tipo para lançá-lo</span>';
    raiz._throw.classList.toggle('longo', !!st.lanca && lancarTxt(st.lanca).length > 40);
    const ordem = st.errada ? ORDEM_ERRADA : ORDEM_CERTA;
    let achou = -1;
    const ests = ordem.map((c, i) => {
      if (!st.lanca) return 'nada';
      if (achou >= 0) return 'pulado';
      if (casa(c, st.lanca)) { achou = i; return 'pega'; }
      return 'nao';
    });
    // As linhas são persistentes (só são refeitas quando a ORDEM muda) para que as
    // mudanças de estado sejam TRANSIÇÕES: com animar, a linha i muda em i × 300 ms
    // (a varredura de cima para baixo) e as "nem é testado" apagam juntas depois da
    // que pegou. Sem animar (voltar, ?estatico), tudo muda na hora.
    const linhas = catchLinhas(raiz, ordem);
    raiz._lista.classList.toggle('instantaneo', !animar);
    linhas.forEach((linha, i) => {
      const est = ests[i];
      const d = !animar ? 0 : est === 'pulado' ? achou * 300 + 650 : i * 300 + 250;
      linha.style.setProperty('--d', d + 'ms');
      ['pega', 'nao', 'pulado', 'nada'].forEach((x) => linha.classList.toggle('k-' + x, x === est));
      linha._estado.textContent = { pega: '✓ pega', nao: '✗ não casa', pulado: 'nem é testado', nada: '' }[est];
      if (animar && st.lanca) reanimar(linha._estado, 'carimba'); else linha._estado.classList.remove('carimba');
      if (animar && est === 'nao') reanimar(linha, 'testa'); else linha.classList.remove('testa');
    });
    if (animar && st.lanca) reanimar(raiz._throw, 'pisca');
    raiz._ordemBt.textContent = st.errada ? 'ordem: geral primeiro ⚠' : 'ordem: específico → geral';
    raiz._ordemBt.classList.toggle('errada', !!st.errada);
    raiz._aviso.classList.toggle('oculta', !st.errada);
    const pegou = achou >= 0 ? ordem[achou] : null;
    raiz._defErro.classList.toggle('oculta', !(st.lanca === 'ErroDeDominio' && !st.errada));
    raiz._veredito.innerHTML = !st.lanca ? 'Os <code>catch</code> são testados de cima para baixo; o <b>primeiro</b> compatível ganha.'
      : st.lanca === 'int' ? 'Um <code>int</code> não deriva de nada: só <code>catch (...)</code> o pega (e sem dar nome a ele).'
        : st.errada && pegou === 'exception' ? 'O geral no topo <b>engole tudo</b>: os catch de baixo ficam inalcançáveis. O GCC avisa mesmo sem <code>-Wall</code>.'
          : pegou === st.lanca ? `<code>${esc(NOMES[st.lanca])}</code>: tipo exato, o catch ${achou + 1} pega.`
            : `<code>${esc(NOMES[st.lanca])}</code> é-um <code>${esc(NOMES[pegou] || '…')}</code>: o catch ${achou + 1} pega.`;
  }

  Deck.widget('generico/catch', {
    iniciar(raiz) {
      raiz.classList.add('w-catch');
      raiz._nos = [];
      const lancar = (id) => catchDesenhar(raiz, { lanca: id, errada: raiz._st ? raiz._st.errada : false }, !Deck.util.reduzido());
      const no = (n, nivel) => {
        const b = el('button', { type: 'button', class: 'k-no' + (n.nosso ? ' nosso' : ''), title: 'lançar ' + n.nome }, el('code', null, n.nome), n.nosso ? el('span', { class: 'k-tag' }, 'do demo') : null);
        b._id = n.id;
        b.addEventListener('click', () => lancar(n.id));
        raiz._nos.push(b);
        const li = el('li', { class: 'k-n' + nivel }, b);
        if (n.filhos) li.append(el('ul', null, ...n.filhos.map((f) => no(f, nivel + 1))));
        return li;
      };
      const arvore = el('ul', { class: 'k-arvore' }, no(ARVORE, 0));
      const intNo = el('button', { type: 'button', class: 'k-no k-int', title: 'lançar int' }, el('code', null, 'int'), el('span', { class: 'k-tag' }, 'fora da árvore'));
      intNo._id = 'int';
      intNo.addEventListener('click', () => lancar('int'));
      raiz._nos.push(intNo);
      raiz._throw = el('code', { class: 'g-cod k-throw' });
      raiz._lista = el('div', { class: 'k-lista' });
      raiz._ordemBt = el('button', { type: 'button', class: 'botao k-ordem' });
      raiz._ordemBt.addEventListener('click', () => catchDesenhar(raiz, { lanca: raiz._st && raiz._st.lanca, errada: !(raiz._st && raiz._st.errada) }, !Deck.util.reduzido()));
      raiz._aviso = el('div', { class: 'k-aviso oculta' },
        el('p', { class: 'g-msg-gcc', html: 'warning: exception of type ‘std::invalid_argument’ will be caught by earlier handler <span class="g-nowrap">[-Wexceptions]</span>' }),
        el('p', { class: 'g-msg-gcc nota' }, 'note: for type ‘std::exception’'));
      raiz._veredito = el('p', { class: 'g-legenda k-veredito' });
      raiz._defErro = Deck.util.codigo([
        'class ErroDeDominio : public std::runtime_error {',
        'public:',
        '    explicit ErroDeDominio(const std::string& msg)',
        '        : std::runtime_error("domínio: " + msg) {}',
        '};',
      ].join('\n'), { arquivo: 'demo_generico.hpp', inicio: 301, classe: 'k-def oculta' });
      raiz.append(
        el('div', { class: 'k-esq' }, el('span', { class: 'g-rot' }, 'clique para lançar'), arvore, intNo),
        el('div', { class: 'k-dir' },
          el('div', { class: 'k-topo' }, el('span', { class: 'g-rot' }, 'try {'), raiz._throw, raiz._ordemBt),
          raiz._lista, raiz._aviso, raiz._veredito, raiz._defErro));
    },
    passo(raiz, k, ctx, dir) {
      const animar = avancouUm(raiz, k, ctx, dir);
      catchDesenhar(raiz, k > 0 ? CATCH_PASSOS[k - 1] : { lanca: null, errada: false }, animar);
    },
  });
})();
