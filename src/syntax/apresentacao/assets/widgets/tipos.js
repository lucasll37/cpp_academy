/* =============================================================================
   widgets/tipos.js — Capítulo 4 · Tipos definidos pelo usuário (demo_tipos.hpp)

   Widgets (cada passo(k) redesenha o ESTADO k do zero — idempotente):
     tipos/acesso      struct ⇄ class: o acesso padrão dos membros e da herança
     tipos/agregado    o que tira um tipo da categoria "agregado" (regra do C++20)
     tipos/config      designated initializers: monte a linha; fora de ordem → erro
     tipos/escopo      enum clássico vaza nomes e colide; enum class os contém
     tipos/flags       enum class como flags: bits, operator| e contem()
     tipos/union       union na memória: little-endian, membro ativo, type punning
     tipos/variant     tagged union à mão × std::variant
     tipos/bitfield    32 bits com nome; codigo_erro = 20 perde o bit de cima
     tipos/incompleto  o que compila com `class Motor;`
     tipos/ciclo       headers que se incluem → forward declaration quebra o ciclo
     tipos/vptr        o ponteiro escondido de ComVirtual, o padding e a vtable

   Todas as mensagens de compilador são reais (GCC 13.3, -std=c++20) e todos os
   números foram medidos — ver o relatório do capítulo.
   ============================================================================= */
