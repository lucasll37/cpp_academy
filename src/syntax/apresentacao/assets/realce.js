/* =============================================================================
   realce.js — realce de sintaxe para C++, shell e saída de terminal

   Sem dependências e sem rede: a apresentação abre direto de file://, então
   nada de CDN. O tokenizador é pequeno, mas cobre o que a aula mostra:
   comentários, pré-processador, raw strings, literais com separador de
   dígitos e sufixos, atributos [[...]], palavras-chave, tipos e macros.

   API (global):
     Realce.cpp(texto)    → HTML com <span class="tk-…">, linha a linha
     Realce.shell(texto)  → idem, para comandos de terminal
     Realce.saida(texto)  → idem, para a saída do ./dist/bin/syntax
     Realce.linhas(html)  → divide o HTML realçado em linhas (spans fechados)
     Realce.escapar(s)
   ============================================================================= */
(function (global) {
  'use strict';

  const escapar = (s) => String(s)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');

  // ── Vocabulário C++ ────────────────────────────────────────────────────────
  const CONTROLE = new Set([
    'if', 'else', 'switch', 'case', 'default', 'for', 'while', 'do', 'break',
    'continue', 'return', 'goto', 'try', 'catch', 'throw', 'co_await',
    'co_yield', 'co_return',
  ]);
  const PALAVRAS = new Set([
    'alignas', 'alignof', 'asm', 'auto', 'class', 'concept', 'const', 'consteval',
    'constexpr', 'constinit', 'const_cast', 'decltype', 'delete', 'dynamic_cast',
    'enum', 'explicit', 'export', 'extern', 'final', 'friend', 'import', 'inline',
    'module', 'mutable', 'namespace', 'new', 'noexcept', 'operator', 'override',
    'private', 'protected', 'public', 'register', 'reinterpret_cast', 'requires',
    'sizeof', 'static', 'static_assert', 'static_cast', 'struct', 'template',
    'this', 'thread_local', 'typedef', 'typeid', 'typename', 'union', 'using',
    'virtual', 'volatile',
  ]);
  const LITERAIS = new Set(['true', 'false', 'nullptr', 'NULL']);
  const FUNDAMENTAIS = new Set([
    'void', 'bool', 'char', 'char8_t', 'char16_t', 'char32_t', 'wchar_t', 'short',
    'int', 'long', 'signed', 'unsigned', 'float', 'double',
  ]);
  const TIPOS_STD = new Set([
    'size_t', 'ptrdiff_t', 'nullptr_t', 'byte', 'max_align_t', 'intptr_t', 'uintptr_t',
    'int8_t', 'int16_t', 'int32_t', 'int64_t', 'uint8_t', 'uint16_t', 'uint32_t',
    'uint64_t', 'string', 'string_view', 'wstring', 'vector', 'array', 'map',
    'unordered_map', 'set', 'unordered_set', 'list', 'deque', 'pair', 'tuple',
    'optional', 'variant', 'any', 'function', 'unique_ptr', 'shared_ptr', 'weak_ptr',
    'span', 'initializer_list', 'va_list', 'exception', 'runtime_error', 'logic_error',
    'invalid_argument', 'out_of_range', 'domain_error', 'overflow_error', 'range_error',
    'system_error', 'bad_variant_access', 'bad_cast', 'bad_alloc', 'mutex',
    'lock_guard', 'unique_lock', 'thread', 'jthread', 'atomic', 'nothrow_t',
    'type_info', 'ostream', 'istream', 'nothrow', 'underlying_type_t', 'is_same_v',
    'is_integral_v', 'is_floating_point_v', 'is_arithmetic_v', 'is_trivial_v',
    'is_standard_layout_v', 'is_trivially_copyable_v', 'integral', 'floating_point',
    'same_as', 'convertible_to', 'expected', 'error_code', 'format_string',
  ]);

  const RE = {
    espaco: /[ \t\r\f\v]+/y,
    nl: /\n/y,
    comBloco: /\/\*[\s\S]*?(?:\*\/|$)/y,
    comLinha: /\/\/[^\n]*/y,
    diretiva: /#[ \t]*[A-Za-z_]+/y,
    headerInc: /[ \t]*(<[^>\n]*>|"[^"\n]*")/y,
    rawIni: /(?:u8|u|U|L)?R"([^()\\\s"]{0,16})\(/y,
    string: /(?:u8|u|U|L)?"(?:[^"\\\n]|\\.)*"?(?:[A-Za-z_]\w*)?/y,
    char: /(?:u8|u|U|L)?'(?:[^'\\\n]|\\.){1,8}'/y,
    atribIni: /\[\[/y,
    atribFim: /\]\]/y,
    numero: /(?:0[xX][\da-fA-F](?:'?[\da-fA-F])*(?:\.[\da-fA-F]*)?(?:[pP][+-]?\d+)?|0[bB][01](?:'?[01])*|\d(?:'?\d)*(?:\.(?:\d(?:'?\d)*)?)?(?:[eE][+-]?\d+)?|\.\d(?:'?\d)*(?:[eE][+-]?\d+)?)(?:[uU](?:ll|LL|l|L|z|Z)?|(?:ll|LL|l|L)[uU]?|[fFlLzZ])?(?:_[A-Za-z]\w*)?/y,
    ident: /[A-Za-z_]\w*/y,
    op: /::|->\*|->|\.\*|\.\.\.|\+\+|--|<=>|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||[+\-*/%&|^]=|[-+*/%=&|^~!<>?:;,.(){}[\]#\\@$`]/y,
    resto: /[^\s]/y,
  };

  function proximoNaoEspaco(src, i) {
    while (i < src.length && (src[i] === ' ' || src[i] === '\t')) i++;
    return src[i];
  }

  // Tokeniza C++ → [{t, v}], t = classe CSS (sem o prefixo tk-) ou '' (texto puro)
  function tokensCpp(src) {
    const out = [];
    let i = 0;
    let inicioDeLinha = true;     // só espaços desde o \n anterior → '#' é diretiva
    let emAtributo = false;
    let aposInclude = false;
    let anterior = '';            // último token significativo (texto)
    let antAnterior = '';

    const push = (t, v) => {
      out.push({ t, v });
      if (t !== 'ws') { antAnterior = anterior; anterior = v; }
    };
    const tenta = (re) => { re.lastIndex = i; const m = re.exec(src); return m && m.index === i ? m : null; };

    while (i < src.length) {
      let m;
      if ((m = tenta(RE.nl))) { push('ws', '\n'); i += 1; inicioDeLinha = true; aposInclude = false; continue; }
      if ((m = tenta(RE.espaco))) { push('ws', m[0]); i += m[0].length; continue; }
      if ((m = tenta(RE.comBloco))) { push('com', m[0]); i += m[0].length; continue; }
      if ((m = tenta(RE.comLinha))) { push('com', m[0]); i += m[0].length; continue; }

      if (inicioDeLinha && src[i] === '#' && (m = tenta(RE.diretiva))) {
        push('pre', m[0]); i += m[0].length; inicioDeLinha = false;
        aposInclude = /include|embed/.test(m[0]);
        if (aposInclude) {
          const h = tenta(RE.headerInc);
          if (h) {
            const ws = h[0].slice(0, h[0].length - h[1].length);
            if (ws) push('ws', ws);
            push('str', h[1]); i += h[0].length;
          }
        }
        continue;
      }
      inicioDeLinha = false;

      if ((m = tenta(RE.rawIni))) {
        const delim = ')' + m[1] + '"';
        const fim = src.indexOf(delim, i + m[0].length);
        const ate = fim < 0 ? src.length : fim + delim.length;
        push('str', src.slice(i, ate)); i = ate; continue;
      }
      if ((m = tenta(RE.string))) { push('str', m[0]); i += m[0].length; continue; }

      // '\'' só abre literal de caractere se NÃO estivermos colados num número
      if ((m = tenta(RE.char))) { push('chr', m[0]); i += m[0].length; continue; }

      if ((m = tenta(RE.atribIni))) { push('atr', '[['); i += 2; emAtributo = true; continue; }
      if (emAtributo && (m = tenta(RE.atribFim))) { push('atr', ']]'); i += 2; emAtributo = false; continue; }

      if ((m = tenta(RE.numero))) { push('num', m[0]); i += m[0].length; continue; }

      if ((m = tenta(RE.ident))) {
        const w = m[0];
        const prox = proximoNaoEspaco(src, i + w.length);
        const seguidoDeEscopo = src.startsWith('::', i + w.length);
        const aposStd = anterior === '::' && antAnterior === 'std';
        let t = '';
        if (emAtributo) t = 'atr';
        else if (CONTROLE.has(w)) t = 'ctl';
        else if (PALAVRAS.has(w)) t = 'kw';
        else if (LITERAIS.has(w)) t = 'lit';
        else if (FUNDAMENTAIS.has(w)) t = 'tipo';
        else if (w === 'std') t = 'ns';
        else if (/^__\w{3,}$/.test(w) || (/^[A-Z][A-Z0-9_]+$/.test(w) && w.length >= 2))
          t = 'mac';
        else if (aposStd && TIPOS_STD.has(w)) t = 'tipo';
        else if (TIPOS_STD.has(w) && /_t$|_v$/.test(w)) t = 'tipo';
        else if (prox === '(') t = 'fn';
        else if (seguidoDeEscopo) t = /^[A-Z]/.test(w) ? 'tipo' : 'ns';
        else if (/^[A-Z]/.test(w)) t = 'tipo';
        else if (aposStd && prox === '<' && !/^\s*<</.test(src.slice(i + w.length))) t = 'fn';
        push(t, w); i += w.length; continue;
      }

      if ((m = tenta(RE.op))) { push('op', m[0]); i += m[0].length; continue; }
      // Qualquer outra coisa (unicode solto, por exemplo)
      push('', src[i]); i += 1;
    }
    return out;
  }

  // ── Shell ──────────────────────────────────────────────────────────────────
  function tokensShell(src) {
    const out = [];
    const linhas = src.split('\n');
    let continuacao = false;          // a linha anterior terminou em "\"
    linhas.forEach((linha, n) => {
      if (n > 0) out.push({ t: 'ws', v: '\n' });
      let i = 0;
      let primeira = !continuacao;
      continuacao = /\\\s*$/.test(linha);
      // prompt "$ " no começo
      const p = /^(\s*)(\$|#|❯|>)( )/.exec(linha);
      if (p && p[2] !== '#') {
        if (p[1]) out.push({ t: 'ws', v: p[1] });
        out.push({ t: 'prompt', v: p[2] + p[3] });
        i = p[0].length;
      }
      while (i < linha.length) {
        const r = linha.slice(i);
        let m;
        if ((m = /^\s+/.exec(r))) { out.push({ t: 'ws', v: m[0] }); i += m[0].length; continue; }
        if (r[0] === '#' && (i === 0 || /\s/.test(linha[i - 1]))) { out.push({ t: 'com', v: r }); break; }
        if ((m = /^'[^']*'?/.exec(r)) && r[0] === "'") { out.push({ t: 'str', v: m[0] }); i += m[0].length; primeira = false; continue; }
        if ((m = /^"(?:[^"\\]|\\.)*"?/.exec(r)) && r[0] === '"') { out.push({ t: 'str', v: m[0] }); i += m[0].length; primeira = false; continue; }
        if ((m = /^\$(?:\{[^}]*\}|\(|[A-Za-z_]\w*|[@#?$!0-9*])/.exec(r))) { out.push({ t: 'var', v: m[0] }); i += m[0].length; if (m[0] === '$(') primeira = true; continue; }
        if ((m = /^(?:\|\||&&|;;|[|&;<>()\\])/.exec(r))) { out.push({ t: 'op', v: m[0] }); i += m[0].length; if (m[0] !== ')' ) primeira = /^(\|\||&&|\||;|\()$/.test(m[0]); continue; }
        if ((m = /^--?[A-Za-z0-9][\w=,.:+/-]*/.exec(r)) && !primeira) { out.push({ t: 'flag', v: m[0] }); i += m[0].length; continue; }
        if ((m = /^[^\s|&;<>()'"$]+/.exec(r))) {
          const w = m[0];
          let t = '';
          if (primeira) t = /=/.test(w) && !/^[./]/.test(w) ? 'var' : 'cmd';
          else if (/^\d+$/.test(w)) t = 'num';
          out.push({ t, v: w }); i += w.length;
          if (t === 'cmd') primeira = false;
          continue;
        }
        out.push({ t: '', v: r[0] }); i += 1;
      }
    });
    return out;
  }

  // ── Saída do programa (classificação por linha) ───────────────────────────
  function classeSaida(linha) {
    if (/^\s*[╔║╚]/.test(linha)) return 'secao';
    if (/^\s*✓ Todos os demos/.test(linha)) return 'fim';
    if (/^\s*──\s*\d+\.\d+/.test(linha)) return 'subsecao';
    if (/⚠/.test(linha)) return 'aviso';
    if (/✗|\bERRO\b|\berro\b:/.test(linha)) return 'erro';
    if (/^\s*\|.*\|\s*$/.test(linha)) return 'tabela';
    if (/^\s*\$ /.test(linha)) return 'cmd';
    if (/\[log\]|\(log:|\[auditoria\]|\(avaliou|\(efeito colateral\)|adquirido|liberado|ctor\(|CÓPIA|ATRIBUIÇÃO|\(abortaria/.test(linha)) return 'log';
    return '';
  }

  function htmlSaida(src) {
    let id = 0;
    return src.split('\n').map((bruta) => {
      const linha = bruta.replace(/[⟦⟧]/g, '');
      const c = classeSaida(linha);
      // ⟦…⟧ marca um trecho (não pode atravessar linhas na saída)
      let h = escapar(bruta).replace(/⟦/g, () => `\u0001${++id}\u0002`).replace(/⟧/g, '\u0003');
      if (c !== 'secao' && c !== 'tabela') {
        h = h.replace(/(→|←|↑)/g, '<span class="tk-seta">$1</span>');
        h = h.replace(/\b(true|false)\b/g, '<span class="tk-lit">$1</span>');
      }
      if (c === 'tabela') h = h.replace(/(\|)/g, '<span class="tk-borda">$1</span>');
      if (c === 'cmd') h = h.replace(/^(\s*)\$ /, '$1<span class="tk-prompt">$ </span>');
      h = h.replace(/\u0001(\d+)\u0002/g, '<mark class="mc" data-m="$1">').replace(/\u0003/g, '</mark>');
      return `<span class="ln-saida${c ? ' s-' + c : ''}">${h || '&#8203;'}</span>`;
    });
  }

  // ── Marcas ⟦…⟧ ─────────────────────────────────────────────────────────────
  // Dentro do código, ⟦trecho⟧ vira <mark class="mc" data-m="n"> (n = 1, 2, 3…
  // na ordem em que as marcas ABREM). Os delimitadores somem do texto realçado
  // e do texto copiado. Marcas podem atravessar linhas e podem se aninhar.
  function extrairMarcas(txt) {
    if (txt.indexOf('⟦') < 0) return { limpo: txt, idEm: null };
    let limpo = '';
    const pilha = [], marcas = [];
    let id = 0;
    for (const ch of txt) {
      if (ch === '⟦') { pilha.push({ ini: limpo.length, id: ++id }); continue; }
      if (ch === '⟧') { const m = pilha.pop(); if (m) marcas.push({ ini: m.ini, fim: limpo.length, id: m.id }); continue; }
      limpo += ch;
    }
    const idEm = new Int32Array(limpo.length);
    marcas.sort((a, b) => a.id - b.id).forEach((m) => idEm.fill(m.id, m.ini, m.fim));
    return { limpo, idEm };
  }

  // ── Renderização: tokens → linhas de HTML ─────────────────────────────────
  function linhasDeTokens(tokens, idEm) {
    const linhas = [];
    let atual = '';
    let pos = 0;
    let aberta = 0;
    const marcaEm = (p) => (idEm && p < idEm.length ? idEm[p] : 0);
    for (const tk of tokens) {
      const v = tk.v;
      let i = 0;
      while (i < v.length) {
        if (v[i] === '\n') {
          if (aberta) { atual += '</mark>'; aberta = 0; }
          linhas.push(atual); atual = '';
          i++; pos++;
          continue;
        }
        const m = marcaEm(pos);
        let j = i;
        while (j < v.length && v[j] !== '\n' && marcaEm(pos + (j - i)) === m) j++;
        if (m !== aberta) {
          if (aberta) atual += '</mark>';
          if (m) atual += `<mark class="mc" data-m="${m}">`;
          aberta = m;
        }
        const e = escapar(v.slice(i, j));
        atual += (tk.t && tk.t !== 'ws') ? `<span class="tk-${tk.t}">${e}</span>` : e;
        pos += j - i;
        i = j;
      }
    }
    if (aberta) atual += '</mark>';
    linhas.push(atual);
    return linhas;
  }

  function comMarcas(txt, tokenizar) {
    const { limpo, idEm } = extrairMarcas(txt);
    return linhasDeTokens(tokenizar(limpo), idEm);
  }

  // Remove a indentação comum e as linhas vazias das pontas
  function normalizar(texto) {
    let ls = String(texto).replace(/\r\n?/g, '\n').replace(/\t/g, '    ').split('\n');
    while (ls.length && !ls[0].trim()) ls.shift();
    while (ls.length && !ls[ls.length - 1].trim()) ls.pop();
    const ind = ls.filter((l) => l.trim()).reduce((min, l) => Math.min(min, l.match(/^ */)[0].length), Infinity);
    if (Number.isFinite(ind) && ind > 0) ls = ls.map((l) => l.slice(ind));
    return ls.join('\n');
  }

  const Realce = {
    escapar,
    normalizar,
    tokensCpp,
    semMarcas: (txt) => String(txt).replace(/[⟦⟧]/g, ''),
    cppLinhas: (txt) => comMarcas(txt, tokensCpp),
    shellLinhas: (txt) => comMarcas(txt, tokensShell),
    saidaLinhas: (txt) => htmlSaida(txt),
    cpp: (txt) => comMarcas(txt, tokensCpp).join('\n'),
    shell: (txt) => comMarcas(txt, tokensShell).join('\n'),
    saida: (txt) => htmlSaida(txt).join('\n'),
    // realça conforme a linguagem e devolve um array de linhas HTML
    linhas(txt, lang) {
      if (lang === 'shell' || lang === 'bash' || lang === 'sh') return Realce.shellLinhas(txt);
      if (lang === 'saida' || lang === 'output') return Realce.saidaLinhas(txt);
      if (lang === 'texto' || lang === 'text') return comMarcas(txt, (t) => [{ t: '', v: t }]);
      return Realce.cppLinhas(txt);
    },
  };

  global.Realce = Realce;
})(window);
