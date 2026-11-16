/* =============================================================================
   deck.js — o motor da apresentação

   Sem dependências, sem módulos ES e sem fetch: tudo funciona abrindo o
   index.html direto do disco (file://), que é como o `make open` o abre.

   Modelo:
     • cada <section class="slide"> mora dentro de um <div class="capitulo">;
     • um slide tem uma lista de PASSOS — cada passo revela um .fragment ou
       avança um widget (data-passos="N"). O clicker/→ percorre os passos e,
       esgotados, vai para o próximo slide. Voltar (←) desfaz na ordem inversa;
     • widgets são registrados por nome com Deck.widget(nome, definição) e
       ligados ao HTML por data-widget="nome". O motor chama:
           iniciar(el, ctx)       uma vez, antes da primeira exibição
           entrar(el, ctx)        sempre que o slide abre
           passo(el, k, ctx, dir) quando k (0..N) muda — renderize o ESTADO k
           sair(el, ctx)          sempre que o slide fecha
     • <script type="text/x-cpp|x-shell|x-saida"> viram blocos de código ou
       terminais realçados (o conteúdo dentro de <script> não precisa de
       escape: escreva std::vector<int> à vontade).

   API pública: window.Deck (ver o fim do arquivo).
   ============================================================================= */
(function () {
  'use strict';

  const D = document;
  const LARGURA = 1600;
  const ALTURA = 900;
  const $ = (sel, raiz = D) => raiz.querySelector(sel);
  const $$ = (sel, raiz = D) => Array.from(raiz.querySelectorAll(sel));
  const limitar = (v, a, b) => Math.max(a, Math.min(b, v));

  const params = new URLSearchParams(location.search);
  const ESTATICO = params.has('estatico');
  const APRESENTADOR = params.has('apresentador');     // a janela da tecla P
  const CHAVE = 'deck:' + (D.documentElement.dataset.deck || location.pathname);

  const registro = new Map();            // nome → definição de widget
  const erros = [];
  const estado = {
    i: 0, passo: 0, slides: [], caps: [], pronto: false,
    inicioRelogio: Date.now(), irBuffer: '', irTimer: null,
    janela: null,       // a outra janela (apresentador ↔ projetor)
    remoto: false,      // true enquanto aplica um estado vindo da outra janela
  };

  window.addEventListener('error', (e) => erros.push(String(e.message || e)));
  window.addEventListener('unhandledrejection', (e) => erros.push('promise: ' + String(e.reason)));

  const reduzido = () => ESTATICO || D.documentElement.classList.contains('movimento-reduzido') ||
    (window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches);

  const armazenar = {
    ler(k, padrao) { try { const v = localStorage.getItem(k); return v == null ? padrao : JSON.parse(v); } catch { return padrao; } },
    gravar(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); } catch { /* modo privado */ } },
  };

  // ── Utilitários expostos aos widgets ─────────────────────────────────────
  function el(tag, props, ...filhos) {
    const n = D.createElement(tag);
    if (props) {
      for (const [k, v] of Object.entries(props)) {
        if (v == null || v === false) continue;
        if (k === 'class' || k === 'className') n.className = v;
        else if (k === 'html') n.innerHTML = v;
        else if (k === 'text') n.textContent = v;
        else if (k === 'style' && typeof v === 'object') {
          // Object.assign ignora variáveis CSS ('--x'): elas exigem setProperty
          for (const [p, x] of Object.entries(v)) {
            if (x == null) continue;
            if (p.startsWith('--')) n.style.setProperty(p, String(x)); else n.style[p] = x;
          }
        }
        else if (k === 'dataset') Object.assign(n.dataset, v);
        else if (k.startsWith('on') && typeof v === 'function') n.addEventListener(k.slice(2), v);
        else n.setAttribute(k, v === true ? '' : v);
      }
    }
    for (const f of filhos.flat()) if (f != null && f !== false) n.append(f.nodeType ? f : D.createTextNode(String(f)));
    return n;
  }
  const NS_SVG = 'http://www.w3.org/2000/svg';
  function svg(tag, attrs, ...filhos) {
    const n = D.createElementNS(NS_SVG, tag);
    if (attrs) for (const [k, v] of Object.entries(attrs)) {
      if (v == null || v === false) continue;
      if (k === 'text') n.textContent = v;
      else if (k === 'class') n.setAttribute('class', v);
      else n.setAttribute(k, v);
    }
    for (const f of filhos.flat()) if (f != null && f !== false) n.append(f.nodeType ? f : D.createTextNode(String(f)));
    return n;
  }
  const esperar = (ms) => new Promise((r) => setTimeout(r, reduzido() ? 0 : ms));

  function animarNumero(no, de, ate, ms = 1200, formatar = (v) => Math.round(v).toLocaleString('pt-BR')) {
    if (reduzido()) { no.textContent = formatar(ate); return Promise.resolve(); }
    return new Promise((ok) => {
      const t0 = performance.now();
      const passo = (t) => {
        const p = Math.min(1, (t - t0) / ms);
        const e = 1 - Math.pow(1 - p, 3);
        no.textContent = formatar(de + (ate - de) * e);
        if (p < 1) requestAnimationFrame(passo); else ok();
      };
      requestAnimationFrame(passo);
    });
  }

  // "1-3,5|7|9-10" → [[1,2,3,5],[7],[9,10]]
  function lerGrupos(spec) {
    return String(spec).split('|').map((g) => {
      const out = [];
      g.split(',').map((s) => s.trim()).filter(Boolean).forEach((s) => {
        const m = /^(\d+)\s*-\s*(\d+)$/.exec(s);
        if (m) { for (let k = +m[1]; k <= +m[2]; k++) out.push(k); } else if (/^\d+$/.test(s)) out.push(+s);
      });
      return out;
    });
  }

  // ── Blocos de código e terminais ─────────────────────────────────────────
  function marcaDaLinha(bruta) {
    const c = bruta.indexOf('//');
    const com = c >= 0 ? bruta.slice(c) : '';
    if (/✗/.test(com)) return ' ln-ruim';
    if (/✓/.test(com)) return ' ln-bom';
    if (/⚠/.test(com)) return ' ln-aviso';
    return '';
  }

  function blocoCodigo(texto, op = {}) {
    const lang = op.lang || 'cpp';
    const txt = Realce.normalizar(texto);
    const limpo = Realce.semMarcas(txt);              // ⟦…⟧ só existem para o realce
    const brutas = limpo.split('\n');
    const linhas = Realce.linhas(txt, lang);
    const inicio = op.inicio != null ? parseInt(op.inicio, 10) : 1;
    const numerado = op.numeros || op.inicio != null;
    const fig = el('figure', { class: ['codigo', numerado ? 'numerado' : '', op.classe || ''].join(' ').trim() });
    fig.dataset.lang = lang;
    if (op.arquivo || op.titulo) {
      const rot = { cpp: 'C++', shell: 'shell', bash: 'shell', texto: 'texto' }[lang] || lang;
      const nome = el('span', { class: 'arquivo' }, op.arquivo || op.titulo);
      if (op.arquivo && op.inicio != null) nome.append(el('b', null, ':' + inicio));
      const copiar = el('button', { class: 'copiar', type: 'button', title: 'Copiar o código', 'aria-label': 'Copiar o código' }, '⧉');
      copiar.addEventListener('click', (e) => {
        e.stopPropagation();
        const feito = (ok) => { copiar.textContent = ok ? '✓' : '✗'; setTimeout(() => (copiar.textContent = '⧉'), 1200); };
        // a API de área de transferência pode ser negada (permissão, file://):
        // cai no execCommand('copy') com um <textarea> temporário
        const alternativa = () => {
          const ta = el('textarea', { style: { position: 'fixed', left: '-9999px', top: '0' } });
          ta.value = limpo;
          D.body.append(ta);
          ta.select();
          let ok = false;
          try { ok = D.execCommand('copy'); } catch { ok = false; }
          ta.remove();
          feito(ok);
        };
        if (navigator.clipboard && navigator.clipboard.writeText) {
          navigator.clipboard.writeText(limpo).then(() => feito(true), alternativa);
        } else alternativa();
      });
      fig.append(el('figcaption', { class: 'codigo-topo' }, el('span', { class: 'dots' }, el('i'), el('i'), el('i')), nome, el('span', { class: 'lang' }, rot), copiar));
    }
    const semMarcas = op.semMarcas || lang !== 'cpp';
    const html = linhas.map((h, k) => `<span class="ln${semMarcas ? '' : marcaDaLinha(brutas[k] || '')}" data-n="${inicio + k}">${h}</span>`).join('');
    fig.append(el('pre', null, el('code', { html })));
    fig._texto = limpo;
    fig._grupos = op.linhas ? lerGrupos(op.linhas) : [];
    fig._marcas = op.marcas ? lerGrupos(op.marcas) : [];
    if (fig._marcas.length) fig.classList.add('marcas-passo');
    const passos = Math.max(fig._grupos.length, fig._marcas.length);
    if (passos) {
      fig.dataset.widget = 'deck/codigo';
      fig.dataset.passos = String(passos);
    }
    return fig;
  }

  function blocoTerminal(texto, op = {}) {
    const txt = Realce.normalizar(texto);
    const fig = el('figure', { class: ['terminal', op.classe || ''].join(' ').trim() });
    fig.append(el('figcaption', null, el('span', { class: 'dots' }, el('i'), el('i'), el('i')), el('span', { class: 'titulo' }, op.titulo || 'bash — cpp_academy')));
    const code = el('code');
    if (op.comando) {
      code.append(el('span', { class: 'linha-cmd', html: `<span class="tk-prompt">$ </span><span class="cmd-txt">${Realce.escapar(op.comando)}</span>` }));
    }
    code.insertAdjacentHTML('beforeend', txt ? Realce.saidaLinhas(txt).join('') : '');
    fig.append(el('pre', null, code));
    fig._comando = op.comando || '';
    const total = $$('.ln-saida', fig).length;
    if (op.revelar) {
      const cortes = String(op.revelar).split('|').map((s) => parseInt(s, 10)).filter((n) => n > 0 && n <= total);
      if (!cortes.length || cortes[cortes.length - 1] < total) cortes.push(total);
      fig._cortes = cortes;
      fig._inicial = parseInt(op.inicial || '0', 10) || 0;
      fig.dataset.passos = String(cortes.length);
    }
    if (op.digitar || op.revelar) { fig.dataset.widget = 'deck/terminal'; fig._digitar = !!op.digitar; }
    return fig;
  }

  function converterScripts(raiz) {
    $$('script[type^="text/x-"]', raiz).forEach((sc) => {
      const tipo = sc.type.slice(7).toLowerCase();
      const d = sc.dataset;
      const classe = [sc.className, d.classe].filter(Boolean).join(' ');
      let no;
      if (tipo === 'saida' || tipo === 'terminal') {
        no = blocoTerminal(sc.textContent, { titulo: d.titulo, comando: d.comando, classe, digitar: 'digitar' in d, revelar: d.revelar, inicial: d.inicial });
      } else {
        no = blocoCodigo(sc.textContent, {
          lang: tipo === 'shell' || tipo === 'sh' || tipo === 'bash' ? 'shell' : tipo,
          arquivo: d.arquivo, titulo: d.titulo, inicio: d.inicio, numeros: 'numeros' in d,
          linhas: d.linhas, marcas: d.marcas, classe, semMarcas: 'semMarcas' in d,
        });
      }
      for (const a of ['id', 'style']) if (sc.getAttribute(a)) no.setAttribute(a, sc.getAttribute(a));
      if (sc.classList.contains('fragment')) no.classList.add('fragment');
      if (d.ordem != null) no.dataset.ordem = d.ordem;
      sc.replaceWith(no);
    });
  }

  // ── Widgets embutidos ────────────────────────────────────────────────────
  // data-linhas="1-2|4" foca linhas; data-marcas="1|2,3" acende marcas ⟦…⟧.
  // O passo k usa o grupo k de cada lista (se uma lista for mais curta, o
  // último grupo dela continua valendo).
  registro.set('deck/codigo', {
    passo(fig, k) {
      const grupo = (lista) => (k > 0 && lista.length ? lista[Math.min(k, lista.length) - 1] : null);
      const gl = grupo(fig._grupos || []);
      const alvo = gl ? new Set(gl) : null;
      fig.classList.toggle('focando', !!alvo);
      $$('.ln', fig).forEach((ln, j) => ln.classList.toggle('foco', !!alvo && alvo.has(j + 1)));
      const acesas = new Set(grupo(fig._marcas || []) || []);
      $$('mark.mc', fig).forEach((m) => m.classList.toggle('on', acesas.has(+m.dataset.m)));
    },
  });

  registro.set('deck/terminal', {
    iniciar(fig) { fig._linhas = $$('.ln-saida', fig); fig._cmd = $('.cmd-txt', fig); },
    passo(fig, k) {
      if (!fig._cortes) return;
      const mostrar = k > 0 ? fig._cortes[k - 1] : fig._inicial;
      fig._linhas.forEach((ln, j) => ln.classList.toggle('escondida', j >= mostrar));
      fig._mostrar = mostrar;
    },
    async entrar(fig, ctx) {
      if (!fig._digitar) return;
      const linhas = fig._linhas;
      const limite = fig._cortes ? (fig._mostrar ?? fig._inicial) : linhas.length;
      if (ctx.reduzido) { linhas.forEach((ln, j) => ln.classList.toggle('escondida', j >= limite)); return; }
      linhas.forEach((ln) => ln.classList.add('escondida'));
      const cmd = fig._cmd;
      if (cmd) {
        const alvo = fig._comando;
        cmd.textContent = '';
        const cursor = el('span', { class: 'cursor' });
        cmd.after(cursor);
        if (!(await ctx.esperar(350))) { cmd.textContent = alvo; cursor.remove(); return; }
        for (let c = 1; c <= alvo.length; c++) {
          cmd.textContent = alvo.slice(0, c);
          if (!(await ctx.esperar(28 + Math.random() * 40))) { cmd.textContent = alvo; cursor.remove(); break; }
        }
        cursor.remove();
        if (!(await ctx.esperar(250))) { linhas.forEach((ln, j) => ln.classList.toggle('escondida', j >= limite)); return; }
      }
      const atraso = Math.max(12, Math.min(70, 1600 / Math.max(1, limite)));
      for (let j = 0; j < limite; j++) {
        linhas[j].classList.remove('escondida');
        if (!(await ctx.esperar(atraso))) { linhas.forEach((ln, x) => ln.classList.toggle('escondida', x >= (fig._cortes ? fig._mostrar : linhas.length))); return; }
      }
    },
    sair(fig) {
      if (fig._cmd) fig._cmd.textContent = fig._comando;
      $$('.cursor', fig).forEach((c) => c.remove());
      if (!fig._cortes) fig._linhas.forEach((ln) => ln.classList.remove('escondida'));
    },
    imprimir(fig) {
      if (fig._cmd) fig._cmd.textContent = fig._comando;
      $$('.cursor', fig).forEach((c) => c.remove());
      fig._linhas.forEach((ln) => ln.classList.remove('escondida'));
    },
  });

  registro.set('deck/quiz', {
    iniciar(q) {
      $$('ol.opcoes > li', q).forEach((li) => {
        li.setAttribute('role', 'button');
        li.tabIndex = 0;
        const escolher = () => {
          if (li.hasAttribute('data-certa')) q.classList.add('respondido');
          else { li.classList.remove('errada'); void li.offsetWidth; li.classList.add('errada'); }
        };
        li.addEventListener('click', escolher);
        li.addEventListener('keydown', (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); escolher(); } });
      });
    },
    passo(q, k) {
      q.classList.toggle('respondido', k > 0);
      if (k === 0) $$('li.errada', q).forEach((li) => li.classList.remove('errada'));
    },
  });

  registro.set('deck/contador', {
    entrar(no, ctx) {
      const ate = parseFloat(no.dataset.ate ?? no.textContent.replace(/\D/g, ''));
      const de = parseFloat(no.dataset.de ?? '0');
      const casas = parseInt(no.dataset.casas ?? '0', 10);
      animarNumero(no, de, ate, parseInt(no.dataset.ms ?? '1400', 10),
        (v) => v.toLocaleString('pt-BR', { minimumFractionDigits: casas, maximumFractionDigits: casas }));
      void ctx;
    },
    iniciar(no) { no._final = no.textContent; },
    imprimir(no) { if (no._final != null) no.textContent = no._final; },   // o PDF nunca pega a contagem no meio
  });

  registro.set('deck/digitar', {
    iniciar(no) { no._alvo = no.textContent; no.setAttribute('aria-label', no._alvo); },
    async entrar(no, ctx) {
      const alvo = no._alvo;
      if (ctx.reduzido) { no.textContent = alvo; return; }
      no.textContent = '';
      if (!(await ctx.esperar(parseInt(no.dataset.atraso ?? '300', 10)))) { no.textContent = alvo; return; }
      const v = parseInt(no.dataset.ms ?? '45', 10);
      for (let c = 1; c <= alvo.length; c++) {
        no.textContent = alvo.slice(0, c);
        if (!(await ctx.esperar(v))) { no.textContent = alvo; return; }
      }
    },
    sair(no) { no.textContent = no._alvo; },
    imprimir(no) { no.textContent = no._alvo; },
  });

  // ── Preparação ───────────────────────────────────────────────────────────
  function corDe(no) {
    const v = getComputedStyle(no).getPropertyValue('--cor').trim();
    return v || getComputedStyle(D.documentElement).getPropertyValue('--turquesa').trim() || '#40e0d0';
  }

  function calcularPassos(slide) {
    const unidades = [];
    $$('.fragment, [data-passos][data-widget]', slide).forEach((no) => {
      if (no.closest('aside.notas')) return;
      // Se o mesmo elemento é .fragment E widget com passos (um código com
      // data-linhas que também aparece por passo), primeiro ele APARECE e só
      // depois percorre os próprios passos.
      if (no.classList.contains('fragment')) {
        const o = no.dataset.ordem;
        unidades.push({ tipo: 'frag', el: no, ordem: o != null && o !== '' ? parseFloat(o) : null });
      }
      if (no.dataset.widget && no.dataset.passos) {
        const n = parseInt(no.dataset.passos, 10) || 0;
        for (let k = 1; k <= n; k++) unidades.push({ tipo: 'widget', el: no, k, ordem: null });
      }
    });
    const explicitos = unidades.some((u) => u.ordem != null);
    unidades.forEach((u, j) => { if (u.ordem == null) u.ordem = explicitos ? j : j; });
    const grupos = new Map();
    unidades.forEach((u, j) => {
      const chave = explicitos ? u.ordem : j;
      if (!grupos.has(chave)) grupos.set(chave, { frags: [], widgets: [] });
      const g = grupos.get(chave);
      if (u.tipo === 'frag') g.frags.push(u.el); else g.widgets.push({ el: u.el, k: u.k });
    });
    return [...grupos.entries()].sort((a, b) => a[0] - b[0]).map((e) => e[1]);
  }

  function tituloDe(slide) {
    if (slide.dataset.titulo) return slide.dataset.titulo;
    const h = $('h1, h2', slide);
    if (h) return h.textContent.replace(/\s+/g, ' ').trim();
    const k = $('.kicker', slide);
    return k ? k.textContent.replace(/\s+/g, ' ').trim() : 'Slide';
  }

  function preparar() {
    const palco = $('#palco');
    converterScripts(palco);

    // índices para animações em cascata
    $$('.entrada, .fragment.cascata, .divisor .topicos, [data-cascata]', palco).forEach((c) => {
      Array.from(c.children).forEach((f, j) => f.style.setProperty('--i', j));
    });
    // quizzes e cartas
    $$('.quiz', palco).forEach((q) => { if (!q.dataset.widget) q.dataset.widget = 'deck/quiz'; if (!q.dataset.passos) q.dataset.passos = '1'; });
    $$('.virar', palco).forEach((v) => v.addEventListener('click', (e) => {
      if (e.target.closest('button, a, input, select, textarea')) return;
      v.classList.toggle('virado');
    }));

    const slides = $$('.slide', palco);
    const caps = [];
    const porNo = new Map();
    slides.forEach((s, i) => {
      const capEl = s.closest('.capitulo');
      let cap = porNo.get(capEl || null);
      if (!cap) {
        cap = {
          el: capEl,
          id: capEl?.dataset.cap || capEl?.id?.replace(/^cap-/, '') || 'slides',
          numero: capEl?.dataset.numero ?? '',
          titulo: capEl?.dataset.titulo || '',
          cor: corDe(capEl || D.documentElement),
          slides: [],
        };
        cap.rotulo = cap.numero !== '' ? `Cap. ${cap.numero} · ${cap.titulo}` : cap.titulo;
        porNo.set(capEl || null, cap);
        caps.push(cap);
      }
      s.dataset.indice = String(i);
      s._cap = cap;
      s._local = cap.slides.length;
      cap.slides.push(s);
      s._titulo = tituloDe(s);
      s._passos = calcularPassos(s);
      s._widgets = $$('[data-widget]', s);
      s._comPassos = s._widgets.filter((w) => w.dataset.passos);
      const notas = $('aside.notas', s);
      s._notas = notas ? notas.innerHTML : '';
      s._busca = (s._titulo + ' ' + s.textContent).toLowerCase();
      s.setAttribute('aria-roledescription', 'slide');
      s.setAttribute('aria-label', `${i + 1}: ${s._titulo}`);
    });
    estado.slides = slides;
    estado.caps = caps;
  }

  // ── Ciclo de vida dos widgets ────────────────────────────────────────────
  function seguro(w, fase, ...args) {
    const def = w.__deckDef;
    if (!def || typeof def[fase] !== 'function') return;
    try {
      const r = def[fase](w, ...args);
      if (r && typeof r.catch === 'function') r.catch((e) => falha(w, fase, e));
    } catch (e) { falha(w, fase, e); }
  }
  function falha(w, fase, e) {
    erros.push(`widget ${w.dataset.widget} (${fase}): ${e && e.message}`);
    console.error(`[deck] widget "${w.dataset.widget}" falhou em ${fase}:`, e);
  }

  function criarCtx(w, slide) {
    let geracao = 0;
    const timers = new Set();
    const ctx = {
      el: w, slide, deck: API, dados: w.dataset, passo: 0,
      get reduzido() { return reduzido(); },
      get ativo() { return slide.classList.contains('ativo'); },
      /** espera ms; devolve false se o slide saiu ou um novo passo/entrada chegou (cancele a animação) */
      esperar(ms) {
        const g = geracao;
        return new Promise((ok) => {
          const t = setTimeout(() => { timers.delete(t); ok(g === geracao && slide.classList.contains('ativo')); }, reduzido() ? 0 : ms);
          timers.add(t);
        });
      },
      intervalo(fn, ms) { const t = setInterval(fn, ms); timers.add(t); return t; },
      _cancelar() { geracao++; timers.forEach((t) => { clearTimeout(t); clearInterval(t); }); timers.clear(); },
    };
    return ctx;
  }

  function iniciarWidgets(slide) {
    for (const w of slide._widgets) {
      if (w.__deckIniciado) continue;
      w.__deckIniciado = true;
      w.__deckK = -1;
      const def = registro.get(w.dataset.widget);
      if (!def) {
        erros.push('widget não registrado: ' + w.dataset.widget);
        console.error('[deck] widget não registrado:', w.dataset.widget);
        continue;
      }
      w.__deckDef = def;
      w.__deckCtx = criarCtx(w, slide);
      seguro(w, 'iniciar', w.__deckCtx);
    }
  }

  function aplicarPasso(slide, n, dir, forcar) {
    const P = slide._passos;
    P.forEach((p, j) => {
      const vis = j < n;
      p.frags.forEach((f) => { f.classList.toggle('visivel', vis); f.classList.toggle('atual', j === n - 1); });
    });
    if (!slide._comPassos.length) return;
    const k = new Map(slide._comPassos.map((w) => [w, 0]));
    P.forEach((p, j) => { if (j < n) p.widgets.forEach((u) => k.set(u.el, Math.max(k.get(u.el), u.k))); });
    k.forEach((kk, w) => {
      if (!w.__deckDef || (!forcar && w.__deckK === kk)) return;
      w.__deckK = kk;
      w.__deckCtx.passo = kk;
      w.__deckCtx._cancelar();
      seguro(w, 'passo', kk, w.__deckCtx, dir);
    });
  }

  function entrarSlide(slide, dir) {
    D.documentElement.style.setProperty('--cor', slide._cap.cor);
    for (const w of slide._widgets) {
      if (!w.__deckDef) continue;
      w.__deckCtx._cancelar();
      seguro(w, 'entrar', w.__deckCtx, dir);
    }
    slide.dispatchEvent(new CustomEvent('deck:entrou', { bubbles: true, detail: { indice: +slide.dataset.indice, dir } }));
  }

  function sairSlide(slide) {
    for (const w of slide._widgets) {
      if (!w.__deckDef) continue;
      w.__deckCtx._cancelar();
      seguro(w, 'sair', w.__deckCtx);
    }
    if (slide.contains(D.activeElement)) D.activeElement.blur();
    slide.dispatchEvent(new CustomEvent('deck:saiu', { bubbles: true }));
  }

  // ── Navegação ────────────────────────────────────────────────────────────
  let timerPerto = null;
  function marcarPerto(i, extra) {
    const S = estado.slides;
    S.forEach((s, j) => s.classList.toggle('perto', Math.abs(j - i) <= 1 || j === extra));
    clearTimeout(timerPerto);
    if (extra != null) timerPerto = setTimeout(() => S.forEach((s, j) => s.classList.toggle('perto', Math.abs(j - estado.i) <= 1)), 950);
  }

  function ir(i, passo = 0, dir) {
    const S = estado.slides;
    if (!S.length) return;
    i = limitar(i | 0, 0, S.length - 1);
    const ant = estado.i;
    const mudou = i !== ant || !estado.pronto;
    if (dir == null) dir = i >= ant ? 1 : -1;
    const novo = S[i];
    if (mudou) {
      if (estado.pronto && S[ant]) sairSlide(S[ant]);
      S.forEach((s, j) => s.classList.toggle('passado', j < i));
      marcarPerto(i, estado.pronto ? ant : null);
      iniciarWidgets(novo);
      if (S[i + 1]) iniciarWidgets(S[i + 1]);
      void novo.offsetWidth;                         // força o layout: a transição precisa de um ponto de partida
      if (estado.pronto && S[ant]) S[ant].classList.remove('ativo');
      novo.classList.add('ativo');
      estado.i = i;
    }
    const total = novo._passos.length;
    const n = passo === 'fim' ? total : limitar(passo | 0, 0, total);
    estado.passo = n;
    aplicarPasso(novo, n, dir, mudou);
    if (mudou) entrarSlide(novo, dir);
    estado.pronto = true;
    atualizarUI();
    persistir();
  }

  function proximo() {
    const s = estado.slides[estado.i];
    if (estado.passo < s._passos.length) {
      estado.passo++;
      aplicarPasso(s, estado.passo, 1, false);
      atualizarUI(); persistir();
    } else if (estado.i < estado.slides.length - 1) ir(estado.i + 1, 0, 1);
  }
  function anterior() {
    const s = estado.slides[estado.i];
    if (estado.passo > 0) {
      estado.passo--;
      aplicarPasso(s, estado.passo, -1, false);
      atualizarUI(); persistir();
    } else if (estado.i > 0) ir(estado.i - 1, 'fim', -1);
  }

  // ── Hash e posição salva ─────────────────────────────────────────────────
  function hashDe(i) {
    const s = estado.slides[i];
    return s._cap.id !== 'slides' ? `#/${s._cap.id}/${s._local + 1}` : `#/${i + 1}`;
  }
  function lerHash() {
    const h = decodeURIComponent(location.hash || '').replace(/^#\/?/, '');
    if (!h) return null;
    const partes = h.split('/').filter(Boolean);
    if (partes.length === 1 && /^\d+$/.test(partes[0])) return limitar(parseInt(partes[0], 10) - 1, 0, estado.slides.length - 1);
    const cap = estado.caps.find((c) => c.id === partes[0]);
    if (cap) {
      const n = partes[1] ? limitar(parseInt(partes[1], 10) - 1, 0, cap.slides.length - 1) : 0;
      return +cap.slides[n].dataset.indice;
    }
    const alvo = D.getElementById(partes[0]);
    const sl = alvo && alvo.closest('.slide');
    return sl ? +sl.dataset.indice : null;
  }
  let ultimoHash = '';
  function persistir() {
    const h = hashDe(estado.i);
    if (h !== ultimoHash) {
      ultimoHash = h;
      try { history.replaceState(null, '', h); } catch { location.hash = h; }
    }
    armazenar.gravar(CHAVE + ':pos', { i: estado.i });
    sincronizar();
  }

  // ── Janela do apresentador (tecla P) ─────────────────────────────────────
  // A janela principal vai para o projetor; a do apresentador fica na tela do
  // professor, com o slide atual, a prévia do próximo, as notas e o cronômetro.
  // As duas andam juntas por postMessage — funciona inclusive em file://.
  function enviar(alvo, tipo) {
    try {
      if (alvo && !alvo.closed) alvo.postMessage({ deck: CHAVE, tipo, i: estado.i, passo: estado.passo }, '*');
    } catch { /* a outra janela fechou */ }
  }
  function sincronizar() {
    if (estado.remoto) return;                     // não devolve o eco do que veio de fora
    if (APRESENTADOR) enviar(window.opener, 'ir');
    else enviar(estado.janela, 'estado');
  }
  function aoReceber(e) {
    const m = e.data;
    if (!m || typeof m !== 'object' || m.deck !== CHAVE) return;
    if (!APRESENTADOR && e.source) estado.janela = e.source;   // o apresentador se anunciou
    if (m.tipo === 'ola') { enviar(e.source, 'estado'); return; }
    if (m.tipo !== 'ir' && m.tipo !== 'estado') return;
    if (!estado.slides[m.i] || (m.i === estado.i && m.passo === estado.passo)) return;
    const dir = m.i > estado.i || (m.i === estado.i && m.passo > estado.passo) ? 1 : -1;
    estado.remoto = true;
    try { ir(m.i, m.passo, dir); } finally { estado.remoto = false; }
  }
  function abrirApresentador() {
    if (APRESENTADOR) return;
    const base = location.href.split('#')[0].split('?')[0];
    const w = window.open(base + '?apresentador' + (location.hash || ''), 'deck-apresentador', 'popup,width=1360,height=820');
    if (!w) {
      toast('O navegador bloqueou a janela do apresentador — permita pop-ups para este arquivo e tecle <kbd>P</kbd> de novo.');
      return;
    }
    estado.janela = w;
    try { w.focus(); } catch { /* ok */ }
  }

  function montarApresentador() {
    D.documentElement.classList.add('modo-apresentador');
    ui.apCap = el('span', { class: 'cap' });
    ui.apTit = el('span', { class: 'tit' });
    ui.apCont = el('span', { class: 'contador' });
    ui.apPassos = el('span', { class: 'passos' });
    ui.apTopo = el('header', { class: 'ap-topo' }, el('span', { class: 'ponto' }), ui.apCap,
      el('span', { class: 'sep' }, '›'), ui.apTit, el('span', { class: 'esp' }), ui.apPassos, ui.apCont);
    ui.apPrevia = el('div', { class: 'ap-previa' });
    ui.apRestam = el('div', { class: 'ap-restam' });
    ui.apNotas = el('div', { class: 'ap-notas' });
    ui.apRelogio = el('div', { class: 'ap-relogio', title: 'Clique para zerar' }, '00:00');
    ui.apRelogio.addEventListener('click', () => { estado.inicioRelogio = Date.now(); tique(); });
    ui.apHora = el('div', { class: 'ap-hora' });
    const botao = (txt, fn, titulo) => {
      const b = el('button', { class: 'botao', type: 'button', title: titulo, 'aria-label': titulo }, txt);
      b.addEventListener('click', () => { fn(); b.blur(); });
      return b;
    };
    ui.apLado = el('aside', { class: 'ap-lado', 'aria-label': 'Painel do apresentador' },
      el('div', { class: 'ap-rotulo' }, 'Próximo'), ui.apPrevia, ui.apRestam,
      el('div', { class: 'ap-rotulo' }, 'Notas'), ui.apNotas,
      el('div', { class: 'ap-rodape' },
        el('div', null, el('div', { class: 'ap-rotulo' }, 'Tempo de aula'), ui.apRelogio),
        el('div', null, el('div', { class: 'ap-rotulo' }, 'Agora'), ui.apHora),
        el('div', { class: 'botoes' }, botao('◀', anterior, 'Anterior (←)'), botao('▶', proximo, 'Próximo (→)'))));
    D.body.append(ui.apTopo, ui.apLado);
    D.title = 'Apresentador · ' + D.title;
  }

  function previaDoProximo() {
    const prox = estado.slides[estado.i + 1];
    if (ui.apPreviaDe === prox && ui.apPrevia.firstChild) return;
    ui.apPreviaDe = prox;
    ui.apPrevia.replaceChildren();
    if (!prox) { ui.apPrevia.append(el('div', { class: 'ap-fim' }, 'fim da apresentação')); return; }
    const copia = prox.cloneNode(true);
    copia.querySelectorAll('aside.notas, script').forEach((n) => n.remove());
    copia.querySelectorAll('[id]').forEach((n) => n.removeAttribute('id'));
    copia.querySelectorAll('.fragment').forEach((f) => f.classList.add('visivel'));
    copia.classList.remove('passado', 'perto');
    copia.classList.add('ativo');
    copia.removeAttribute('aria-label');
    // o CSS dos widgets é escopado por #cap-<slug>: a cópia precisa do mesmo invólucro
    const cap = el('div', { class: 'capitulo', 'data-cap': prox._cap.id }, copia);
    if (prox._cap.el && prox._cap.el.id) cap.id = prox._cap.el.id;
    const mini = el('div', { class: 'ap-mini', 'aria-hidden': 'true', inert: true }, cap);
    ui.apPrevia.append(mini);
    const escalar = () => mini.style.setProperty('--escala-previa', String((ui.apPrevia.clientWidth || 400) / LARGURA));
    escalar();
    requestAnimationFrame(escalar);
  }

  function atualizarApresentador() {
    const S = estado.slides;
    const s = S[estado.i];
    ui.apCap.textContent = s._cap.rotulo || '';
    ui.apTit.textContent = s._titulo;
    ui.apCont.textContent = `${estado.i + 1} / ${S.length}`;
    const total = s._passos.length;
    ui.apPassos.replaceChildren(...Array.from({ length: Math.min(total, 30) }, (_, j) => el('i', { class: j < estado.passo ? 'on' : null })));
    const restam = total - estado.passo;
    ui.apRestam.textContent = restam > 0
      ? `faltam ${restam} ${restam === 1 ? 'passo' : 'passos'} neste slide`
      : (S[estado.i + 1] ? '→ o próximo toque muda de slide' : 'último slide');
    ui.apRestam.classList.toggle('vira', restam === 0);
    if (ui.apNotasDe !== s) {
      ui.apNotasDe = s;
      ui.apNotas.innerHTML = s._notas || '<p class="ap-sem">Sem notas para este slide.</p>';
      ui.apNotas.scrollTop = 0;
    }
    previaDoProximo();
  }

  // ── Interface ────────────────────────────────────────────────────────────
  const ICONES = {
    ant: '<path d="M15 18l-6-6 6-6"/>',
    prox: '<path d="M9 18l6-6-6-6"/>',
    sumario: '<rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/>',
    tema: '<path d="M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z"/>',
    tela: '<path d="M8 3H5a2 2 0 0 0-2 2v3M21 8V5a2 2 0 0 0-2-2h-3M3 16v3a2 2 0 0 0 2 2h3M16 21h3a2 2 0 0 0 2-2v-3"/>',
    notas: '<path d="M4 4h16v12H8l-4 4z"/><path d="M8 9h8M8 12h5"/>',
    ajuda: '<circle cx="12" cy="12" r="9"/><path d="M9.5 9a2.5 2.5 0 1 1 3.5 2.3c-.7.3-1 .9-1 1.7M12 17h.01"/>',
    apresentador: '<rect x="2" y="4" width="20" height="13" rx="2"/><path d="M8 21h8M12 17v4"/>',
  };
  const icone = (n) => `<svg viewBox="0 0 24 24" aria-hidden="true">${ICONES[n]}</svg>`;
  const ui = {};

  function montarUI() {
    const corpo = D.body;
    // progresso segmentado por capítulo
    ui.progresso = el('nav', { class: 'ui-progresso', 'aria-label': 'Capítulos' });
    estado.caps.forEach((c) => {
      const seg = el('div', { class: 'seg', 'data-rotulo': c.rotulo || c.id, title: c.rotulo });
      seg.style.setProperty('--n', c.slides.length);
      seg.style.setProperty('--c', c.cor);
      seg.append(el('i'));
      seg.addEventListener('click', () => ir(+c.slides[0].dataset.indice, 0));
      c.seg = seg;
      ui.progresso.append(seg);
    });

    // rodapé
    const btn = (acao, nome, titulo) => el('button', { class: 'ui-btn', type: 'button', 'data-acao': acao, title: titulo, 'aria-label': titulo, html: icone(nome) });
    ui.cap = el('span', { class: 'cap' });
    ui.tit = el('span', { class: 'tit' });
    ui.contador = el('span', { class: 'contador' });
    ui.passos = el('span', { class: 'passos', 'aria-hidden': 'true' });
    ui.rodape = el('footer', { class: 'ui-rodape' },
      el('div', { class: 'onde' }, el('span', { class: 'ponto' }), ui.cap, el('span', { class: 'sep' }, '›'), ui.tit),
      el('div', { class: 'acoes' },
        ui.passos,
        btn('anterior', 'ant', 'Anterior (←)'), ui.contador, btn('proximo', 'prox', 'Próximo (→ / espaço)'),
        btn('sumario', 'sumario', 'Sumário (O)'), btn('notas', 'notas', 'Notas do apresentador (N)'),
        btn('apresentador', 'apresentador', 'Janela do apresentador, para a 2ª tela (P)'),
        btn('tema', 'tema', 'Tema claro/escuro (T)'), btn('tela', 'tela', 'Tela cheia (F)'), btn('ajuda', 'ajuda', 'Atalhos (?)')));
    ui.rodape.addEventListener('click', (e) => {
      const b = e.target.closest('[data-acao]');
      if (!b) return;
      const a = b.dataset.acao;
      ({ anterior, proximo, sumario: () => alternar('sumario'), notas: () => alternar('notas'), apresentador: abrirApresentador, tema: alternarTema, tela: telaCheia, ajuda: () => alternar('ajuda') })[a]?.();
      b.blur();
    });

    // ajuda
    const atalhos = [
      ['Avançar (passo ou slide)', ['→', '↓', 'espaço', 'PgDn']],
      ['Voltar', ['←', '↑', 'PgUp']],
      ['Pular slide inteiro', ['⇧ →', '⇧ ←']],
      ['Primeiro / último', ['Home', 'End']],
      ['Ir para o slide N', ['N', 'Enter']],
      ['Sumário e busca', ['O', 'Esc']],
      ['Notas e cronômetro (nesta tela)', ['N']],
      ['Janela do apresentador (2ª tela)', ['P']],
      ['Tema claro/escuro', ['T']],
      ['Tela cheia', ['F']],
      ['Ponteiro laser', ['L']],
      ['Tela preta (pausa)', ['B', '.']],
      ['Reiniciar os passos do slide', ['R']],
      ['Imprimir / PDF (todos os passos)', ['Ctrl', 'P']],
      ['Esta ajuda', ['?']],
    ];
    ui.ajuda = el('div', { class: 'ui-overlay ui-ajuda', role: 'dialog', 'aria-label': 'Atalhos de teclado' },
      el('div', { class: 'ui-painel' }, el('h2', null, 'Atalhos de teclado'),
        el('div', { class: 'ui-ajuda-grade' }, atalhos.map(([o, ks]) => el('div', null, el('span', null, o), el('span', null, ks.map((k) => el('kbd', null, k)))))),
        el('p', { class: 'mini', style: { marginTop: '18px', color: 'var(--texto-3)', fontSize: '14px' } },
          'Clique nos widgets para explorar livremente; o clicker (→) percorre os mesmos passos na ordem da aula.')));
    ui.ajuda.addEventListener('click', (e) => { if (e.target === ui.ajuda) alternar('ajuda', false); });

    // sumário
    ui.busca = el('input', { type: 'search', placeholder: 'Buscar em todos os slides (título e conteúdo)…', 'aria-label': 'Buscar slides' });
    ui.grade = el('div', { class: 'grade' });
    estado.caps.forEach((c) => {
      const bloco = el('section', { class: 'bloco' });
      bloco.style.setProperty('--c', c.cor);
      bloco.append(el('h3', null, el('span', null, c.numero !== '' ? String(c.numero).padStart(2, '0') : '·'), c.titulo || c.id));
      c.slides.forEach((s) => {
        const i = +s.dataset.indice;
        const it = el('button', { class: 'item', type: 'button' }, el('b', null, String(i + 1)), s._titulo);
        it.addEventListener('click', () => { alternar('sumario', false); ir(i, 0); });
        s._item = it;
        bloco.append(it);
      });
      c.bloco = bloco;
      ui.grade.append(bloco);
    });
    ui.sumario = el('div', { class: 'ui-overlay ui-sumario', role: 'dialog', 'aria-label': 'Sumário' },
      el('div', { class: 'ui-painel' }, el('div', { class: 'busca' }, el('h2', { style: { margin: 0 } }, 'Sumário'), ui.busca), ui.grade));
    ui.sumario.addEventListener('click', (e) => { if (e.target === ui.sumario) alternar('sumario', false); });
    ui.busca.addEventListener('input', () => {
      const q = ui.busca.value.trim().toLowerCase();
      estado.caps.forEach((c) => {
        let algum = false;
        c.slides.forEach((s) => { const ok = !q || s._busca.includes(q); s._item.classList.toggle('escondido', !ok); algum = algum || ok; });
        c.bloco.classList.toggle('escondido', !algum);
      });
    });
    ui.busca.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        const prim = estado.slides.find((s) => !s._item.classList.contains('escondido'));
        if (prim) { alternar('sumario', false); ir(+prim.dataset.indice, 0); }
      }
    });

    // notas
    ui.notasTxt = el('div', { class: 'txt' });
    ui.relogio = el('div', { class: 'relogio', title: 'Clique para zerar' }, '00:00');
    ui.relogio.addEventListener('click', () => { estado.inicioRelogio = Date.now(); tique(); });
    ui.prox = el('div', { class: 'prox' });
    ui.notas = el('aside', { class: 'ui-notas', 'aria-label': 'Notas do apresentador' }, ui.notasTxt,
      el('div', { class: 'lado' }, el('div', null, 'tempo de aula'), ui.relogio, ui.prox));
    setInterval(tique, 1000);

    ui.toast = el('div', { class: 'ui-toast', role: 'status' });
    ui.laser = el('div', { class: 'ui-laser' });
    ui.cortina = el('div', { class: 'ui-cortina' }, 'pausa — B para voltar');
    ui.cortina.addEventListener('click', () => alternar('cortina', false));
    ui.irpara = el('div', { class: 'ui-irpara' });
    ui.vivo = el('div', { class: 'sr-only', 'aria-live': 'polite' });

    corpo.append(ui.progresso, ui.rodape, ui.ajuda, ui.sumario, ui.notas, ui.toast, ui.laser, ui.cortina, ui.irpara, ui.vivo);
    if (APRESENTADOR) montarApresentador();
  }

  function tique() {
    const s = Math.floor((Date.now() - estado.inicioRelogio) / 1000);
    const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), ss = s % 60;
    const txt = (h ? h + ':' + String(m).padStart(2, '0') : String(m).padStart(2, '0')) + ':' + String(ss).padStart(2, '0');
    ui.relogio.textContent = txt;
    if (ui.apRelogio) {
      ui.apRelogio.textContent = txt;
      ui.apHora.textContent = new Date().toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' });
    }
  }

  function atualizarUI() {
    const S = estado.slides;
    const s = S[estado.i];
    const cap = s._cap;
    ui.cap.textContent = cap.rotulo || '';
    ui.tit.textContent = s._titulo;
    ui.contador.textContent = `${estado.i + 1} / ${S.length}`;
    const total = s._passos.length;
    if (ui._totalPassos !== total) {
      ui.passos.replaceChildren(...Array.from({ length: Math.min(total, 24) }, () => el('i')));
      ui._totalPassos = total;
    }
    $$('i', ui.passos).forEach((b, j) => b.classList.toggle('on', j < estado.passo));
    const iCap = estado.caps.indexOf(cap);
    estado.caps.forEach((c, j) => {
      const p = j < iCap ? 100 : j > iCap ? 0 : ((s._local + 1) / c.slides.length) * 100;
      c.seg.style.setProperty('--p', p + '%');
    });
    S.forEach((x) => x._item && x._item.classList.toggle('atual', x === s));
    ui.notasTxt.innerHTML = s._notas;
    const prox = S[estado.i + 1];
    ui.prox.innerHTML = prox ? `próximo slide <b>${Realce.escapar(prox._titulo)}</b>` : '<b>fim da apresentação</b>';
    if (ui._vivo !== estado.i) { ui.vivo.textContent = `${estado.i + 1}: ${s._titulo}`; ui._vivo = estado.i; }
    if (APRESENTADOR) atualizarApresentador();
  }

  const abertos = { ajuda: false, sumario: false, notas: false, cortina: false };
  function alternar(qual, forcar) {
    const novo = forcar == null ? !abertos[qual] : forcar;
    if (novo && (qual === 'ajuda' || qual === 'sumario')) {
      for (const o of ['ajuda', 'sumario']) if (o !== qual && abertos[o]) alternar(o, false);
    }
    abertos[qual] = novo;
    ui[qual].classList.toggle('aberto', novo);
    if (qual === 'sumario' && novo) {
      setTimeout(() => { ui.busca.focus(); estado.slides[estado.i]._item?.scrollIntoView({ block: 'center' }); }, 60);
    }
    if (qual === 'sumario' && !novo) ui.busca.blur();
  }

  function alternarTema() {
    const atual = D.documentElement.dataset.tema === 'claro' ? 'escuro' : 'claro';
    D.documentElement.dataset.tema = atual;
    armazenar.gravar('deck:tema', atual);
    // as cores dos capítulos dependem do tema só via color-mix; recalcula a raiz
    D.documentElement.style.setProperty('--cor', estado.slides[estado.i]._cap.cor);
  }

  function telaCheia() {
    if (!D.fullscreenElement) D.documentElement.requestFullscreen?.().catch(() => {});
    else D.exitFullscreen?.();
  }

  let timerToast = null;
  function toast(html, botoes = [], ms = 7000) {
    ui.toast.replaceChildren(el('span', { html }), ...botoes.map(([rot, fn, sec]) => {
      const b = el('button', { type: 'button', class: sec ? 'sec' : '' }, rot);
      b.addEventListener('click', () => { fn(); ui.toast.classList.remove('aberto'); });
      return b;
    }));
    ui.toast.classList.add('aberto');
    clearTimeout(timerToast);
    timerToast = setTimeout(() => ui.toast.classList.remove('aberto'), ms);
  }

  function ajustarEscala() {
    if (APRESENTADOR) {
      // o slide atual ocupa a área à esquerda do painel; a escala vem da caixa
      const lado = 440, margem = 20, topo = 60;
      const W = window.innerWidth - lado - margem * 2;
      const H = window.innerHeight - topo - margem;
      const w = Math.max(240, Math.min(W, (H * LARGURA) / ALTURA));
      const deck = $('.deck');
      Object.assign(deck.style, { left: margem + 'px', top: topo + 'px', width: w + 'px', height: (w * ALTURA) / LARGURA + 'px' });
      D.documentElement.style.setProperty('--escala', String(w / LARGURA));
      if (ui.apPrevia) { ui.apPreviaDe = undefined; previaDoProximo(); }
      return;
    }
    const e = Math.min(window.innerWidth / LARGURA, window.innerHeight / ALTURA);
    D.documentElement.style.setProperty('--escala', String(e));
  }

  // ── Entrada: teclado, toque, mouse ───────────────────────────────────────
  function mostrarIrPara() {
    ui.irpara.innerHTML = `${estado.irBuffer}<small>Enter para ir · Esc cancela</small>`;
    ui.irpara.classList.add('aberto');
    clearTimeout(estado.irTimer);
    estado.irTimer = setTimeout(cancelarIrPara, 2500);
  }
  function cancelarIrPara() { estado.irBuffer = ''; ui.irpara.classList.remove('aberto'); }

  function teclado(e) {
    const t = e.target;
    if (e.defaultPrevented) return;
    if (t && (t.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(t.tagName))) {
      if (e.key === 'Escape') { t.blur(); if (abertos.sumario) alternar('sumario', false); return; }
      // O clicker manda PageDown/PageUp: ele tem de funcionar mesmo que o foco
      // tenha ficado num campo de um widget (a busca do sumário fica de fora).
      if ((e.key === 'PageDown' || e.key === 'PageUp') && !t.closest('.ui-sumario') && !e.ctrlKey && !e.metaKey && !e.altKey) {
        e.preventDefault();
        t.blur();
        if (e.key === 'PageDown') proximo(); else anterior();
      }
      return;
    }
    if (e.ctrlKey || e.metaKey || e.altKey) return;
    if (t && /^(BUTTON|A|SUMMARY)$/.test(t.tagName) && (e.key === 'Enter' || e.key === ' ')) return;
    if (t && t.getAttribute && t.getAttribute('role') === 'button' && (e.key === 'Enter' || e.key === ' ')) return;

    const k = e.key;
    if (estado.irBuffer && k === 'Enter') { e.preventDefault(); const n = parseInt(estado.irBuffer, 10); cancelarIrPara(); ir(n - 1, 0); return; }
    if (/^\d$/.test(k)) { estado.irBuffer = (estado.irBuffer + k).slice(-4); mostrarIrPara(); return; }
    if (k === 'Escape' && estado.irBuffer) { cancelarIrPara(); return; }
    if (k === 'Escape') {
      if (abertos.ajuda) return alternar('ajuda', false);
      if (abertos.cortina) return alternar('cortina', false);
      if (abertos.notas && !abertos.sumario) return alternar('notas', false);
      return alternar('sumario');
    }
    if (abertos.cortina && !['b', 'B', '.'].includes(k)) return;

    switch (k) {
      case 'ArrowRight': case 'ArrowDown': case 'PageDown': case ' ': case 'Enter':
        e.preventDefault();
        if (e.shiftKey && (k === 'ArrowRight' || k === 'ArrowDown')) ir(estado.i + 1, 0, 1); else proximo();
        break;
      case 'ArrowLeft': case 'ArrowUp': case 'PageUp': case 'Backspace':
        e.preventDefault();
        if (e.shiftKey && (k === 'ArrowLeft' || k === 'ArrowUp')) ir(estado.i - 1, 0, -1); else anterior();
        break;
      case 'Home': e.preventDefault(); ir(0, 0); break;
      case 'End': e.preventDefault(); ir(estado.slides.length - 1, 0); break;
      case 'o': case 'O': case 'm': case 'M': alternar('sumario'); break;
      case 'n': case 'N': alternar('notas'); break;
      case 't': case 'T': alternarTema(); break;
      case 'f': case 'F': telaCheia(); break;
      case 'l': case 'L': D.body.classList.toggle('laser'); break;
      case 'p': case 'P': abrirApresentador(); break;
      case 'b': case 'B': case '.': alternar('cortina'); break;
      case 'r': case 'R': reiniciarSlide(); break;
      case '?': case 'h': case 'H': alternar('ajuda'); break;
      default: return;
    }
  }

  function toque() {
    let x0 = 0, y0 = 0, alvoInterativo = false;
    D.addEventListener('touchstart', (e) => {
      const t = e.touches[0];
      x0 = t.clientX; y0 = t.clientY;
      alvoInterativo = !!e.target.closest('input, select, textarea, button, [role="button"], .quiz, .virar, .interativo');
    }, { passive: true });
    D.addEventListener('touchend', (e) => {
      if (alvoInterativo) return;
      const t = e.changedTouches[0];
      const dx = t.clientX - x0, dy = t.clientY - y0;
      if (Math.abs(dx) > 50 && Math.abs(dx) > Math.abs(dy) * 1.3) { if (dx < 0) proximo(); else anterior(); }
    }, { passive: true });
  }

  function ocioso() {
    let timer = null;
    const acordar = () => {
      D.body.classList.remove('ocioso');
      clearTimeout(timer);
      timer = setTimeout(() => { if (!abertos.sumario && !abertos.ajuda && !ui.rodape.matches(':hover')) D.body.classList.add('ocioso'); }, 2800);
    };
    D.addEventListener('mousemove', (e) => {
      acordar();
      if (D.body.classList.contains('laser')) { ui.laser.style.left = e.clientX + 'px'; ui.laser.style.top = e.clientY + 'px'; }
    });
    acordar();
  }

  // R: volta o slide ao passo 0 e o reabre — inclusive widgets mexidos por clique
  function reiniciarSlide() {
    const s = estado.slides[estado.i];
    estado.passo = 0;
    aplicarPasso(s, 0, -1, true);
    s.classList.remove('ativo');
    void s.offsetWidth;
    s.classList.add('ativo');
    entrarSlide(s, 1);
    atualizarUI();
    persistir();
  }

  // ── Impressão: todos os slides, todos os passos ──────────────────────────
  // .imprimindo dá layout a todos os slides (display:flex) ANTES de os widgets
  // irem ao estado final: quem mede offsetWidth/offsetTop não recebe 0. Depois,
  // o gancho opcional imprimir(el, ctx) de cada widget completa o que for
  // animação (contadores, digitação).
  function antesDeImprimir() {
    D.documentElement.classList.add('imprimindo');
    void D.body.offsetWidth;
    estado.slides.forEach((s) => {
      iniciarWidgets(s);
      aplicarPasso(s, s._passos.length, 1, true);
      for (const w of s._widgets) if (w.__deckDef) seguro(w, 'imprimir', w.__deckCtx);
    });
  }
  function depoisDeImprimir() {
    D.documentElement.classList.remove('imprimindo');
    estado.slides.forEach((s, j) => { if (j !== estado.i) aplicarPasso(s, 0, -1, true); });
    aplicarPasso(estado.slides[estado.i], estado.passo, 1, true);
  }

  // ── Início ───────────────────────────────────────────────────────────────
  function iniciar() {
    if (!$('#palco')) return;
    if (ESTATICO) D.documentElement.classList.add('estatico');
    D.documentElement.dataset.tema = armazenar.ler('deck:tema', D.documentElement.dataset.tema || 'escuro');
    preparar();
    if (!estado.slides.length) return;
    montarUI();
    ajustarEscala();
    window.addEventListener('resize', ajustarEscala);
    D.addEventListener('keydown', teclado);
    toque();
    ocioso();
    window.addEventListener('hashchange', () => { const i = lerHash(); if (i != null && i !== estado.i) ir(i, 0); });
    window.addEventListener('beforeprint', antesDeImprimir);
    window.addEventListener('afterprint', depoisDeImprimir);
    window.addEventListener('message', aoReceber);

    const doHash = lerHash();
    estado.i = doHash ?? 0;
    ir(estado.i, params.has('fim') ? 'fim' : 0, 1);
    if (APRESENTADOR) {
      try { window.opener && window.opener.postMessage({ deck: CHAVE, tipo: 'ola' }, '*'); } catch { /* sem projetor */ }
    }

    const salvo = armazenar.ler(CHAVE + ':pos', null);
    if (doHash == null && salvo && salvo.i > 0 && salvo.i < estado.slides.length && !ESTATICO && !APRESENTADOR) {
      const s = estado.slides[salvo.i];
      toast(`Continuar de onde parou? <b>${salvo.i + 1}</b> · ${Realce.escapar(s._cap.rotulo)} — ${Realce.escapar(s._titulo)}`,
        [['Continuar', () => ir(salvo.i, 0)], ['Do início', () => {}, true]], 9000);
    }
    D.documentElement.classList.add('deck-pronto');
  }

  // ── API pública ──────────────────────────────────────────────────────────
  const API = {
    widget(nome, def) {
      if (registro.has(nome)) console.warn('[deck] widget redefinido:', nome);
      registro.set(nome, def);
    },
    ir: (i, passo = 0) => ir(i, passo),
    proximo, anterior,
    get atual() { return estado.i; },
    get passo() { return estado.passo; },
    get total() { return estado.slides.length; },
    get slides() { return estado.slides; },
    get capitulos() { return estado.caps; },
    info() {
      const s = estado.slides[estado.i];
      return { indice: estado.i, passo: estado.passo, passos: s?._passos.length ?? 0, total: estado.slides.length, cap: s?._cap.id, titulo: s?._titulo };
    },
    erros,
    toast,
    widgetsRegistrados: () => [...registro.keys()],
    util: {
      el, svg, esperar, animarNumero, lerGrupos,
      escapar: (s) => Realce.escapar(s),
      realcar: (txt, lang = 'cpp') => Realce.linhas(Realce.normalizar(txt), lang).join('\n'),
      codigo: (txt, op) => blocoCodigo(txt, op),
      terminal: (txt, op) => blocoTerminal(txt, op),
      reduzido,
      $, $$,
    },
  };
  window.Deck = API;

  if (D.readyState === 'loading') D.addEventListener('DOMContentLoaded', iniciar);
  else setTimeout(iniciar, 0);
})();