(function () {
  'use strict';
  const { el, svg } = Deck.util;

  // ── utilitários ──────────────────────────────────────────────────────────
  const realcarLinhas = (texto) => Deck.util.realcar(texto, 'cpp').split('\n');

  /** <figure class="codigo"> com uma linha por item.
      item = 'texto' | { t: 'texto', cls: 'ln-bom foco …', fim: Node } */
  function figuraCodigo(itens, op = {}) {
    const its = itens.map((i) => (typeof i === 'string' ? { t: i } : i));
    const html = realcarLinhas(its.map((i) => i.t).join('\n'));
    const fig = el('figure', { class: ['codigo', op.classe || ''].join(' ').trim() });
    if (op.titulo) {
      fig.append(el('figcaption', { class: 'codigo-topo' },
        el('span', { class: 'dots' }, el('i'), el('i'), el('i')),
        el('span', { class: 'arquivo' }, op.titulo),
        el('span', { class: 'lang' }, 'C++')));
    }
    const code = el('code');
    its.forEach((it, k) => {
      const ln = el('span', { class: 'ln' + (it.cls ? ' ' + it.cls : ''), html: html[k] || '' });
      if (it.fim) ln.append(it.fim);
      code.append(ln);
    });
    fig.append(el('pre', null, code));
    return fig;
  }

  /** define variáveis CSS num nó (Object.assign em style ignora propriedades '--x') */
  function vars(no, props) {
    for (const [k, v] of Object.entries(props)) no.style.setProperty(k, String(v));
    return no;
  }

  const msg = (tipo, ...filhos) => el('div', { class: 'tp-msg' + (tipo ? ' ' + tipo : '') }, ...filhos);
  const pressionar = (b, on) => b.setAttribute('aria-pressed', on ? 'true' : 'false');
  const limitar = (v, a, b) => Math.max(a, Math.min(b, v));
  const anima = (ctx, dir) => dir > 0 && !ctx.reduzido;

  /** reinicia uma animação CSS (para repetir num clique) */
  function reiniciar(no, classe) {
    no.classList.remove(classe);
    void no.offsetWidth;
    no.classList.add(classe);
  }

  /** ponta de seta (triângulo) em (x, y), alinhada à direção (cx, cy) → (x, y) */
  function ponta(x, y, cx, cy, cls, t = 13) {
    const a = Math.atan2(y - cy, x - cx);
    const p = (ang) => `${(x - t * Math.cos(a + ang)).toFixed(1)} ${(y - t * Math.sin(a + ang)).toFixed(1)}`;
    return svg('path', { d: `M${x} ${y} L${p(0.42)} L${p(-0.42)} Z`, class: cls });
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  tipos/acesso — struct ⇄ class
  // ═══════════════════════════════════════════════════════════════════════
  const AC_MSG = {
    xPriv: '‘int Ponto::x’ is private within this context',
    xInac: '‘int Ponto::x’ is inaccessible within this context',
    zPriv: '‘int Ponto3D::z’ is private within this context',
  };
  const AC_ESTADOS = [
    { b: 'struct', d: 'struct', e: false },
    { b: 'class', d: 'struct', e: false },
    { b: 'struct', d: 'class', e: false },
    { b: 'struct', d: 'class', e: true },
  ];

  Deck.widget('tipos/acesso', {
    iniciar(raiz) {
      raiz.classList.add('tp-acesso');
      const grupo = (rotulo, chave) => {
        const g = el('div', { class: 'tp-seg', role: 'group', 'aria-label': rotulo }, el('span', { class: 'tp-seg-rot' }, rotulo));
        for (const kw of ['struct', 'class']) {
          g.append(el('button', {
            class: 'botao', type: 'button', 'data-v': kw,
            onclick: () => { raiz._st[chave] = kw; acDesenhar(raiz, true); },
          }, kw));
        }
        return g;
      };
      raiz._gb = grupo('Ponto é', 'b');
      raiz._gd = grupo('Ponto3D é', 'd');
      raiz._be = el('button', {
        class: 'botao', type: 'button',
        onclick: () => { raiz._st.e = !raiz._st.e; acDesenhar(raiz, true); },
      }, 'escrever o acesso: public');
      raiz._cod = el('div', { class: 'tp-ac-cod' });
      raiz._uso = el('div', { class: 'tp-ac-uso' });
      raiz._msgs = el('div', { class: 'tp-ac-msgs' });
      raiz._regra = el('p', { class: 'tp-ac-regra' });
      raiz.append(
        el('div', { class: 'tp-ac-ctl' }, raiz._gb, raiz._gd, raiz._be),
        el('div', { class: 'tp-ac-esq' }, raiz._cod),
        el('div', { class: 'tp-ac-dir' }, raiz._uso, raiz._msgs),
        raiz._regra);
      raiz._st = { ...AC_ESTADOS[0] };
      raiz._ant = null;
      acDesenhar(raiz, false);
    },
    passo(raiz, k, ctx, dir) {
      raiz._st = { ...AC_ESTADOS[limitar(k, 0, AC_ESTADOS.length - 1)] };
      acDesenhar(raiz, anima(ctx, dir));
    },
  });

  function acDesenhar(raiz, animar) {
    const { b, d, e } = raiz._st;
    const ant = raiz._ant;
    raiz._ant = { b, d, e };
    raiz._gb.querySelectorAll('button').forEach((x) => pressionar(x, x.dataset.v === b));
    raiz._gd.querySelectorAll('button').forEach((x) => pressionar(x, x.dataset.v === d));
    pressionar(raiz._be, e);

    const acPonto = b === 'struct' ? 'public' : 'private';
    const heranca = e || d === 'struct' ? 'public' : 'private';
    const acZ = e || d === 'struct' ? 'public' : 'private';
    const mudouB = animar && ant && ant.b !== b;
    const mudouD = animar && ant && (ant.d !== d || ant.e !== e);
    const tag = (txt, pub, mudou) => el('span', { class: 'tp-tag ' + (pub ? 'pub' : 'priv') + (mudou ? ' mudou' : '') }, txt);

    const linhas = [
      { t: `⟦${b}⟧ Ponto {` },
      { t: '    int x = 0;', fim: tag(acPonto, acPonto === 'public', mudouB) },
      { t: '    int y = 0;', fim: tag(acPonto, acPonto === 'public', mudouB) },
      { t: '};' },
      { t: `⟦${d}⟧ Ponto3D : ${e ? '⟦public⟧ ' : ''}Ponto {`, fim: tag('herança ' + heranca, heranca === 'public', mudouD) },
    ];
    if (e) linhas.push({ t: '⟦public:⟧' });
    linhas.push({ t: '    int z = 0;', fim: tag(acZ, acZ === 'public', mudouD) }, { t: '};' });
    raiz._cod.replaceChildren(figuraCodigo(linhas, { classe: 'compacto', titulo: 'ponto.hpp' }));

    const px = acPonto === 'public' ? null : AC_MSG.xPriv;
    const qx = acPonto !== 'public' ? AC_MSG.xPriv : heranca === 'public' ? null : AC_MSG.xInac;
    const qz = acZ === 'public' ? null : AC_MSG.zPriv;
    const st = (m) => el('span', { class: 'tp-tag ' + (m ? 'priv' : 'pub') }, m ? '✗ erro' : '✓ compila');
    raiz._uso.replaceChildren(figuraCodigo([
      'Ponto p;',
      { t: 'p.x = 1;', cls: px ? 'ln-ruim' : 'ln-bom', fim: st(px) },
      'Ponto3D q;',
      { t: 'q.x = 1;', cls: qx ? 'ln-ruim' : 'ln-bom', fim: st(qx) },
      { t: 'q.z = 3;', cls: qz ? 'ln-ruim' : 'ln-bom', fim: st(qz) },
    ], { classe: 'compacto', titulo: 'main.cpp — o uso, de fora' }));

    const erros = [[2, px], [4, qx], [5, qz]].filter(([, m]) => m);
    raiz._msgs.replaceChildren(...(erros.length
      ? erros.map(([n, m]) => msg('', el('b', null, `main.cpp:${n}:3: error: `), m))   // col. 3 = o membro (x/z), como o GCC aponta
      : [msg('ok', '✓ as três linhas compilam')]));
    if (animar && erros.length) raiz._msgs.querySelectorAll('.tp-msg').forEach((m) => m.classList.add('entra'));

    let regra;
    if (e && b === 'class') regra = 'Ponto3D está explícito, mas <b>x</b> e <b>y</b> continuam <b>private</b>: quem fecha é o Ponto.';
    else if (e) regra = 'Escreva o acesso — <code>: public Ponto</code> e <code>public:</code> — e a palavra-chave deixa de importar.';
    else if (b === 'struct' && d === 'struct') regra = '<b>struct</b>: tudo público — os membros <b>e</b> a herança.';
    else if (b === 'class' && d === 'struct') regra = '<b>class Ponto</b>: membro sem rótulo é <b>private</b> — e herdar como public não reabre o que era fechado.';
    else if (b === 'struct') regra = '<b>class Ponto3D : Ponto</b> herda como <b>private</b>: o x herdado fica <b>inacessível</b> de fora (e o z, private).';
    else regra = 'Tudo fechado: membros <b>private</b> e herança <b>private</b>.';
    raiz._regra.innerHTML = regra;
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  tipos/agregado — o que tira um tipo da categoria "agregado"
  // ═══════════════════════════════════════════════════════════════════════
  const AG_OPS = [
    { id: 'metodo', rot: 'método', linhas: ['    int area() const;'] },
    { id: 'nsdmi', rot: 'inicializador de membro' },
    { id: 'static', rot: 'membro static', linhas: ['    static int criados;'] },
    { id: 'default', rot: '= default', linhas: ['    Retangulo() = default;'], regra: 0 },
    { id: 'ctor', rot: 'construtor', linhas: ['    Retangulo(int l, int a);'], regra: 0 },
    { id: 'privado', rot: 'membro private', linhas: ['private:', '    int id = 0;'], regra: 1 },
    { id: 'virtual', rot: 'função virtual', linhas: ['    virtual void desenhar() const;'], regra: 2 },
  ];
  const AG_REGRAS = [
    'nenhum construtor declarado pelo usuário — nem <code>= default</code>',
    'nenhum membro de dados <code>private</code> / <code>protected</code>',
    'nenhuma função <code>virtual</code>',
    'nenhuma base <code>virtual</code>, <code>private</code> ou <code>protected</code>',
  ];
  const AG_ESTADOS = [[], ['metodo', 'nsdmi'], ['metodo', 'nsdmi', 'default'], ['metodo', 'nsdmi', 'privado'], ['metodo', 'nsdmi', 'virtual']];
  const AG_ORDEM = ['static', 'ctor', 'default', 'metodo', 'virtual'];   // ordem das linhas no corpo
  // o PORQUÊ de cada regra (o "= default" sozinho tem o seu aviso do C++20, abaixo)
  const AG_PORQUE = {
    ctor: 'Declarou um construtor? Então é <b>ele</b> quem decide como o objeto nasce: <code>{10, 20}</code> passa a chamá-lo — e designadores não servem para construtor.',
    privado: 'As chaves preenchem os membros <b>de fora</b> do tipo — e de fora ninguém escreve num membro <code>private</code>.',
    virtual: 'Com <code>virtual</code>, o objeto ganha um ponteiro escondido, o <b>vptr</b> (4.7), e quem o preenche é o construtor — chaves membro a membro não dão conta.',
  };

  Deck.widget('tipos/agregado', {
    iniciar(raiz) {
      raiz.classList.add('tp-agregado');
      raiz._bts = {};
      const ops = el('div', { class: 'tp-ag-ops' }, el('span', { class: 'tp-seg-rot' }, 'acrescente:'));
      for (const op of AG_OPS) {
        const bt = el('button', {
          class: 'botao' + (op.id === 'default' ? ' mono' : ''), type: 'button',
          onclick: () => {
            const s = raiz._sel;
            const liga = !s.has(op.id);
            if (liga) s.add(op.id); else s.delete(op.id);
            agDesenhar(raiz, new Set(liga ? [op.id] : []), op.id, true);
          },
        }, op.rot);
        raiz._bts[op.id] = bt;
        ops.append(bt);
      }
      raiz._cod = el('div', { class: 'tp-ag-cod' });
      raiz._nota = el('div', { class: 'tp-ag-nota' });
      raiz._ver = el('div', { class: 'tp-ag-ver' });
      raiz._teste = el('div', { class: 'tp-ag-teste' });
      raiz._regras = el('ul', { class: 'tp-ag-regras' });
      raiz.append(ops,
        el('div', { class: 'tp-ag-esq' }, raiz._cod, raiz._nota),
        el('div', { class: 'tp-ag-dir' }, raiz._ver, raiz._teste,
          el('p', { class: 'tp-rot' }, 'agregado no C++20 ='), raiz._regras));
      raiz._sel = new Set();
      raiz._agAnt = null;
      agDesenhar(raiz, new Set(), null, false);
    },
    passo(raiz, k, ctx, dir) {
      const i = limitar(k, 0, AG_ESTADOS.length - 1);
      raiz._sel = new Set(AG_ESTADOS[i]);
      const animar = anima(ctx, dir);
      // ao avançar, as linhas que este passo acrescentou piscam (o que mudou salta aos olhos)
      const novos = new Set(animar && i > 0 ? AG_ESTADOS[i].filter((id) => !AG_ESTADOS[i - 1].includes(id)) : []);
      agDesenhar(raiz, novos, null, animar);
    },
  });

  function agDesenhar(raiz, novos, clicado, animar) {
    const s = raiz._sel;
    for (const op of AG_OPS) pressionar(raiz._bts[op.id], s.has(op.id));
    const quebras = AG_OPS.filter((o) => s.has(o.id) && o.regra != null);
    const violadas = new Set(quebras.map((o) => o.regra));
    const agregado = quebras.length === 0;
    const marca = (id) => (s.has(id) ? (AG_OPS.find((o) => o.id === id).regra != null ? 'ln-ruim' : 'ln-bom') : '');
    const novo = (id) => (novos.has(id) ? ' tp-novo' : '');

    const linhas = ['struct Retangulo {'];
    linhas.push({ t: s.has('nsdmi') ? '    int largura = 1;' : '    int largura;', cls: marca('nsdmi') + novo('nsdmi') });
    linhas.push('    int altura;');
    for (const id of AG_ORDEM) {
      if (!s.has(id)) continue;
      AG_OPS.find((o) => o.id === id).linhas.forEach((t) => linhas.push({ t, cls: marca(id) + novo(id) }));
    }
    if (s.has('privado')) AG_OPS.find((o) => o.id === 'privado').linhas.forEach((t) => linhas.push({ t, cls: 'ln-ruim' + novo('privado') }));
    linhas.push('};');
    raiz._cod.replaceChildren(figuraCodigo(linhas, { classe: 'compacto' + (linhas.length > 9 ? ' tp-longo' : ''), titulo: 'retangulo.hpp' }));

    raiz._nota.replaceChildren();
    // o aviso do C++17 só vale quando o "= default" é o ÚNICO motivo (com virtual,
    // membro private ou construtor de verdade, o C++17 também rejeitava) — verificado
    const porque = quebras.filter((o) => AG_PORQUE[o.id]);
    const motivo = porque.find((o) => o.id === clicado) || porque[0];
    if (s.has('default') && quebras.every((o) => o.id === 'default')) {
      raiz._nota.append(el('div', { class: 'callout aviso compacto', html: '<b>Mudou no C++20</b>Com <code>-std=c++17</code>, <code>Retangulo r{10, 20};</code> ainda compilava com <code>= default</code>. Agora, não.' }));
    } else if (motivo) {
      raiz._nota.append(el('div', { class: 'callout nota compacto', html: `<b>Por quê? · ${motivo.rot}</b>${AG_PORQUE[motivo.id]}` }));
    } else if (agregado && s.size) {
      raiz._nota.append(el('div', { class: 'callout ok compacto', html: '<b>Continua agregado</b>Métodos, membros <code>static</code> e inicializadores de membro são permitidos.' }));
    } else {
      raiz._nota.append(el('div', { class: 'callout dica compacto', html: '<b>Preveja antes de clicar</b>Qual destes itens tira o <code>Retangulo</code> da categoria <i>agregado</i>?' }));
    }

    const val = el('span', { class: 'tp-ag-val ' + (agregado ? 'ok-txt' : 'erro-txt') }, agregado ? 'true' : 'false');
    if (animar && raiz._agAnt !== null && raiz._agAnt !== agregado) val.classList.add('muda');
    raiz._agAnt = agregado;
    raiz._ver.replaceChildren(el('span', { class: 'tp-ag-trait mono' }, 'std::is_aggregate_v<Retangulo>'), val);
    raiz._teste.replaceChildren(
      figuraCodigo([{ t: 'Retangulo r{.largura = 10, .altura = 20};', cls: agregado ? 'ln-bom' : 'ln-ruim' }], { classe: 'compacto' }),
      agregado ? msg('ok', '✓ compila — inicialização agregada')
        : msg('', 'error: designated initializers cannot be used with a non-aggregate type ‘Retangulo’'));
    raiz._regras.replaceChildren(...AG_REGRAS.map((r, i) => el('li', { class: violadas.has(i) ? 'violada' : '' },
      el('span', { class: 'tp-ag-ic' }, violadas.has(i) ? '✗' : '✓'), el('span', { html: r }))));
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  tipos/config — designated initializers
  // ═══════════════════════════════════════════════════════════════════════
  const CF = [
    { id: 'host', valor: '"api.exemplo.com"', padrao: '"localhost"' },
    { id: 'porta', valor: '443', padrao: '8080' },
    { id: 'tls', valor: 'true', padrao: 'false' },
    { id: 'timeout', valor: '60', padrao: '30' },
  ];
  const CF_IDX = Object.fromEntries(CF.map((c, i) => [c.id, i]));
  const CF_ESTADOS = [[], ['host'], ['host', 'porta', 'tls'], ['porta', 'host']];

  Deck.widget('tipos/config', {
    iniciar(raiz) {
      raiz.classList.add('tp-config');
      const decl = Deck.util.codigo([
        'struct Config {',
        '    std::string host    = "localhost";',
        '    int         porta   = 8080;',
        '    bool        tls     = false;',
        '    int         timeout = 30;',
        '};',
      ].join('\n'), { arquivo: 'demo_tipos.hpp', inicio: 76, classe: 'compacto' });
      raiz._obj = el('div', { class: 'tp-cf-obj' });
      raiz._bts = {};
      const bts = el('div', { class: 'tp-cf-bts' });
      for (const c of CF) {
        const bt = el('button', {
          class: 'botao mono', type: 'button',
          onclick: () => {
            const ant = raiz._sel.slice();
            const i = raiz._sel.indexOf(c.id);
            if (i >= 0) raiz._sel.splice(i, 1); else raiz._sel.push(c.id);
            cfDesenhar(raiz, true, ant);
          },
        }, '.' + c.id);
        raiz._bts[c.id] = bt;
        bts.append(bt);
      }
      bts.append(
        el('button', { class: 'botao', type: 'button', onclick: () => { const ant = raiz._sel.slice(); raiz._sel.reverse(); cfDesenhar(raiz, true, ant); } }, '⇄ inverter'),
        el('button', { class: 'botao', type: 'button', onclick: () => { const ant = raiz._sel.slice(); raiz._sel = []; cfDesenhar(raiz, true, ant); } }, 'limpar'));
      raiz._linha = el('div', { class: 'tp-cf-linha' });
      raiz._status = el('div', { class: 'tp-cf-status' });
      raiz.append(
        el('div', { class: 'tp-cf-esq' }, decl, el('p', { class: 'tp-rot' }, 'o objeto que sai'), raiz._obj),
        el('div', { class: 'tp-cf-dir' }, el('p', { class: 'tp-rot' }, 'clique nos campos — a linha segue a ordem dos cliques'), bts, raiz._linha, raiz._status));
      raiz._sel = [];
      cfDesenhar(raiz, false, []);
    },
    passo(raiz, k, ctx, dir) {
      const i = limitar(k, 0, CF_ESTADOS.length - 1);
      raiz._sel = CF_ESTADOS[i].slice();
      cfDesenhar(raiz, anima(ctx, dir), i > 0 ? CF_ESTADOS[i - 1] : []);
    },
  });

  /** ant = a seleção anterior: só serve para piscar o que mudou (o estado vem de raiz._sel) */
  function cfDesenhar(raiz, animar, ant) {
    const sel = raiz._sel;
    const novo = (id) => animar && !ant.includes(id);
    for (const c of CF) pressionar(raiz._bts[c.id], sel.includes(c.id));
    // o primeiro designador que "volta" na ordem da declaração é o que o GCC acusa
    let maior = -1, erro = -1;
    sel.forEach((id, j) => { if (erro < 0) { if (CF_IDX[id] < maior) erro = j; else maior = CF_IDX[id]; } });

    let linhas;
    if (!sel.length) linhas = [{ t: 'Config c{};', cls: 'ln-bom' }];
    else {
      const larg = Math.max(...sel.map((id) => id.length + 1));
      linhas = [{ t: 'Config c{' }];
      sel.forEach((id, j) => {
        const c = CF[CF_IDX[id]];
        linhas.push({ t: `    ${('.' + id).padEnd(larg)} = ${c.valor}${j < sel.length - 1 ? ',' : ''}`, cls: (j === erro ? 'ln-ruim' : '') + (novo(id) ? ' tp-novo' : '') });
      });
      linhas.push({ t: '};' });
    }
    const fig = figuraCodigo(linhas, { titulo: 'main.cpp' });
    raiz._linha.replaceChildren(fig);
    if (animar && erro >= 0) fig.classList.add('tp-treme');

    const nomeados = new Set(sel);
    const mudou = (id) => animar && erro < 0 && nomeados.has(id) !== ant.includes(id);
    raiz._obj.classList.toggle('tp-sem', erro >= 0);
    raiz._obj.replaceChildren(...CF.map((c) => el('div', { class: 'tp-cf-campo' + (nomeados.has(c.id) ? ' nomeado' : '') + (mudou(c.id) ? ' tp-muda' : '') },
      el('span', { class: 'tp-cf-nome' }, c.id),
      el('span', { class: 'tp-cf-val' }, nomeados.has(c.id) ? c.valor : c.padrao),
      el('span', { class: 'tp-cf-orig' }, nomeados.has(c.id) ? 'nomeado' : 'padrão do membro'))),
    el('div', { class: 'tp-cf-x' }, '✗ não compila — não existe objeto'));

    if (erro >= 0) {
      raiz._status.replaceChildren(msg('', el('b', null, 'error: '), `designator order for field ‘Config::${sel[erro]}’ does not match declaration order in ‘Config’`),
        el('p', { class: 'tp-cf-nota', html: 'Em C++, os designadores seguem a ordem da <b>declaração</b>: host, porta, tls, timeout.' }));
    } else {
      const n = sel.length;
      raiz._status.replaceChildren(msg('ok', '✓ compila'),
        el('p', { class: 'tp-cf-nota', html: n ? `${n} nomeado${n > 1 ? 's' : ''}; ${4 - n === 0 ? 'nenhum ficou' : (4 - n) + ' fica' + (4 - n > 1 ? 'm' : '')} com o padrão da declaração.` : 'Nenhum nomeado: cada membro fica com o inicializador da declaração.' }));
    }
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  tipos/escopo — enum clássico vaza; enum class contém
  // ═══════════════════════════════════════════════════════════════════════
  const ES = [
    { t: 'enum CorAntiga { Vermelho, Verde, Azul };', tipo: 'CorAntiga', vals: ['Vermelho', 'Verde', 'Azul'] },
    { t: 'enum Fruta     { Maca, Banana, Verde };', t2: 'enum Fruta     { Maca, Banana /*, Verde */ };', tipo: 'Fruta', vals: ['Maca', 'Banana'], colide: 'Verde' },
    { t: 'enum class Cor : std::uint8_t { Vermelho, Verde, Azul };', tipo: 'Cor', vals: ['Vermelho', 'Verde', 'Azul'], escopo: true },
    { t: 'enum class Semaforo { Vermelho, Amarelo, Verde };', tipo: 'Semaforo', vals: ['Vermelho', 'Amarelo', 'Verde'], escopo: true },
  ];

  Deck.widget('tipos/escopo', {
    iniciar(raiz) {
      raiz.classList.add('tp-escopo');
      // em cima, o código (largura total, 24 px); embaixo, a narração à esquerda e a caixa do escopo à direita
      raiz._cod = el('div', { class: 'tp-es-cod' });
      raiz._msg = el('div', { class: 'tp-es-msg' });
      raiz._caixa = el('div', { class: 'tp-es-linhas' });
      raiz.append(raiz._cod, raiz._msg,
        el('div', { class: 'tp-es-caixa' }, el('p', { class: 'tp-rot' }, 'escopo de fora · ', el('span', { class: 'tp-nu' }, 'namespace demo_tipos')), raiz._caixa));
    },
    passo(raiz, k, ctx, dir) {
      const animar = anima(ctx, dir);
      // código
      raiz._cod.replaceChildren(figuraCodigo(ES.map((d, i) => ({
        t: i === 1 && k >= 3 ? d.t2 : d.t,
        cls: i === k - 1 ? 'foco' : i >= k ? 'tp-futuro' : '',
      })), { classe: 'focando', titulo: 'quatro enums no mesmo escopo (Semaforo não está no demo)' }));
      // caixa do escopo
      const chip = (txt, cls, d) => vars(el('span', { class: 'tp-chip ' + (cls || '') }, txt), { '--d': d || 0 });
      // no último passo, os três Verde ganham o seu valor (a contagem começa em 0: posição na lista)
      const verde = (c, valor, ordem) => {
        if (k < 4 || c.textContent !== 'Verde') return c;
        c.classList.add('verde');
        c.append(vars(el('small', { class: 'tp-es-val' + (animar ? ' entra' : '') }, String(valor)), { '--o': ordem }));
        return c;
      };
      const linhas = [];
      ES.forEach((d, i) => {
        if (i >= k) return;
        const nova = animar && i === k - 1;
        const linha = el('div', { class: 'tp-es-linha' + (nova ? ' entra' : '') });
        if (!d.escopo) {
          linha.append(chip(d.tipo, 'tipo', 0));
          d.vals.forEach((v, j) => linha.append(verde(chip(v, i === 0 && v === 'Verde' && k === 2 ? 'colide' : '', j + 1), j, 0)));
          if (d.colide && k === 2) linha.append(chip(d.colide, 'colide fantasma', d.vals.length + 1));
          if (d.colide && k >= 3) linha.append(chip('/* Verde */', 'comentado', d.vals.length + 1));
        } else {
          const dentro = el('div', { class: 'tp-es-dentro' }, ...d.vals.map((v, j) => verde(chip(v, 'contido', j + 1), j, i - 1)));
          linha.append(chip(d.tipo, 'tipo', 0), vars(el('div', { class: 'tp-es-ninho' },
            el('span', { class: 'tp-es-pref mono' }, d.tipo + '::'), dentro), { '--d': 1 }));
        }
        linhas.push(linha);
      });
      if (!linhas.length) linhas.push(el('p', { class: 'tp-es-vazio' }, '(vazio — avance para declarar)'));
      raiz._caixa.replaceChildren(...linhas);
      raiz._caixa.parentNode.classList.toggle('erro', k === 2);
      if (animar && k === 2) reiniciar(raiz._caixa.parentNode, 'tp-treme');
      // mensagem
      const M = [
        () => el('p', { class: 'tp-es-txt', html: 'Cada passo declara um enum no <b>mesmo</b> escopo. Observe a caixa da direita.' }),
        () => el('div', null, msg('aviso', 'Vermelho, Verde e Azul viraram nomes do escopo de FORA.'),
          figuraCodigo([{ t: 'int x = Verde;   // ⚠ compila: x = 1', cls: '' }], { classe: 'compacto' })),
        () => el('div', null, msg('', el('b', null, 'error: '), '‘Verde’ conflicts with a previous declaration'),
          msg('nota', 'note: previous declaration ‘CorAntiga Verde’')),
        () => el('div', null, msg('ok', '✓ enum class: só o nome Cor entrou no escopo de fora.'),
          figuraCodigo([{ t: 'Cor c = Cor::Verde;   // ✓ qualificado' }], { classe: 'compacto' })),
        () => el('div', null, msg('ok', '✓ três Verde convivendo, cada um no seu escopo:'),
          figuraCodigo([
            'int a = Verde;                             // 1',
            'int b = static_cast<int>(Cor::Verde);      // 1',
            'int c = static_cast<int>(Semaforo::Verde); // 2',
          ], { classe: 'compacto' })),
      ];
      raiz._msg.replaceChildren(M[limitar(k, 0, 4)]());
    },
  });

  // ═══════════════════════════════════════════════════════════════════════
  //  tipos/flags — enum class como conjunto de bits
  // ═══════════════════════════════════════════════════════════════════════
  const FL = [{ n: 'Ler', b: 0 }, { n: 'Escrever', b: 1 }, { n: 'Executar', b: 2 }];
  const FL_EST = [0, 1, 3, 7];
  const bin3 = (v) => '0b' + v.toString(2).padStart(3, '0');

  Deck.widget('tipos/flags', {
    iniciar(raiz) {
      raiz.classList.add('tp-flags');
      raiz._bts = FL.map((f) => el('button', {
        class: 'botao', type: 'button',
        onclick: () => { raiz._v ^= 1 << f.b; flDesenhar(raiz, 1 << f.b); },
      }, el('b', null, f.n), el('small', { class: 'mono' }, `1u << ${f.b}`)));
      raiz._bits = el('div', { class: 'tp-fl-bits' });
      raiz._val = el('div', { class: 'tp-fl-val' });
      raiz._expr = el('div', { class: 'tp-fl-expr' });
      raiz._cont = el('div', { class: 'tp-fl-cont' });
      raiz.append(
        el('div', { class: 'tp-fl-bts' }, ...raiz._bts),
        el('div', { class: 'tp-fl-meio' }, raiz._bits, raiz._val),
        raiz._expr,
        raiz._cont);
      raiz._v = 0;
      flDesenhar(raiz, 0);
    },
    passo(raiz, k, ctx, dir) {
      const i = limitar(k, 0, FL_EST.length - 1);
      raiz._v = FL_EST[i];
      // ao avançar, os bits que este passo ligou "pulam" (máscara das diferenças)
      flDesenhar(raiz, anima(ctx, dir) && i > 0 ? FL_EST[i] ^ FL_EST[i - 1] : 0);
    },
  });

  /** mudou = máscara dos bits que acabaram de mudar (só para animar) */
  function flDesenhar(raiz, mudou) {
    const v = raiz._v;
    FL.forEach((f, i) => pressionar(raiz._bts[i], (v >> f.b) & 1));
    raiz._bits.replaceChildren(...[2, 1, 0].map((b) => el('div', { class: 'tp-fl-bit' + ((v >> b) & 1 ? ' on' : '') + ((mudou >> b) & 1 ? ' muda' : '') },
      el('span', { class: 'tp-fl-nome' }, FL[b].n),
      el('span', { class: 'tp-fl-d' }, String((v >> b) & 1)),
      el('span', { class: 'tp-fl-i' }, 'bit ' + b))));
    raiz._val.replaceChildren(el('span', { class: 'mono' }, bin3(v)), el('small', null, ' = ' + v));
    const nomes = FL.filter((f) => (v >> f.b) & 1).map((f) => 'Permissao::' + f.n);
    const linhas = !nomes.length ? ['auto perms = Permissao::Nenhuma;']
      : nomes.map((n, i) => (i === 0 ? 'auto perms = ' : '           | ') + n + (i === nomes.length - 1 ? ';' : ''));
    raiz._expr.replaceChildren(figuraCodigo(linhas, { classe: 'compacto' }));
    raiz._cont.replaceChildren(...FL.map((f) => {
      const m = 1 << f.b, r = v & m;
      return el('div', { class: 'tp-fl-linha' },
        el('span', { class: 'mono tp-fl-chamada' }, `contem(perms, ${f.n})`),
        el('span', { class: 'mono tp-fl-conta' }, `${bin3(v)} & ${bin3(m)} = ${bin3(r)}`),
        el('span', { class: 'tp-tag ' + (r ? 'pub' : 'priv') }, r ? 'true' : 'false'));
    }));
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  tipos/union — os mesmos 4 bytes, três leituras
  // ═══════════════════════════════════════════════════════════════════════
  const dv4 = () => new DataView(new ArrayBuffer(4));
  function bytesDe(membro, valor) {
    const d = dv4();
    if (membro === 'inteiro') d.setInt32(0, valor, true); else d.setFloat32(0, valor, true);
    return [0, 1, 2, 3].map((i) => d.getUint8(i));
  }
  function lerComo(bytes, membro) {
    const d = dv4();
    bytes.forEach((x, i) => d.setUint8(i, x));
    return membro === 'inteiro' ? d.getInt32(0, true) : d.getFloat32(0, true);
  }
  const hex2 = (x) => x.toString(16).toUpperCase().padStart(2, '0');
  const fmtFloat = (f) => (Number.isFinite(f) ? String(parseFloat(f.toPrecision(9))) : String(f));
  const ascii = (x) => (x >= 0x20 && x < 0x7f ? String.fromCharCode(x) : '·');

  const UN_EST = [
    { esc: null, ler: null, txt: 'Bruto b{}; — o <code>{}</code> inicializa o PRIMEIRO membro: inteiro = 0.' },
    { esc: ['inteiro', 0x41424344, 'b.inteiro = 0x41424344;'], ler: 'inteiro', txt: 'Little-endian: o byte <b>menos</b> significativo (44) vai para o <b>menor</b> endereço.' },
    { esc: ['inteiro', 0x41424344, 'b.inteiro = 0x41424344;'], ler: 'bytes', txt: 'Ler <code>b.bytes</code> depois de escrever <code>inteiro</code>: é o que o demo imprime — e é UB pelo padrão.' },
    { esc: ['inteiro', 0x41424344, 'b.inteiro = 0x41424344;'], ler: 'real', txt: 'Para reinterpretar bits de verdade: <code>std::bit_cast&lt;float&gt;(b.inteiro)</code> → 12.1414223, sem UB.' },
    { esc: ['real', 1.0, 'b.real = 1.0f;'], ler: 'real', txt: 'Agora o membro ativo é <code>real</code>: os bytes são os do float 1.0 (0x3F800000).' },
  ];
  const UN_BOTOES = [['inteiro', 0x41424344, 'b.inteiro = 0x41424344;'], ['real', 1.0, 'b.real = 1.0f;'], ['real', 3.5, 'b.real = 3.5f;']];
  const UN_COL = 150;      // largura de uma coluna de byte (px)

  Deck.widget('tipos/union', {
    iniciar(raiz) {
      raiz.classList.add('tp-union');
      const decl = Deck.util.codigo([
        'union Bruto {',
        '    std::int32_t inteiro;',
        '    float        real;',
        '    std::uint8_t bytes[4];',
        '};',
      ].join('\n'), { arquivo: 'demo_tipos.hpp', inicio: 184, classe: 'compacto' });
      const tam = el('div', { class: 'tp-un-tam' },
        el('span', { class: 'tp-cod-rot' }, 'sizeof(Bruto)'),
        el('span', { class: 'tp-un-num' }, '4'),
        el('span', { class: 'tp-un-tam-txt', html: 'o <b>maior</b> membro —<br>não 4 + 4 + 4 = 12' }));
      const bts = el('div', { class: 'tp-un-bts' }, ...UN_BOTOES.map((b) => el('button', {
        class: 'botao mono', type: 'button',
        onclick: () => { raiz._st = { esc: b, ler: b[0], txt: null }; unDesenhar(raiz, true, false); },
      }, b[2])));
      raiz._instr = el('div', { class: 'tp-un-instr' });
      raiz._topo = el('div', { class: 'tp-un-topo' });
      raiz._x = svg('svg', { class: 'tp-un-x', viewBox: `0 0 ${UN_COL * 4} 64`, width: UN_COL * 4, height: 64, 'aria-hidden': 'true' });
      raiz._mem = el('div', { class: 'tp-un-mem' });
      raiz._lentes = el('div', { class: 'tp-un-lentes' });
      raiz._nota = el('p', { class: 'tp-un-nota' });
      raiz.append(
        el('div', { class: 'tp-un-esq' }, decl, tam, el('p', { class: 'tp-rot' }, 'escreva um membro'), bts),
        el('div', { class: 'tp-un-dir' }, raiz._instr,
          el('div', { class: 'tp-un-diagrama' },
            el('span', { class: 'tp-un-lado' }, 'valor escrito'), raiz._topo,
            el('span', { class: 'tp-un-lado' }, ''), raiz._x,
            el('span', { class: 'tp-un-lado' }, 'memória'), raiz._mem),
          raiz._lentes, raiz._nota));
      raiz._st = UN_EST[0];
    },
    passo(raiz, k, ctx, dir) {
      const i = limitar(k, 0, UN_EST.length - 1);
      raiz._st = UN_EST[i];
      // a memória só se re-anima quando o passo ESCREVE algo novo; se só muda a leitura,
      // quem pulsa é a lente (os bytes não mudaram — é esse o ponto)
      const animar = anima(ctx, dir), ant = i > 0 ? UN_EST[i - 1] : null;
      const escreveu = !ant || (ant.esc && ant.esc[2]) !== (raiz._st.esc && raiz._st.esc[2]);
      unDesenhar(raiz, animar && escreveu, animar && !!ant && ant.ler !== raiz._st.ler);
    },
  });

  /** animar = bytes e fios se redesenham (houve escrita) · pulsar = a lente lida pisca */
  function unDesenhar(raiz, animar, pulsar) {
    const st = raiz._st;
    const ativo = st.esc ? st.esc[0] : 'inteiro';
    const bytes = st.esc ? bytesDe(st.esc[0], st.esc[1]) : [0, 0, 0, 0];
    const ler = st.ler;
    // instrução atual (a leitura aparece nas lentes e na nota)
    raiz._instr.replaceChildren(figuraCodigo([{ t: st.esc ? st.esc[2] : 'Bruto b{};', cls: 'ln-bom' }], { classe: 'compacto' }));
    // valor escrito, do byte mais significativo ao menos (como se escreve em hex)
    const msb = bytes.slice().reverse();
    raiz._topo.replaceChildren(el('span', { class: 'tp-un-0x mono' }, '0x'),
      ...msb.map((x, i) => el('span', { class: 'tp-un-b tp-c' + (3 - i) + (animar ? ' entra' : '') }, hex2(x))));
    // linhas cruzadas: byte de peso i (coluna 3 − i no topo) → endereço +i
    raiz._x.replaceChildren(...[0, 1, 2, 3].flatMap((i) => {
      const x1 = (3 - i) * UN_COL + UN_COL / 2, x2 = i * UN_COL + UN_COL / 2;
      const est = { '--d': (i * 0.08) + 's' };
      const fio = svg('path', { d: `M${x1} 4 C ${x1} 30, ${x2} 26, ${x2} 52`, class: 'tp-un-fio tp-c' + i + (animar ? ' desenha' : ''), pathLength: 1 });
      const pt = ponta(x2, 62, x2, 26, 'tp-un-ponta tp-c' + i + (animar ? ' surge' : ''), 12);
      Object.entries(est).forEach(([k, v]) => { fio.style.setProperty(k, v); pt.style.setProperty(k, v); });
      return [fio, pt];
    }));
    // memória
    raiz._mem.replaceChildren(...bytes.map((x, i) => vars(el('div', { class: 'tp-un-cel tp-c' + i + (animar ? ' cai' : '') },
      el('span', { class: 'tp-un-hex' }, hex2(x)),
      el('span', { class: 'tp-un-asc' }, `'${ascii(x)}'`),
      el('span', { class: 'tp-un-end' }, '&b + ' + i)), { '--d': (0.55 + i * 0.09) + 's' })));
    // lentes
    const vInt = lerComo(bytes, 'inteiro');
    const vFlt = lerComo(bytes, 'real');
    const lentes = [
      ['inteiro', 'std::int32_t', `${vInt}  (0x${(vInt >>> 0).toString(16).toUpperCase().padStart(8, '0')})`],
      ['real', 'float', fmtFloat(vFlt)],
      ['bytes', 'std::uint8_t[4]', bytes.map(hex2).join(' ')],
    ];
    raiz._lentes.replaceChildren(...lentes.map(([m, tipo, valor]) => {
      const lendo = ler === m;
      const ub = lendo && m !== ativo;
      const tag = m === ativo ? el('span', { class: 'tp-tag pub' }, 'membro ativo')
        : ub ? el('span', { class: 'tp-tag alerta' }, '⚠ ler = UB') : el('span', { class: 'tp-tag neutro' }, 'inativo');
      const l = el('button', {
        class: 'tp-un-lente' + (lendo ? ' lendo' : '') + (ub ? ' ub' : '') + (lendo && pulsar ? ' pulsa' : ''), type: 'button', title: 'ler b.' + m,
        onclick: () => { const nova = raiz._st.ler !== m; raiz._st = { esc: raiz._st.esc, ler: m, txt: null }; unDesenhar(raiz, false, nova); },
      }, el('span', { class: 'tp-un-m mono' }, 'b.' + (m === 'bytes' ? 'bytes' : m)), el('span', { class: 'tp-un-t mono' }, tipo),
      el('span', { class: 'tp-un-v mono' }, valor), tag);
      return l;
    }));
    let nota = st.txt;
    if (!nota) {
      nota = ler && ler !== ativo
        ? `Ler <code>b.${ler}</code> depois de escrever <code>b.${ativo}</code>: UB pelo padrão. Use <code>std::bit_cast</code> ou <code>memcpy</code>.`
        : `Membro ativo: <code>b.${ativo}</code>. Os bytes na memória são os dele.`;
    }
    raiz._nota.innerHTML = nota;
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  tipos/variant — tagged union à mão × std::variant
  // ═══════════════════════════════════════════════════════════════════════
  const VA_EST = [
    {
      m: { qual: 'Inteiro', ativo: 'i', valor: '0', cod: ['ValorEtiquetado v{};'], res: ['ok', 'zerado: qual = Inteiro, i = 0'] },
      v: { idx: 0, tipo: 'int', valor: '0', cod: ['std::variant<int, double, std::string> moderno;'], res: ['ok', 'index() = 0 → guarda um int 0'] },
    },
    {
      m: { qual: 'Real', ativo: 'd', valor: '3.5', cod: ['v.qual = ValorEtiquetado::Qual::Real;', 'v.d    = 3.5;'], res: ['aviso', 'DUAS escritas: a etiqueta é responsabilidade sua'] },
      v: { idx: 1, tipo: 'double', valor: '3.5', cod: ['moderno = 3.5;'], res: ['ok', 'index() = 1 e std::get<double>(moderno) = 3.5 — a etiqueta veio junto'] },
    },
    {
      m: { qual: 'Real', ativo: 'd', valor: '3.5', lendo: 'i', cod: ['int x = v.i;    // mas qual diz Real…'], res: ['erro', 'x = 0 (os 4 bytes baixos de 3.5) — UB silencioso: compila, roda, ninguém avisa'] },
      v: { idx: 1, tipo: 'double', valor: '3.5', lendo: true, cod: ['int x = std::get<int>(moderno);'], res: ['erro', 'lança std::bad_variant_access — what(): "std::get: wrong index for variant"'] },
    },
    {
      m: { qual: 'Real', ativo: 'd', valor: '3.5', quebrado: true, cod: ['union { int i; double d; std::string s; };'], res: ['erro', 'error: use of deleted function ‘ValorEtiquetado::~ValorEtiquetado()’'] },
      v: { idx: 2, tipo: 'std::string', valor: '"oi"', cod: ['moderno = std::string{"oi"};'], res: ['ok', 'index() = 2 — o variant constrói e destrói a string'] },
    },
  ];
  const VA_PLACAR = [
    ['quem guarda a etiqueta', 'você', 'ele'],
    ['ler o membro errado', 'UB silencioso', 'std::bad_variant_access'],
    ['tipos não-triviais', 'só escrevendo ctor/dtor à mão', 'sim'],
  ];

  Deck.widget('tipos/variant', {
    iniciar(raiz) {
      raiz.classList.add('tp-variant');
      const painel = (lado, titulo, def, bytes) => {
        const p = el('div', { class: 'tp-va-painel tp-va-' + lado });
        p._obj = el('div', { class: 'tp-va-obj' });
        p._cod = el('div', { class: 'tp-va-cod' });
        p._res = el('div', { class: 'tp-va-res' });
        p._placar = el('div', { class: 'tp-va-placar' });
        p.append(
          el('div', { class: 'tp-va-cab' }, el('span', { class: 'tp-va-tit' }, titulo), el('span', { class: 'tp-tag neutro' }, bytes)),
          el('code', { class: 'cpp tp-va-def' }, def),
          p._obj, p._cod, p._res, p._placar);
        return p;
      };
      raiz._m = painel('m', 'tagged union — à mão', 'struct { enum Qual qual; union { int i; double d; }; }', 'sizeof = 16');
      raiz._v = painel('v', 'std::variant — C++17', 'std::variant<int, double, std::string>', 'sizeof = 40');
      raiz.append(raiz._m, raiz._v);
    },
    passo(raiz, k, ctx, dir) {
      const e = VA_EST[limitar(k, 0, VA_EST.length - 1)];
      const animar = anima(ctx, dir);
      // à mão
      const m = e.m;
      raiz._m._obj.replaceChildren(
        el('div', { class: 'tp-va-caixa etiqueta' + (k === 1 && animar ? ' pisca' : '') }, el('span', { class: 'tp-va-r' }, 'qual'), el('span', { class: 'tp-va-x mono' }, m.qual)),
        el('div', { class: 'tp-va-caixa carga' + (m.lendo ? ' errado' : '') + (m.quebrado ? ' quebrado' : '') },
          el('span', { class: 'tp-va-r' }, m.lendo ? 'lendo i  (ativo: d)' : `union · ativo: ${m.ativo}`),
          el('span', { class: 'tp-va-x mono' }, m.lendo ? 'i = 0 ?' : `${m.ativo} = ${m.valor}`)));
      // variant
      const v = e.v;
      raiz._v._obj.replaceChildren(
        el('div', { class: 'tp-va-caixa etiqueta auto' }, el('span', { class: 'tp-va-r' }, 'index()'), el('span', { class: 'tp-va-x mono' }, String(v.idx))),
        el('div', { class: 'tp-va-caixa carga' + (v.lendo ? ' lanca' : '') }, el('span', { class: 'tp-va-r' }, v.tipo), el('span', { class: 'tp-va-x mono' }, v.valor)));
      for (const [p, d] of [[raiz._m, m], [raiz._v, v]]) {
        p._cod.replaceChildren(figuraCodigo(d.cod.map((t) => ({ t, cls: d.res[0] === 'erro' ? 'ln-ruim' : d.res[0] === 'aviso' ? 'ln-aviso' : 'ln-bom' })), { classe: 'compacto' }));
        const r = msg(d.res[0] === 'erro' ? '' : d.res[0], d.res[1]);
        if (animar && d.res[0] === 'erro') r.classList.add('entra');
        p._res.replaceChildren(r);
      }
      // placar (a tabela do README, uma linha por passo)
      raiz._m._placar.replaceChildren(...VA_PLACAR.map((l, i) => el('div', { class: 'tp-va-pl' + (i < k ? ' visto' : '') }, el('span', null, l[0]), el('b', { class: 'erro-txt' }, l[1]))));
      raiz._v._placar.replaceChildren(...VA_PLACAR.map((l, i) => el('div', { class: 'tp-va-pl' + (i < k ? ' visto' : '') }, el('span', null, l[0]), el('b', { class: 'ok-txt' }, l[2]))));
    },
  });

  // ═══════════════════════════════════════════════════════════════════════
  //  tipos/bitfield — RegistradorStatus, bit a bit
  // ═══════════════════════════════════════════════════════════════════════
  const BF = [  // do bit 0 para cima (layout do GCC/x86-64, medido com std::bit_cast)
    { n: 'pronto', w: 1, c: 'a' },
    { n: 'erro', w: 1, c: 'b' },
    { n: 'codigo_erro', w: 4, c: 'c' },
    { n: 'versao', w: 8, c: 'd' },
    { n: 'reservado', w: 18, c: 'e' },
  ];
  const BF_EST = [
    { v: [0, 0, 0, 0], cod: ['RegistradorStatus reg{};'] },
    { v: [1, 1, 9, 3], cod: ['reg.pronto = 1;   reg.erro = 1;', 'reg.codigo_erro = 9;   reg.versao = 3;'] },
    { v: [1, 1, 20, 3], cod: ['reg.codigo_erro = 20;   // 20 = 1 0100₂: 5 bits'], est: 20 },
    { v: [1, 1, 20, 3], cod: ['reg.codigo_erro = 20;   // 20 = 1 0100₂: 5 bits'], est: 20, diag: true },
  ];

  Deck.widget('tipos/bitfield', {
    iniciar(raiz) {
      raiz.classList.add('tp-bitfield');
      raiz._cod = el('div', { class: 'tp-bf-cod' });
      raiz._palavra = el('div', { class: 'tp-bf-palavra' });
      raiz._ent = el('input', { type: 'number', min: 0, max: 255, value: 20, 'aria-label': 'valor para codigo_erro' });
      const atr = el('button', {
        class: 'botao primario', type: 'button',
        onclick: () => {
          const n = limitar(parseInt(raiz._ent.value, 10) || 0, 0, 255);
          raiz._ent.value = n;
          const ant = raiz._st.v;
          raiz._st = { v: [ant[0], ant[1], n, ant[3]], cod: [`reg.codigo_erro = ${n};`], est: n > 15 ? n : null };
          bfDesenhar(raiz, true, ant);
        },
      }, 'atribuir');
      raiz._ent.addEventListener('keydown', (ev) => { if (ev.key === 'Enter') { ev.preventDefault(); atr.click(); } });
      raiz._grupos = el('div', { class: 'tp-bf-grupos' });
      raiz._diag = el('div', { class: 'tp-bf-diag' });
      raiz.append(
        el('div', { class: 'tp-bf-topo' }, raiz._cod, raiz._palavra),
        raiz._grupos,
        el('div', { class: 'tp-bf-base' },
          el('div', { class: 'tp-bf-ctl' }, el('span', { class: 'tp-seg-rot mono' }, 'reg.codigo_erro ='), raiz._ent, atr),
          raiz._diag));
      raiz._st = BF_EST[0];
    },
    passo(raiz, k, ctx, dir) {
      const i = limitar(k, 0, BF_EST.length - 1);
      raiz._st = BF_EST[i];
      const animar = anima(ctx, dir);
      bfDesenhar(raiz, animar && i === 2, animar && i > 0 ? BF_EST[i - 1].v : null);
    },
  });

  /** animar = o valor que estoura desce e perde o bit de cima · ant = valores anteriores
      (os bits que mudaram em relação a eles "viram", como placas) */
  function bfDesenhar(raiz, animar, ant) {
    const st = raiz._st;
    const mascara = (v) => [v[0] & 1, v[1] & 1, v[2] & 15, v[3] & 255, 0];
    const vals = mascara(st.v);
    const antes = ant ? mascara(ant) : vals;
    let palavra = 0, base = 0;
    const inicio = [];
    BF.forEach((f, i) => { inicio.push(base); palavra += vals[i] * 2 ** base; base += f.w; });
    raiz._cod.replaceChildren(figuraCodigo(st.cod.map((t) => ({ t, cls: st.est ? 'ln-aviso' : '' })), { classe: 'compacto' }));
    raiz._palavra.replaceChildren(
      el('span', { class: 'tp-cod-rot' }, 'std::bit_cast<std::uint32_t>(reg)'),
      el('span', { class: 'tp-bf-hex mono' }, '0x' + palavra.toString(16).toUpperCase().padStart(8, '0')),
      el('span', { class: 'tp-cod-rot' }, 'sizeof(RegistradorStatus) = 4'));
    // grupos, do bit 31 (esquerda) ao bit 0 (direita)
    const grupos = BF.map((f, i) => {
      const g = el('div', { class: `tp-bf-g tp-f${f.c}` + (f.w === 18 ? ' reservado' : '') + (st.est && f.n === 'codigo_erro' ? ' alvo' : '') });
      g.append(el('div', { class: 'tp-bf-nome' }, el('b', { class: 'mono' }, f.n), el('small', null, f.w === 1 ? '1 bit' : f.w + ' bits')));
      const cels = el('div', { class: 'tp-bf-cels' });
      for (let b = f.w - 1; b >= 0; b--) {
        const bit = (vals[i] >> b) & 1;
        const virou = bit !== ((antes[i] >> b) & 1);
        cels.append(el('span', { class: 'tp-bf-bit' + (bit ? ' on' : '') + (virou ? ' muda' : ''), title: 'bit ' + (inicio[i] + b) }, f.w === 18 ? '' : String(bit)));
      }
      g.append(cels);
      const idx = f.w === 18 ? '31 … 14' : f.w === 1 ? String(inicio[i]) : `${inicio[i] + f.w - 1} … ${inicio[i]}`;
      g.append(el('div', { class: 'tp-bf-idx mono' }, idx));
      g.append(el('div', { class: 'tp-bf-v mono' }, f.w === 18 ? '—' : '= ' + vals[i]));
      if (st.est && f.n === 'codigo_erro') g.append(bfEstouro(st.est, animar));
      return g;
    }).reverse();
    raiz._grupos.replaceChildren(...grupos);
    // diagnóstico
    if (st.diag) {
      raiz._diag.replaceChildren(
        el('div', { class: 'tp-bf-dl' }, el('span', { class: 'tp-tag alerta' }, 'constante'), msg('aviso', 'warning: conversion from ‘unsigned int’ to ‘unsigned char:4’ changes value from ‘20’ to ‘4’ [-Woverflow]')),
        el('div', { class: 'tp-bf-dl' }, el('span', { class: 'tp-tag neutro' }, 'runtime'), msg('nota', 'silêncio com -Wall -Wextra -Wpedantic · só -Wconversion: “conversion … may change value”')));
    } else if (st.est) {
      raiz._diag.replaceChildren(el('p', { class: 'tp-bf-txt', html: `${st.est} = ${st.est.toString(2).replace(/(\d)(?=(\d{4})+$)/g, '$1 ')}₂ — o campo tem 4 bits: o que passa do bit 3 <b>cai fora</b>. Fica ${st.est & 15} (${st.est} mod 16).` }));
    } else {
      raiz._diag.replaceChildren(el('p', { class: 'tp-bf-txt', html: 'No GCC/x86-64, o <b>primeiro</b> campo declarado ocupa o bit 0 — outro compilador pode inverter.' }));
    }
  }

  /** o valor que não cabe: os bits extras ficam pendurados à esquerda e caem */
  function bfEstouro(n, animar) {
    const bits = n.toString(2).split('').map(Number);          // do mais significativo ao menos
    const extra = bits.length - 4;
    const e = el('div', { class: 'tp-bf-est' + (animar ? ' anim' : '') });
    bits.forEach((b, i) => e.append(vars(el('span', { class: 'tp-bf-eb' + (i < extra ? ' fora' : '') + (b ? ' on' : '') }, String(b)), { '--d': i * 0.06 + 's' })));
    return e;
  }

  // ═══════════════════════════════════════════════════════════════════════
  //  tipos/incompleto — o que compila com `class Motor;`
  // ═══════════════════════════════════════════════════════════════════════
  const IN_ITENS = [
    { s: 'ok', c: 'Motor* p = nullptr;', d: 'ponteiro: 8 bytes, seja Motor o que for', k: 1 },
    { s: 'ok', c: 'void usa(Motor& m);', d: 'referência', k: 1 },
    { s: 'ok', c: 'void consome(Motor m);', d: 'parâmetro por valor — só a declaração', k: 1 },
    { s: 'ok', c: 'Motor fabrica();', d: 'retorno por valor — só a declaração', k: 1 },
    { s: 'erro', c: 'class Carro { Motor motor_; };', d: 'error: field ‘motor_’ has incomplete type ‘Motor’', k: 2 },
    { s: 'erro', c: 'class Turbo : public Motor {};', d: 'error: invalid use of incomplete type ‘class Motor’', k: 3 },
    { s: 'erro', c: 'p->ligar();    new Motor;', d: 'error: invalid use of incomplete type ‘class Motor’', k: 3 },
    { s: 'erro', c: 'sizeof(Motor)', d: 'error: invalid application of ‘sizeof’ to incomplete type ‘Motor’', k: 4 },
    { s: 'aviso', c: 'delete p;', d: 'COMPILA — warning: possible problem detected in invocation of ‘operator delete’ · note: neither the destructor nor the class-specific ‘operator delete’ will be called, …', k: 5 },
  ];

  Deck.widget('tipos/incompleto', {
    iniciar(raiz) {
      raiz.classList.add('tp-incompleto');
      raiz._lista = el('div', { class: 'tp-in-lista' });
      raiz._lista.append(el('div', { class: 'tp-in-item cab' },
        el('span', { class: 'tp-in-ic' }, '▸'),
        el('code', { class: 'cpp tp-in-c', html: Deck.util.realcar('class Motor;') }),
        el('span', { class: 'tp-in-d' }, 'forward declaration: “Motor existe” — e mais nada. Compila cada linha abaixo?')));
      raiz._itens = IN_ITENS.map((it) => {
        const r = el('div', { class: 'tp-in-item ' + it.s },
          el('span', { class: 'tp-in-ic' }),
          el('code', { class: 'cpp tp-in-c', html: Deck.util.realcar(it.c) }),
          el('span', { class: 'tp-in-d' + (it.s === 'ok' ? '' : ' mono') }, it.d));
        r.addEventListener('click', () => r.classList.toggle('revelado'));
        raiz._lista.append(r);
        return r;
      });
      raiz._porque = el('div', { class: 'tp-in-porque' });
      raiz.append(raiz._lista, raiz._porque);
    },
    passo(raiz, k, ctx, dir) {
      const animar = anima(ctx, dir);
      IN_ITENS.forEach((it, i) => {
        const r = raiz._itens[i];
        const vis = it.k <= k;
        r.classList.toggle('revelado', vis);
        r.classList.toggle('atual', vis && it.k === k);
        r.classList.toggle('sem-anim', !animar);
        r.querySelector('.tp-in-ic').textContent = it.s === 'ok' ? '✓' : it.s === 'erro' ? '✗' : '⚠';
      });
      const seg = (txt, bytes, cls) => vars(el('span', { class: 'tp-in-seg ' + (cls || '') }, el('b', { class: 'mono' }, txt), el('small', null, bytes ? bytes + ' B' : '?? B')), { '--w': bytes || 8 });
      const lay = (rot, ultimo, tag, oculto) => el('div', { class: 'tp-in-lay' + (oculto ? ' oculto' : '') },
        el('span', { class: 'tp-in-lay-r mono' }, rot),
        el('div', { class: 'tp-in-segs' }, seg('rodas_', 4), seg('cambio_', 4), ultimo), tag);
      raiz._porque.replaceChildren(
        el('p', { class: 'tp-rot' }, 'por quê: para montar o layout, o compilador precisa do TAMANHO'),
        el('div', { class: 'tp-in-lays' },
          lay('Carro com Motor*', seg('motor_', 8, 'ptr'), el('span', { class: 'tp-tag pub' }, 'sizeof = 16'), false),
          lay('Carro com Motor', seg('motor_', 0, 'duvida'), el('span', { class: 'tp-tag priv' }, 'sizeof = ??'), k < 2)));
    },
  });

  // ═══════════════════════════════════════════════════════════════════════
  //  tipos/ciclo — headers que se incluem mutuamente
  // ═══════════════════════════════════════════════════════════════════════
  // Os três arquivos são exatamente os compilados na verificação (GCC 13.3): no
  // estado com ciclo, `g++ -c carro.cpp` dá só o erro mostrado; com as forward
  // declarations, carro.cpp + motor.cpp + main.cpp compilam e linkam.
  const CI_LINHA = 30, CI_TOPO = 40 + 16;          // altura da linha e do topo do bloco (compacto)
  const ciY = (n) => CI_TOPO + (n - 0.5) * CI_LINHA; // centro da linha n de um bloco
  const CI_BASE = 288, CI_CPP = 330, CI_ALT = 590;   // base dos headers (7 linhas) · topo do carro.cpp · altura

  Deck.widget('tipos/ciclo', {
    iniciar(raiz) {
      raiz.classList.add('tp-ciclo');
      raiz._a = el('div', { class: 'tp-ci-caixa tp-ci-carro' });
      raiz._b = el('div', { class: 'tp-ci-caixa tp-ci-motor' });
      raiz._c = el('div', { class: 'tp-ci-caixa tp-ci-cpp' });
      raiz._nota = el('div', { class: 'tp-ci-nota' });
      raiz._erro = el('div', { class: 'tp-ci-erro' });
      raiz._selo = el('div', { class: 'tp-ci-selo' });
      raiz._dica = el('p', { class: 'tp-ci-dica' });
      const y2 = ciY(2), y1 = ciY(1), y5 = ciY(5), yc = CI_CPP + y2;
      raiz._svg = svg('svg', { class: 'tp-ci-svg', viewBox: `0 0 1408 ${CI_ALT}`, width: 1408, height: CI_ALT, 'aria-hidden': 'true' },
        svg('g', { class: 'tp-ci-ciclo' },
          svg('path', { class: 'tp-ci-fio', d: `M562 ${y2} C 650 ${y2 - 70}, 760 ${y1 - 40}, 834 ${y1}` }),
          ponta(846, y1, 760, y1 - 40, 'tp-ci-ponta'),
          svg('path', { class: 'tp-ci-fio', d: `M846 ${y2} C 760 ${y5 + 50}, 650 ${y5 + 50}, 574 ${y5}` }),
          ponta(562, y5, 650, y5 + 50, 'tp-ci-ponta')),
        svg('g', { class: 'tp-ci-cppfio' },
          svg('path', { class: 'tp-ci-fio', d: `M562 ${yc} C 800 ${yc}, 1128 ${yc - 10}, 1128 ${CI_BASE + 12}` }),
          ponta(1128, CI_BASE, 1128, yc, 'tp-ci-ponta')));
      raiz.append(raiz._svg, raiz._a, raiz._b, raiz._c, raiz._dica, raiz._erro, raiz._selo, raiz._nota);
    },
    passo(raiz, k, ctx, dir) {
      const animar = anima(ctx, dir);
      raiz.dataset.k = k;
      const resolvido = k >= 2;
      raiz._a.replaceChildren(figuraCodigo([
        '#pragma once',
        resolvido ? { t: 'class Motor;          // basta declarar', cls: 'ln-bom' } : { t: '#include "motor.hpp"', cls: 'ln-ruim' },
        'class Carro {',
        '    Motor* motor_ = nullptr;',
        'public:',
        '    void ligar();',
        '};',
      ], { classe: 'compacto', titulo: 'carro.hpp' }));
      raiz._b.replaceChildren(figuraCodigo([
        '#pragma once',
        resolvido ? { t: 'class Carro;', cls: 'ln-bom' } : { t: '#include "carro.hpp"', cls: 'ln-ruim' },
        'class Motor {',
        { t: '    Carro* dono_ = nullptr;', cls: k === 1 ? 'ln-ruim' : '' },
        'public:',
        '    void girar();',
        '};',
      ], { classe: 'compacto', titulo: 'motor.hpp' }));
      raiz._c.replaceChildren(figuraCodigo([
        '#include "carro.hpp"',
        { t: '#include "motor.hpp"   // só aqui', cls: 'ln-bom' },
        'void Carro::ligar() {',
        '    motor_->girar();',
        '}',
      ], { classe: 'compacto', titulo: 'carro.cpp' }));
      raiz._erro.replaceChildren(
        msg('', el('span', { class: 'tp-ci-cmd' }, '$ g++ -std=c++20 -c carro.cpp   # carro.cpp:1 → #include "carro.hpp"'), el('br'),
          el('span', { class: 'apagado' }, 'In file included from carro.hpp:2,'), el('br'),
          el('span', { class: 'apagado', style: { whiteSpace: 'pre' } }, '                 from carro.cpp:1:'), el('br'),
          el('b', null, 'motor.hpp:4:5: error: '), '‘Carro’ does not name a type'),
        el('p', { class: 'tp-ci-txt', html: 'O <code>#pragma once</code> corta o laço infinito — mas alguém é lido <b>primeiro</b>: o <code>motor.hpp</code> entra no meio do <code>carro.hpp</code>, antes de <code>class Carro</code> existir.' }));
      raiz._selo.replaceChildren(el('span', { class: 'tp-tag ' + (resolvido ? 'pub' : 'priv') }, resolvido ? '✓ compila' : 'ciclo de #include'));
      // a frase do passo (no 1 o erro ocupa o lugar dela; no 3, o carro.cpp)
      raiz._dica.innerHTML = k === 2
        ? 'Cada um guarda só um <b>ponteiro</b> — e, para um ponteiro, o tamanho do outro tipo não importa. Basta <b>declarar</b>: <code>class Motor;</code>. Nenhum header inclui o outro, e o ciclo some.'
        : 'Carro guarda um <code>Motor*</code>; Motor guarda um <code>Carro*</code>. Cada header inclui o outro — <b>quem é lido primeiro?</b>';
      raiz._nota.innerHTML = '<b>O pesado só entra no .cpp.</b> Quem inclui <code>carro.hpp</code> não arrasta <code>motor.hpp</code> — e não recompila quando ele muda. É o espírito do <b>pimpl</b>: <a class="aprofundar" href="../../smart-pointers/README.md" target="_blank">src/smart-pointers/</a>';
      if (animar && k === 1) reiniciar(raiz._erro, 'tp-treme');
    },
  });

  // ═══════════════════════════════════════════════════════════════════════
  //  tipos/vptr — os 8 bytes escondidos de um tipo com virtual
  // ═══════════════════════════════════════════════════════════════════════
  const VP_CEL = 80;   // px por byte
  const VP_TXT = [
    '<code>struct SemVirtual { int a; };</code> — 4 bytes, trivial, standard-layout.',
    'Uma função virtual e o compilador insere o <b>vptr</b> no offset 0: o <code>a</code> vai para o offset 8.',
    '8 + 4 = 12, mas o alinhamento é o do ponteiro (8): <b>4 bytes de padding</b> → sizeof = 16.',
    'O vptr aponta para a <b>vtable</b> — uma por classe, gerada pelo compilador (layout real do GCC).',
    'A cópia precisa escrever o vptr da classe <b>certa</b>: não é trivial, nem trivially copyable — e com virtual nunca é standard-layout.',
  ];

  Deck.widget('tipos/vptr', {
    iniciar(raiz) {
      raiz.classList.add('tp-vptr');
      raiz._cod = el('div', { class: 'tp-vp-cod' });
      raiz._fatos = el('div', { class: 'tp-vp-fatos' });
      raiz._vt = el('div', { class: 'tp-vp-vt' },
        el('p', { class: 'tp-cod-rot' }, 'vtable de ComVirtual'),
        ...[['+0', 'offset-to-top = 0'], ['+8', 'typeinfo de ComVirtual'], ['+16', '~ComVirtual()  completo'], ['+24', '~ComVirtual()  deleting']]
          .map(([o, t], i) => el('div', { class: 'tp-vp-ent' + (i === 2 ? ' alvo' : '') }, el('span', { class: 'mono tp-vp-off' }, o), el('span', { class: 'mono' }, t))));
      const regua = el('div', { class: 'tp-vp-regua' });
      for (let i = 0; i < 16; i++) regua.append(el('span', { class: 'tp-vp-cel', style: { left: i * VP_CEL + 'px' } }, el('i', null, String(i))));
      raiz._seg = {
        vptr: el('div', { class: 'tp-vp-seg vptr' }, el('b', { class: 'mono' }, 'vptr'), el('small', null, '8 bytes')),
        a: el('div', { class: 'tp-vp-seg a' }, el('b', { class: 'mono' }, 'int a'), el('small', null, '4 bytes')),
        pad: el('div', { class: 'tp-vp-seg pad' }, el('b', { class: 'mono' }, 'padding'), el('small', null, '4 bytes')),
      };
      regua.append(raiz._seg.vptr, raiz._seg.a, raiz._seg.pad);
      raiz._seta = svg('svg', { class: 'tp-vp-svg', viewBox: '0 0 1408 600', width: 1408, height: 600, 'aria-hidden': 'true' },
        svg('path', { class: 'tp-vp-fio', d: 'M 690 334 C 690 440, 760 506, 852 506', pathLength: 1 }),
        ponta(866, 506, 760, 506, 'tp-vp-ponta'));
      raiz._txt = el('p', { class: 'tp-vp-txt' });
      raiz.append(raiz._seta,
        el('div', { class: 'tp-vp-topo' }, raiz._cod, raiz._fatos),
        el('div', { class: 'tp-vp-meio' }, el('span', { class: 'tp-rot' }, 'offset'), regua),
        raiz._txt, raiz._vt);
    },
    passo(raiz, k, ctx, dir) {
      raiz.dataset.k = k;
      raiz.classList.toggle('sem-anim', !anima(ctx, dir));
      raiz._cod.replaceChildren(figuraCodigo(k === 0
        ? ['struct SemVirtual {', '    int a;', '};']
        : ['struct ComVirtual {', { t: '    virtual ~ComVirtual() = default;', cls: 'ln-aviso' }, '    int a;', '};'],
      { classe: 'compacto', titulo: k === 0 ? 'antes' : 'demo_tipos.hpp' }));
      const S = raiz._seg;
      S.vptr.style.width = 8 * VP_CEL + 'px';
      S.a.style.width = 4 * VP_CEL + 'px';
      S.pad.style.width = 4 * VP_CEL + 'px';
      S.a.style.transform = `translateX(${(k >= 1 ? 8 : 0) * VP_CEL}px)`;
      S.pad.style.transform = `translateX(${12 * VP_CEL}px)`;
      const tam = k === 0 ? '4' : k === 1 ? '12?' : '16';
      const ali = k === 0 ? '4' : '8';
      const tr = (v) => (k === 0 ? el('b', { class: 'ok-txt' }, 'true') : k >= 4 ? el('b', { class: 'erro-txt' }, v ? 'true' : 'false') : el('b', { class: 'apagado' }, '?'));
      raiz._fatos.replaceChildren(
        el('div', { class: 'tp-vp-tam' },
          el('div', { class: 'tp-vp-f grande' }, el('span', { class: 'mono' }, 'sizeof'), el('b', { class: 'tp-vp-num' }, tam)),
          el('div', { class: 'tp-vp-f' }, el('span', { class: 'mono' }, 'alignof'), el('b', null, ali))),
        el('div', { class: 'tp-vp-traits' },
          el('div', { class: 'tp-vp-f' }, el('span', { class: 'mono' }, 'is_trivial_v'), tr(false)),
          el('div', { class: 'tp-vp-f' }, el('span', { class: 'mono' }, 'is_standard_layout_v'), tr(false)),
          el('div', { class: 'tp-vp-f' }, el('span', { class: 'mono' }, 'is_trivially_copyable_v'), tr(false))));
      raiz._txt.innerHTML = VP_TXT[limitar(k, 0, VP_TXT.length - 1)];
    },
  });
})();
