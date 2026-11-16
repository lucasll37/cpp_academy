/* =============================================================================
   widgets/encerramento.js — os widgets do Encerramento

     encerramento/regras    as regras de ouro: lê-se a situação, o passo revela o "faça"
     encerramento/validar   os três portões da validação, com os resultados medidos
     encerramento/trilha    a trilha do src/meson.build como uma linha de metrô
     encerramento/final     a última linha do programa, digitada, e uma celebração

   passo(k) desenha o estado k do zero; animações param quando o slide sai.
   ============================================================================= */
(function () {
  'use strict';
  const { el, svg } = Deck.util;

  const CORES = ['#38bdf8', '#7c8cff', '#a78bfa', '#e879f9', '#f472b6', '#fb7185', '#fb923c', '#facc15', '#a3e635', '#40e0d0'];

  // ───────────────────────────────────────────────────────────────────────────
  //  1. Regras de ouro — uma coluna (tema) por passo; clique revela um cartão
  // ───────────────────────────────────────────────────────────────────────────
  Deck.widget('encerramento/regras', {
    iniciar(raiz) {
      raiz._cols = [...raiz.querySelectorAll('.en-col')];
      raiz._cols.forEach((c) => c.querySelectorAll('.en-regra').forEach((r, i) => {
        r.style.setProperty('--i', i);
        r.tabIndex = 0;
        r.setAttribute('role', 'button');
        const virar = () => { if (!c.classList.contains('en-revelada')) r.classList.toggle('en-vista'); };
        r.addEventListener('click', virar);
        r.addEventListener('keydown', (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); virar(); } });
      }));
    },
    passo(raiz, k) {
      raiz._cols.forEach((c, i) => {
        c.classList.toggle('en-revelada', i < k);
        c.classList.toggle('en-atual', i === k - 1);
        c.querySelectorAll('.en-vista').forEach((r) => r.classList.remove('en-vista'));
      });
    },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  2. Validar — cada passo "roda" um portão: barra enche, resultado aparece
  // ───────────────────────────────────────────────────────────────────────────
  function etapa(e, estado) {
    e.classList.toggle('en-espera', estado === 'espera');
    e.classList.toggle('en-rodando', estado === 'rodando');
    e.classList.toggle('en-ok', estado === 'ok');
  }

  async function rodarEtapa(e, ctx) {
    etapa(e, 'espera');
    void e.offsetWidth;                     // ponto de partida para a barra encher
    etapa(e, 'rodando');
    if (!(await ctx.esperar(1250))) { etapa(e, 'ok'); return; }
    etapa(e, 'ok');
  }

  Deck.widget('encerramento/validar', {
    iniciar(raiz, ctx) {
      raiz._et = [...raiz.querySelectorAll('.en-etapa')];
      // o realce de shell para a flag no U+200D: devolve o resto da palavra ao mesmo token
      raiz.querySelectorAll('.codigo .tk-flag').forEach((sp) => {
        const prox = sp.nextSibling;
        const m = prox && /^\u200d[\w=,.:+/-]*/.exec(prox.textContent);
        if (!m) return;
        sp.textContent += m[0];
        prox.textContent = prox.textContent.slice(m[0].length);
        if (!prox.textContent && prox.nodeType === 1) prox.remove();
      });
      // o comando do UBSan leva um U+200D invisível em "un‍defined": quem copiar recebe o texto limpo
      raiz.addEventListener('copy', (e) => {
        const t = String(window.getSelection() || '');
        if (!t.includes('\u200d') || !e.clipboardData) return;
        e.preventDefault();
        e.clipboardData.setData('text/plain', t.replace(/\u200d/g, ''));
      });
      raiz._et.forEach((e) => {
        const n = e.querySelector('.en-num');
        n.setAttribute('role', 'button');
        n.title = 'rodar este portão de novo';
        n.addEventListener('click', () => { if (e.classList.contains('en-ok') && !ctx.reduzido) rodarEtapa(e, ctx); });
      });
    },
    passo(raiz, k, ctx, dir) {
      raiz._et.forEach((e, i) => {
        if (i < k - 1) etapa(e, 'ok');
        else if (i === k - 1) {
          if (dir > 0 && !ctx.reduzido && ctx.ativo) rodarEtapa(e, ctx);
          else etapa(e, 'ok');
        } else etapa(e, 'espera');
      });
    },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  3. Trilha — linha de metrô vertical; cada passo acende uma estação (tier)
  //     As posições são medidas (as linhas de chips quebram conforme a fonte).
  // ───────────────────────────────────────────────────────────────────────────
  // Um slide distante do atual fica com display:none (medidas 0) — e é assim que o motor o
  // leva ao estado final antes de imprimir (Ctrl+P): mede com o slide em layout, por um instante.
  function comLayout(raiz, fn) {
    const s = raiz.closest('.slide');
    const oculto = !!s && getComputedStyle(s).display === 'none';
    if (oculto) s.style.display = 'flex';
    try { return fn(); } finally { if (oculto) s.style.display = ''; }
  }

  function trilhaMedir(raiz) {
    const tiers = raiz._tiers;
    const [ys, h] = comLayout(raiz, () => [
      tiers.map((t) => { const b = t.querySelector('.en-tier-t b'); return b.offsetTop + b.offsetHeight / 2; }),
      raiz.offsetHeight,
    ]);
    raiz._svg.setAttribute('height', h);
    raiz._svg.setAttribute('viewBox', `0 0 40 ${h}`);
    raiz._linha.setAttribute('d', `M 20 ${ys[0]} V ${ys[ys.length - 1]}`);
    raiz._fundo.setAttribute('d', `M 20 ${ys[0]} V ${ys[ys.length - 1]}`);
    raiz._est.forEach((c, i) => c.setAttribute('cy', ys[i]));
    raiz._ys = ys;
  }

  function trilhaDesenhar(raiz, k) {
    const ys = raiz._ys;
    const tot = ys[ys.length - 1] - ys[0] || 1;
    const fr = k <= 1 ? 0 : (ys[Math.min(k, ys.length) - 1] - ys[0]) / tot;
    raiz._linha.style.strokeDashoffset = String(1 - fr);
    raiz._linha.classList.toggle('en-on', k >= 1);
    raiz._est.forEach((c, i) => { c.classList.toggle('en-on', i < k); c.classList.toggle('en-atual', i === k - 1); });
    raiz._tiers.forEach((t, i) => { t.classList.toggle('en-on', i < k); t.classList.toggle('en-atual', i === k - 1); });
  }

  Deck.widget('encerramento/trilha', {
    iniciar(raiz) {
      raiz._tiers = [...raiz.querySelectorAll('.en-tier')];
      raiz._fundo = svg('path', { class: 'en-metro-fundo' });
      raiz._linha = svg('path', { class: 'en-metro', pathLength: '1' });
      raiz._est = raiz._tiers.map((t) => svg('circle', { class: 'en-estacao', cx: 20, cy: 0, r: 9, style: `--c:${t.style.getPropertyValue('--c')}` }));
      raiz._svg = svg('svg', { class: 'en-metro-svg', width: 40, 'aria-hidden': 'true' }, raiz._fundo, raiz._linha, ...raiz._est);
      raiz.prepend(raiz._svg);
      raiz._tiers.forEach((t) => t.querySelectorAll('.en-chip, .en-grupo').forEach((c, i) => c.style.setProperty('--i', i)));
      trilhaMedir(raiz);
    },
    passo(raiz, k) { trilhaMedir(raiz); trilhaDesenhar(raiz, k); },
    entrar(raiz, ctx) { trilhaMedir(raiz); trilhaDesenhar(raiz, ctx.passo); },
  });

  // ───────────────────────────────────────────────────────────────────────────
  //  4. Final — a última linha do programa, digitada; partículas nas cores dos
  //     capítulos; depois, "Perguntas?". Com movimento reduzido: estado final.
  // ───────────────────────────────────────────────────────────────────────────
  // origem das partículas: (800, 330) no palco de 1600×900 — ver .en-origem no CSS
  const OX = 800, OY = 330;
  function confeteParado(raiz) {
    const r = (s) => { const x = Math.sin(s * 12.9898) * 43758.5453; return x - Math.floor(x); };
    const pts = [];
    for (let i = 0, k = 0; pts.length < 40 && k < 400; k++) {
      const x = 40 + r(k + 1) * 1520, y = 30 + r(k + 101) * 830;
      if (x > 250 && x < 1350 && y > 200 && y < 690) continue;      // longe do texto
      const p = el('i', { class: 'en-pt en-pt-parado' });
      p.style.setProperty('--x', (x - OX).toFixed(0) + 'px');
      p.style.setProperty('--y', (y - OY).toFixed(0) + 'px');
      p.style.setProperty('--c', CORES[i++ % CORES.length]);
      p.style.setProperty('--s', (4 + r(k + 3) * 6).toFixed(1) + 'px');
      pts.push(p);
    }
    raiz._orig.replaceChildren(...pts);
  }

  function explodir(raiz) {
    const pts = [];
    for (let i = 0; i < 90; i++) {
      const ang = Math.random() * Math.PI * 2;
      const rad = 180 + Math.random() * 520;
      const p = el('i', { class: 'en-pt' + (Math.random() < 0.35 ? ' en-pt-fita' : '') });
      p.style.setProperty('--x', (Math.cos(ang) * rad * 1.3).toFixed(0) + 'px');
      p.style.setProperty('--y', (Math.sin(ang) * rad * 0.7 - 60).toFixed(0) + 'px');
      p.style.setProperty('--c', CORES[i % CORES.length]);
      p.style.setProperty('--s', (5 + Math.random() * 8).toFixed(1) + 'px');
      p.style.setProperty('--r', (Math.random() * 720 - 360).toFixed(0) + 'deg');
      p.style.setProperty('--d', (1.6 + Math.random() * 1.6).toFixed(2) + 's');
      p.style.setProperty('--a', (Math.random() * 0.25).toFixed(2) + 's');
      pts.push(p);
    }
    raiz._orig.replaceChildren(...pts);
  }

  Deck.widget('encerramento/final', {
    iniciar(raiz) {
      raiz._txt = raiz.querySelector('.en-fim-txt');
      raiz._alvo = raiz._txt.textContent;
      raiz._orig = raiz.querySelector('.en-origem');
      // estado final por padrão: é o que sai no PDF (Ctrl+P) se o slide nunca foi aberto
      raiz.classList.add('en-digitado', 'en-pronto');
    },
    async entrar(raiz, ctx) {
      if (ctx.reduzido) {
        raiz._txt.textContent = raiz._alvo;
        raiz.classList.add('en-digitado', 'en-pronto');
        confeteParado(raiz);
        return;
      }
      raiz.classList.remove('en-digitado', 'en-pronto');
      raiz._orig.replaceChildren();
      raiz._txt.textContent = '';
      // (se o slide sair no meio, ctx.esperar nunca resolve e o sair() arruma o estado)
      if (!(await ctx.esperar(900))) return;
      const alvo = raiz._alvo;
      for (let c = 1; c <= alvo.length; c++) {
        raiz._txt.textContent = alvo.slice(0, c);
        if (!(await ctx.esperar(c === 1 ? 260 : 34 + Math.random() * 30))) return;
      }
      raiz.classList.add('en-digitado');
      explodir(raiz);
      if (!(await ctx.esperar(650))) return;
      raiz.classList.add('en-pronto');
    },
    sair(raiz) {
      raiz._txt.textContent = raiz._alvo;
      raiz.classList.add('en-digitado', 'en-pronto');
      raiz._orig.replaceChildren();
    },
  });
})();
