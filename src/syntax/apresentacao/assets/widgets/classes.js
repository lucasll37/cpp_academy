/* =============================================================================
   widgets/classes.js — Capítulo 5 · Classes (demo_classes.hpp)

   classes/esqueleto    o esqueleto do topo do demo, peça por peça (clique nas linhas)
   classes/vida         três ContaBancaria: static, delegante, friend, mutable, destrutores
   classes/lista-corpo  lista de inicialização × atribuição no corpo, evento por evento
   classes/ordem        a ordem de construção é a de DECLARAÇÃO: o membro lido antes de nascer
   classes/refqual      ref-qualifiers: b.vetor() empresta, Buffer{8}.vetor() rouba
   classes/vtable       despacho dinâmico: Forma* → vptr → vtable → a função certa
   classes/hinnant      o que o compilador declara implicitamente (tabela de Hinnant)

   Todo passo(k) desenha o estado k do zero; animações só enfeitam a ida k-1 → k
   e terminam no mesmo estado final (ctx.esperar devolve false se o passo mudou).
   ============================================================================= */
(function () {
  'use strict';
  const { el, svg } = Deck.util;

  // ── utilidades ─────────────────────────────────────────────────────────────
  const codigo = (texto, op = {}) => Deck.util.codigo(texto, op);
  function focar(fig, linhas) {
    const alvo = new Set(linhas || []);
    fig.classList.toggle('focando', alvo.size > 0);
    fig.querySelectorAll('.ln').forEach((ln, j) => ln.classList.toggle('foco', alvo.has(j + 1)));
  }
  function acender(fig, marcas) {
    fig.classList.add('marcas-passo');
    const on = new Set(marcas || []);
    fig.querySelectorAll('mark.mc').forEach((m) => m.classList.toggle('on', on.has(+m.dataset.m)));
  }
  function reanimar(no, classe) { no.classList.remove(classe); void no.offsetWidth; no.classList.add(classe); }
  /** troca o texto; devolve true se mudou (para animar a mudança) */
  function trocar(no, valor) {
    const v = String(valor);
    if (no.textContent === v) return false;
    no.textContent = v;
    return true;
  }
  const cadeado = () => svg('svg', { class: 'cl-cadeado', viewBox: '0 0 24 24', 'aria-hidden': 'true' },
    svg('path', { class: 'arco-fechado', d: 'M7.5 10V7.5a4.5 4.5 0 0 1 9 0V10' }),
    svg('path', { class: 'arco-aberto', d: 'M7.5 10V7.5a4.5 4.5 0 0 1 8.6-1.9' }),
    svg('rect', { x: '5', y: '10', width: '14', height: '10', rx: '2.5' }));
  const seta = (id, cor) => svg('marker', { id, viewBox: '0 0 10 10', refX: '8.5', refY: '5', markerWidth: '7', markerHeight: '7', orient: 'auto-start-reverse' },
    svg('path', { d: 'M0 0 L10 5 L0 10 z', fill: cor }));

  // ════════════════════════════════════════════════════════════════════════
  //  classes/esqueleto
  // ════════════════════════════════════════════════════════════════════════
  const ESQ_COD = [
    'class Nome ⟦: public Base⟧ {',
    '⟦public:⟧',
    '    ⟦Nome();⟧',
    '    ⟦Nome(int x);⟧',
    '    ⟦~Nome();⟧',
    '    int  metodo() ⟦const⟧;',
    '    ⟦static⟧ int contador();',
    '    ⟦friend⟧ void espiar(const Nome&);',
    '⟦private:⟧',
    '    int membro_ ⟦= 0⟧;',
    '    ⟦inline static⟧ int total_ = 0;',
    '};',
  ].join('\n');
  const ESQ_INTRO = {
    titulo: 'Nove peças',
    sec: 'topo do demo_classes.hpp',
    texto: 'Tudo o que uma classe pode conter, num lugar só. Avance (→) ou clique numa linha para ver cada peça — e em que seção ela volta.',
  };
  const ESQ = [
    { curto: 'herança', l: [1], m: [1], sec: '5.5', titulo: 'Herança',
      texto: '<code>Nome</code> <b>é-um</b> <code>Base</code>: herda os membros dela e pode substituir os métodos <code>virtual</code>. Sem a palavra <code>public</code>, uma <code>class</code> herdaria como <b>private</b>.' },
    { curto: 'acesso', l: [2, 9], m: [2, 9], sec: '5.1', titulo: 'Rótulos de acesso',
      texto: '<code>public:</code> é a interface — qualquer código usa. <code>private:</code> só a própria classe (e seus amigos) enxerga. <code>protected:</code> abre também para as derivadas.' },
    { curto: 'construtores', l: [3, 4], m: [3, 4], sec: '5.1 · 5.2', titulo: 'Construtores', pill: ['cpp11', 'delegante: C++11'],
      texto: 'Fazem o objeto nascer <b>válido</b>. Podem ser sobrecarregados, delegar um ao outro e ser <code>explicit</code>; a lista de inicialização roda antes do corpo.' },
    { curto: 'destrutor', l: [5], m: [5], sec: '5.1 · 5.5', titulo: 'Destrutor',
      texto: 'Roda sozinho quando o objeto morre: fim do escopo, <code>delete</code>, exceção. É a metade “devolva” do RAII. Numa base polimórfica, <code>virtual</code> (ou protegido).' },
    { curto: 'método const', l: [6], m: [6], sec: '5.4', titulo: 'Método const',
      texto: 'Promete não modificar o objeto: lá dentro, <code>this</code> é <code>const Nome*</code>. É o único tipo de método não-static que se pode chamar por um <code>const Nome&amp;</code>.' },
    { curto: 'static', l: [7], m: [7], sec: '5.1', titulo: 'Membro estático',
      texto: 'Pertence à <b>classe</b>, não a cada objeto: existe um só, chamado como <code>Nome::contador()</code>. Um método static não tem <code>this</code>.' },
    { curto: 'friend', l: [8], m: [8], sec: '5.1', titulo: 'friend',
      texto: 'Uma função (ou classe) de fora que enxerga o privado. Quem concede é a classe; não é herdado nem recíproco.' },
    { curto: 'init na classe', l: [10], m: [10], sec: '5.1 · 5.2', titulo: 'Inicializador na classe', pill: ['cpp11', 'C++11'],
      texto: 'Valor padrão do membro. Se um construtor citar o membro na lista de inicialização, a lista vence.' },
    { curto: 'inline static', l: [11], m: [11], sec: '5.1', titulo: 'inline static', pill: ['cpp17', 'C++17'],
      texto: 'Estático <b>definido</b> aqui mesmo, no header. Antes do C++17 era preciso <code>int Nome::total_ = 0;</code> em uma — e só uma — .cpp.' },
  ];

  function mostrarEsq(raiz, k) {
    const p = k > 0 ? ESQ[k - 1] : null;
    const d = p || ESQ_INTRO;
    focar(raiz._fig, p ? p.l : []);
    acender(raiz._fig, p ? p.m : []);
    const F = raiz._ficha;
    F.num.textContent = p ? String(k) : '9';
    F.num.classList.toggle('intro', !p);
    F.sec.textContent = p ? 'volta em ' + d.sec : d.sec;
    F.pill.className = 'pill' + (p && p.pill ? ' ' + p.pill[0] : '');
    F.pill.textContent = p && p.pill ? p.pill[1] : '';
    F.pill.hidden = !(p && p.pill);
    F.tit.innerHTML = d.titulo;
    F.txt.innerHTML = d.texto;
    raiz._chips.forEach((c, i) => {
      c.classList.toggle('atual', i === k - 1);
      c.classList.toggle('visto', i < k - 1);
    });
    if (raiz._kw !== k) reanimar(F.caixa, 'troca');
    raiz._kw = k;
  }

  Deck.widget('classes/esqueleto', {
    iniciar(raiz) {
      raiz.classList.add('w-esq');
      const f = codigo(ESQ_COD, { titulo: 'o esqueleto — topo de demo_classes.hpp' });
      f.classList.add('esq-cod');
      f.querySelectorAll('.ln').forEach((ln, j) => {
        const i = ESQ.findIndex((p) => p.l.includes(j + 1));
        if (i < 0) return;
        ln.classList.add('esq-clicavel');
        ln.append(el('span', { class: 'esq-badge', 'aria-hidden': 'true' }, String(i + 1)));
        ln.addEventListener('click', () => mostrarEsq(raiz, i + 1));
      });
      const F = {
        num: el('span', { class: 'esq-num' }),
        sec: el('span', { class: 'esq-sec' }),
        pill: el('span', { class: 'pill' }),
        tit: el('h3', { class: 'esq-tit' }),
        txt: el('p', { class: 'esq-txt' }),
      };
      F.caixa = el('div', { class: 'esq-ficha' }, el('div', { class: 'esq-topo' }, F.num, F.sec, F.pill), F.tit, F.txt);
      const mapa = el('div', { class: 'esq-mapa' });
      raiz._chips = ESQ.map((p, i) => {
        const c = el('button', { class: 'esq-chip', type: 'button' }, el('b', null, String(i + 1)), p.curto);
        c.addEventListener('click', () => mostrarEsq(raiz, i + 1));
        mapa.append(c);
        return c;
      });
      raiz._fig = f;
      raiz._ficha = F;
      raiz.append(f, el('div', { class: 'esq-lado' }, F.caixa, mapa));
    },
    passo(raiz, k) { mostrarEsq(raiz, k); },
  });

  // ════════════════════════════════════════════════════════════════════════
  //  classes/vida — demo_anatomia(), objeto por objeto
  // ════════════════════════════════════════════════════════════════════════
  const VIDA_COD = [
    'ContaBancaria::reiniciar_numeros();',
    '{',
    '    ContaBancaria a{"ana", 100.0};',
    '    ContaBancaria b{"bruno"};',
    '    ContaBancaria padrao;',
    '    a.depositar(50.0);',
    '    bool ok = a.sacar(30.0);',
    '    auditar(a);',
    '    const ContaBancaria& somente_leitura = a;',
    '    somente_leitura.consultas();',
    '    somente_leitura.consultas();',
    '}',
  ].join('\n');
  // linhas EXATAS da saída real (saida/classes.txt)
  const VS = {
    explicito: '      construtor explícito: ContaBancaria a{"ana", 100.0}',
    padrao: '      argumento padrão:     ContaBancaria b{"bruno"} → saldo 0.00',
    delegante: '      delegante:            ContaBancaria padrao;  → titular "sem-titular"',
    abertas: '        contas abertas agora: 3',
    numeros: '        números atribuídos: a=#1 b=#2 padrao=#3',
    saldo: '        após depositar(50) e sacar(30) (ok=true) → saldo 120.00',
    auditoria: '        [auditoria] conta #1 de ana → saldo 120.00',
    consultas: '        consultas(): 2 → 3 (o objeto é const!)',
    fim: '      Ao sair do bloco os destrutores rodaram: abertas = 0',
  };
  const OBJS = ['a', 'b', 'padrao'];
  const NASC = { a: 'construtor explícito', b: 'argumento padrão', padrao: 'delegante' };
  const MORTE = { a: 3, b: 2, padrao: 1 };   // destrutores: ordem inversa da construção
  const A = (s, o, x) => Object.assign({ t: '"ana"', s, n: 1, o }, x);
  const B = (x) => Object.assign({ t: '"bruno"', s: 0, n: 2, o: 0 }, x);
  const P = (x) => Object.assign({ t: '"sem-titular"', s: 0, n: 3, o: 0 }, x);
  const MORTO = { morto: true };
  // roteiro do demo — um rótulo por estado k (0..7), mostrado embaixo da legenda
  const VIDA_ROT = ['static', 'a', 'b', 'padrao', 'invariante', 'friend', 'mutable', '}'];

  const VIDA = [
    { l: [1], prox: 0, ab: 0, objs: {}, out: [],
      leg: '<code>reiniciar_numeros()</code> é <b>static</b>: chamado pela classe, sem objeto nenhum. Os contadores são da classe.' },
    { l: [3], prox: 1, ab: 1, objs: { a: A(100, 0) }, novo: 'a', out: [VS.explicito],
      leg: 'A lista inicializa <code>titular_</code>, <code>saldo_</code> e <code>numero_ = ++proximo_numero_</code>; o corpo faz <code>++abertas_</code>.' },
    { l: [4], prox: 2, ab: 2, objs: { a: A(100, 0), b: B() }, novo: 'b', out: [VS.padrao],
      leg: 'Um argumento só: <code>saldo_inicial</code> assume o <b>padrão</b>, 0.0.' },
    { l: [5], prox: 3, ab: 3, objs: { a: A(100, 0), b: B(), padrao: P() }, novo: 'padrao', out: [VS.delegante, VS.abertas, VS.numeros],
      leg: '<b>Delegante</b>: <code>ContaBancaria()</code> repassa para <code>("sem-titular", 0.0)</code>. Quem incrementa é o alvo — <code>abertas_</code> sobe <b>uma</b> vez.' },
    { l: [6, 7], prox: 3, ab: 3, objs: { a: A(120, 1), b: B(), padrao: P() }, out: [VS.saldo],
      seq: [{ l: [6], objs: { a: A(150, 1) }, out: [] }],
      leg: 'A invariante trabalha: <code>depositar(50)</code> aceita (+1 operação); <code>sacar(30)</code> confere o saldo antes.' },
    { l: [8], prox: 3, ab: 3, objs: { a: A(120, 1, { amigo: true }), b: B(), padrao: P() }, out: [VS.auditoria],
      leg: '<b>friend</b>: <code>auditar()</code> não é membro — mas a classe lhe deu a chave do privado.' },
    { l: [9, 10, 11], prox: 3, ab: 3, objs: { a: A(120, 3, { cst: true }), b: B(), padrao: P() }, out: [VS.consultas],
      seq: [{ l: [9], objs: { a: A(120, 1, { cst: true }) }, out: [] }, { l: [10], objs: { a: A(120, 2, { cst: true }) }, out: [] }],
      leg: 'Pela <code>const&amp;</code> só passam métodos const — mas <code>operacoes_</code> é <b>mutable</b>: 1 → 2 → 3.' },
    { l: [12], prox: 3, ab: 0, objs: { a: A(120, 3, MORTO), b: B(MORTO), padrao: P(MORTO) }, out: [VS.fim],
      seq: [
        { ab: 3, objs: { a: A(120, 3), b: B(), padrao: P() }, out: [] },
        { ab: 2, objs: { a: A(120, 3), b: B(), padrao: P(MORTO) }, out: [] },
        { ab: 1, objs: { a: A(120, 3), b: B(MORTO), padrao: P(MORTO) }, out: [] },
      ],
      leg: 'Fim do bloco: destrutores na ordem <b>inversa</b> — padrao, b, a. <code>abertas_</code> volta a 0; <code>proximo_numero_</code> fica em 3.' },
  ];

  function desenharVida(raiz, E, animar) {
    focar(raiz._fig, E.l);
    const R = raiz._r;
    if (trocar(R.prox, E.prox) && animar) reanimar(R.prox, 'bump');
    if (trocar(R.ab, E.ab) && animar) reanimar(R.ab, 'bump');
    OBJS.forEach((nome) => {
      const c = R.cards[nome];
      const o = E.objs[nome] || null;
      c.root.classList.toggle('vivo', !!o && !o.morto);
      c.root.classList.toggle('morto', !!o && !!o.morto);
      c.root.classList.toggle('amigo', !!(o && o.amigo));
      c.root.classList.toggle('cst', !!(o && o.cst));
      if (o) {
        c.num.textContent = '#' + o.n;
        trocar(c.t, o.t);
        if (trocar(c.s, o.s.toFixed(2)) && animar) reanimar(c.s, 'bump');
        trocar(c.n, o.n);
        if (trocar(c.o, o.o) && animar) reanimar(c.o, 'bump');
      } else {
        c.num.textContent = '';
        [c.t, c.s, c.n, c.o].forEach((x) => { x.textContent = '—'; });
      }
      if (animar && E.novo === nome) reanimar(c.root, 'nasce');
      if (animar && o && o.morto && !c.root.classList.contains('foi')) reanimar(c.root, 'foi');
      if (!o || !o.morto) c.root.classList.remove('foi');
    });
    R.leg.innerHTML = E.leg || R.leg.innerHTML;
    // etapa intermediária (animação): a linha que o passo confirma ainda não saiu — caixa vazia, sem o aviso
    // "(nada impresso ainda)", que só vale antes da primeira conta
    const linhas = E.out.length ? E.out : E.etapa ? [] : [''];
    R.out.replaceChildren(...linhas.map((t) => el('span', { class: 'vd-ln' + (t ? '' : ' vazia') }, t || '(nada impresso ainda)')));
    if (animar && E.out.length) reanimar(R.out, 'chegou');
  }

  Deck.widget('classes/vida', {
    iniciar(raiz) {
      raiz.classList.add('w-vida');
      const f = codigo(VIDA_COD, { titulo: 'demo_anatomia()', classe: 'minimo' });
      const leg = el('div', { class: 'vd-leg-txt' });
      const R = { leg };
      R.rot = VIDA_ROT.map((t) => el('span', { class: 'vd-rot' }, t));
      const legenda = el('div', { class: 'vd-leg' }, leg, el('div', { class: 'vd-roteiro', 'aria-hidden': 'true' }, ...R.rot));
      R.prox = el('b', { class: 'vd-n' }, '0');
      R.ab = el('b', { class: 'vd-n' }, '0');
      const classe = el('div', { class: 'vd-classe' },
        el('div', { class: 'vd-classe-tit' }, el('span', null, el('span', { class: 'vd-kw' }, 'class'), ' ContaBancaria'), el('small', null, 'membros static — um só para todos os objetos')),
        el('div', { class: 'vd-conts' },
          el('div', { class: 'vd-cont' }, el('span', { class: 'vd-k' }, 'proximo_numero_'), R.prox),
          el('div', { class: 'vd-cont' }, el('span', { class: 'vd-k' }, 'abertas_'), R.ab)));
      R.cards = {};
      const objs = el('div', { class: 'vd-objs' });
      OBJS.forEach((nome) => {
        const c = {};
        const campo = (k, alvo, mut) => el('div', { class: 'vd-campo' + (mut ? ' vd-mut' : '') },
          el('span', { class: 'vd-k' }, k), alvo, mut ? el('span', { class: 'vd-tag-mut' }, 'mutable') : cadeado());
        c.num = el('span', { class: 'vd-num' });
        c.t = el('span', { class: 'vd-v' }, '—');
        c.s = el('span', { class: 'vd-v' }, '—');
        c.n = el('span', { class: 'vd-v' }, '—');
        c.o = el('span', { class: 'vd-v' }, '—');
        c.root = el('div', { class: 'vd-obj', dataset: { nome } },
          el('div', { class: 'vd-obj-topo' }, el('b', { class: 'vd-nome' }, nome), c.num),
          el('div', { class: 'vd-nasc' }, NASC[nome]),
          campo('titular_', c.t), campo('saldo_', c.s), campo('numero_', c.n), campo('operacoes_', c.o, true),
          el('span', { class: 'vd-selo vd-selo-amigo' }, 'friend auditar() lê'),
          el('span', { class: 'vd-selo vd-selo-const' }, 'visto por const&'),
          el('span', { class: 'vd-carimbo' }, el('b', null, MORTE[nome] + 'º'), '~ContaBancaria()'));
        R.cards[nome] = c;
        objs.append(c.root);
      });
      R.out = el('div', { class: 'vd-out-linhas' });
      const saida = el('div', { class: 'vd-saida' }, el('span', { class: 'vd-saida-rot' }, 'saída real · trecho'), R.out);
      raiz._fig = f;
      raiz._r = R;
      raiz.append(el('div', { class: 'vd-esq' }, f, legenda), el('div', { class: 'vd-dir' }, classe, objs, saida));
      raiz._kw = -1;
    },
    async passo(raiz, k, ctx, dir) {
      const anterior = raiz._kw;
      raiz._kw = k;
      const E = VIDA[k];
      raiz._r.rot.forEach((r, i) => { r.classList.toggle('feito', i < k); r.classList.toggle('atual', i === k); });
      if (ctx.reduzido || dir < 0 || anterior !== k - 1) { desenharVida(raiz, E, false); return; }
      const etapas = (E.seq || []).map((x) => Object.assign({}, E, { leg: E.leg, novo: null, etapa: true }, x, { objs: Object.assign({}, E.objs, x.objs) }));
      for (const etapa of etapas) {
        desenharVida(raiz, etapa, true);
        if (!(await ctx.esperar(k === 7 ? 650 : 750))) return;
      }
      desenharVida(raiz, E, true);
    },
  });

  // ════════════════════════════════════════════════════════════════════════
  //  classes/lista-corpo
  // ════════════════════════════════════════════════════════════════════════
  const LC_LEG = [
    'Duas classes, o mesmo membro <code>Rastreado</code> — que imprime cada construção, cópia e atribuição. Quantos eventos em cada uma?',
    '<b>Lista</b>: o membro <b>nasce</b> já como cópia de <code>fonte</code> — um evento.',
    '<b>Corpo</b>: antes de a <code>{</code> abrir, o membro precisa existir — nasce como <code>"vazio"</code>…',
    '…e só então o corpo atribui. O <code>"vazio"</code> foi construído para nada.',
    'No corpo você paga <b>construção + atribuição</b>. Num tipo caro — um <code>std::vector</code>, uma <code>std::string</code> longa — isso pode ser alocar e jogar fora.',
  ];

  Deck.widget('classes/lista-corpo', {
    iniciar(raiz) {
      raiz.classList.add('w-lc');
      const fonte = el('div', { class: 'lc-fonte' },
        el('span', { class: 'lc-rot' }, 'antes das duas'),
        el('code', { class: 'cpp lc-fonte-cod', html: Deck.util.realcar('Rastreado fonte{"fonte"};') }),
        el('span', { class: 'lc-seta' }),
        el('span', { class: 'lc-ev log ligado' }, 'ctor(fonte)'));
      const trilha = (tipo, titulo, sub, cod) => {
        const f = codigo(cod, { classe: 'compacto' });
        const evs = el('div', { class: 'lc-evs' });
        const mem = { val: el('span', { class: 'lc-mem-val' }, '—'), velho: el('span', { class: 'lc-mem-velho' }, '"vazio"') };
        const custo = { n: el('b', { class: 'lc-n' }, '0'), u: el('span', { class: 'lc-u' }, 'eventos') };
        const root = el('div', { class: 'lc-trilha lc-' + tipo },
          el('div', { class: 'lc-cab' }, el('span', { class: 'lc-tit mono' }, titulo), el('span', { class: 'lc-sub' }, sub)),
          el('div', { class: 'lc-linha' }, f, evs,
            el('div', { class: 'lc-mem' }, el('span', { class: 'lc-mem-rot' }, 'membro'), mem.velho, mem.val),
            el('div', { class: 'lc-custo' }, custo.n, custo.u)));
        return { root, f, evs, mem, custo };
      };
      const L = trilha('lista', ': membro(r)', 'lista de inicialização',
        'explicit UsaLista(const Rastreado& r)\n    : ⟦membro(r)⟧ {}       // UMA vez');
      const C = trilha('corpo', '{ membro = r; }', 'atribuição no corpo',
        'explicit UsaCorpo(const Rastreado& r)\n    : ⟦membro("vazio")⟧ { ⟦membro = r;⟧ }');
      const ev = (txt, cls) => el('span', { class: 'lc-ev log ' + cls }, txt);
      L.e1 = ev('CÓPIA de fonte', 'copia');
      L.evs.append(L.e1);
      C.e1 = ev('ctor(vazio)', 'ctor');
      C.e2 = ev('ATRIBUIÇÃO de fonte', 'atrib');
      C.evs.append(C.e1, el('span', { class: 'lc-mais' }, '+'), C.e2);
      raiz._L = L; raiz._C = C;
      raiz._leg = el('p', { class: 'lc-leg' });
      raiz.append(fonte, L.root, C.root, raiz._leg);
      raiz._kw = -1;
    },
    passo(raiz, k, ctx, dir) {
      const animar = !ctx.reduzido && dir > 0 && raiz._kw === k - 1;
      raiz._kw = k;
      const L = raiz._L, C = raiz._C;
      const liga = (no, on) => {
        const era = no.classList.contains('ligado');
        no.classList.toggle('ligado', on);
        if (on && !era && animar) reanimar(no, 'voa');
      };
      liga(L.e1, k >= 1);
      liga(C.e1, k >= 2);
      liga(C.e2, k >= 3);
      C.e1.classList.toggle('perdido', k >= 3);
      C.root.querySelector('.lc-mais').classList.toggle('ligado', k >= 3);
      acender(L.f, k >= 1 ? [1] : []);
      acender(C.f, k === 2 ? [1] : k >= 3 ? [2] : []);
      const mem = (m, v, velho) => {
        if (trocar(m.val, v) && animar) reanimar(m.val, 'bump');
        m.val.classList.toggle('nao-existe', v === '—');
        m.velho.classList.toggle('ligado', !!velho);
      };
      mem(L.mem, k >= 1 ? '"fonte"' : '—', false);
      mem(C.mem, k >= 3 ? '"fonte"' : k >= 2 ? '"vazio"' : '—', k >= 3);
      const custo = (c, n) => {
        if (trocar(c.n, n) && animar) reanimar(c.n, 'bump');
        c.u.textContent = n === 1 ? 'evento' : 'eventos';
      };
      custo(L.custo, k >= 1 ? 1 : 0);
      custo(C.custo, k >= 3 ? 2 : k >= 2 ? 1 : 0);
      L.root.classList.toggle('vence', k >= 4);
      C.root.classList.toggle('perde', k >= 4);
      raiz._leg.innerHTML = LC_LEG[k];
      raiz._leg.classList.toggle('final', k >= 4);
    },
  });

  // ════════════════════════════════════════════════════════════════════════
  //  classes/ordem — declaração × lista
  // ════════════════════════════════════════════════════════════════════════
  const ORD_LEG = [
    'Dois membros e uma lista. Em que ordem eles <b>nascem</b>?',
    'Você <b>escreveu</b> <code>b_</code> primeiro…',
    '…mas quem manda é a <b>declaração</b>: <code>a_</code> nasce primeiro e lê <code>b_</code> — que ainda é lixo.',
    'Só então <code>b_</code> recebe 1. Tarde demais para <code>a_</code>.',
    'Compila. Com <code>-Wall</code>, dois avisos; sem <code>-Wall</code>, silêncio. Regra: lista na ordem da declaração.',
  ];

  Deck.widget('classes/ordem', {
    iniciar(raiz) {
      raiz.classList.add('w-ord');
      const f = codigo('struct Ordem {\n    int a_;        // declarado 1º\n    int b_;        // declarado 2º\n    Ordem() : ⟦b_(1)⟧, ⟦a_(b_)⟧ {}\n};', { titulo: 'ordem.cpp' });
      const chip = (txt) => el('span', { class: 'or-chip mono' }, txt);
      const linha = (rot, a, b) => el('div', { class: 'or-linha' }, el('span', { class: 'or-rot' }, rot),
        el('span', { class: 'or-n' }, '1'), chip(a), el('span', { class: 'or-sep' }, '→'), el('span', { class: 'or-n' }, '2'), chip(b));
      raiz._escrito = linha('você escreveu', 'b_(1)', 'a_(b_)');
      raiz._real = linha('acontece', 'a_(b_)', 'b_(1)');
      raiz._leg = el('p', { class: 'or-leg' });
      const cel = (nome, end, cls) => {
        const v = el('span', { class: 'valor' }, '—');
        const root = el('div', { class: 'var or-cel ' + cls }, el('span', { class: 'nome' }, nome), v, el('span', { class: 'end' }, end));
        return { root, v };
      };
      raiz._a = cel('a_', 'this + 0', 'or-cel-a');
      raiz._b = cel('b_', 'this + 4', 'or-cel-b');
      raiz._cursor = el('div', { class: 'or-cursor' }, '▲ construindo');
      const arco = svg('svg', { class: 'or-arco', viewBox: '0 0 520 120', 'aria-hidden': 'true' },
        svg('defs', null, seta('classes-or-seta', '#f87171')),
        svg('path', { class: 'or-arco-linha', d: 'M392 112 C 360 18, 160 18, 128 104', pathLength: '1', 'marker-end': 'url(#classes-or-seta)' }));
      raiz._arcoRot = el('span', { class: 'or-arco-rot mono' }, 'a_(b_) lê b_');
      raiz._ub = el('span', { class: 'pill ub or-ub' }, 'UB');
      const mem = el('div', { class: 'or-mem' },
        el('span', { class: 'or-obj-rot' }, 'Ordem o; — os 8 bytes do objeto'),
        arco, raiz._arcoRot, raiz._cursor, raiz._a.root, raiz._b.root, raiz._ub);
      raiz._avisos = el('div', { class: 'or-avisos' },
        el('div', { class: 'cl-diag aviso' }, el('b', null, '-Wall'), el('span', { html: '‘Ordem::b_’ will be initialized after [-Wreorder]<br>&nbsp;&nbsp;‘int Ordem::a_’' })),
        el('div', { class: 'cl-diag aviso' }, el('b', null, '-Wall'), el('span', null, 'member ‘Ordem::b_’ is used uninitialized [-Wuninitialized]')),
        el('p', { class: 'or-medido', html: 'Rodei 3×: <b>-O0</b> → <span class="mono">a_=32764</span>, <span class="mono">32764</span>, <span class="mono">32766</span> (lixo) · <b>-O2</b> → <span class="mono">a_=0</span>' }));
      raiz._fig = f;
      raiz.append(
        el('div', { class: 'or-esq' }, f, el('div', { class: 'or-ordens' }, raiz._escrito, raiz._real), raiz._leg),
        el('div', { class: 'or-dir' }, mem, raiz._avisos));
      raiz._kw = -1;
    },
    passo(raiz, k, ctx, dir) {
      const animar = !ctx.reduzido && dir > 0 && raiz._kw === k - 1;
      raiz._kw = k;
      raiz.dataset.k = k;
      acender(raiz._fig, k === 1 ? [1, 2] : k === 2 ? [2] : k === 3 ? [1] : []);
      focar(raiz._fig, k === 2 ? [2, 4] : k === 3 ? [3, 4] : []);
      raiz._escrito.classList.toggle('ligado', k >= 1);
      raiz._real.classList.toggle('ligado', k >= 2);
      raiz._real.classList.toggle('fase2', k >= 3);
      const pinta = (c, estado, txt) => {
        c.root.classList.toggle('lixo', estado === 'lixo');
        c.root.classList.toggle('ativa', estado === 'ok');
        c.root.classList.toggle('vazia', estado === 'vazia');
        if (trocar(c.v, txt) && animar) reanimar(c.root, 'bump');
      };
      pinta(raiz._a, k >= 2 ? 'lixo' : 'vazia', k >= 2 ? '??' : '—');
      pinta(raiz._b, k >= 3 ? 'ok' : k >= 2 ? 'lixo' : 'vazia', k >= 3 ? '1' : k >= 2 ? '??' : '—');
      raiz._cursor.dataset.em = k === 2 ? 'a' : k === 3 ? 'b' : '';
      raiz._leg.innerHTML = ORD_LEG[k];
    },
  });

  // ════════════════════════════════════════════════════════════════════════
  //  classes/refqual — empresta × rouba
  // ════════════════════════════════════════════════════════════════════════
  const RQ_SAIDA = [
    'b.vetor()          → `const&`: devolve referência (sem cópia), size=4',
    'Buffer{8}.vetor()  → `&&`: MOVE o interior, size=8',
  ];
  const RQ_LEG_E = [
    '<code>b</code> tem nome: alguém ainda vai usá-lo depois.',
    '<code>b</code> tem nome → <b>const&amp;</b>: devolve uma referência ao vetor interno. Zero cópias.',
  ];
  const RQ_LEG_D = [
    'Um temporário: <code>Buffer{8}</code> morre no fim da expressão.',
    'Um temporário: <code>Buffer{8}</code> morre no fim da expressão.',
    'Temporário → a versão <b>&amp;&amp;</b>, que vai roubar.',
    '<code>return std::move(dados_);</code> constrói <code>roubado</code> por move: o cabeçalho (24 bytes) muda de dono; os 8 ints ficam onde estão.',
    'O temporário morre vazio. Roubar foi seguro: ninguém o usaria.',
  ];

  function vetorCab(rot) {
    const ptr = el('span', { class: 'rq-c rq-ptr' }, '●');
    const size = el('span', { class: 'rq-c' }, '0');
    const cap = el('span', { class: 'rq-c' }, '0');
    const root = el('div', { class: 'rq-vec' },
      el('span', { class: 'rq-vec-rot mono' }, rot),
      el('div', { class: 'rq-cels' },
        el('span', { class: 'rq-cel' }, el('i', null, 'ptr'), ptr),
        el('span', { class: 'rq-cel' }, el('i', null, 'size'), size),
        el('span', { class: 'rq-cel' }, el('i', null, 'cap'), cap)));
    return { root, ptr, size, cap };
  }
  function heap(valores, cls) {
    return el('div', { class: 'rq-heap ' + (cls || '') }, el('span', { class: 'rq-heap-rot' }, 'heap'),
      el('div', { class: 'mem' }, valores.map((v) => el('div', { class: 'cel' }, String(v)))));
  }

  Deck.widget('classes/refqual', {
    iniciar(raiz) {
      raiz.classList.add('w-rq');
      const cab = (cham, cat, sig, verbo) => el('div', { class: 'rq-cab' },
        el('div', { class: 'rq-cham mono' }, cham, el('span', { class: 'rq-cat' }, cat)),
        el('div', { class: 'rq-sig' }, el('code', { class: 'cpp', html: Deck.util.realcar(sig) }), el('span', { class: 'rq-verbo' }, verbo)));
      // lado esquerdo — lvalue
      const E = {};
      E.cab = cab('b.vetor()', 'lvalue · tem nome', 'const std::vector<int>& vetor() const&', 'empresta');
      E.vec = vetorCab('dados_');
      E.vec.size.textContent = '4'; E.vec.cap.textContent = '4';
      E.obj = el('div', { class: 'rq-obj rq-b' }, el('span', { class: 'rq-obj-tit' }, el('b', null, 'b'), ' · Buffer na pilha'), E.vec.root);
      E.heap = heap([10, 20, 0, 0], 'rq-h4');
      E.quem = el('div', { class: 'rq-obj rq-quem' }, el('span', { class: 'rq-obj-tit' }, 'quem chamou'),
        el('div', { class: 'rq-ref mono' }, 'const vector<int>&'), el('span', { class: 'rq-zero' }, '0 cópias'));
      E.leg = el('p', { class: 'rq-leg' });
      // lado direito — rvalue
      const D = {};
      D.cab = cab('Buffer{8}.vetor()', 'rvalue · temporário', 'std::vector<int> vetor() &&', 'rouba');
      D.vec = vetorCab('dados_');
      D.obj = el('div', { class: 'rq-obj rq-tmp' }, el('span', { class: 'rq-obj-tit' }, el('b', null, 'Buffer{8}'), ' · temporário'), D.vec.root,
        el('span', { class: 'rq-carimbo' }, '~Buffer() — fim da expressão'));
      D.heap = heap([0, 0, 0, 0, 0, 0, 0, 0], 'rq-h8');
      D.dest = vetorCab('cabeçalho · 24 bytes');
      D.quem = el('div', { class: 'rq-obj rq-roubado' }, el('span', { class: 'rq-obj-tit' }, el('b', null, 'roubado'), ' · vector<int>'), D.dest.root);
      D.leg = el('p', { class: 'rq-leg' });
      // setas (coordenadas fixas do palco 1408 × 470)
      const pE = (d, cls, extra) => svg('path', Object.assign({ d, class: cls, pathLength: '1' }, extra || {}));
      const setas = svg('svg', { class: 'rq-setas', viewBox: '0 0 1408 470', 'aria-hidden': 'true' },
        svg('defs', null, seta('classes-rq-seta', '#9fb3d9'), seta('classes-rq-seta-cor', '#fb7185')),
        pE('M 76 296 L 76 348', 'rq-s e-ptr', { 'marker-end': 'url(#classes-rq-seta)' }),
        pE('M 438 236 L 324 236', 'rq-s e-ref', { 'marker-end': 'url(#classes-rq-seta-cor)' }),
        pE('M 800 296 L 800 348', 'rq-s d-tmp', { 'marker-end': 'url(#classes-rq-seta)' }),
        pE('M 1200 296 C 1200 330, 1110 322, 1080 348', 'rq-s d-roubado', { 'marker-end': 'url(#classes-rq-seta-cor)' }));
      raiz._E = E; raiz._D = D;
      raiz._saida = el('div', { class: 'rq-saida' }, el('span', { class: 'rq-saida-rot' }, 'saída real'),
        RQ_SAIDA.map((t) => el('span', { class: 'rq-ln mono' }, t)));
      raiz.append(
        el('div', { class: 'rq-lado rq-esq' }, E.cab, E.obj, E.heap, E.quem, E.leg),
        el('div', { class: 'rq-lado rq-dir' }, D.cab, D.obj, D.heap, D.quem, D.leg),
        setas, raiz._saida);
      raiz._setas = setas;
      raiz._kw = -1;
    },
    passo(raiz, k, ctx, dir) {
      const animar = !ctx.reduzido && dir > 0 && raiz._kw === k - 1;
      raiz._kw = k;
      raiz.dataset.k = k;
      const E = raiz._E, D = raiz._D;
      E.cab.classList.toggle('ligado', k >= 1);
      D.cab.classList.toggle('ligado', k >= 2);
      raiz.querySelector('.rq-dir').classList.toggle('existe', k >= 2);
      const roubou = k >= 3;
      trocar(D.vec.ptr, roubou ? '∅' : '●'); D.vec.ptr.classList.toggle('nulo', roubou);
      trocar(D.vec.size, roubou ? '0' : '8');
      trocar(D.vec.cap, roubou ? '0' : '8');
      // `roubado` só passa a existir no `return std::move(dados_)` (elisão garantida: o retorno É roubado)
      trocar(D.dest.ptr, roubou ? '●' : '—'); D.dest.ptr.classList.toggle('nulo', !roubou);
      if (trocar(D.dest.size, roubou ? '8' : '—') && animar) reanimar(D.dest.root, 'bump');
      trocar(D.dest.cap, roubou ? '8' : '—');
      D.quem.classList.toggle('futuro', !roubou);
      D.dest.root.firstChild.textContent = roubou ? 'cabeçalho · 24 bytes' : 'ainda não existe';
      D.obj.classList.toggle('morreu', k >= 4);
      E.leg.innerHTML = RQ_LEG_E[k >= 1 ? 1 : 0];
      D.leg.innerHTML = RQ_LEG_D[k];
      const s = raiz._saida.querySelectorAll('.rq-ln');
      s[0].classList.toggle('ligado', k >= 1);
      s[1].classList.toggle('ligado', k >= 3);
      const on = (sel, v) => raiz._setas.querySelector(sel).classList.toggle('ligado', v);
      on('.e-ptr', true);
      on('.e-ref', k >= 1);
      on('.d-tmp', k >= 2 && !roubou);
      on('.d-roubado', roubou);
    },
  });

  // ════════════════════════════════════════════════════════════════════════
  //  classes/vtable — Forma* → vptr → vtable → função
  // ════════════════════════════════════════════════════════════════════════
  const VT_LEG = [
    'Dois objetos atrás de <code>Forma*</code>. Como <code>f-&gt;area()</code> acha a função certa?',
    '<code>f = formas[0]</code>: o ponteiro é <code>Forma*</code>; o objeto, um <b>Circulo</b>.',
    '<b>mov</b>: lê os 8 primeiros bytes do objeto — o <b>vptr</b> — que apontam para a vtable de Circulo.',
    '<b>jmp [rax+16]</b>: o slot de <code>area()</code>. Roda <code>Circulo::area</code>.',
    '<code>formas[1]</code>: as <b>mesmas</b> duas instruções — outro vptr, outra tabela, outra função.',
    'Custo: 8 bytes de vptr por objeto (<code>sizeof(Forma)</code> = 8, <code>Circulo</code> = 16) e uma indireção.',
  ];
  const VT_RES = ['', '', '', '        círculo → área 12.57', '        quadrado → área 9.00', ''];

  Deck.widget('classes/vtable', {
    iniciar(raiz) {
      raiz.classList.add('w-vt');
      const S = (tag, a, ...f) => svg(tag, a, ...f);
      const T = (x, y, txt, cls) => S('text', { x, y, class: cls || '' }, txt);
      const caixa = (x, y, w, h, cls) => S('rect', { x, y, width: w, height: h, rx: 12, class: cls });
      const g = (cls, ...f) => S('g', { class: cls }, ...f);
      const caminho = (d, cls) => S('path', { d, class: 'vt-s ' + cls, 'marker-end': 'url(#classes-vt-seta)' });
      const tabela = (y, nome, cls) => g('vt-tab ' + cls,
        T(730, y - 14, 'vtable de ' + nome, 'vt-tit'),
        ...[['+0', '~' + nome + '()'], ['+8', '~' + nome + '() + delete'], ['+16', nome + '::area'], ['+24', nome + '::nome']].map(([off, f], i) =>
          g('vt-slot' + (i === 2 ? ' vt-area' : ''), caixa(730, y + i * 42, 340, 38, 'vt-cx'), T(746, y + i * 42 + 25, off, 'vt-off'), T(806, y + i * 42 + 25, f, 'vt-fn'))));
      const objeto = (y, nome, campo, cls) => g('vt-obj ' + cls,
        caixa(330, y, 300, 160, 'vt-cx vt-objcx'),
        T(348, y + 30, nome + ' · heap · 16 bytes', 'vt-tit'),
        g('vt-vptr', caixa(346, y + 46, 268, 46, 'vt-cx'), T(362, y + 76, '+0', 'vt-off'), T(408, y + 76, 'vptr', 'vt-fn'), S('circle', { cx: 592, cy: y + 69, r: 7, class: 'vt-dot' })),
        g('vt-dado', caixa(346, y + 100, 268, 46, 'vt-cx'), T(362, y + 130, '+8', 'vt-off'), T(408, y + 130, campo, 'vt-fn')));
      const slot = (y, i, cls) => g('vt-slotv ' + cls, caixa(10, y, 220, 60, 'vt-cx'), T(28, y + 37, '[' + i + ']', 'vt-off'), T(72, y + 37, 'unique_ptr', 'vt-fn'), S('circle', { cx: 206, cy: y + 30, r: 7, class: 'vt-dot' }));
      const funcao = (y, nome, res, cls) => g('vt-func ' + cls, caixa(1150, y, 250, 92, 'vt-cx vt-fcx'), T(1168, y + 36, nome, 'vt-fn vt-forte'), T(1168, y + 70, res, 'vt-res'));
      const d = svg('svg', { class: 'diagrama vt-svg', viewBox: '0 0 1408 430', 'aria-hidden': 'true' },
        S('defs', null, seta('classes-vt-seta', '#fb7185')),
        T(10, 34, 'formas', 'vt-tit vt-grande'), T(10, 60, 'vector<unique_ptr<Forma>>', 'vt-sub'),
        slot(110, 0, 'c0'), slot(290, 1, 'c1'),
        objeto(62, 'Circulo', 'raio_ = 2.0', 'c0'), objeto(252, 'Quadrado', 'lado_ = 3.0', 'c1'),
        tabela(40, 'Circulo', 'c0'), tabela(252, 'Quadrado', 'c1'),
        funcao(98, 'Circulo::area()', 'π · r² = 12.57', 'c0'), funcao(310, 'Quadrado::area()', 'l² = 9.00', 'c1'),
        caminho('M 206 140 C 270 140, 268 131, 328 131', 'p0 c0'),
        caminho('M 206 320 C 270 320, 268 321, 328 321', 'p1 c1'),
        caminho('M 592 131 C 668 131, 662 59, 728 59', 'v0 c0'),
        caminho('M 592 321 C 668 321, 662 271, 728 271', 'v1 c1'),
        caminho('M 1070 143 C 1110 143, 1110 144, 1148 144', 'a0 c0'),
        caminho('M 1070 355 C 1110 355, 1110 356, 1148 356', 'a1 c1'));
      const asm = (ins, nota) => el('div', { class: 'vt-asm-ln' }, el('code', { class: 'vt-ins' }, ins), el('span', { class: 'vt-nota' }, nota));
      raiz._asm = [asm('mov rax, QWORD PTR [rdi]', 'rdi = f → rax = vptr'), asm('jmp [QWORD PTR 16[rax]]', 'slot +16 → area()')];
      raiz._leg = el('p', { class: 'vt-leg' });
      raiz._res = el('div', { class: 'vt-res-ln mono' });
      raiz._svg = d;
      raiz.append(d, el('div', { class: 'vt-baixo' },
        el('div', { class: 'vt-asm' }, el('span', { class: 'vt-asm-rot' }, 'f->area() · g++ -O2'), ...raiz._asm),
        el('div', { class: 'vt-texto' }, raiz._leg, raiz._res)));
    },
    passo(raiz, k) {
      raiz.dataset.k = k;
      const on = (sel, v) => raiz._svg.querySelectorAll(sel).forEach((n) => n.classList.toggle('ligado', v));
      const c0 = k >= 1 && k <= 3, c1 = k === 4;
      const tudo = k >= 5;
      on('.vt-slotv.c0, .p0, .vt-obj.c0', c0 || tudo);
      on('.vt-slotv.c1, .p1, .vt-obj.c1', c1 || tudo);
      on('.vt-obj.c0 .vt-vptr, .v0, .vt-tab.c0', (k >= 2 && k <= 3) || tudo);
      on('.vt-obj.c1 .vt-vptr, .v1, .vt-tab.c1', c1 || tudo);
      on('.vt-tab.c0 .vt-area, .a0, .vt-func.c0', k === 3 || tudo);
      on('.vt-tab.c1 .vt-area, .a1, .vt-func.c1', c1 || tudo);
      raiz._svg.classList.toggle('focando', k >= 1);
      raiz._asm[0].classList.toggle('ligado', k >= 2);
      raiz._asm[1].classList.toggle('ligado', k >= 3);
      raiz._leg.innerHTML = VT_LEG[k];
      raiz._res.textContent = VT_RES[k];
      raiz._res.classList.toggle('vazio', !VT_RES[k]);
    },
  });

  // ════════════════════════════════════════════════════════════════════════
  //  classes/hinnant — o que o compilador declara implicitamente
  //  [class.default.ctor] [class.copy.ctor] [class.copy.assign] [class.dtor] [depr.impldec]
  // ════════════════════════════════════════════════════════════════════════
  const HN_OPS = [
    { id: 'ctor', nome: 'um construtor qualquer', sig: 'T(int)' },
    { id: 'def', nome: 'construtor padrão', sig: 'T()' },
    { id: 'dtor', nome: 'destrutor', sig: '~T()' },
    { id: 'cc', nome: 'construtor de cópia', sig: 'T(const T&)' },
    { id: 'ca', nome: 'atribuição de cópia', sig: 'operator=(const T&)' },
    { id: 'mc', nome: 'construtor de move', sig: 'T(T&&)' },
    { id: 'ma', nome: 'atribuição de move', sig: 'operator=(T&&)' },
  ];
  const HN_ROT = { user: 'você declarou', default: 'gerado', deprecated: 'gerado · deprecated', delete: '= delete', nao: 'não declarado' };
  const HN_CEN = [
    { U: {}, tit: 'Regra do 0', txt: 'Nada declarado: o compilador gera as seis — e o move é move de verdade.' },
    { U: { dtor: 1 }, tit: 'só o destrutor', txt: 'A cópia continua (deprecated) e o move some: <code>std::move</code> passa a <b>copiar</b>. Sem erro e sem aviso — nem com <code>-Wall -Wextra</code>.' },
    { U: { cc: 1 }, tit: 'só a cópia', txt: 'O construtor de cópia também é construtor: some o padrão. E o move vira cópia.' },
    { U: { mc: 1 }, tit: 'só o move', txt: 'Declarar o move apaga a cópia (<code>= delete</code>). E sem atribuição de move, nem <code>b = std::move(a)</code> compila.' },
    { U: { dtor: 1, cc: 1, ca: 1, mc: 1, ma: 1 }, tit: 'Regra dos 5', txt: 'As cinco por sua conta. Repare: nenhum construtor padrão — declare-o também, se precisar.' },
  ];

  function hinnant(U) {
    return {
      def: U.def ? 'user' : (U.ctor || U.cc || U.mc) ? 'nao' : 'default',
      dtor: U.dtor ? 'user' : 'default',
      cc: U.cc ? 'user' : (U.mc || U.ma) ? 'delete' : (U.ca || U.dtor) ? 'deprecated' : 'default',
      ca: U.ca ? 'user' : (U.mc || U.ma) ? 'delete' : (U.cc || U.dtor) ? 'deprecated' : 'default',
      mc: U.mc ? 'user' : (U.cc || U.ca || U.ma || U.dtor) ? 'nao' : 'default',
      ma: U.ma ? 'user' : (U.cc || U.ca || U.mc || U.dtor) ? 'nao' : 'default',
    };
  }
  const existe = (s) => s === 'user' || s === 'default' || s === 'deprecated';
  function veredito(r) {
    const mov = (m, c) => (m === 'user' ? ['ok', 'o seu move'] : m === 'default' ? ['ok', 'move membro a membro']
      : existe(c) ? ['aviso', 'COPIA — o move virou cópia'] : ['erro', 'não compila']);
    return [
      ['T t;', r.def === 'nao' ? ['erro', 'não compila: sem construtor padrão'] : ['ok', r.def === 'user' ? 'o seu construtor' : 'construtor gerado']],
      ['T b = a;', r.cc === 'delete' ? ['erro', 'não compila: cópia = delete'] : r.cc === 'deprecated' ? ['aviso', 'copia — deprecated'] : ['ok', r.cc === 'user' ? 'a sua cópia' : 'cópia membro a membro']],
      ['T c = std::move(a);', mov(r.mc, r.cc)],
      ['b = std::move(a);', mov(r.ma, r.ca)],
    ];
  }
  const mesmo = (U, V) => HN_OPS.every((o) => !!U[o.id] === !!V[o.id]);

  function desenharHn(raiz, U, animar) {
    const r = hinnant(U);
    raiz._linhas.forEach((ln) => {
      const op = ln.dataset.op;
      ln.setAttribute('aria-pressed', U[op] ? 'true' : 'false');
      let s, txt;
      if (op === 'ctor') { s = U.ctor ? 'user' : 'nada'; txt = U.ctor ? HN_ROT.user : 'não é especial'; }
      else {
        s = r[op];
        txt = HN_ROT[s];
        if (s === 'nao' && (op === 'mc' || op === 'ma')) txt += existe(op === 'mc' ? r.cc : r.ca) ? ' → usa a cópia' : ' → e a cópia é delete';
      }
      if (ln.dataset.s && ln.dataset.s !== s && animar) reanimar(ln._chip, 'mudou');
      ln.dataset.s = s;
      ln._chip.textContent = txt;
    });
    veredito(r).forEach(([cod, [c, t]], i) => {
      const v = raiz._vered[i];
      if (v.dataset.c && v.dataset.c !== c && animar) reanimar(v, 'mudou');
      v.dataset.c = c;
      v._cod.textContent = cod;
      v._res.textContent = t;
    });
    const cen = HN_CEN.find((c) => mesmo(c.U, U));
    raiz._cenTit.textContent = cen ? cen.tit : 'sua combinação';
    raiz._cenTxt.innerHTML = cen ? cen.txt : resumoHn(r);
  }
  function resumoHn(r) {
    const f = [];
    if (r.def === 'nao') f.push('sem construtor padrão (há outro construtor declarado)');
    if (r.cc === 'delete' || r.ca === 'delete') f.push('a cópia virou <code>= delete</code> (há move declarado)');
    if ((r.mc === 'nao' && existe(r.cc)) || (r.ma === 'nao' && existe(r.ca))) f.push('<code>std::move</code> cai na cópia');
    if (r.cc === 'deprecated' || r.ca === 'deprecated') f.push('cópia gerada, mas deprecated');
    return f.length ? f.join(' · ') + '.' : 'Nada de surpreendente nesta combinação.';
  }

  Deck.widget('classes/hinnant', {
    iniciar(raiz) {
      raiz.classList.add('w-hn');
      raiz._U = {};
      raiz._linhas = HN_OPS.map((o) => {
        const chip = el('span', { class: 'hn-chip' });
        const ln = el('button', { class: 'hn-linha' + (o.id === 'ctor' ? ' hn-extra' : ''), type: 'button', 'aria-pressed': 'false', dataset: { op: o.id } },
          el('span', { class: 'hn-caixa', 'aria-hidden': 'true' }), el('span', { class: 'hn-nome' }, o.nome), el('code', { class: 'hn-sig' }, o.sig), chip);
        ln._chip = chip;
        ln.addEventListener('click', () => {
          raiz._U = Object.assign({}, raiz._U, { [o.id]: raiz._U[o.id] ? 0 : 1 });
          desenharHn(raiz, raiz._U, true);
        });
        return ln;
      });
      raiz._vered = [0, 1, 2, 3].map(() => {
        const v = el('div', { class: 'hn-ver' });
        v._cod = el('code', { class: 'hn-ver-cod' });
        v._res = el('span', { class: 'hn-ver-res' });
        v.append(v._cod, v._res);
        return v;
      });
      raiz._cenTit = el('span', { class: 'hn-cen-tit' });
      raiz._cenTxt = el('span', { class: 'hn-cen-txt' });
      raiz.append(
        el('div', { class: 'hn-cab' }, el('span', null, ''), el('span', null, 'você declarou… (clique)'), el('span', null, 'assinatura'), el('span', null, 'o compilador')),
        el('div', { class: 'hn-tab' }, ...raiz._linhas),
        el('div', { class: 'hn-veres' }, ...raiz._vered),
        el('p', { class: 'hn-cen' }, raiz._cenTit, raiz._cenTxt));
    },
    passo(raiz, k, ctx, dir) {
      raiz._U = Object.assign({}, HN_CEN[k].U);
      desenharHn(raiz, raiz._U, !ctx.reduzido && dir > 0);
    },
  });
})();
