/* =============================================================================
   widgets/abertura.js — os widgets da Abertura

     abertura/capa          campo de tokens de C++ em 3 camadas de profundidade
     abertura/constelacao   o syntax no centro, os subprojetos que aprofundam em volta
     abertura/arvore        a árvore de src/syntax e o caminho 8 headers → 1 TU
     abertura/main          main() executando: section() + run(), linha a linha
     abertura/agenda        os capítulos da aula, com as subseções reais
     abertura/teclas        os atalhos do deck, acendendo com as teclas de verdade

   Regras do motor: passo(k) desenha o estado k do zero (idempotente);
   animações assíncronas usam ctx.esperar e param quando o slide sai.
   ============================================================================= */
(function () {
  'use strict';
  const { el, svg } = Deck.util;

  const fmt = (n) => Math.round(n).toLocaleString('pt-BR');

  // PRNG determinístico (mulberry32): o mesmo arranjo em toda abertura e nos screenshots
  function prng(semente) {
    let s = semente >>> 0;
    return () => {
      s = (s + 0x6D2B79F5) >>> 0;
      let t = s;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  // Conta de `de` até `ate` em ms; para sozinho se raiz._exec mudar (outro passo/estado).
  // (Deck.util.animarNumero não é cancelável: voltar no meio deixaria um número velho.)
  function contar(raiz, no, de, ate, ms) {
    const minha = raiz._exec;
    if (Deck.util.reduzido()) { no.textContent = fmt(ate); return; }
    const t0 = performance.now();
    const quadro = (t) => {
      if (minha !== raiz._exec) return;
      const p = Math.min(1, (t - t0) / ms);
      no.textContent = fmt(de + (ate - de) * (1 - Math.pow(1 - p, 3)));
      if (p < 1) requestAnimationFrame(quadro);
    };
    requestAnimationFrame(quadro);
  }

  // Cor de cada capítulo (a mesma do motor)
  const COR = ['#38bdf8', '#7c8cff', '#a78bfa', '#e879f9', '#f472b6', '#fb7185', '#fb923c', '#facc15', '#a3e635'];

  // ───────────────────────────────────────────────────────────────────────────
  //  1. Capa — tokens flutuando em camadas (longe · meio · perto)
  //     Cada token tem a cor do capítulo em que aparece.
  // ───────────────────────────────────────────────────────────────────────────
  const TOKENS = [
    ['{}', 1], ['auto', 1], ['decltype', 1], ['constexpr', 1], ['constinit', 1], ["1'000'000", 1], ['R"(…)"', 1],
    ['if constexpr', 2], ['switch', 2], ['[[fallthrough]]', 2], ['for (auto& x : v)', 2],
    ['noexcept', 3], ['[[nodiscard]]', 3], ['-> int', 3], ['inline', 3], ['int (*f)(int)', 3],
    ['enum class', 4], ['.porta = 443', 4], ['unsigned x : 4', 4], ['struct', 4],
    ['explicit', 5], ['= default', 5], ['= delete', 5], ['override', 5], ['virtual', 5], ['~T()', 5],
    ['#include', 6], ['namespace', 6], ['::', 6], ['[[likely]]', 6], ['extern "C"', 6], ['#pragma once', 6],
    ['static_cast<T>', 7], ['->', 7], ['&&', 7], ['<=>', 7], ['sizeof', 7],
    ['template', 8], ['<>', 8], ['typename', 8], ['requires', 8], ['concept', 8], ['try', 8], ['throw', 8],
    ['int main()', 0], ['//', 0],
  ];
  // z: 0 longe, 1 meio, 2 perto — tamanho, opacidade, desfoque, amplitude do balanço e paralaxe
  const CAMADAS = [
    { fs: 18, op: 0.26, blur: 1.2, amp: 6, dur: [24, 38], k: 6 },
    { fs: 25, op: 0.48, blur: 0.3, amp: 10, dur: [18, 30], k: 14 },
    { fs: 34, op: 0.86, blur: 0, amp: 14, dur: [14, 24], k: 26 },
  ];
  // retângulos ocupados pelo texto da capa (coordenadas do palco 1600×900)
  const LIVRE_DE = [
    { x0: 80, y0: 175, x1: 1125, y1: 515 },     // kicker + título + subtítulo
    { x0: 80, y0: 515, x1: 1490, y1: 712 },     // a fileira de números
  ];

  function intersecta(a, b) { return a.x0 < b.x1 && a.x1 > b.x0 && a.y0 < b.y1 && a.y1 > b.y0; }

  function montarCampo(raiz) {
    const r = prng(20260520);
    const ordem = TOKENS.map((t, i) => [t, r(), i]).sort((a, b) => a[1] - b[1]).map((x) => x[0]);
    // os 10 primeiros vão para perto, os 16 seguintes para o meio, o resto para longe
    const porCamada = [[], [], []];
    ordem.forEach((t, i) => porCamada[i < 10 ? 2 : i < 26 ? 1 : 0].push(t));
    const ocupados = [];
    const camadas = [];
    for (let z = 2; z >= 0; z--) {                       // posiciona primeiro os de perto (maiores)
      const C = CAMADAS[z];
      const cam = el('div', { class: 'ab-camada', dataset: { z: String(z) } });
      cam.style.setProperty('--k', C.k);
      cam.style.setProperty('--z', z);
      for (const [txt, cap] of porCamada[z]) {
        const w = txt.length * C.fs * 0.56 + 10;
        const h = C.fs * 1.3;
        let caixa = null;
        for (let tent = 0; tent < 600 && !caixa; tent++) {
          const x = 24 + r() * (1600 - 48 - w);
          const y = 30 + r() * (900 - 60 - h);
          const c = { x0: x - 14, y0: y - 10, x1: x + w + 14, y1: y + h + 10 };
          if (LIVRE_DE.some((z0) => intersecta(c, z0))) continue;
          if (ocupados.some((o) => intersecta(c, o))) continue;
          caixa = { x, y, c };
        }
        if (!caixa) continue;                              // sem lugar: fica de fora (raro)
        ocupados.push(caixa.c);
        const ang = r() * Math.PI * 2;
        const tok = el('span', { class: 'ab-tok' + (z === 2 && r() < 0.5 ? ' ab-brilha' : '') }, txt);
        const st = tok.style;
        st.left = caixa.x.toFixed(1) + 'px';
        st.top = caixa.y.toFixed(1) + 'px';
        st.fontSize = C.fs + 'px';
        st.setProperty('--tc', COR[cap]);
        st.setProperty('--op', C.op);
        st.setProperty('--blur', C.blur + 'px');
        st.setProperty('--dx', (Math.cos(ang) * C.amp).toFixed(1) + 'px');
        st.setProperty('--dy', (Math.sin(ang) * C.amp - C.amp * 0.6).toFixed(1) + 'px');
        const dur = C.dur[0] + r() * (C.dur[1] - C.dur[0]);
        st.setProperty('--dur', dur.toFixed(1) + 's');
        st.setProperty('--atraso', (-r() * dur).toFixed(1) + 's');
        st.setProperty('--brilho', (6 + r() * 7).toFixed(1) + 's');
        st.setProperty('--brilho-atraso', (-r() * 12).toFixed(1) + 's');
        cam.append(tok);
      }
      camadas.unshift(cam);
    }
    raiz.replaceChildren(...camadas);
  }

  Deck.widget('abertura/capa', {
    iniciar(raiz, ctx) {
      montarCampo(raiz);
      raiz._mover = (e) => {
        const mx = (e.clientX / window.innerWidth) * 2 - 1;
        const my = (e.clientY / window.innerHeight) * 2 - 1;
        raiz.style.setProperty('--mx', (-mx).toFixed(3));
        raiz.style.setProperty('--my', (-my).toFixed(3));
      };
      // Ctrl+P logo ao abrir: o contador e a digitação do motor podem estar no meio —
      // o PDF sai com os números e o subtítulo completos
      const slide = raiz.closest('.slide');
      window.addEventListener('beforeprint', () => {
        slide.querySelectorAll('[data-widget="deck/contador"]').forEach((n) => { n.textContent = fmt(+n.dataset.ate); });
        slide.querySelectorAll('[data-widget="deck/digitar"]').forEach((n) => { if (n._alvo) n.textContent = n._alvo; });
      });
      void ctx;
    },
    entrar(raiz, ctx) {
      raiz.classList.toggle('ab-parado', !!ctx.reduzido);
      raiz.style.setProperty('--mx', 0);
      raiz.style.setProperty('--my', 0);
      if (!ctx.reduzido) ctx.slide.addEventListener('mousemove', raiz._mover);
    },
    sair(raiz, ctx) {
      ctx.slide.removeEventListener('mousemove', raiz._mover);
    },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  2. Constelação — "Aqui você vê → Para se aprofundar"
  //     As 12 linhas da tabela do README (13 subprojetos: smart-pointers e
  //     memory dividem um cartão) — 6 cartões à esquerda, 6 à direita.
  // ───────────────────────────────────────────────────────────────────────────
  const TEMAS = ['13 subprojetos em volta', 'tipos e valores', 'genérico e compile-time', 'objetos e recursos', 'por dentro'];
  const CW = 1408, CH = 600, SW = 470, SH = 88, NW = 230, NH = 170;

  Deck.widget('abertura/constelacao', {
    iniciar(raiz) {
      const sats = [...raiz.querySelectorAll('.ab-sat')];
      const centro = raiz.querySelector('.ab-centro');
      const gap = (CH - 6 * SH) / 5;
      const cx = CW / 2, cy = CH / 2;
      Object.assign(centro.style, { left: cx - NW / 2 + 'px', top: cy - NH / 2 + 'px', width: NW + 'px', height: NH + 'px' });
      const fios = svg('g', { class: 'ab-fios' });
      const pulsos = svg('g', { class: 'ab-pulsos' });
      const nos = svg('g', { class: 'ab-nos' });
      raiz._itens = sats.map((sat, i) => {
        const lado = i < 6 ? 0 : 1;
        const j = i % 6;
        const x = lado ? CW - SW : 0;
        const y = j * (SH + gap);
        Object.assign(sat.style, { left: x + 'px', top: y + 'px', width: SW + 'px', height: SH + 'px' });
        const x0 = lado ? cx + NW / 2 : cx - NW / 2;
        const x1 = lado ? x - 6 : x + SW + 6;
        const y0 = cy + (j - 2.5) * 16;                    // os fios saem espalhados da borda do nó
        const y1 = y + SH / 2;
        const mx = (x0 + x1) / 2;
        const d = `M ${x0} ${y0.toFixed(1)} C ${mx} ${y0.toFixed(1)}, ${mx} ${y1}, ${x1} ${y1}`;
        const cor = sat.style.getPropertyValue('--c') || '#40e0d0';
        const fio = svg('path', { d, class: 'ab-fio', pathLength: '1', style: `--c:${cor}` });
        const pulso = svg('path', { d, class: 'ab-pulso', pathLength: '1', style: `--c:${cor}` });
        const no = svg('circle', { cx: x1, cy: y1, r: 5, class: 'ab-no', style: `--c:${cor}` });
        fios.append(fio); pulsos.append(pulso); nos.append(no);
        sat.addEventListener('click', (e) => {
          if (e.target.closest('a')) return;               // o link abre o README
          const ja = sat.classList.contains('ab-foco');
          sats.forEach((s) => s.classList.remove('ab-foco'));
          raiz._itens.forEach((it) => it.pulso.classList.remove('ab-foco'));
          raiz.classList.toggle('ab-focando', !ja);
          if (!ja) { sat.classList.add('ab-foco'); pulso.classList.add('ab-foco'); }
        });
        return { sat, fio, pulso, no, g: +sat.dataset.g };
      });
      const s = svg('svg', { class: 'ab-const-svg', viewBox: `0 0 ${CW} ${CH}`, width: CW, height: CH, 'aria-hidden': 'true' }, fios, pulsos, nos);
      raiz.prepend(s);
      raiz._tema = raiz.querySelector('.ab-centro-tema');
    },
    passo(raiz, k) {
      raiz.dataset.k = k;
      raiz.classList.remove('ab-focando');
      raiz._itens.forEach(({ sat, fio, pulso, no, g }) => {
        const aceso = g <= k;
        sat.classList.toggle('ab-acesa', aceso);
        sat.classList.toggle('ab-atual', g === k);
        sat.classList.remove('ab-foco');
        fio.classList.toggle('ab-on', aceso);
        no.classList.toggle('ab-on', aceso);
        pulso.classList.toggle('ab-on', g === k);
        pulso.classList.remove('ab-foco');
      });
      raiz._tema.textContent = TEMAS[k] || TEMAS[0];
    },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  3. Árvore — src/syntax/ e o caminho 8 headers → 1 TU
  //     As linhas da lista têm altura fixa (CSS), então a geometria é exata.
  // ───────────────────────────────────────────────────────────────────────────
  const LH = 41;            // altura de cada linha da árvore (igual ao CSS)

  Deck.widget('abertura/arvore', {
    iniciar(raiz) {
      const lista = raiz.querySelector('.ab-arv-lista');
      const lis = [...lista.children];
      const iMain = lis.findIndex((li) => li.dataset.f === 'main');
      const iDemos = lis.map((li, i) => (li.dataset.f === 'demo' ? i : -1)).filter((i) => i >= 0);
      const yc = (i) => i * LH + LH / 2;
      const X = 12, XT = 30;
      const g = svg('g', null);
      g.append(svg('path', { class: 'ab-bus', d: `M ${X} ${yc(iDemos[iDemos.length - 1])} V ${yc(iMain)} H ${XT + 4}` }));
      iDemos.forEach((i) => g.append(svg('path', { class: 'ab-bus ab-bus-r', d: `M ${XT} ${yc(i)} H ${X}`, style: `--c:${lis[i].style.getPropertyValue('--c')}` })));
      g.append(svg('path', { class: 'ab-bus-ponta', d: `M ${XT - 2} ${yc(iMain) - 6} L ${XT + 6} ${yc(iMain)} L ${XT - 2} ${yc(iMain) + 6} Z` }));
      const s = svg('svg', { class: 'ab-arv-svg', width: 44, height: lis.length * LH, viewBox: `0 0 44 ${lis.length * LH}`, 'aria-hidden': 'true' }, g);
      lista.after(s);
      // selos que aparecem por passo
      lis[iMain].append(el('span', { class: 'ab-selo ab-selo-tu' }, '1 TU'), el('span', { class: 'ab-selo ab-selo-ruim' }, '+ outro.cpp ✗'));
      raiz._paineis = [...raiz.querySelectorAll('.ab-painel')];
      raiz._cont = raiz.querySelector('.ab-tu-cont');
    },
    passo(raiz, k, ctx, dir) {
      raiz._exec = (raiz._exec || 0) + 1;            // cancela uma contagem em andamento
      raiz.dataset.k = k;
      raiz._paineis.forEach((p) => p.classList.toggle('on', +p.dataset.k === k));
      if (k === 1 && dir > 0 && !ctx.reduzido) {
        raiz._cont.textContent = '58';
        ctx.esperar(250).then((ok) => { if (ok) contar(raiz, raiz._cont, 58, 70439, 1500); });
      } else {
        raiz._cont.textContent = fmt(70439);
      }
    },
    sair(raiz) { raiz._exec = (raiz._exec || 0) + 1; raiz._cont.textContent = fmt(70439); },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  4. main() executando — o "contador de programa" desce pelo main.cpp real
  //     e cada run() enche a barra do seu bloco com as linhas REAIS da saída.
  // ───────────────────────────────────────────────────────────────────────────
  // passos do roteiro: linha do main.cpp e o que ela faz (sec/run = índice do bloco)
  const ROTEIRO = [{ ln: 31 }];
  for (let b = 0; b < 8; b++) ROTEIRO.push({ ln: 32 + 3 * b, sec: b }, { ln: 33 + 3 * b, run: b });
  ROTEIRO.push({ ln: 56, fim: true }, { ln: 58 });
  const TOTAL = 1090;

  function mainEstado(raiz, final) {
    raiz._exec = (raiz._exec || 0) + 1;              // invalida uma execução em andamento
    raiz.classList.remove('ab-rodando');
    raiz.dataset.k = final ? 1 : 0;
    raiz.classList.toggle('ab-fim', final);
    raiz._blocos.forEach((li) => { li.classList.toggle('ab-sec', final); li.classList.toggle('ab-cheio', final); });
    raiz._soma.textContent = final ? fmt(TOTAL) : '0';
    mainPC(raiz, final ? null : 31, final ? 'fim' : 'topo');
  }

  // Mede com o slide em layout: um slide distante do atual fica com display:none (medidas 0),
  // e é assim que o motor o deixa no estado final antes de imprimir (Ctrl+P).
  function comLayout(raiz, fn) {
    const s = raiz.closest('.slide');
    const oculto = !!s && getComputedStyle(s).display === 'none';
    if (oculto) s.style.display = 'flex';
    try { return fn(); } finally { if (oculto) s.style.display = ''; }
  }

  function mainPC(raiz, n, rolar) {
    let alvo = null;
    raiz._lns.forEach((ln) => { const on = +ln.dataset.n === n; ln.classList.toggle('ab-pc', on); if (on) alvo = ln; });
    const off = comLayout(raiz, () => {
      const jan = raiz._janela.clientHeight;
      const max = Math.max(0, raiz._fig.offsetHeight - jan);
      if (rolar === 'fim') return max;
      if (rolar !== 'topo' && alvo) return Math.min(max, Math.max(0, alvo.offsetTop - jan * 0.42));
      return 0;
    });
    raiz._fig.style.transform = `translateY(${-off}px)`;
  }

  async function mainRodar(raiz, ctx) {
    mainEstado(raiz, false);
    const minha = raiz._exec;
    raiz.classList.add('ab-rodando');
    let soma = 0;
    for (const p of ROTEIRO) {
      if (minha !== raiz._exec) return;
      mainPC(raiz, p.ln);
      if (p.sec != null) raiz._blocos[p.sec].classList.add('ab-sec');
      if (p.run != null) {
        const li = raiz._blocos[p.run];
        li.classList.add('ab-cheio');
        const de = soma;
        soma += +li.dataset.n;
        contar(raiz, raiz._soma, de, soma, 380);
      }
      if (p.fim) { raiz.classList.add('ab-fim'); contar(raiz, raiz._soma, soma, TOTAL, 300); soma = TOTAL; }
      const ok = await ctx.esperar(p.run != null ? 430 : 330);
      if (!ok || minha !== raiz._exec) { raiz.classList.remove('ab-rodando'); return; }
    }
    raiz.classList.remove('ab-rodando');
    mainPC(raiz, null, 'fim');
    raiz.dataset.k = 1;
  }

  Deck.widget('abertura/main', {
    iniciar(raiz, ctx) {
      raiz._janela = raiz.querySelector('.ab-main-janela');
      raiz._fig = raiz._janela.querySelector('.codigo');
      raiz._lns = [...raiz._fig.querySelectorAll('.ln')];
      raiz._blocos = [...raiz.querySelectorAll('.ab-main-blocos > li')];
      raiz._soma = raiz.querySelector('.ab-main-soma');
      const max = Math.max(...raiz._blocos.map((li) => +li.dataset.n));
      raiz._blocos.forEach((li) => li.style.setProperty('--w', (+li.dataset.n / max).toFixed(3)));
      raiz.querySelector('.ab-main-rodar').addEventListener('click', (e) => {
        e.stopPropagation();
        if (ctx.reduzido) { mainEstado(raiz, true); return; }
        mainRodar(raiz, ctx);
      });
    },
    passo(raiz, k, ctx, dir) {
      if (k === 0) { mainEstado(raiz, false); return; }
      if (dir > 0 && !ctx.reduzido && ctx.ativo) mainRodar(raiz, ctx);
      else mainEstado(raiz, true);
    },
    entrar(raiz, ctx) { mainEstado(raiz, ctx.passo > 0); },
    sair(raiz, ctx) { if (raiz.classList.contains('ab-rodando')) mainEstado(raiz, ctx.passo > 0); },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  5. Agenda — passar o mouse (ou Tab) num capítulo mostra as subseções reais.
  //     Sem interação, um passeio lento acende um capítulo de cada vez.
  // ───────────────────────────────────────────────────────────────────────────
  function agendaMostrar(raiz, a) {
    raiz._caps.forEach((c) => c.classList.toggle('ab-sel', c === a));
    if (!a) { raiz._rot.textContent = raiz._padrao; raiz._chips.replaceChildren(); raiz._det.style.removeProperty('--c'); return; }
    const titulo = a.querySelector('.ab-cap-t').textContent;
    const n = a.querySelector('.ab-cap-n').textContent;
    raiz._rot.textContent = (/^\d+$/.test(n) ? 'Cap. ' + +n + ' · ' : '') + titulo;
    raiz._det.style.setProperty('--c', a.style.getPropertyValue('--c'));
    raiz._chips.replaceChildren(...a.dataset.subs.split('|').map((s, i) => {
      const c = el('span', { class: 'ab-ag-chip' }, s);
      c.style.setProperty('--i', i);
      return c;
    }));
  }

  Deck.widget('abertura/agenda', {
    iniciar(raiz) {
      raiz._caps = [...raiz.querySelectorAll('.ab-cap')];
      raiz._det = raiz.querySelector('.ab-ag-detalhe');
      raiz._rot = raiz._det.querySelector('.ab-ag-rot');
      raiz._chips = raiz._det.querySelector('.ab-ag-chips');
      raiz._padrao = raiz._rot.textContent;
      raiz._caps.forEach((a) => {
        const usar = () => { raiz._manual = true; agendaMostrar(raiz, a); };
        a.addEventListener('mouseenter', usar);
        a.addEventListener('focus', usar);
        // "#/<slug>/1" vai para o hash DESTE documento (o motor atende no hashchange).
        // Sem isso, uma página com <base href> (a prévia) resolveria o link contra a
        // pasta do deck e sairia da apresentação. Ctrl/⌘/Shift+clique seguem o padrão.
        a.addEventListener('click', (e) => {
          const h = a.getAttribute('href') || '';
          if (e.button !== 0 || e.ctrlKey || e.metaKey || e.shiftKey || e.altKey || h[0] !== '#') return;
          e.preventDefault();
          if (location.hash !== h) location.hash = h;
        });
      });
      agendaMostrar(raiz, raiz._caps[1]);             // estado de repouso (ver sair)
    },
    entrar(raiz, ctx) {
      raiz._manual = false;
      if (ctx.reduzido) { agendaMostrar(raiz, raiz._caps[1]); return; }
      agendaMostrar(raiz, null);                     // "passe o mouse…" e, depois, o passeio
      let i = 0;
      ctx.esperar(1600).then((ok) => {
        if (!ok || raiz._manual) return;
        agendaMostrar(raiz, raiz._caps[i]);
        ctx.intervalo(() => {
          if (raiz._manual || !ctx.ativo) return;
          i = (i + 1) % raiz._caps.length;
          agendaMostrar(raiz, raiz._caps[i]);
        }, 2600);
      });
    },
    // fora do slide, o painel descansa mostrando o Cap. 1: é o que sai no PDF (Ctrl+P)
    sair(raiz) { agendaMostrar(raiz, raiz._caps[1]); },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  6. Teclas — os grupos acendem por passo; as teclas de verdade acendem a linha
  //     correspondente (sem impedir o motor de agir: nada de preventDefault).
  // ───────────────────────────────────────────────────────────────────────────
  // `irPara` = há um número sendo digitado (o motor guarda os dígitos por 2,5 s):
  // aí o Enter não avança — ele conclui o "número + Enter".
  function teclaCasa(e, tok, irPara) {
    switch (tok) {
      case 'shift': return e.shiftKey && /^Arrow(Left|Right|Up|Down)$/.test(e.key);
      case 'espaco': return e.key === ' ';
      case 'digito': return /^\d$/.test(e.key) || (irPara && e.key === 'Enter');
      case 'ctrl-p': return (e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'p';
      case 'Enter': return !irPara && !e.ctrlKey && !e.metaKey && !e.altKey && e.key === 'Enter';
      default: return !e.ctrlKey && !e.metaKey && !e.altKey && e.key === tok && !(e.shiftKey && /^Arrow/.test(e.key));
    }
  }

  // O mesmo filtro do motor: digitando na busca do sumário, as teclas não são atalhos
  // (só o Esc, que sai da busca); Enter/espaço num botão ou link acionam o botão.
  function teclaIgnorada(e) {
    const t = e.target;
    if (!t || !t.tagName) return false;
    const campo = t.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(t.tagName);
    if (campo) return e.key !== 'Escape';
    const botao = /^(BUTTON|A|SUMMARY)$/.test(t.tagName) || t.getAttribute('role') === 'button';
    return botao && (e.key === 'Enter' || e.key === ' ');
  }

  function teclaAcender(raiz, at, origem) {
    at.classList.remove('ab-apertada');
    void at.offsetWidth;
    at.classList.add('ab-apertada');
    clearTimeout(at._t);
    at._t = setTimeout(() => at.classList.remove('ab-apertada'), 700);
    const ks = at.querySelector('.ab-ks').cloneNode(true);
    const desc = at.lastElementChild.textContent;
    raiz._ult.replaceChildren(ks, el('span', { class: 'ab-ult-seta' }, '→'), el('span', null, desc), el('small', null, origem));
    raiz._ult.classList.remove('ab-novo'); void raiz._ult.offsetWidth; raiz._ult.classList.add('ab-novo');
  }

  Deck.widget('abertura/teclas', {
    iniciar(raiz) {
      raiz._grupos = [...raiz.querySelectorAll('.ab-tgrupo')];
      raiz._ats = [...raiz.querySelectorAll('.ab-at')];
      raiz._ult = raiz.querySelector('.ab-ult-txt');
      raiz._ats.forEach((at) => at.addEventListener('click', () => teclaAcender(raiz, at, 'clique')));
      raiz._dig = 0;                                  // instante do último dígito (ver teclaCasa)
      raiz._tecla = (e) => {
        if (teclaIgnorada(e)) return;
        const irPara = performance.now() - raiz._dig < 2500;
        const at = raiz._ats.find((a) => a.dataset.t.split(/\s+/).some((t) => teclaCasa(e, t, irPara)));
        if (/^\d$/.test(e.key) && !e.ctrlKey && !e.metaKey && !e.altKey) raiz._dig = performance.now();
        else if (e.key === 'Enter' || e.key === 'Escape') raiz._dig = 0;
        if (at) teclaAcender(raiz, at, 'teclado');
      };
    },
    passo(raiz, k) {
      raiz._grupos.forEach((g, i) => {
        g.classList.toggle('ab-on', i + 1 === k);
        g.classList.toggle('ab-depois', k > 0 && i + 1 > k);
      });
    },
    entrar(raiz) { document.addEventListener('keydown', raiz._tecla); },
    sair(raiz) { document.removeEventListener('keydown', raiz._tecla); },
  });
})();
