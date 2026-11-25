/* gLabels Neo website. Small and dependency free. Everything works without it; this only adds polish. */
(function () {
  'use strict';
  var root = document.documentElement;
  root.classList.add('js');
  var reduce = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  if (reduce) root.classList.add('reduce');

  /* ---- light / dark ---- */
  function isDark() {
    var t = root.getAttribute('data-theme');
    if (t) return t === 'dark';
    return !!(window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches);
  }
  var modeBtn = document.getElementById('mode');
  if (modeBtn) {
    modeBtn.addEventListener('click', function () {
      var next = isDark() ? 'light' : 'dark';
      root.setAttribute('data-theme', next);
      try { localStorage.setItem('gn-theme', next); } catch (e) {}
      syncShots();
    });
  }

  /* ---- reveal on scroll ---- */
  var revealed = document.querySelectorAll('.rv');
  if ('IntersectionObserver' in window && !reduce) {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting) { e.target.classList.add('in'); io.unobserve(e.target); }
      });
    }, { threshold: 0.12, rootMargin: '0px 0px -6% 0px' });
    revealed.forEach(function (el) { io.observe(el); });
  } else {
    revealed.forEach(function (el) { el.classList.add('in'); });
  }

  /* ---- count up ---- */
  document.querySelectorAll('[data-count]').forEach(function (el) {
    var target = parseInt(el.getAttribute('data-count'), 10);
    var fmt = function (n) { return n.toLocaleString('en-US'); };
    if (reduce || !('IntersectionObserver' in window)) { el.textContent = fmt(target); return; }
    el.textContent = '0';
    var o = new IntersectionObserver(function (entries) {
      if (!entries[0].isIntersecting) return;
      o.disconnect();
      var start = null, dur = 1400;
      (function step(ts) {
        if (start === null) start = ts;
        var p = Math.min(1, (ts - start) / dur);
        el.textContent = fmt(Math.round(target * (1 - Math.pow(1 - p, 3))));
        if (p < 1) requestAnimationFrame(step);
      })(performance.now());
    }, { threshold: 0.6 });
    o.observe(el);
  });

  /* ---- the right download for this computer ---- */
  var ua = (navigator.userAgent || '') + ' ' + (navigator.platform || '');
  var os = /Windows/i.test(ua) ? 'windows' : /Mac/i.test(ua) && !/iPhone|iPad/i.test(ua) ? 'macos' : /Linux|X11/i.test(ua) && !/Android/i.test(ua) ? 'linux' : '';
  var names = { windows: 'Windows', macos: 'macOS', linux: 'Linux' };
  if (os) {
    document.querySelectorAll('[data-os="' + os + '"]').forEach(function (el) { el.classList.add('rec'); });
    var hero = document.getElementById('hero-download');
    var target = document.querySelector('.dl[data-os="' + os + '"] a.btn');
    if (hero && target) {
      hero.href = target.href;
      hero.querySelector('span').textContent = 'Download for ' + names[os];
    }
    var note = document.getElementById('os-note');
    if (note) note.hidden = false;
  }

  /* ---- colour themes of the app ---- */
  var shotA = document.getElementById('shot-a');
  var shotB = document.getElementById('shot-b');
  var current = 'graphite';
  var front = shotA;
  function shotSrc(scheme) {
    var base = shotA ? shotA.getAttribute('data-base') : '';
    return base + 'theme-' + scheme + '-' + (isDark() ? 'dark' : 'light') + '.webp';
  }
  function syncShots() {
    if (!shotA || !shotB) return;
    front.src = shotSrc(current);
  }
  function showScheme(scheme, label) {
    if (!shotA || !shotB) return;
    current = scheme;
    var back = front === shotA ? shotB : shotA;
    back.onload = function () { back.classList.remove('off'); front.classList.add('off'); front = back; };
    back.src = shotSrc(scheme);
    document.querySelectorAll('.swatch').forEach(function (b) {
      b.setAttribute('aria-pressed', b.getAttribute('data-scheme') === scheme ? 'true' : 'false');
    });
    var cap = document.getElementById('scheme-name');
    if (cap && label) cap.textContent = label;
  }
  document.querySelectorAll('.swatch').forEach(function (b) {
    b.addEventListener('click', function () { showScheme(b.getAttribute('data-scheme'), b.textContent.trim()); });
  });
  if (shotA && shotB) {
    // Preload and start on Graphite; cycle through the themes once while the section is on screen
    shotA.src = shotSrc('graphite');
    var section = document.getElementById('themes');
    if (section && 'IntersectionObserver' in window && !reduce) {
      var order = ['neo', 'paper', 'harbor', 'signal', 'graphite'];
      var idx = 0, timer = null, userTouched = false;
      section.addEventListener('click', function () { userTouched = true; if (timer) clearInterval(timer); });
      var o2 = new IntersectionObserver(function (entries) {
        if (entries[0].isIntersecting && !timer && !userTouched) {
          timer = setInterval(function () {
            if (idx >= order.length) { clearInterval(timer); return; }
            var btn = document.querySelector('.swatch[data-scheme="' + order[idx] + '"]');
            if (btn) showScheme(order[idx], btn.textContent.trim());
            idx++;
          }, 2200);
        } else if (!entries[0].isIntersecting && timer) { clearInterval(timer); timer = null; }
      }, { threshold: 0.5 });
      o2.observe(section);
    }
  }

  /* ---- gentle tilt on the product shot ---- */
  var tilt = document.querySelector('.tilt');
  if (tilt && !reduce && window.matchMedia('(hover: hover)').matches) {
    tilt.addEventListener('mousemove', function (e) {
      var r = tilt.getBoundingClientRect();
      var x = (e.clientX - r.left) / r.width - 0.5, y = (e.clientY - r.top) / r.height - 0.5;
      tilt.style.setProperty('--ry', (x * 5).toFixed(2) + 'deg');
      tilt.style.setProperty('--rx', (-y * 4).toFixed(2) + 'deg');
    });
    tilt.addEventListener('mouseleave', function () {
      tilt.style.setProperty('--ry', '0deg'); tilt.style.setProperty('--rx', '0deg');
    });
  }

  /* ---- fill from a spreadsheet demo ---- */
  var demo = document.getElementById('fill-demo');
  if (demo) {
    var rows = demo.querySelectorAll('.csv tbody tr');
    var labs = demo.querySelectorAll('.mini-lab:not(.empty)');
    if (reduce || !('IntersectionObserver' in window)) {
      labs.forEach(function (l) { l.classList.add('on'); });
    } else {
      var played = false;
      new IntersectionObserver(function (entries, ob) {
        if (!entries[0].isIntersecting || played) return;
        played = true; ob.disconnect();
        var i = 0;
        var t = setInterval(function () {
          rows.forEach(function (r) { r.classList.remove('on'); });
          if (i < rows.length) rows[i].classList.add('on');
          if (i < labs.length) labs[i].classList.add('on');
          i++;
          if (i > Math.max(rows.length, labs.length)) { clearInterval(t); rows.forEach(function (r) { r.classList.remove('on'); }); }
        }, 380);
      }, { threshold: 0.5 }).observe(demo);
    }
  }

  /* ---- template search ---- */
  var q = document.getElementById('q');
  if (q) {
    var out = document.getElementById('results');
    var data = null;
    var load = function (cb) {
      if (data) return cb();
      fetch(q.getAttribute('data-index')).then(function (r) { return r.json(); }).then(function (j) { data = j; cb(); });
    };
    var norm = function (s) { return s.toLowerCase().replace(/[^a-z0-9]+/g, ''); };
    var render = function () {
      var text = norm(q.value);
      out.textContent = '';
      if (text.length < 2) return;
      var hits = [];
      for (var i = 0; i < data.length && hits.length < 40; i++) {
        var d = data[i];
        var hay = norm(d[0] + d[1]);
        if (hay.indexOf(text) !== -1 || norm(d[1]).indexOf(text) === 0) hits.push(d);
      }
      hits.sort(function (a, b) {
        var ea = norm(a[1]) === text ? 0 : 1, eb = norm(b[1]) === text ? 0 : 1;
        return ea - eb;
      });
      hits.forEach(function (d) {
        var li = document.createElement('li'), a = document.createElement('a'), l = document.createElement('span'), s = document.createElement('small');
        a.href = d[2];
        l.textContent = d[0] + ' ' + d[1] + ' – ' + d[3];
        s.textContent = d[4] + ' per sheet · ' + d[5];
        a.appendChild(l); a.appendChild(s); li.appendChild(a); out.appendChild(li);
      });
      if (!hits.length) { var li2 = document.createElement('li'); li2.textContent = 'No template found. Try just the number, for example 5160.'; out.appendChild(li2); }
    };
    q.addEventListener('input', function () { load(render); });
    var preset = new URLSearchParams(location.search).get('q');
    if (preset) { q.value = preset; load(render); }
  }
})();
