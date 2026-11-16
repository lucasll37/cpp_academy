/* lucaslima.ai — a cortina da passagem landing → aplicação. FONTE: landing-page/design/house/curtain.js; nas
   aplicações é uma cópia (public/ll/), escrita por `make design-sync`. Script clássico e síncrono no <head>, antes de
   qualquer CSS da aplicação: marca <html data-ll-curtain> antes da primeira pintura, e o house.css desenha a cortina
   (o mesmo quadro em que a landing terminou) e a levanta. Só quando a landing tocou a passagem: o sinal ?ll=c, que ela
   põe no destino (src/ui/handoff.ts) e que sai da URL aqui, antes de a aplicação a ler, junto com o Referer (que a
   política strict-origin-when-cross-origin da landing manda como a origem). Sem o sinal (uma nova aba, os links do
   /sistema/, a pausa do site), nada de cortina: antes, toda chegada vinda de lucaslima.ai a armava. Só numa navegação
   nova (não num recarregar nem no voltar), e com movimento. Falhar aqui nunca pode quebrar a aplicação: tudo dentro
   do try. */
(function () {
  try {
    var marked = /[?&]ll=c(&|$)/.test(location.search);
    if (marked) {
      var u = new URL(location.href);
      u.searchParams.delete('ll');
      history.replaceState(history.state, '', u.pathname + u.search + u.hash);
    }
    if (!marked || !/^https:\/\/(www\.)?lucaslima\.ai\//.test(document.referrer)) return;
    var nav = performance.getEntriesByType && performance.getEntriesByType('navigation')[0];
    if (nav && nav.type !== 'navigate') return;
    if (window.matchMedia && matchMedia('(prefers-reduced-motion: reduce)').matches) return;
    var root = document.documentElement;
    // 1) a cortina parada, já no primeiro quadro (o mesmo em que a landing terminou)
    root.setAttribute('data-ll-curtain', '');
    // a placa é o fundo da cortina: pedida já, para estar no primeiro quadro mesmo na primeira visita
    var src = document.currentScript && document.currentScript.src;
    if (src) {
      var pre = document.createElement('link');
      pre.rel = 'preload';
      pre.as = 'image';
      pre.href = src.replace(/curtain\.js(\?.*)?$/, 'motif.svg');
      document.head.appendChild(pre);
    }
    // 2) a subida. Uma animação CSS só começa quando a thread principal desenha um quadro; uma aplicação pesada a ocupa
    // logo no começo (os módulos, o WebGL), e a subida com atraso fixo nunca chegava a começar: a cortina sumia de uma
    // vez. Então a subida é disparada daqui, de dentro de um requestAnimationFrame, quando a aplicação terminou a
    // carga inicial (DOMContentLoaded e dois quadros seguidos), com no mínimo 600 ms de cortina e no máximo 3 s.
    // Começada, ela corre no compositor (house.css anima só transform), mesmo que a aplicação volte a ocupar a thread.
    var t0 = Date.now();
    var lifted = false;
    var done = function () {
      root.removeAttribute('data-ll-curtain');
      document.removeEventListener('animationend', end);
    };
    var end = function (e) {
      if (e.animationName === 'll-lift') done();
    };
    var lift = function () {
      if (lifted) return;
      lifted = true;
      requestAnimationFrame(function () {
        root.setAttribute('data-ll-curtain', 'lift');
        document.addEventListener('animationend', end);
        setTimeout(done, 1600); // a subida dura 760 ms; isto é só a rede de segurança
      });
    };
    var ready = function () {
      requestAnimationFrame(function () {
        requestAnimationFrame(function () {
          setTimeout(lift, Math.max(0, 600 - (Date.now() - t0)));
        });
      });
    };
    if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', ready);
    else ready();
    setTimeout(lift, 3000);
  } catch (e) {
    /* sem cortina */
  }
})();
