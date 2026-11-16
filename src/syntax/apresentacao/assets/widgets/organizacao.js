/* =============================================================================
   widgets/organizacao.js — Capítulo 6 · Organização (demo_organizacao.hpp)

   Dez widgets, todos com o mesmo contrato:
     • iniciar() monta o esqueleto uma vez;
     • passo(k) desenha o ESTADO k do zero (idempotente) — o clicker percorre
       os estados na ordem da aula; os botões exploram livremente;
     • as transições são CSS (animações com atraso --d), então nada depende de
       timers: voltar (←) ou entrar pelo fim mostra o estado final na hora.

   Todo texto de compilador/terminal aqui é saída REAL (g++ 13.3, nm, c++filt),
   capturada em scratchpad/verif/organizacao/.
   ============================================================================= */
(function () {
  'use strict';
  const { el, svg } = Deck.util;
  const esc = (s) => Deck.util.escapar(String(s));
  const cpp = (s) => Deck.util.realcar(String(s), 'cpp');
  // realça um PEDAÇO de linha sem perder os espaços das pontas (o realcar normaliza a indentação)
  function cppSeg(s) {
    const m = /^(\s*)([\s\S]*?)(\s*)$/.exec(String(s));
    return esc(m[1]) + (m[2] ? cpp(m[2]) : '') + esc(m[3]);
  }

  // ── utilitários comuns ────────────────────────────────────────────────────
  function botao(rotulo, aoClicar, classe, html) {
    const b = el('button', { class: 'botao ' + (classe || ''), type: 'button' });
    if (html) b.innerHTML = rotulo; else b.textContent = rotulo;
    b.addEventListener('click', (e) => { e.stopPropagation(); aoClicar(); });
    return b;
  }

  function classeLinha(s) {
    if (/^\$ /.test(s)) return 'cmd';
    if (/^\s*…\s*$/.test(s)) return 'eco';
    if (/^\s*\d*\s+\|/.test(s)) return 'eco';
    if (/\berror\b|AttributeError|✗/.test(s)) return 'erro';
    if (/\bwarning\b|⚠/.test(s)) return 'aviso';
    if (/\bnote\b|In function|In file included|^\s+from /.test(s)) return 'nota';
    return 'out';
  }

  // terminal próprio dos widgets (quebra linha longa, ao contrário do .terminal do motor)
  // a flag de aviso ([-Wxxx]) nunca se parte no hífen: vai inteira para a linha de baixo;
  // um nome qualificado longo pode quebrar depois de um `::` (e não no meio de um nome)
  const semQuebrarFlag = (h) => h.replace(/\[-W[^\]\s]+\]/g, '<span class="org-nobr">$&</span>').replace(/::(?=\w)/g, '::<wbr>');
  function terminal(titulo, linhas, classe) {
    const corpo = el('div', { class: 'org-term-corpo' });
    linhas.forEach((l, i) => {
      const o = typeof l === 'string' ? { s: l } : l;
      const t = o.t || classeLinha(o.s);
      const n = el('div', { class: 'org-tl org-tl-' + t });
      n.style.setProperty('--d', (60 + i * 55) + 'ms');
      n.innerHTML = t === 'cmd'
        ? '<span class="org-prompt">$ </span>' + esc(o.s.replace(/^\$ /, ''))
        : (o.h != null ? o.h : (semQuebrarFlag(esc(o.s)) || '&#8203;'));
      corpo.append(n);
    });
    return el('div', { class: 'org-term ' + (classe || '') },
      el('div', { class: 'org-term-topo' }, el('span', { class: 'org-dots' }, el('i'), el('i'), el('i')),
        el('span', { class: 'org-term-tit' }, titulo)),
      corpo);
  }

  // posição de um nó relativa à raiz do widget, em px do palco (ignora transforms)
  function pos(raiz, no) {
    let x = 0, y = 0, n = no;
    while (n && n !== raiz) {
      x += n.offsetLeft; y += n.offsetTop;
      const p = n.offsetParent;
      if (!p || !raiz.contains(p)) break;
      n = p;
    }
    return { x, y, w: no.offsetWidth, h: no.offsetHeight };
  }
  function camadaSetas(raiz) {
    let c = raiz.querySelector(':scope > svg.org-setas');
    if (!c) { c = svg('svg', { class: 'org-setas', 'aria-hidden': 'true' }); raiz.append(c); }
    c.setAttribute('width', raiz.offsetWidth); c.setAttribute('height', raiz.offsetHeight);
    return c;
  }
  // seta curva de `de` até `para`; lados escolhidos pela geometria
  function seta(raiz, de, para, classe, atraso, rotulo) {
    if (!de || !para || !raiz.offsetWidth) return;
    const a = pos(raiz, de), b = pos(raiz, para);
    const ca = { x: a.x + a.w / 2, y: a.y + a.h / 2 }, cb = { x: b.x + b.w / 2, y: b.y + b.h / 2 };
    const dx = cb.x - ca.x, dy = cb.y - ca.y;
    let p1, p2, c1, c2;
    if (Math.abs(dx) > Math.abs(dy) * 1.2) {
      const s = Math.sign(dx) || 1;
      p1 = { x: s > 0 ? a.x + a.w : a.x, y: ca.y };
      p2 = { x: s > 0 ? b.x - 6 : b.x + b.w + 6, y: cb.y };
      const m = (p2.x - p1.x) * 0.5;
      c1 = { x: p1.x + m, y: p1.y }; c2 = { x: p2.x - m, y: p2.y };
    } else {
      const s = Math.sign(dy) || 1;
      p1 = { x: ca.x, y: s > 0 ? a.y + a.h : a.y };
      p2 = { x: cb.x, y: s > 0 ? b.y - 6 : b.y + b.h + 6 };
      const m = (p2.y - p1.y) * 0.55;
      c1 = { x: p1.x, y: p1.y + m }; c2 = { x: p2.x, y: p2.y - m };
    }
    curva(raiz, p1, c1, c2, p2, classe, atraso, rotulo);
  }
  // curva cúbica p1→p2 (c1, c2 = controles) com ponta de seta no fim;
  // `rotulo` (opcional) = uma etiqueta curta sobre o meio da curva
  function curva(raiz, p1, c1, c2, p2, classe, atraso, rotulo) {
    if (!raiz.offsetWidth) return;
    const c = camadaSetas(raiz);
    const g = svg('g', { class: 'org-seta ' + (classe || '') });
    g.style.setProperty('--d', (atraso || 0) + 'ms');
    g.append(svg('path', { class: 'org-seta-linha', d: `M${p1.x} ${p1.y} C${c1.x} ${c1.y} ${c2.x} ${c2.y} ${p2.x} ${p2.y}`, pathLength: 1 }));
    const ang = Math.atan2(p2.y - c2.y, p2.x - c2.x);
    const L = 13, W = 7;
    const bx = p2.x - Math.cos(ang) * L, by = p2.y - Math.sin(ang) * L;
    const px = -Math.sin(ang) * W, py = Math.cos(ang) * W;
    g.append(svg('path', { class: 'org-seta-ponta', d: `M${p2.x} ${p2.y} L${bx + px} ${by + py} L${bx - px} ${by - py} Z` }));
    if (rotulo) {
      const mx = (p1.x + 3 * c1.x + 3 * c2.x + p2.x) / 8, my = (p1.y + 3 * c1.y + 3 * c2.y + p2.y) / 8;
      const w = rotulo.length * 10.2 + 18, h = 28;
      g.append(svg('g', { class: 'org-seta-rot', transform: `translate(${mx} ${my})` },
        svg('rect', { x: -w / 2, y: -h / 2, width: w, height: h, rx: 8 }),
        svg('text', { x: 0, y: 1 }, rotulo)));
    }
    c.append(g);
  }
  function limparSetas(raiz) { const c = raiz.querySelector(':scope > svg.org-setas'); if (c) c.remove(); }
  // etiqueta solta na camada das setas (centrada em x, y)
  function etiqueta(raiz, x, y, texto, classe, atraso) {
    if (!raiz.offsetWidth) return;
    const w = texto.length * 10.2 + 26, h = 32;
    const g = svg('g', { class: 'org-seta ' + (classe || '') },
      svg('g', { class: 'org-seta-rot', transform: `translate(${x} ${y})` },
        svg('rect', { x: -w / 2, y: -h / 2, width: w, height: h, rx: 9 }),
        svg('text', { x: 0, y: 1 }, texto)));
    g.style.setProperty('--d', (atraso || 0) + 'ms');
    camadaSetas(raiz).append(g);
  }

  // reinicia animações CSS de um conjunto de classes
  function reiniciar(raiz) { void raiz.offsetWidth; }

  // ═══════════════════════════════════════════════════════════════════════════
  // 1. organizacao/mapa — namespaces como caixas; resolver um nome é descer
  // ═══════════════════════════════════════════════════════════════════════════
  const MAPA = [
    { chamada: 'empresa::produto::v1::versao()', caminho: ['empresa', 'produto', 'v1'], alvo: 'versao', res: '"produto v1"',
      saida: ['      aninhado clássico:  empresa::produto::v1::versao() → produto v1'],
      nota: 'Cada <code>::</code> desce uma caixa: <code>empresa</code> → <code>produto</code> → <code>v1</code>.' },
    { chamada: 'outra::camada::interna::onde()', caminho: ['outra', 'camada', 'interna'], alvo: 'onde', res: '"outra::camada::interna"',
      saida: ['      aninhado C++17:     namespace outra::camada::interna { } → outra::camada::interna'],
      nota: 'Escrito numa linha só (C++17) — mas são três namespaces, um dentro do outro.' },
    { chamada: 'pv1::versao()', caminho: ['empresa', 'produto', 'v1'], alias: true, alvo: 'versao', res: '"produto v1"',
      saida: ['      aninhado clássico:  empresa::produto::v1::versao() → produto v1',
        '      alias:              namespace pv1 = empresa::produto::v1;'],
      nota: '<code>pv1</code> não é um namespace novo: é outro nome para o mesmo caminho.' },
    { chamada: 'biblioteca::api()', caminho: ['biblioteca', 'v2'], vaza: true, alvo: 'api2', res: '"v2 (a atual)"',
      saida: ['        biblioteca::api()     → v2 (a atual)  ← resolve para o inline'],
      nota: 'Não há <code>api</code> direto em <code>biblioteca</code>: os membros do <b>inline</b> <code>v2</code> contam como se estivessem lá.' },
    { chamada: 'biblioteca::v1::api()', caminho: ['biblioteca', 'v1b'], alvo: 'api1', res: '"v1 (legada)"',
      saida: ['        biblioteca::v1::api() → v1 (legada)  ← a antiga continua acessível'],
      nota: 'A versão antiga continua lá, pelo nome completo: quem depende dela não quebra.' },
    { chamada: 'so_deste_arquivo()', caminho: ['anon'], alvo: 'sda', res: '1, depois 2', muro: true,
      saida: ['        so_deste_arquivo() = 1, depois 2 ← invisível a outras .cpp'],
      nota: 'Visível sem qualificar em toda <b>esta</b> TU; outra .cpp nem consegue nomeá-lo.' },
  ];

  function nsBox(id, nome, classe, ...filhos) {
    return el('div', { class: 'org-ns ' + (classe || ''), dataset: { id } },
      el('div', { class: 'org-ns-nome' }, nome),
      el('div', { class: 'org-ns-dentro' }, ...filhos));
  }
  function membro(id, txt, classe) { return el('span', { class: 'org-membro ' + (classe || ''), dataset: { id } }, txt); }

  Deck.widget('organizacao/mapa', {
    iniciar(raiz) {
      raiz.classList.add('org-mapa');
      // duas colunas independentes; o alias pv1 fica logo abaixo de `empresa`, para a seta
      // do alias ser curta: sobe e entra em v1
      const arvore = el('div', { class: 'org-mapa-arvore' },
        nsBox('raiz', 'namespace demo_organizacao', 'org-raiz',
          el('div', { class: 'org-mapa-grade' },
            el('div', { class: 'org-mapa-col' },
              nsBox('empresa', 'empresa', '', nsBox('produto', 'produto', '', nsBox('v1', 'v1', '', membro('versao', 'versao()')))),
              el('div', { class: 'org-alias', dataset: { id: 'alias' } }, el('b', null, 'pv1'), el('span', null, '= empresa::produto::v1')),
              nsBox('anon', 'namespace { } — anônimo', 'org-anon', membro('cp', 'contador_privado'), membro('sda', 'so_deste_arquivo()'),
                el('span', { class: 'org-muro' }, '✗ outra .cpp não enxerga'))),
            el('div', { class: 'org-mapa-col' },
              nsBox('outra', 'outra', 'org-c17', nsBox('camada', 'camada', '', nsBox('interna', 'interna', '', membro('onde', 'onde()')))),
              // o "fantasma" (api() visível direto em biblioteca) tem lugar reservado no
              // fim da caixa: aparece no passo do inline sem empurrar o resto do mapa
              nsBox('biblioteca', 'biblioteca', 'org-bib',
                el('div', { class: 'org-bib-filhos' },
                  nsBox('v2', 'inline v2', 'org-inline', membro('api2', 'api()')),
                  nsBox('v1b', 'v1', '', membro('api1', 'api()'))),
                membro('fantasma', 'api()  ⟵ do inline v2', 'org-fantasma'))))));
      const chamadas = el('div', { class: 'org-chamadas' });
      raiz._botoes = MAPA.map((c, i) => {
        const b = botao(c.chamada, () => mapaMostrar(raiz, i, true), 'org-mono');
        chamadas.append(b);
        return b;
      });
      raiz._res = el('div', { class: 'org-mapa-res' });
      raiz._term = el('div', { class: 'org-mapa-term' });
      raiz.append(arvore, el('div', { class: 'org-mapa-painel' }, el('p', { class: 'org-rotulo-mini' }, 'chamado de dentro de demo_organizacao'), chamadas, raiz._res), raiz._term);
      raiz._por = {};
      raiz.querySelectorAll('[data-id]').forEach((n) => { raiz._por[n.dataset.id] = n; });
    },
    passo(raiz, k, ctx, dir) { mapaMostrar(raiz, k - 1, dir > 0 && !ctx.reduzido); },
    entrar(raiz) { requestAnimationFrame(() => raiz._redesenhar && raiz._redesenhar()); },
  });

  function mapaMostrar(raiz, i, animar) {
    raiz.classList.toggle('org-sem-anim', !animar);
    Object.values(raiz._por).forEach((n) => n.classList.remove('org-caminho', 'org-alvo', 'org-vazou', 'org-murado'));
    raiz.classList.remove('org-vaza', 'org-com-muro');
    raiz._botoes.forEach((b, j) => b.setAttribute('aria-pressed', String(j === i)));
    limparSetas(raiz);
    reiniciar(raiz);
    raiz._redesenhar = null;
    if (i < 0) {
      raiz._res.replaceChildren(el('p', { class: 'org-dica' }, 'Avance (→) ou clique num nome: o caminho acende, caixa por caixa.'));
      raiz._term.replaceChildren(terminal('saída real — ./dist/bin/syntax', [{ s: '── 6.1 Namespaces ──', t: 'secao' }]));
      return;
    }
    const c = MAPA[i];
    const seq = ['raiz', ...(c.alias ? ['alias'] : []), ...c.caminho];
    seq.forEach((id, j) => { const n = raiz._por[id]; n.classList.add('org-caminho'); n.style.setProperty('--d', j * 230 + 'ms'); });
    const alvo = raiz._por[c.alvo];
    alvo.classList.add('org-alvo'); alvo.style.setProperty('--d', seq.length * 230 + 'ms');
    if (c.vaza) { raiz.classList.add('org-vaza'); raiz._por.fantasma.style.setProperty('--d', (seq.length + 1) * 230 + 'ms'); }
    if (c.muro) raiz.classList.add('org-com-muro');
    // cada "›" anda junto com o chip seguinte (a quebra de linha nunca deixa um "›" sozinho)
    const chips = el('div', { class: 'org-caminho-chips' });
    const par = (j, ...xs) => (j ? el('span', { class: 'org-chip-par' }, el('span', { class: 'org-sep' }, '›'), ...xs) : xs[0]);
    seq.filter((id) => id !== 'raiz').forEach((id, j) => {
      const nome = id === 'alias' ? 'pv1 (alias)' : id === 'v1b' ? 'v1' : id === 'v2' ? 'inline v2' : id === 'anon' ? '(anônimo)' : id;
      chips.append(par(j, el('span', { class: 'org-chip' }, nome)));
    });
    chips.append(par(1, el('span', { class: 'org-chip org-chip-alvo' }, alvo.textContent.replace(/\s.*$/, ''))));
    raiz._res.replaceChildren(
      el('div', { class: 'org-mapa-chamada', html: cpp(c.chamada) }),
      chips,
      el('div', { class: 'org-mapa-valor' }, el('span', null, '→ '), el('b', null, c.res)),
      el('p', { class: 'org-nota', html: c.nota }));
    raiz._term.replaceChildren(terminal('saída real — ./dist/bin/syntax', c.saida));
    raiz._redesenhar = () => {
      limparSetas(raiz);
      if (c.alias) seta(raiz, raiz._por.alias, raiz._por.v1, 'org-seta-cor', 260);
      if (c.vaza) {
        const a = pos(raiz, raiz._por.api2), f = pos(raiz, raiz._por.fantasma);
        const p1 = { x: a.x - 2, y: a.y + a.h / 2 }, p2 = { x: f.x - 6, y: f.y + f.h / 2 };
        const bojo = Math.min(p1.x, p2.x) - 34;
        curva(raiz, p1, { x: bojo, y: p1.y }, { x: bojo, y: p2.y }, p2, 'org-seta-vaza', (seq.length + 1) * 230);
      }
    };
    raiz._redesenhar();
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 2. organizacao/adl — busca comum (para fora) × ADL (namespace do argumento)
  // ═══════════════════════════════════════════════════════════════════════════
  const ADL = {
    norma: {
      botao: 'norma_quadrada(v)',
      arvore: { id: 'global', nome: ':: (global)', filhos: [
        { id: 'demo', nome: 'namespace demo_organizacao', filhos: [
          { id: 'geometria', nome: 'namespace geometria', membros: [['vetor', 'struct Vetor'], ['norma', 'norma_quadrada(const Vetor&)']] },
          { id: 'funcao', nome: 'void demo_namespaces() { … }', classe: 'org-fn', bloco: ['geometria::Vetor v{3.0, 4.0};'],
            chamada: [['norma_quadrada('], ['v', 'geometria::Vetor', 'geometria'], [');']], tipos: 'v : geometria::Vetor' },
        ] },
      ] },
      comum: ['funcao', 'demo', 'global'],
      achouComum: null,
      txtComum: 'Bloco → <code>demo_organizacao</code> → <code>::</code>: nenhum <code>norma_quadrada</code>. <code>geometria</code> é vizinha, não envolve a chamada — a busca comum nem entra lá.',
      // linha real (g++ 13.3, `(norma_quadrada)(v)`), sem o prefixo arquivo:linha:coluna
      semAdl: '‘norma_quadrada’ was not declared in this scope; did you mean ‘demo_organizacao::geometria::norma_quadrada’?',
      txtAdl: 'O argumento <code>v</code> é um <code>geometria::Vetor</code> → o namespace <b>geometria</b> entra na busca → achou.',
      candidatos: [['norma', 'geometria::norma_quadrada', true]],
      saidaTit: 'saída real — ./dist/bin/syntax',
      saida: ['        norma_quadrada(v) = 25 — chamado SEM qualificar `geometria::`'],
    },
    cout: {
      botao: 'std::cout << s',
      arvore: { id: 'global', nome: ':: (global)', filhos: [
        { id: 'std', nome: 'namespace std', membros: [['ostream', 'extern ostream cout;'], ['string', 'using string = basic_string<char>;'], ['op', 'operator<<(ostream&, const string&)']] },
        { id: 'funcao', nome: 'int main() { … }', classe: 'org-fn', bloco: ['std::string s = "oi";'],
          chamada: [['std::cout', 'std::ostream', 'std'], [' << '], ['s', 'std::string', 'std'], [';']], tipos: 'std::ostream << std::string', equivale: '≡ operator<<(std::cout, s)' },
      ] },
      comum: ['funcao', 'global'],
      achouComum: null,
      txtComum: '<code>main()</code> → <code>::</code>: nenhum <code>operator&lt;&lt;</code> que aceite <code>string</code>.',
      semAdl: null,
      txtAdl: 'Os argumentos são <code>std::ostream</code> e <code>std::string</code> → <b>std</b> entra na busca → <code>std::operator&lt;&lt;</code>. Sem ADL, seria preciso escrever <code>std::operator&lt;&lt;(std::cout, s)</code>.',
      candidatos: [['op', 'std::operator<<(ostream&, const string&)', true]],
      saidaTit: 'adl.cpp — operator<<(std::cout, s), sem std::',
      saida: ['oi'],
    },
    swapbuf: {
      botao: 'swap(a, b) — Buffer',
      arvore: { id: 'global', nome: ':: (global)', grade: true, filhos: [
        { id: 'std', nome: 'namespace std', membros: [['stdswap', 'template<class T> void swap(T&, T&)']] },
        { id: 'minha', nome: 'namespace minha', membros: [['buffer', 'struct Buffer { int* dados; };'], ['minhaswap', 'void swap(Buffer&, Buffer&) noexcept']] },
        { id: 'funcao', nome: 'template<class T> void trocar(T& a, T& b)   [T = minha::Buffer]', classe: 'org-fn org-largo', bloco: ['using std::swap;   // plano B'],
          chamada: [['swap('], ['a', 'minha::Buffer', 'minha'], [', '], ['b', 'minha::Buffer', 'minha'], [');']], tipos: 'a, b : minha::Buffer' },
      ] },
      comum: ['funcao'],
      achouComum: 'stdswap',
      txtComum: 'Já no bloco: o <code>using std::swap;</code> faz a busca comum achar <code>std::swap</code> — e uma using-declaration <b>não</b> desliga a ADL.',
      semAdl: null,
      txtAdl: 'Os argumentos são <code>minha::Buffer</code> → <b>minha</b> entra → <code>minha::swap</code>. Na sobrecarga, o não-template exato vence o template.',
      candidatos: [['stdswap', 'std::swap<Buffer> (template)', false], ['minhaswap', 'minha::swap (exato)', true]],
      saidaTit: 'adl.cpp — trocar(a, b) com minha::Buffer',
      saida: ['trocar(Buffer):', '  minha::swap (troca 1 ponteiro)'],
    },
    swapint: {
      botao: 'swap(i, j) — int',
      arvore: { id: 'global', nome: ':: (global)', grade: true, filhos: [
        { id: 'std', nome: 'namespace std', membros: [['stdswap', 'template<class T> void swap(T&, T&)']] },
        { id: 'minha', nome: 'namespace minha', membros: [['buffer', 'struct Buffer { int* dados; };'], ['minhaswap', 'void swap(Buffer&, Buffer&) noexcept']] },
        { id: 'funcao', nome: 'template<class T> void trocar(T& a, T& b)   [T = int]', classe: 'org-fn org-largo', bloco: ['using std::swap;   // plano B'],
          chamada: [['swap('], ['a', 'int', null], [', '], ['b', 'int', null], [');']], tipos: 'a, b : int' },
      ] },
      comum: ['funcao'],
      achouComum: 'stdswap',
      txtComum: 'O <code>using std::swap;</code> do bloco: a busca comum acha <code>std::swap</code>.',
      semAdl: null,
      txtAdl: '<code>int</code> é um tipo fundamental: <b>nenhum</b> namespace associado. Sobra o plano B, <code>std::swap</code>.',
      candidatos: [['stdswap', 'std::swap<int>', true]],
      saidaTit: 'adl.cpp — trocar(i, j) com int',
      saida: ['trocar(int) -> i=2 j=1 (std::swap)'],
    },
  };
  const ADL_ORDEM = ['norma', 'cout', 'swapbuf', 'swapint'];
  const ADL_PASSOS = [['norma', 0], ['norma', 1], ['norma', 2], ['cout', 2], ['swapbuf', 2], ['swapint', 2]];

  function adlEscopo(sp, por) {
    const dentro = el('div', { class: 'org-esc-dentro' + (sp.filhos ? ' org-esc-filhos' : '') + (sp.grade ? ' org-esc-grade' : '') });
    const box = el('div', { class: 'org-esc ' + (sp.classe || ''), dataset: { id: sp.id } },
      el('div', { class: 'org-esc-nome' }, sp.nome, el('span', { class: 'org-esc-tag' })), dentro);
    por[sp.id] = box;
    (sp.membros || []).forEach(([id, txt]) => {
      const m = el('div', { class: 'org-esc-membro', dataset: { id }, html: cpp(txt) });
      por[id] = m; dentro.append(m);
    });
    if (sp.bloco) {
      const cod = el('div', { class: 'org-esc-bloco' });
      sp.bloco.forEach((l) => cod.append(el('div', { class: 'org-esc-linha', html: cpp(l) })));
      const ch = el('div', { class: 'org-esc-linha org-esc-chamada' });
      sp.chamada.forEach(([t, tipo, ns]) => {
        if (tipo) {
          const a = el('span', { class: 'org-arg', dataset: { ns: ns || '' }, html: cppSeg(t) });
          ch.append(a);
          (por._args = por._args || []).push(a);
        } else ch.append(el('span', { html: cppSeg(t) }));
      });
      cod.append(ch);
      if (sp.tipos) { const t = el('div', { class: 'org-esc-linha org-tipos', html: '<span class="tk-com">// ' + esc(sp.tipos) + '</span>' }); cod.append(t); por._tipos = t; }
      if (sp.equivale) cod.append(el('div', { class: 'org-esc-linha org-equivale', html: '<span class="tk-com">// ' + esc(sp.equivale) + '</span>' }));
      dentro.append(cod);
    }
    (sp.filhos || []).forEach((f) => dentro.append(adlEscopo(f, por)));
    return box;
  }

  Deck.widget('organizacao/adl', {
    iniciar(raiz) {
      raiz.classList.add('org-adl');
      raiz._diag = el('div', { class: 'org-adl-diag' });
      const modos = el('div', { class: 'org-adl-modos' });
      raiz._botoes = {};
      ADL_ORDEM.forEach((m) => { const b = botao(ADL[m].botao, () => adlMostrar(raiz, m, 2, true), 'org-mono'); raiz._botoes[m] = b; modos.append(b); });
      raiz._painel = el('div', { class: 'org-adl-painel' });
      raiz._res = el('div', { class: 'org-adl-res' });
      raiz.append(el('div', { class: 'org-adl-esq' }, raiz._diag, raiz._res), el('div', { class: 'org-adl-lado' }, modos, raiz._painel));
    },
    passo(raiz, k, ctx, dir) { const [m, f] = ADL_PASSOS[k]; adlMostrar(raiz, m, f, dir > 0 && !ctx.reduzido); },
    entrar(raiz) { requestAnimationFrame(() => raiz._redesenhar && raiz._redesenhar()); },
  });

  function adlMostrar(raiz, modo, fase, animar) {
    const d = ADL[modo];
    raiz.classList.toggle('org-sem-anim', !animar);
    // da fase 1 para a 2 no MESMO caso: a busca comum já aconteceu na tela — fica parada,
    // e a animação vai direto à ADL (num caso novo, a busca inteira roda de novo)
    const ant = raiz._adlAnt;
    const continua = animar && ant && ant.modo === modo && ant.fase === fase - 1;
    raiz._adlAnt = { modo, fase };
    raiz.classList.toggle('org-adl-ja', !!continua);
    Object.entries(raiz._botoes).forEach(([m, b]) => b.setAttribute('aria-pressed', String(m === modo)));
    limparSetas(raiz);
    const por = {};
    raiz._diag.replaceChildren(adlEscopo(d.arvore, por));
    const T = 330;                                   // ms por anel da busca comum
    if (fase >= 1) {
      d.comum.forEach((id, j) => {
        const n = por[id];
        n.classList.add('org-buscado'); n.style.setProperty('--d', j * T + 'ms');
        const achou = d.achouComum && j === 0;
        n.querySelector(':scope > .org-esc-nome > .org-esc-tag').textContent = achou ? '✓ achou' : '✗ nada';
        n.classList.toggle('org-tag-ok', !!achou);
      });
      if (d.achouComum) { por[d.achouComum].classList.add('org-candidato'); por[d.achouComum].style.setProperty('--d', T + 'ms'); }
    }
    const tADL = continua ? 150 : d.comum.length * T + 200;
    if (fase >= 2) {
      (por._args || []).forEach((a, j) => {
        a.classList.add('org-arg-on'); a.style.setProperty('--d', tADL + j * 120 + 'ms');
        if (a.dataset.ns) { por[a.dataset.ns].classList.add('org-associado'); por[a.dataset.ns].style.setProperty('--d', tADL + 250 + 'ms'); }
      });
      if (por._tipos) { por._tipos.classList.add('org-tipos-on'); por._tipos.style.setProperty('--d', tADL + 'ms'); }
      d.candidatos.forEach(([id, , vence]) => {
        if (!por[id]) return;
        por[id].classList.add('org-candidato'); por[id].classList.toggle('org-vence', vence);
        por[id].style.setProperty('--d', tADL + 520 + 'ms');
      });
    }
    // painel
    const f1 = el('div', { class: 'org-fase' + (fase >= 1 ? ' org-fase-on' : '') },
      el('div', { class: 'org-fase-tit' }, el('b', null, '1'), ' busca comum — de dentro para fora'),
      el('p', { html: fase >= 1 ? d.txtComum : '<span class="apagado">…</span>' }));
    if (fase === 1 && d.semAdl) f1.append(terminal('se não houvesse ADL — g++ 13.3', [{ s: 'error: ' + d.semAdl, t: 'erro' }], 'org-term-mini'));
    const f2 = el('div', { class: 'org-fase org-fase-2' + (fase >= 2 ? ' org-fase-on' : '') },
      el('div', { class: 'org-fase-tit' }, el('b', null, '2'), ' ADL — os namespaces dos argumentos'),
      el('p', { html: fase >= 2 ? d.txtAdl : '<span class="apagado">…</span>' }));
    f1.style.setProperty('--d', '0ms'); f2.style.setProperty('--d', tADL + 'ms');
    const partes = [];
    if (fase >= 2) {
      const cand = el('div', { class: 'org-candidatos' }, el('span', { class: 'org-rotulo-mini' }, 'candidatos'));
      d.candidatos.forEach(([, nome, vence]) => cand.append(el('span', { class: 'org-cand' + (vence ? ' org-cand-vence' : '') }, (vence ? '✓ ' : '') + nome)));
      cand.style.setProperty('--d', tADL + 650 + 'ms');
      const t = terminal(d.saidaTit, d.saida, 'org-term-mini org-adl-saida');
      t.style.setProperty('--d', tADL + 800 + 'ms');
      partes.push(cand, t);
    }
    raiz._painel.replaceChildren(f1, f2);
    raiz._res.replaceChildren(...partes);
    // a seta da ADL: sai do quadro da função, na altura da linha que diz o TIPO do
    // argumento, e chega ao candidato que a ADL achou no namespace desse tipo
    raiz._redesenhar = () => {
      limparSetas(raiz);
      if (fase < 2) return;
      const feitos = new Set();
      (por._args || []).forEach((a) => {
        const ns = a.dataset.ns;
        if (!ns || feitos.has(ns)) return;
        feitos.add(ns);
        const F = pos(raiz, por.funcao), T = pos(raiz, por[ns]);
        const L = pos(raiz, por._tipos || a);
        const alvo = d.candidatos.map(([id]) => por[id]).find((n) => n && por[ns].contains(n));
        const M = alvo ? pos(raiz, alvo) : T;
        if (T.x + T.w <= F.x) {              // namespace à esquerda do quadro da função
          const p1 = { x: F.x - 3, y: L.y + L.h / 2 }, p2 = { x: M.x + M.w + 9, y: M.y + M.h / 2 };
          const m = (p1.x - p2.x) * 0.45;
          curva(raiz, p1, { x: p1.x - m, y: p1.y }, { x: p2.x + m, y: p2.y }, p2, 'org-seta-info', tADL + 150, 'ADL');
        } else {                              // namespace acima do quadro
          const cx = Math.max(F.x + 30, Math.min(F.x + F.w - 30, M.x + M.w / 2));
          const p1 = { x: cx, y: F.y - 3 }, p2 = { x: cx, y: M.y + M.h + 9 };
          const m = (p1.y - p2.y) * 0.4;
          curva(raiz, p1, { x: cx, y: p1.y - m }, { x: cx, y: p2.y + m }, p2, 'org-seta-info', tADL + 150, 'ADL');
        }
      });
      // nenhum argumento com namespace (int): o corredor da seta fica vazio — e diz por quê
      if (!feitos.size && por.minha) {
        const F = pos(raiz, por.funcao), N = pos(raiz, por.minha);
        etiqueta(raiz, F.x + F.w / 2, (N.y + N.h + F.y) / 2, 'ADL: int não traz namespace nenhum', 'org-seta-nada', tADL + 150);
      }
    };
    raiz._redesenhar();
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 3. organizacao/include — o #include colando texto (e a guarda)
  // ═══════════════════════════════════════════════════════════════════════════
  const INC_MODOS = {
    nada: { rotulo: 'sem guarda', ponto: ['struct Ponto { int x, y; };'] },
    once: { rotulo: '#pragma once', ponto: ['#pragma once', 'struct Ponto { int x, y; };'] },
    guarda: { rotulo: '#ifndef PONTO_HPP', ponto: ['#ifndef PONTO_HPP', '#define PONTO_HPP', 'struct Ponto { int x, y; };', '#endif'] },
  };
  const INC_FORMA = ['#include "ponto.hpp"', 'struct Forma { Ponto origem; };'];
  const INC_MAIN = ['#include "ponto.hpp"', '#include "forma.hpp"', 'int main() { Forma f{}; return f.origem.x; }'];
  // etapas: 0 nada colado · 1 main.cpp:1 colou ponto.hpp · 2 main.cpp:2 colou forma.hpp (que
  // cola ponto.hpp DE NOVO) · 3 o resto do main.cpp + o veredito do compilador
  const INC_PASSOS = [['nada', 0], ['nada', 1], ['nada', 2], ['nada', 3], ['once', 3], ['guarda', 3]];
  const INC_ERRO = [
    '$ g++ -std=c++20 -Wall main.cpp',
    'In file included from forma.hpp:1,',
    '                 from main.cpp:2:',
    'ponto.hpp:1:8: error: redefinition of ‘struct Ponto’',
    '…',
    'ponto.hpp:1:8: note: previous definition of ‘struct Ponto’',
  ];

  function arquivoMini(nome, linhas, destaque) {
    const cod = el('div', { class: 'org-arq-cod' });
    linhas.forEach((l, i) => cod.append(el('div', { class: 'org-arq-ln' + (destaque && destaque.includes(i) ? ' org-ln-on' : '') + (/^#/.test(l) ? ' org-ln-dir' : ''), html: cppSeg(l) })));
    return el('div', { class: 'org-arq' }, el('div', { class: 'org-arq-nome' }, nome), cod);
  }

  Deck.widget('organizacao/include', {
    iniciar(raiz) {
      raiz.classList.add('org-inc');
      raiz._arqs = el('div', { class: 'org-inc-arqs' });
      raiz._modos = el('div', { class: 'org-inc-modos' }, el('span', { class: 'org-rotulo-mini' }, 'ponto.hpp começa com:'));
      raiz._bm = {};
      Object.entries(INC_MODOS).forEach(([m, d]) => { const b = botao(d.rotulo, () => incMostrar(raiz, m, 3, true), 'org-mono'); raiz._bm[m] = b; raiz._modos.append(b); });
      raiz._tu = el('div', { class: 'org-inc-tu' });
      const stats = el('div', { class: 'org-inc-stats' },
        // medido (g++ 13.3, -std=c++20): linhas com conteúdo do `g++ -E -P` (grep -c '[^[:space:]]');
        // arquivos = headers distintos do `g++ -H`. main.cpp com as flags do build (compile_commands.json).
        el('div', { class: 'org-stat' }, el('div', { class: 'org-stat-num', html: '<b>192</b> arquivos · <b>29.315</b> linhas' }),
          el('div', { class: 'org-stat-txt', html: 'é o que um <code>#include &lt;iostream&gt;</code> cola' })),
        el('div', { class: 'org-stat' }, el('div', { class: 'org-stat-num', html: '<b>58</b> linhas → <b>69.543</b>' }),
          el('div', { class: 'org-stat-txt', html: '<code>src/syntax/main.cpp</code> e a sua TU (307 arquivos)' })));
      raiz.append(el('div', { class: 'org-inc-esq' }, raiz._modos, raiz._arqs, stats), raiz._tu);
    },
    passo(raiz, k, ctx, dir) { const [m, e] = INC_PASSOS[k]; incMostrar(raiz, m, e, dir > 0 && !ctx.reduzido); },
  });

  // quantos segmentos a TU tem em cada etapa (para animar só o que o passo acrescenta)
  const INC_NSEGS = [0, 1, 3, 4];

  function incMostrar(raiz, modo, etapa, animar) {
    raiz.classList.toggle('org-sem-anim', !animar);
    // avançou uma etapa no mesmo modo? então o que já estava colado fica parado
    const ant = raiz._incAnt;
    const jaVistos = animar && ant && ant.modo === modo && ant.etapa === etapa - 1 ? INC_NSEGS[ant.etapa] : 0;
    raiz._incAnt = { modo, etapa };
    Object.entries(raiz._bm).forEach(([m, b]) => b.setAttribute('aria-pressed', String(m === modo)));
    const ponto = INC_MODOS[modo].ponto;
    const dMain = [[], [0], [0, 1], [0, 1, 2]][etapa];
    raiz._arqs.replaceChildren(
      arquivoMini('ponto.hpp', ponto, modo !== 'nada' ? ponto.map((l, i) => (/^#/.test(l) ? i : -1)).filter((i) => i >= 0) : []),
      arquivoMini('forma.hpp', INC_FORMA, etapa >= 2 ? [0] : []),
      arquivoMini('main.cpp', INC_MAIN, dMain));
    const segs = [];
    const corpoPonto = ['struct Ponto { int x, y; };'];
    if (etapa >= 1) segs.push({ de: 'ponto.hpp', via: 'main.cpp:1', linhas: corpoPonto, cl: 'org-seg-ponto',
      nota: modo === 'guarda' ? '#ifndef PONTO_HPP → verdadeiro (ainda não definida): entra' : modo === 'once' ? 'primeira vez: cola' : '' });
    if (etapa >= 2) {
      if (modo === 'nada') segs.push({ de: 'ponto.hpp', via: 'forma.hpp:1 ← main.cpp:2', linhas: corpoPonto, cl: 'org-seg-ponto org-seg-dup', nota: 'de novo! a mesma struct, colada outra vez' });
      else segs.push({ de: 'ponto.hpp', via: 'forma.hpp:1 ← main.cpp:2', linhas: [], cl: 'org-seg-pulado',
        nota: modo === 'once' ? '#pragma once: este arquivo já foi colado — nada' : '#ifndef PONTO_HPP → falso: o bloco inteiro é descartado' });
      segs.push({ de: 'forma.hpp', via: 'main.cpp:2', linhas: ['struct Forma { Ponto origem; };'], cl: 'org-seg-forma' });
    }
    if (etapa >= 3) segs.push({ de: 'main.cpp', via: 'linha 3', linhas: ['int main() { Forma f{}; return f.origem.x; }'], cl: 'org-seg-main' });
    const lista = el('div', { class: 'org-tu-lista' });
    segs.forEach((s, i) => {
      const seg = el('div', { class: 'org-seg ' + s.cl },
        el('div', { class: 'org-seg-orig' }, el('b', null, s.de), el('span', null, ' ← ' + s.via)),
        ...s.linhas.map((l) => el('div', { class: 'org-seg-ln', html: cpp(l) })),
        s.nota ? el('div', { class: 'org-seg-nota' }, s.nota) : null);
      if (i < jaVistos) seg.classList.add('org-ja');
      seg.style.setProperty('--d', (Math.max(0, i - jaVistos) * 260) + 'ms');
      lista.append(seg);
    });
    const novos = segs.length - jaVistos;
    if (!segs.length) lista.append(el('p', { class: 'org-dica' }, 'Avance (→): o pré-processador vai trocar cada #include pelo texto do arquivo.'));
    if (etapa === 2 && modo === 'nada') {
      const q = el('p', { class: 'org-dica org-inc-pergunta' }, 'Duas struct Ponto na mesma TU. O que o compilador vai dizer? Avance (→).');
      q.style.setProperty('--d', (novos * 260 + 150) + 'ms');
      lista.append(q);
    }
    const partes = [el('div', { class: 'org-tu-tit' }, el('span', null, 'a unidade de tradução — o que o compilador recebe'), el('code', { class: 'org-tu-cmd' }, 'g++ -E main.cpp')), lista];
    if (etapa >= 3) {
      const v = modo === 'nada'
        ? terminal('g++ 13.3', INC_ERRO, 'org-term-mini org-inc-veredito')
        : el('div', { class: 'org-inc-ok' }, '✓ compila — ', el('code', null, 'g++ -std=c++20 -Wall main.cpp'), ' sem erro nem aviso');
      v.style.setProperty('--d', (novos * 260 + 150) + 'ms');
      partes.push(v);
    }
    raiz._tu.replaceChildren(...partes);
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 4. organizacao/expansor — a macro sendo colada, pedaço por pedaço
  // ═══════════════════════════════════════════════════════════════════════════
  const S = (s, c) => ({ s, c });           // texto da própria macro
  const Pm = (s, c) => ({ s, p: true, c }); // parâmetro na definição
  const A = (i, s, c) => ({ s, a: i, c });  // pedaço que veio do argumento i
  const DEF_Q = [S('#define QUADRADO('), Pm('x'), S(') (('), Pm('x'), S(') * ('), Pm('x'), S('))')];
  const DEF_LOG = [S('#define LOG('), Pm('fmt_str'), S(', ...) fmt::print("        [log] " '), Pm('fmt_str'), S(' "\\n" '), Pm('__VA_OPT__(,)', 'va'), S(' '), Pm('__VA_ARGS__', 'va'), S(')')];
  const CASOS = [
    { botao: 'VERSAO_SINTAXE', grupo: 'objeto', chamada: [S('VERSAO_SINTAXE')],
      def: [S('#define VERSAO_SINTAXE "1.0.0"')], exp: [S('"1.0.0"', 'str')],
      leitura: 'um literal comum, <span class="org-x-str">"1.0.0"</span> — o compilador nunca vê o nome <code>VERSAO_SINTAXE</code>',
      saida: '      Macro de objeto:  VERSAO_SINTAXE = 1.0.0' },
    { botao: 'QUADRADO(3)', grupo: 'função', chamada: [S('QUADRADO('), A(0, '3'), S(')')], def: DEF_Q,
      exp: [S('(('), A(0, '3'), S(') * ('), A(0, '3'), S('))')],
      leitura: '<span class="org-g">3 * 3</span> = <b>9</b>', esperado: '9', obtido: '9',
      saida: '        QUADRADO(3)          = 9' },
    { botao: 'QUADRADO(1 + 2)', grupo: 'função', chamada: [S('QUADRADO('), A(0, '1 + 2'), S(')')], def: DEF_Q,
      exp: [S('(('), A(0, '1 + 2'), S(') * ('), A(0, '1 + 2'), S('))')],
      leitura: '<span class="org-g">(1 + 2)</span> * <span class="org-g">(1 + 2)</span> = 3 * 3 = <b>9</b>', esperado: '9', obtido: '9',
      saida: '        QUADRADO(1 + 2)      = 9   ← ((1+2)*(1+2))' },
    { botao: 'QUADRADO_ERRADO(1+2)', grupo: 'função', chamada: [S('QUADRADO_ERRADO('), A(0, '1+2'), S(')')],
      def: [S('#define QUADRADO_ERRADO('), Pm('x'), S(') '), Pm('x'), S('* '), Pm('x')],
      exp: [A(0, '1+2'), S('* '), A(0, '1+2')],
      leitura: '1 + <span class="org-g org-g-ruim" data-rot="* antes de +">2 * 1</span> + 2 = <b>5</b>', esperado: '9', obtido: '5',
      saida: '        QUADRADO_ERRADO(1+2) = 5   ← vira 1+2*1+2 = 5. Precedência!' },
    { botao: 'QUADRADO(proximo())', grupo: 'função', chamada: [S('QUADRADO('), A(0, 'proximo()'), S(')')], def: DEF_Q,
      exp: [S('(('), A(0, 'proximo()'), S(') * ('), A(0, 'proximo()'), S('))')],
      leitura: '<span class="org-g" data-rot="1ª chamada → 1">proximo()</span> * <span class="org-g" data-rot="2ª chamada → 2">proximo()</span> = 1 * 2 = <b>2</b>',
      extra: 'contador_de_efeitos: 0 → 1 → <b>2</b> (e a ordem das duas chamadas nem é especificada)', esperado: '1', obtido: '2',
      saida: '        QUADRADO(proximo()) → resultado 2 (esperava-se 1), ' },
    { botao: 'MOSTRAR(a * b)', grupo: '#', chamada: [S('MOSTRAR('), A(0, 'a * b'), S(')')],
      def: [S('#define MOSTRAR('), Pm('expr'), S(') fmt::print("        {} = {}\\n", '), Pm('#expr', 'str'), S(', ('), Pm('expr'), S('))')],
      exp: [S('fmt::print("        {} = {}\\n", '), A(0, '"a * b"', 'str'), S(', ('), A(0, 'a * b'), S('))')],
      leitura: '<code>#expr</code> virou a string <span class="org-x-str">"a * b"</span>; o <code>(expr)</code> continua código, e vale 42',
      saida: '        a * b = 42' },
    { botao: 'DECLARAR_VAR(temporaria)', grupo: '##', chamada: [S('DECLARAR_VAR('), A(0, 'temporaria'), S(')')],
      def: [S('#define DECLARAR_VAR('), Pm('sufixo'), S(') int variavel_'), Pm('##', 'cola'), Pm('sufixo'), S(' = 0')],
      exp: [S('int '), S('variavel_', 'cola'), A(0, 'temporaria', 'cola'), S(' = 0')],
      leitura: 'um identificador <b>novo</b>, colado: <code>variavel_temporaria</code> — o demo depois faz <code>variavel_temporaria = 99</code>',
      saida: '        DECLARAR_VAR(temporaria) criou `variavel_temporaria` = 99' },
    { botao: 'LOG("sem argumentos")', grupo: '...', chamada: [S('LOG('), A(0, '"sem argumentos"'), S(')')], def: DEF_LOG,
      exp: [S('fmt::print("        [log] " '), A(0, '"sem argumentos"'), S(' "\\n" '), S(')')],
      leitura: 'sem argumentos extras, <code>__VA_OPT__(,)</code> não emite nada; os três literais vizinhos viram um só: <span class="org-x-str">"        [log] sem argumentos\\n"</span>',
      saida: '        [log] sem argumentos' },
    { botao: 'LOG("com {} e {}", 1, "dois")', grupo: '...', chamada: [S('LOG('), A(0, '"com {} e {}"'), S(', '), A(1, '1, "dois"'), S(')')], def: DEF_LOG,
      exp: [S('fmt::print("        [log] " '), A(0, '"com {} e {}"'), S(' "\\n" '), A(1, ',', 'va'), S(' '), A(1, '1, "dois"'), S(')')],
      leitura: 'agora há argumentos: <code>__VA_OPT__(,)</code> vira a vírgula e <code>__VA_ARGS__</code> vira <code>1, "dois"</code>',
      saida: '        [log] com 1 e dois' },
    { botao: 'TROCAR(x, y)', grupo: 'instrução', chamada: [S('TROCAR('), A(0, 'x'), S(', '), A(1, 'y'), S(')')],
      def: [S('#define TROCAR('), Pm('a'), S(', '), Pm('b'), S(') do { auto tmp_ = ('), Pm('a'), S('); ('), Pm('a'), S(') = ('), Pm('b'), S('); ('), Pm('b'), S(') = tmp_; } while (0)')],
      exp: [S('do { auto tmp_ = ('), A(0, 'x'), S('); ('), A(0, 'x'), S(') = ('), A(1, 'y'), S('); ('), A(1, 'y'), S(') = tmp_; } while (0)')],
      leitura: 'uma instrução só — o <code>;</code> que vem depois completa o <code>while (0)</code> (o porquê, no slide do <code>do-while(0)</code>)',
      saida: '        TROCAR(x, y) → x=2 y=1  (funciona mesmo em `if (c) TROCAR(x,y);`)' },
  ];
  // k=0..4 → índice do caso: macro de OBJETO, depois QUADRADO(1 + 2) (k=1 só a definição),
  // QUADRADO_ERRADO e a dupla avaliação
  const EXP_PASSOS = [0, 2, 2, 3, 4];
  const EXP_SO_DEF = 1;                         // passo que mostra só a chamada e a definição

  function pedacos(lista, modo, base) {
    // modo 'def' pinta parâmetros; 'exp'/'chamada' pinta os pedaços dos argumentos
    // (base = quanto do relógio da animação já passou: os atrasos começam dali)
    const linha = el('div', { class: 'org-x-cod' });
    let nA = 0;
    lista.forEach((p) => {
      const span = el('span', { class: 'org-x-p', html: cppSeg(p.s) });
      if (p.p) span.classList.add('org-x-param');
      if (p.a != null) {
        span.classList.add('org-x-arg', 'org-x-arg' + p.a);
        span.style.setProperty('--d', (modo === 'exp' ? Math.max(0, 620 + nA * 170 - (base || 0)) : 0) + 'ms');
        nA++;
      }
      if (p.c) span.classList.add('org-x-' + p.c);
      linha.append(span);
    });
    return linha;
  }

  Deck.widget('organizacao/expansor', {
    iniciar(raiz) {
      raiz.classList.add('org-exp');
      const escolha = el('div', { class: 'org-exp-escolha' });
      raiz._botoes = CASOS.map((c, i) => {
        const b = botao(c.botao, () => expMostrar(raiz, i, true), 'org-mono');
        b.dataset.grupo = c.grupo;
        escolha.append(b);
        return b;
      });
      raiz._palco = el('div', { class: 'org-exp-palco' });
      raiz.append(escolha, raiz._palco);
    },
    passo(raiz, k, ctx, dir) { expMostrar(raiz, EXP_PASSOS[k], dir > 0 && !ctx.reduzido, k === EXP_SO_DEF); },
  });

  function expMostrar(raiz, i, animar, soDef) {
    const c = CASOS[i];
    raiz.classList.toggle('org-sem-anim', !animar);
    raiz._botoes.forEach((b, j) => b.setAttribute('aria-pressed', String(j === i)));
    // do passo "só a definição" para a colagem do MESMO caso: as duas primeiras linhas já
    // estavam na tela e ficam paradas; a animação começa direto na colagem
    const ant = raiz._expAnt;
    const continua = animar && !soDef && ant && ant.i === i && ant.soDef;
    raiz._expAnt = { i, soDef };
    const base = continua ? 380 : 0;
    const txt = (l) => l.map((p) => p.s).join('');
    const maior = Math.max(txt(c.def).length, txt(c.exp).length, txt(c.chamada).length);
    // a linha mais longa (a definição do TROCAR, 94 colunas) ainda cabe a 18 px nos ~1130 px da caixa
    const fs = Math.max(18, Math.min(30, Math.floor(1100 / (maior * 0.62))));
    const linha = (rot, conteudo, cls, d) => {
      const n = el('div', { class: 'org-x-linha ' + (cls || '') }, el('div', { class: 'org-x-rot' }, rot), conteudo);
      n.style.setProperty('--d', Math.max(0, d - base) + 'ms');
      return n;
    };
    const partes = [
      linha('você escreve', pedacos(c.chamada, 'chamada'), 'org-x-chamada', 0),
      linha('a definição', pedacos(c.def, 'def'), 'org-x-def', 150),
    ];
    if (continua) partes.forEach((p) => p.classList.add('org-ja'));
    if (!soDef) {
      const lExp = linha('o pré-processador cola', pedacos(c.exp, 'exp', base), 'org-x-exp', 380);
      partes.push(lExp);
      const leit = el('div', { class: 'org-x-leitura' }, el('span', { html: c.leitura }));
      if (c.extra) leit.append(el('span', { class: 'org-x-extra', html: c.extra }));
      const nA = c.exp.filter((p) => p.a != null).length;
      const tLeit = 700 + nA * 170;
      partes.push(linha('o compilador lê', leit, 'org-x-le', tLeit));
      const fim = el('div', { class: 'org-x-fim' });
      if (c.esperado) {
        const ok = c.esperado === c.obtido;
        fim.append(el('span', { class: 'org-veredito ' + (ok ? 'org-v-ok' : 'org-v-ruim') }, ok ? '✓ ' : '✗ ', 'esperado ', el('b', null, c.esperado), ' · obtido ', el('b', null, c.obtido)));
      }
      fim.append(terminal('saída real — ./dist/bin/syntax', [c.saida], 'org-term-mini'));
      fim.style.setProperty('--d', Math.max(0, tLeit + 260 - base) + 'ms');
      partes.push(fim);
    } else {
      partes.push(el('p', { class: 'org-dica' }, 'Uma macro de função recebe TEXTO e devolve TEXTO. Avance (→) para ver a colagem.'));
    }
    const palco = el('div', { class: 'org-exp-linhas' }, ...partes);
    palco.style.setProperty('--fs', fs + 'px');
    raiz._palco.replaceChildren(palco);
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 5. organizacao/dowhile — o ; que sobra e o else órfão
  // ═══════════════════════════════════════════════════════════════════════════
  const DW = [
    { botao: '{ … }', tipo: 'erro',
      def: '#define TROCAR(a, b) { auto tmp_ = (a); (a) = (b); (b) = tmp_; }',
      uso: ['int x = 1, y = 2;', 'bool c = true;', 'if (c) TROCAR(x, y);', 'else   x = 0;'],
      exp: [[['if (c) ', 'cab'], ['{ auto tmp_ = (x); (x) = (y); (y) = tmp_; }', 'corpo', 'o if controla este bloco…'], [';', 'vazio', '…e este ; encerra o if']],
        [['else', 'orfao', 'else sem if!'], [' x = 0;', '']]],
      term: { tit: 'g++ -std=c++20 -Wall -Wextra -Wpedantic trocar_chaves.cpp -o tc', l: [
        'trocar_chaves.cpp: In function ‘int main()’:', 'trocar_chaves.cpp:6:5: error: ‘else’ without a previous ‘if’', '    6 |     else   x = 0;', '      |     ^~~~'] },
      explica: 'O <code>}</code> fecha o bloco; o <code>;</code> que o usuário escreveu vira uma instrução vazia e <b>encerra o if</b>. O <code>else</code> fica órfão: não compila.' },
    { botao: 'sem nada', tipo: 'aviso',
      def: '#define TROCAR(a, b) (a) ^= (b); (b) ^= (a); (a) ^= (b)',
      uso: ['int x = 1, y = 2;', 'bool c = false;', 'if (c) TROCAR(x, y);'],
      exp: [[['if (c) ', 'cab'], ['(x) ^= (y);', 'corpo', 'o if controla só isto'], [' '], ['(y) ^= (x); (x) ^= (y);', 'sempre', 'isto roda SEMPRE']]],
      term: { tit: 'g++ -std=c++20 -Wall -Wextra -Wpedantic trocar_nu.cpp -o tn && ./tn', l: [
        'trocar_nu.cpp:2:22: warning: macro expands to multiple statements [-Wmultistatement-macros]', '…',
        'trocar_nu.cpp:6:5: note: some parts of macro expansion are not guarded by this ‘if’ clause', '…', 'x=2 y=3'] },
      explica: 'Compila — e roda errado: com <code>c = false</code>, dois terços da troca rodam assim mesmo (<b>x=2 y=3</b>, era para ficar 1 e 2). Sem <code>-Wall</code>, nem um aviso; com ele, o GCC acusa.' },
    { botao: 'do { … } while (0)', tipo: 'ok',
      def: '#define TROCAR(a, b) do { auto tmp_ = (a); (a) = (b); (b) = tmp_; } while (0)',
      uso: ['int x = 1, y = 2;', 'bool c = true;', 'if (c) TROCAR(x, y);', 'else   x = 0;'],
      exp: [[['if (c) ', 'cab'], ['do { auto tmp_ = (x); (x) = (y); (y) = tmp_; } while (0);', 'corpo org-dw-ok', 'UMA instrução — e o ; é dela']],
        [['else', 'par', 'acha o seu if'], [' x = 0;', '']]],
      term: { tit: 'g++ -std=c++20 -Wall -Wextra -Wpedantic trocar_dowhile.cpp -o td && ./td', l: ['x=2 y=1'] },
      explica: '<code>do … while (0)</code> é uma instrução que <b>exige</b> o <code>;</code> — exatamente o que o usuário escreve. Roda uma vez, e o <code>else</code> encontra o seu <code>if</code>.' },
  ];

  Deck.widget('organizacao/dowhile', {
    iniciar(raiz) {
      raiz.classList.add('org-dw');
      const abas = el('div', { class: 'org-dw-abas' }, el('span', { class: 'org-rotulo-mini' }, 'a macro TROCAR, escrita como:'));
      raiz._botoes = DW.map((d, i) => { const b = botao(d.botao, () => dwMostrar(raiz, i, true, true), 'org-mono'); abas.append(b); return b; });
      raiz._palco = el('div', { class: 'org-dw-palco' });
      raiz.append(abas, raiz._palco);
    },
    passo(raiz, k, ctx, dir) { dwMostrar(raiz, Math.max(0, k - 1), dir > 0 && !ctx.reduzido, k > 0); },
  });

  function dwMostrar(raiz, i, animar, analisar) {
    const d = DW[i];
    raiz.classList.toggle('org-sem-anim', !animar);
    raiz._botoes.forEach((b, j) => b.setAttribute('aria-pressed', String(analisar && j === i)));
    const defBox = el('div', { class: 'org-dw-def', html: cpp(d.def) });
    const uso = el('div', { class: 'org-dw-uso' }, el('div', { class: 'org-rotulo-mini' }, 'o usuário escreve'),
      el('div', { class: 'org-dw-bloco' }, ...d.uso.map((l) => el('div', { class: 'org-dw-ln', html: cppSeg(l) }))));
    const exp = el('div', { class: 'org-dw-exp' }, el('div', { class: 'org-rotulo-mini' }, 'depois do pré-processador (g++ -E)'));
    let n = 0;
    d.exp.forEach((ln) => {
      const linha = el('div', { class: 'org-dw-ln' });
      ln.forEach(([t, cls, rot]) => {
        const s = el('span', { class: 'org-dw-s ' + (cls ? cls.split(' ').map((c) => (c.startsWith('org-') ? c : 'org-dw-' + c)).join(' ') : ''), html: cppSeg(t) });
        if (rot) { s.dataset.rot = rot; s.style.setProperty('--d', (300 + n++ * 380) + 'ms'); }
        linha.append(s);
      });
      exp.append(linha);
    });
    exp.classList.toggle('org-dw-analise', !!analisar);
    const partes = [el('div', { class: 'org-dw-topo' }, uso, el('div', { class: 'org-dw-defs' }, el('div', { class: 'org-rotulo-mini' }, 'a definição'), defBox)), exp];
    if (analisar) {
      const fim = el('div', { class: 'org-dw-fim org-dw-' + d.tipo },
        el('p', { class: 'org-dw-explica', html: d.explica }),
        terminal(d.term.tit, d.term.l, 'org-term-mini'));
      fim.style.setProperty('--d', (300 + n * 380 + 150) + 'ms');
      partes.push(fim);
    } else {
      partes.push(el('p', { class: 'org-dica' }, 'Por que não basta pôr chaves em volta? Avance (→).'));
    }
    raiz._palco.replaceChildren(...partes);
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 6. organizacao/condicional — o pré-processador apagando galhos
  // ═══════════════════════════════════════════════════════════════════════════
  const STDS = [['c++14', 201402], ['c++17', 201703], ['c++20', 202002], ['c++23', 202100]];
  const COND = [
    { d: '#ifdef NDEBUG', cond: (c) => c.nd, mostra: (c) => (c.nd ? 'NDEBUG definida' : 'NDEBUG não definida') },
    { s: '    print("NDEBUG definido    → RELEASE …");', vivo: (c) => c.nd },
    { d: '#else' },
    { s: '    print("NDEBUG não definido → DEBUG …");', vivo: (c) => !c.nd },
    { d: '#endif' },
    { d: '#if __cplusplus >= 202002L', cond: (c) => c.v >= 202002, mostra: (c) => c.v + 'L ≥ 202002L' },
    { s: '    print("… → C++20 ou superior");', vivo: (c) => c.v >= 202002 },
    { d: '#elif __cplusplus >= 201703L', elif: true, cond: (c) => c.v < 202002 && c.v >= 201703, mostra: (c) => c.v + 'L ≥ 201703L' },
    { s: '    print("… → C++17");', vivo: (c) => c.v < 202002 && c.v >= 201703 },
    { d: '#else' },
    { s: '    print("… → anterior ao C++17");', vivo: (c) => c.v < 201703 },
    { d: '#endif' },
    { d: '#if defined(__GNUC__) && !defined(__clang__)', cond: () => true, mostra: () => 'GCC 13.3: sim' },
    { s: '    print("__GNUC__ = {}.{} → GCC");', vivo: () => true },
    { d: '#endif' },
  ];
  const COND_SAIDA = {
    nd: ['        NDEBUG não definido → build de DEBUG (assert ativo)', '        NDEBUG definido    → build de RELEASE (assert desativado)'],
    v: { 201402: '        __cplusplus = 201402L → anterior ao C++17', 201703: '        __cplusplus = 201703L → C++17',
      202002: '        __cplusplus = 202002L → C++20 ou superior', 202100: '        __cplusplus = 202100L → C++20 ou superior' },
    gnu: '        __GNUC__ = 13.3 → GCC',
  };
  const COND_PASSOS = [['c++20', false], ['c++20', true], ['c++17', true], ['c++14', false], ['c++23', false]];

  Deck.widget('organizacao/condicional', {
    iniciar(raiz) {
      raiz.classList.add('org-cond');
      raiz._cod = el('div', { class: 'org-cond-cod' });
      raiz._estado = { std: 'c++20', nd: false };
      raiz._bnd = botao('-DNDEBUG', () => { raiz._estado.nd = !raiz._estado.nd; condMostrar(raiz, true); }, 'org-mono');
      raiz._bstd = {};
      const grupo = el('div', { class: 'org-seg-grupo' });
      STDS.forEach(([s]) => { const b = botao('-std=' + s, () => { raiz._estado.std = s; condMostrar(raiz, true); }, 'org-mono'); raiz._bstd[s] = b; grupo.append(b); });
      raiz._cmd = el('div', { class: 'org-cond-cmd' });
      raiz._macros = el('div', { class: 'org-cond-macros' });
      raiz._saida = el('div', { class: 'org-cond-saida' });
      raiz.append(
        el('div', { class: 'org-cond-esq' }, el('div', { class: 'org-rotulo-mini' }, 'demo_organizacao.hpp · 6.2 (resumido)'), raiz._cod),
        el('div', { class: 'org-cond-dir' },
          el('div', { class: 'org-cond-ctrl' }, el('span', { class: 'org-rotulo-mini' }, 'flags'), raiz._bnd, grupo),
          raiz._cmd, raiz._macros, raiz._saida));
    },
    passo(raiz, k, ctx, dir) {
      const [s, nd] = COND_PASSOS[k];
      raiz._estado = { std: s, nd };
      raiz._real = k === 0;
      condMostrar(raiz, dir > 0 && !ctx.reduzido);
    },
  });

  function condMostrar(raiz, animar) {
    const e = raiz._estado;
    const v = STDS.find(([s]) => s === e.std)[1];
    const c = { nd: e.nd, v };
    raiz.classList.toggle('org-sem-anim', !animar);
    raiz._bnd.setAttribute('aria-pressed', String(e.nd));
    Object.entries(raiz._bstd).forEach(([s, b]) => b.setAttribute('aria-pressed', String(s === e.std)));
    raiz._cod.replaceChildren(...COND.map((l, i) => {
      const n = el('div', { class: 'org-c-ln' }, el('span', { class: 'org-c-txt', html: cppSeg(l.d || l.s) }));
      n.style.setProperty('--d', i * 35 + 'ms');
      if (l.d) {
        n.classList.add('org-c-dir');
        if (l.cond) {
          const ok = l.cond(c);
          const na = l.elif && c.v >= 202002;
          n.append(el('span', { class: 'org-c-av ' + (na ? 'org-c-na' : ok ? 'org-c-sim' : 'org-c-nao') }, na ? '— já resolvido acima' : (ok ? '✓ ' : '✗ ') + l.mostra(c)));
        }
      } else n.classList.add(l.vivo(c) ? 'org-c-vivo' : 'org-c-morto');
      return n;
    }));
    raiz._cmd.innerHTML = '<span class="org-prompt">$ </span>' + esc(`g++ -std=${e.std}${e.nd ? ' -DNDEBUG' : ''} …`);
    const mac = [['NDEBUG', e.nd ? '1' : '(não definida)'], ['__cplusplus', v + 'L'], ['__GNUC__', '13'], ['__GNUC_MINOR__', '3'], ['__clang__', '(não definida)']];
    raiz._macros.replaceChildren(el('div', { class: 'org-rotulo-mini' }, 'o que o g++ 13.3 define com essas flags'),
      el('div', { class: 'org-cond-tab' }, ...mac.map(([m, val]) => el('div', { class: 'org-cond-mac' }, el('code', null, m), el('b', null, val)))));
    const real = raiz._real && e.std === 'c++20' && !e.nd;
    raiz._saida.replaceChildren(terminal(real ? 'saída real — ./dist/bin/syntax (o build do projeto)' : 'saída medida — o mesmo bloco, g++ 13.3',
      ['      Compilação condicional:', COND_SAIDA.nd[e.nd ? 1 : 0], COND_SAIDA.v[v], COND_SAIDA.gnu], 'org-term-mini'));
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 7. organizacao/atributos — cada atributo e a resposta REAL do compilador
  // ═══════════════════════════════════════════════════════════════════════════
  const ATRIB = [
    { nome: '[[nodiscard]]', v: 'cpp17', vt: 'C++17', papel: 'Retorno que não pode ser ignorado. No C++20, com motivo: <code>[[nodiscard("…")]]</code>.',
      arq: 'nodiscard.cpp', cod: '[[nodiscard]] int calcular_importante() { return 42; }\nint main() {\n    calcular_importante();          // retorno ignorado\n}',
      term: ['$ g++ -std=c++20 -Wall -Wextra -c nodiscard.cpp', 'nodiscard.cpp: In function ‘int main()’:',
        'nodiscard.cpp:3:24: warning: ignoring return value of ‘int calcular_importante()’, declared with attribute ‘nodiscard’ [-Wunused-result]', '…',
        'nodiscard.cpp:1:19: note: declared here', '…'] },
    { nome: '[[maybe_unused]]', v: 'cpp17', vt: 'C++17', papel: 'Cala o aviso de “não usado” — p.ex. uma variável lida só dentro de um <code>assert</code>.',
      arq: 'unused.cpp', cod: 'int main() {\n    int nao_usado = 0;\n    [[maybe_unused]] int talvez = 0;\n}',
      term: ['$ g++ -std=c++20 -Wall -Wextra -c unused.cpp', 'unused.cpp: In function ‘int main()’:', 'unused.cpp:2:9: warning: unused variable ‘nao_usado’ [-Wunused-variable]', '…'],
      nota: 'Sobre <code>talvez</code>, nenhuma palavra.' },
    { nome: '[[deprecated]]', v: 'cpp14', vt: 'C++14', papel: 'Avisa em cada uso — com a mensagem que você escolher.',
      arq: 'depr.cpp', cod: '[[deprecated("use nova()")]] void velha() {}\nint main() { velha(); }',
      term: ['$ g++ -std=c++20 -Wall -Wextra -c depr.cpp', 'depr.cpp: In function ‘int main()’:',
        'depr.cpp:2:19: warning: ‘void velha()’ is deprecated: use nova() [-Wdeprecated-declarations]', '…', 'depr.cpp:1:35: note: declared here', '…'] },
    { nome: '[[noreturn]]', v: 'cpp11', vt: 'C++11', papel: '“Esta função não volta” (abort, throw, exit): o compilador para de exigir o que viria depois.',
      arq: 'com.cpp', cod: '#include <cstdlib>\n[[noreturn]] void falhar(const char* motivo) { (void)motivo; std::abort(); }\nint sinal(int n) {\n    if (n > 0) return 1;\n    falhar("n <= 0");\n}',
      term: ['$ g++ -std=c++20 -Wall -Wextra -c sem.cpp', 'sem.cpp: In function ‘int sinal(int)’:',
        'sem.cpp:5:11: warning: control reaches end of non-void function [-Wreturn-type]', '…', '$ g++ -std=c++20 -Wall -Wextra -c com.cpp'],
      nota: '<code>sem.cpp</code> é o mesmo arquivo, sem o atributo. Com ele (<code>com.cpp</code>), nenhum aviso.' },
    { nome: '[[fallthrough]]', v: 'cpp17', vt: 'C++17', papel: '“A queda para o próximo <code>case</code> é de propósito.”',
      arq: 'fall.cpp', inicio: 3, cod: '    switch (n) {\n        case 1: r += 1;\n        case 2: r += 2; break;\n        case 3: r += 3; [[fallthrough]];\n        case 4: r += 4; break;\n    }',
      term: ['$ g++ -std=c++20 -Wall -Wextra -c fall.cpp', 'fall.cpp: In function ‘int f(int)’:',
        'fall.cpp:4:19: warning: this statement may fall through [-Wimplicit-fallthrough=]', '…', 'fall.cpp:5:9: note: here', '…'],
      nota: 'Só o <code>case 1</code> é acusado; o <code>case 3</code> declarou a intenção.' },
    { nome: '[[likely]] [[unlikely]]', v: 'cpp20', vt: 'C++20', papel: 'Dica de qual galho é o comum: muda o arranjo do código gerado, nunca o resultado.',
      arq: 'demo_organizacao.hpp', inicio: 237, cod: 'int classificar(int n) {\n    if (n > 0) [[likely]]   { return 1; }        // C++20: dica ao otimizador\n    else if (n < 0) [[unlikely]] { return -1; }\n    return 0;\n}',
      term: ['$ ./dist/bin/syntax', '…', '      [[likely]]/[[unlikely]] (C++20) → dica de ramificação: classificar(5)=1'] },
    { nome: '[[no_unique_address]]', v: 'cpp20', vt: 'C++20', papel: 'Um membro vazio pode não ocupar espaço próprio.',
      arq: 'nua.cpp', cod: '#include <cstdio>\nstruct Vazio {};                    // ex.: um alocador sem estado\nstruct Sem { int x; Vazio v; };\nstruct Com { int x; [[no_unique_address]] Vazio v; };\nint main() {\n    std::printf("sizeof(Vazio) = %zu\\n", sizeof(Vazio));\n    std::printf("sizeof(Sem)   = %zu\\n", sizeof(Sem));\n    std::printf("sizeof(Com)   = %zu\\n", sizeof(Com));\n}',
      term: ['$ g++ -std=c++20 nua.cpp -o nua && ./nua', 'sizeof(Vazio) = 1', 'sizeof(Sem)   = 8', 'sizeof(Com)   = 4'],
      nota: '<code>Sem</code> paga 4 bytes pelo membro vazio (1 dele + 3 de enchimento); <code>Com</code>, nenhum: <code>v</code> divide o endereço com <code>x</code>.' },
  ];

  Deck.widget('organizacao/atributos', {
    iniciar(raiz) {
      raiz.classList.add('org-atr');
      const lista = el('div', { class: 'org-atr-lista' });
      raiz._botoes = ATRIB.map((a, i) => {
        const b = el('button', { class: 'org-atr-item', type: 'button' }, el('span', { class: 'org-atr-nome' }, a.nome), el('span', { class: 'pill ' + a.v }, a.vt));
        b.addEventListener('click', (e) => { e.stopPropagation(); atrMostrar(raiz, i, true); });
        lista.append(b);
        return b;
      });
      raiz._palco = el('div', { class: 'org-atr-palco' });
      raiz.append(lista, raiz._palco);
    },
    passo(raiz, k, ctx, dir) { atrMostrar(raiz, Math.max(0, k - 1), dir > 0 && !ctx.reduzido, k === 0); },
  });

  function atrMostrar(raiz, i, animar, inicial) {
    const a = ATRIB[i];
    raiz.classList.toggle('org-sem-anim', !animar);
    raiz._botoes.forEach((b, j) => b.setAttribute('aria-pressed', String(!inicial && j === i)));
    if (inicial) {
      raiz._palco.replaceChildren(
        el('div', { class: 'org-atr-intro' },
          el('p', { class: 'lead', html: 'Um atributo é um <b>bilhete</b> para o compilador: <code>[[nome]]</code> antes da declaração ou do comando.' }),
          el('p', { class: 'pequeno suave', html: 'Quase sempre não muda o que o programa calcula — muda o que o compilador <b>avisa</b> e como ele <b>otimiza</b> (a exceção é <code>[[no_unique_address]]</code>, que muda o layout). Avance (→) para ver cada um, com a resposta real do g++ 13.3.' })));
      return;
    }
    const fig = Deck.util.codigo(a.cod, { lang: 'cpp', arquivo: a.arq, inicio: a.inicio || 1, classe: 'minimo' });
    const partes = [el('p', { class: 'org-atr-papel', html: '<b>' + esc(a.nome) + '</b> — ' + a.papel }), fig, terminal('g++ 13.3', a.term, 'org-term-mini')];
    if (a.nota) partes.push(el('p', { class: 'org-nota', html: a.nota }));
    partes.forEach((p, j) => p.style.setProperty('--d', j * 160 + 'ms'));
    raiz._palco.replaceChildren(...partes);
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 8. organizacao/nm — a tabela de linkage, lida no objeto
  // ═══════════════════════════════════════════════════════════════════════════
  // cor da letra do nm pelo ESCOPO do símbolo: global (maiúscula, ou o `u` do GNU, que é
  // minúsculo mas global) × local (as outras minúsculas)
  const corLetra = (l) => (l === 'u' || l !== l.toLowerCase() ? 'org-maius' : 'org-minus');
  const NM_COD = [
    'int                  global_a;',
    'static int           estatico_b;',
    'namespace { int      anonimo_c; }',
    'extern int           externo_d;',
    'inline int           inline_e = 1;',
    'const int            const_f = 1;',
    'extern const int     const_ext_g = 1;',
    'constexpr int        constexpr_h = 2;',
    'inline constexpr int inl_constexpr_i = 3;',
    'const void* usa[] = { &global_a, … };  // usa todas',
  ];
  const NM_SAIDA = [
    ['0000000000000004', 'b', '_ZL10estatico_b'],
    ['0000000000000008', 'r', '_ZL11constexpr_h'],
    ['0000000000000000', 'r', '_ZL7const_f'],
    ['0000000000000008', 'b', '_ZN12_GLOBAL__N_19anonimo_cE'],
    ['0000000000000004', 'R', 'const_ext_g'],
    ['                ', 'U', 'externo_d'],
    ['0000000000000000', 'B', 'global_a'],
    ['0000000000000000', 'u', 'inl_constexpr_i'],
    ['0000000000000000', 'u', 'inline_e'],
    ['0000000000000000', 'D', 'usa'],
  ];
  const NM_PASSOS = [
    { cod: [0], nm: [6], link: 'externa', txt: '<b>B</b> maiúsculo: símbolo <b>global</b> — linkage externa —, na seção .bss (zerada). Como está no namespace global, o nome nem ganha mangling.' },
    { cod: [1], nm: [0], link: 'interna', txt: '<b>b</b> minúsculo: símbolo <b>local</b> — só esta TU o vê. O <code>L</code> de <code>_ZL…</code> é o mangling marcando a linkage interna.' },
    { cod: [2], nm: [3], link: 'interna', txt: 'Também <b>b</b>: o namespace anônimo vira <code>_GLOBAL__N_1</code>. Interno — e, ao contrário do <code>static</code>, serve até para tipos.' },
    { cod: [3], nm: [5], link: 'externa', txt: '<b>U</b> = <i>undefined</i>: o .o <b>precisa</b> de <code>externo_d</code>. A definição tem de vir de outra TU — senão o link falha com <code>undefined reference</code>.' },
    // cod[k] liga a nm[k]: inline_e (cod 4) → nm 8; inl_constexpr_i (cod 8) → nm 7
    { cod: [4, 8], nm: [8, 7], link: 'externa · única', txt: '<b>u</b> = <i>unique global</i> (extensão GNU) — a exceção à regra: minúscula, mas <b>global</b>. Dez TUs podem incluir o header, o linker mantém <b>uma</b> instância. É o <code>inline</code> de variável do C++17.' },
    // const_f (cod 5) → _ZL7const_f (nm 2); constexpr_h (cod 7) → _ZL11constexpr_h (nm 1)
    { cod: [5, 7], nm: [2, 1], link: 'interna ⚠', alerta: true, txt: '⚠ <b>r</b> minúsculo: <code>const</code> (e <code>constexpr</code>) global é <b>interno</b> em C++ — em C seria externo. Cada TU que incluir o header tem a sua cópia.' },
    { cod: [6], nm: [4], link: 'externa', txt: '<b>R</b> maiúsculo: <code>extern const</code> devolve a linkage externa — uma definição, visível ao programa todo, em memória só de leitura.' },
  ];

  Deck.widget('organizacao/nm', {
    iniciar(raiz) {
      raiz.classList.add('org-nm');
      raiz._cod = NM_COD.map((l) => el('div', { class: 'org-nm-c', html: cppSeg(l) }));
      raiz._nm = NM_SAIDA.map(([end, letra, nome]) => el('div', { class: 'org-nm-l' },
        el('span', { class: 'org-nm-end' }, end), el('span', { class: 'org-nm-letra ' + corLetra(letra) }, letra), el('span', { class: 'org-nm-nome' }, nome)));
      raiz._exp = el('div', { class: 'org-nm-exp' });
      raiz.append(
        el('div', { class: 'org-nm-painel org-nm-esq' }, el('div', { class: 'org-nm-tit' }, el('b', null, 'linkage.cpp'), ' (resumido)'), el('div', { class: 'org-nm-corpo' }, ...raiz._cod)),
        el('div', { class: 'org-nm-painel org-nm-dir' }, el('div', { class: 'org-nm-tit', html: '<span class="org-prompt">$ </span>g++ -std=c++20 -c linkage.cpp &amp;&amp; nm linkage.o' }), el('div', { class: 'org-nm-corpo' }, ...raiz._nm)),
        raiz._exp);
    },
    passo(raiz, k, ctx, dir) { nmMostrar(raiz, k - 1, dir > 0 && !ctx.reduzido); },
    entrar(raiz) { requestAnimationFrame(() => raiz._redesenhar && raiz._redesenhar()); },
  });

  function nmMostrar(raiz, i, animar) {
    raiz.classList.toggle('org-sem-anim', !animar);
    raiz._cod.forEach((n) => n.classList.remove('org-on', 'org-alerta'));
    raiz._nm.forEach((n) => n.classList.remove('org-on', 'org-alerta'));
    limparSetas(raiz);
    reiniciar(raiz);
    raiz._redesenhar = null;
    if (i < 0) {
      raiz._exp.replaceChildren(
        el('div', { class: 'org-nm-legenda', html: 'Como ler o <code>nm</code>: <b>MAIÚSCULA</b> = global (linkage externa) · <b>minúscula</b> = local (interna) · ' +
          '<code>B/b</code> .bss · <code>D</code> dados · <code>R/r</code> só leitura · <code>U</code> indefinido · <code>u</code> global única (GNU) — a exceção: minúscula, mas global' }));
      return;
    }
    const p = NM_PASSOS[i];
    p.cod.forEach((j) => raiz._cod[j].classList.add('org-on'));
    p.nm.forEach((j) => raiz._nm[j].classList.add('org-on'));
    if (p.alerta) { p.cod.forEach((j) => raiz._cod[j].classList.add('org-alerta')); p.nm.forEach((j) => raiz._nm[j].classList.add('org-alerta')); }
    const letra = NM_SAIDA[p.nm[0]][1];
    raiz._exp.replaceChildren(
      el('div', { class: 'org-nm-badge' + (p.alerta ? ' org-alerta' : '') + ' ' + corLetra(letra) }, letra),
      el('div', { class: 'org-nm-txt' }, el('div', { class: 'org-nm-link' }, 'linkage: ', el('b', null, p.link)), el('p', { html: p.txt })));
    raiz._redesenhar = () => {
      limparSetas(raiz);
      p.cod.forEach((c, j) => seta(raiz, raiz._cod[c], raiz._nm[p.nm[Math.min(j, p.nm.length - 1)]], p.alerta ? 'org-seta-aviso' : 'org-seta-cor', 120));
    };
    raiz._redesenhar();
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 9. organizacao/odr — dois dobro(), o linker escolhe um
  // ═══════════════════════════════════════════════════════════════════════════
  const ODR_COD = {
    a: ['inline int dobro(int x) { return x * 2; }', 'void usa_em_a() { … dobro(10) … }'],
    b: ['inline int dobro(int x) { return x + x + 1; }   // ✗', 'void usa_em_b() { … dobro(10) … }'],
  };
  const ODR_SIMB = {
    O0: { a: [['W', '_Z5dobroi', 'dobro'], ['T', '_Z8usa_em_av'], ['U', 'printf']], b: [['W', '_Z5dobroi', 'dobro'], ['T', '_Z8usa_em_bv'], ['U', 'printf']] },
    O2: { a: [['r', '.LC0'], ['T', '_Z8usa_em_av'], ['U', '__printf_chk']], b: [['r', '.LC0'], ['T', '_Z8usa_em_bv'], ['U', '__printf_chk']] },
  };
  const ODR_SAIDA = {
    'O0-ab': ['a.cpp: dobro(10) = 20', 'b.cpp: dobro(10) = 20'],
    'O0-ba': ['a.cpp: dobro(10) = 21', 'b.cpp: dobro(10) = 21'],
    'O2-ab': ['a.cpp: dobro(10) = 20', 'b.cpp: dobro(10) = 21'],
    'O2-ba': ['a.cpp: dobro(10) = 20', 'b.cpp: dobro(10) = 21'],
  };
  const ODR_PASSOS = [['O0', 'ab', false], ['O0', 'ab', true], ['O0', 'ba', true], ['O2', 'ab', true]];

  Deck.widget('organizacao/odr', {
    iniciar(raiz) {
      raiz.classList.add('org-odr');
      raiz._st = { o: 'O0', ordem: 'ab', rodou: false };
      const mk = (rot, fn) => botao(rot, fn, 'org-mono');
      raiz._b = {
        O0: mk('-O0', () => { raiz._st.o = 'O0'; raiz._st.rodou = true; odrMostrar(raiz, true); }),
        O2: mk('-O2', () => { raiz._st.o = 'O2'; raiz._st.rodou = true; odrMostrar(raiz, true); }),
        ab: mk('main.cpp a.cpp b.cpp', () => { raiz._st.ordem = 'ab'; raiz._st.rodou = true; odrMostrar(raiz, true); }),
        ba: mk('main.cpp b.cpp a.cpp', () => { raiz._st.ordem = 'ba'; raiz._st.rodou = true; odrMostrar(raiz, true); }),
      };
      raiz._tus = el('div', { class: 'org-odr-tus' });
      raiz._ld = el('div', { class: 'org-odr-ld' });
      raiz.append(el('div', { class: 'org-odr-ctrl' },
        el('span', { class: 'org-rotulo-mini' }, 'otimização'), el('div', { class: 'org-seg-grupo' }, raiz._b.O0, raiz._b.O2),
        el('span', { class: 'org-rotulo-mini' }, 'ordem no link'), el('div', { class: 'org-seg-grupo' }, raiz._b.ab, raiz._b.ba)),
      raiz._tus, raiz._ld);
    },
    passo(raiz, k, ctx, dir) {
      const [o, ordem, rodou] = ODR_PASSOS[k];
      raiz._st = { o, ordem, rodou };
      odrMostrar(raiz, dir > 0 && !ctx.reduzido);
    },
    entrar(raiz) { requestAnimationFrame(() => raiz._redesenhar && raiz._redesenhar()); },
  });

  function odrMostrar(raiz, animar) {
    const { o, ordem, rodou } = raiz._st;
    raiz.classList.toggle('org-sem-anim', !animar);
    ['O0', 'O2', 'ab', 'ba'].forEach((x) => raiz._b[x].setAttribute('aria-pressed', String(x === o || x === ordem)));
    limparSetas(raiz);
    const primeiro = ordem === 'ab' ? 'a' : 'b';
    const ref = {};
    const tu = (q) => {
      const cod = el('div', { class: 'org-odr-cod' }, ...ODR_COD[q].map((l) => el('div', { class: 'org-odr-ln' + (/✗/.test(l) ? ' org-odr-ruim' : ''), html: cppSeg(l) })));
      const simb = el('div', { class: 'org-odr-simb' }, el('div', { class: 'org-rotulo-mini' }, 'nm ' + q + '.o'));
      ODR_SIMB[o][q].forEach(([letra, nome, papel]) => {
        const linha = el('div', { class: 'org-odr-s' }, el('span', { class: 'org-nm-letra ' + corLetra(letra) }, letra), el('span', null, nome));
        if (papel === 'dobro' && rodou) {
          const fica = q === primeiro;
          linha.classList.add(fica ? 'org-mantido' : 'org-descartado');
          linha.append(el('span', { class: 'org-odr-tag' }, fica ? '← o linker fica com este' : 'descartado'));
          linha.style.setProperty('--d', '350ms');
          if (fica) ref.mantido = linha;
        }
        if (/usa_em/.test(nome)) ref['usa_' + q] = linha;
        simb.append(linha);
      });
      if (o === 'O2') simb.append(el('div', { class: 'org-odr-inl' }, 'sem símbolo dobro: a versão de ' + q + '.cpp foi expandida dentro de usa_em_' + q));
      return el('div', { class: 'org-odr-tu' + (rodou && o === 'O0' && q === primeiro ? ' org-odr-vence' : '') },
        el('div', { class: 'org-arq-nome' }, q + '.cpp'), cod, simb);
    };
    raiz._tus.replaceChildren(tu('a'), tu('b'));
    const cmd = `g++ -Wall -Wextra -${o} main.cpp ${ordem === 'ab' ? 'a.cpp b.cpp' : 'b.cpp a.cpp'} -o odr && ./odr`;
    const saida = rodou ? ODR_SAIDA[o + '-' + ordem] : [];
    const t = terminal('bash — nenhum aviso, nenhum erro', ['$ ' + cmd, ...saida], 'org-term-mini');
    const txt = !rodou
      ? 'Duas TUs, a mesma função <code>inline</code>, dois corpos. Qual <code>dobro</code> roda? Avance (→).'
      : o === 'O0'
        ? `Em <code>-O0</code>, cada .o traz um <code>W _Z5dobroi</code> (fraco). O linker fica com o do <b>primeiro</b> .o da linha (${primeiro}.o) — e as <b>duas</b> TUs passam a usá-lo.`
        : 'Em <code>-O2</code>, nenhum .o exporta <code>dobro</code>: cada TU expandiu a <b>sua</b> versão no lugar. O mesmo programa tem dois comportamentos.';
    const p = el('p', { class: 'org-odr-txt', html: txt });
    // a regra, como o demo a imprime em 6.4
    const regra = el('p', { class: 'org-odr-regra', html: '<b>A regra</b> (6.4 no demo): N declarações, mas <b>uma</b> definição no programa. Classes, templates e <code>inline</code> podem repetir-se em várias TUs — <b>desde que token a token idênticas</b>. Estes dois <code>dobro</code> não são.' });
    raiz._ld.replaceChildren(el('div', { class: 'org-odr-esq' }, p, regra), t);
    if (rodou) raiz._ld.append(el('div', { class: 'org-odr-selo' }, el('b', null, 'ill-formed, no diagnostic required'), el('span', null, ' — nem o -flto avisou este caso')));
    raiz._redesenhar = () => {
      limparSetas(raiz);
      if (rodou && o === 'O0' && ref.mantido) {
        const outro = primeiro === 'a' ? 'b' : 'a';
        seta(raiz, ref['usa_' + outro], ref.mantido, 'org-seta-aviso', 600, 'chama');
      }
    };
    raiz._redesenhar();
  }

  // ═══════════════════════════════════════════════════════════════════════════
  // 10. organizacao/mangling — desmontando nomes de símbolos reais
  // ═══════════════════════════════════════════════════════════════════════════
  const MG = [
    { botao: 'soma_para_c(int, int)', sim: '_Z11soma_para_cii', dem: 'soma_para_c(int, int)', nota: 'A mesma função, compilada <b>sem</b> <code>extern "C"</code>.',
      seg: [['_Z', 'pre', 'prefixo: “isto é um nome C++”'], ['11', 'num', '11 letras a seguir'], ['soma_para_c', 'nome', 'o nome'], ['i', 'tipo', '1º parâmetro: int'], ['i', 'tipo', '2º parâmetro: int']] },
    { botao: 'soma(double, double)', sim: '_Z4somadd', dem: 'soma(double, double)', nota: 'A sobrecarga <code>soma(int, int)</code> sai como <code>_Z4somaii</code>: nomes diferentes, e o linker distingue as duas.',
      seg: [['_Z', 'pre', 'prefixo'], ['4', 'num', '4 letras'], ['soma', 'nome', 'o nome'], ['d', 'tipo', 'double'], ['d', 'tipo', 'double']] },
    { botao: 'geometria::norma_quadrada', sim: '_ZN16demo_organizacao9geometria14norma_quadradaERKNS0_5VetorE',
      dem: 'demo_organizacao::geometria::norma_quadrada(demo_organizacao::geometria::Vetor const&)', nota: 'O namespace inteiro entra no nome — por isso dois <code>Vetor</code> em namespaces diferentes nunca colidem no linker.',
      seg: [['_Z', 'pre', 'prefixo'], ['N', 'ani', 'abre um nome aninhado'], ['16demo_organizacao', 'nome', 'namespace (16 letras)'], ['9geometria', 'nome', 'namespace'], ['14norma_quadrada', 'nome', 'a função'],
        ['E', 'ani', 'fecha o nome'], ['R', 'qual', 'referência (&)'], ['K', 'qual', 'const'], ['N', 'ani', 'abre outro nome'], ['S0_', 'sub', 'repete um prefixo já visto: demo_organizacao::geometria'], ['5Vetor', 'nome', 'o tipo'], ['E', 'ani', 'fecha']] },
    { botao: 'biblioteca::api() (v2)', sim: '_ZN16demo_organizacao10biblioteca2v23apiB5cxx11Ev', dem: 'demo_organizacao::biblioteca::v2::api[abi:cxx11]()',
      nota: 'Você escreve <code>biblioteca::api()</code>; o símbolo diz <b>v2</b>. É isso que deixa a v1 conviver com a v2 no mesmo programa.',
      seg: [['_Z', 'pre', 'prefixo'], ['N', 'ani', 'abre'], ['16demo_organizacao', 'nome', 'namespace'], ['10biblioteca', 'nome', 'namespace'], ['2v2', 'dest', 'o inline namespace ESTÁ no símbolo'], ['3api', 'nome', 'a função'],
        ['B5cxx11', 'abi', 'etiqueta [abi:cxx11]: devolve a std::string da ABI nova'], ['E', 'ani', 'fecha'], ['v', 'tipo', 'sem parâmetros (void)']] },
    { botao: '(anônimo)::so_deste_arquivo', sim: '_ZN16demo_organizacao12_GLOBAL__N_116so_deste_arquivoEv', dem: 'demo_organizacao::(anonymous namespace)::so_deste_arquivo()',
      nota: 'No <code>nm</code> ele aparece com <code>t</code> minúsculo: símbolo local, invisível às outras TUs.',
      seg: [['_Z', 'pre', 'prefixo'], ['N', 'ani', 'abre'], ['16demo_organizacao', 'nome', 'namespace'], ['12_GLOBAL__N_1', 'dest', 'o namespace anônimo'], ['16so_deste_arquivo', 'nome', 'a função'], ['E', 'ani', 'fecha'], ['v', 'tipo', 'sem parâmetros']] },
    { botao: 'abortar_de_vez(const char*)', sim: '_ZN16demo_organizacao14abortar_de_vezEPKc', dem: 'demo_organizacao::abortar_de_vez(char const*)', nota: '<code>PKc</code> se lê da esquerda para a direita — ponteiro (<code>P</code>) para const (<code>K</code>) char (<code>c</code>): é <code>const char*</code> lido de trás para frente.',
      seg: [['_Z', 'pre', 'prefixo'], ['N', 'ani', 'abre'], ['16demo_organizacao', 'nome', 'namespace'], ['14abortar_de_vez', 'nome', 'a função'], ['E', 'ani', 'fecha'], ['P', 'qual', 'ponteiro (*)'], ['K', 'qual', 'const'], ['c', 'tipo', 'char']] },
    { botao: 'dobro(int)', sim: '_Z5dobroi', dem: 'dobro(int)', nota: 'Letra <code>W</code> (fraco) no <code>nm</code>: é <code>inline</code> — o linker aceita várias cópias e fica com uma (o slide da ODR).',
      seg: [['_Z', 'pre', 'prefixo'], ['5', 'num', '5 letras'], ['dobro', 'nome', 'o nome'], ['i', 'tipo', 'int']] },
    { botao: 'extern "C" soma_para_c', sim: 'soma_para_c', dem: 'soma_para_c', extc: true,
      nota: 'Sem <code>_Z</code>, sem tipos: o formato do C. Por isso uma função <code>extern "C"</code> não pode ser sobrecarregada.',
      seg: [['soma_para_c', 'nome', 'o nome — e só']] },
  ];
  const MG_PASSOS = [[0, false], [0, true], [1, true], [2, true], [3, true], [7, true]];

  Deck.widget('organizacao/mangling', {
    iniciar(raiz) {
      raiz.classList.add('org-mg');
      const lista = el('div', { class: 'org-mg-lista' });
      raiz._botoes = MG.map((m, i) => { const b = botao(m.botao, () => mgMostrar(raiz, i, true, true), 'org-mono'); lista.append(b); return b; });
      raiz._palco = el('div', { class: 'org-mg-palco' });
      raiz.append(lista, raiz._palco);
    },
    passo(raiz, k, ctx, dir) { const [i, dec] = MG_PASSOS[k]; mgMostrar(raiz, i, dir > 0 && !ctx.reduzido, dec); },
  });

  function mgMostrar(raiz, i, animar, decodificar) {
    const m = MG[i];
    raiz.classList.toggle('org-sem-anim', !animar);
    raiz._botoes.forEach((b, j) => b.setAttribute('aria-pressed', String(j === i)));
    const nseg = m.seg.length;
    const fs = Math.max(20, Math.min(40, Math.floor((1300 - nseg * 12) / (m.sim.length * 0.62))));
    const linha = el('div', { class: 'org-mg-sim' + (decodificar ? ' org-mg-dec' : '') });
    linha.style.setProperty('--fs', fs + 'px');
    m.seg.forEach(([t, cat], j) => {
      const s = el('span', { class: 'org-mg-seg org-mg-' + cat, dataset: { n: decodificar && nseg > 1 ? String(j + 1) : '' } }, t);
      s.style.setProperty('--d', (decodificar ? 200 + j * 150 : 0) + 'ms');
      linha.append(s);
    });
    const partes = [el('div', { class: 'org-mg-rot' }, el('span', { class: 'org-rotulo-mini' }, m.extc ? 'o mesmo corpo, no .o (nm)' : 'símbolo no .o (nm)'))];
    if (!m.extc) partes.push(linha);
    else {
      const comp = el('div', { class: 'org-mg-comp' },
        el('div', { class: 'org-mg-comp-l org-mg-velho' }, el('span', { class: 'org-mg-comp-rot' }, 'int soma_para_c(int, int)'), el('span', { class: 'org-mg-comp-sim' }, '_Z11soma_para_cii')),
        el('div', { class: 'org-mg-comp-l org-mg-novo' }, el('span', { class: 'org-mg-comp-rot' }, 'extern "C" int soma_para_c(int, int)'), el('span', { class: 'org-mg-comp-sim' }, 'soma_para_c')));
      comp.style.setProperty('--fs', '40px');
      partes.push(comp);
    }
    if (decodificar && m.extc) {
      const t = terminal('c++filt — nada a desmontar', ['$ echo ' + m.sim + ' | c++filt', m.dem], 'org-term-mini org-mg-filt');
      t.style.setProperty('--d', '500ms');
      const nota = el('p', { class: 'org-nota', html: m.nota });
      nota.style.setProperty('--d', '650ms');
      partes.push(t, nota);
    } else if (decodificar) {
      if (nseg > 1) {
        const leg = el('div', { class: 'org-mg-legenda' });
        m.seg.forEach(([t, cat, txt], j) => {
          const it = el('div', { class: 'org-mg-item' }, el('span', { class: 'org-mg-n org-mg-' + cat }, String(j + 1)), el('code', { class: 'org-mg-cod org-mg-' + cat }, t), el('span', null, txt));
          it.style.setProperty('--d', 200 + j * 150 + 'ms');
          leg.append(it);
        });
        partes.push(leg);
      }
      const t = terminal('c++filt', ['$ echo ' + m.sim + ' | c++filt', m.dem], 'org-term-mini org-mg-filt');
      t.style.setProperty('--d', 260 + nseg * 150 + 'ms');
      const nota = el('p', { class: 'org-nota', html: m.nota });
      nota.style.setProperty('--d', 400 + nseg * 150 + 'ms');
      partes.push(t, nota);
    } else {
      partes.push(el('p', { class: 'org-dica', html: 'O linker só compara <b>strings</b>. Como o C++ tem sobrecarga e namespaces, o nome do símbolo precisa carregar os tipos. Avance (→) para desmontar.' }));
    }
    raiz._palco.replaceChildren(...partes);
  }
})();
