const ICONS = {
  plus: '<line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>',
  minus: '<line x1="5" y1="12" x2="19" y2="12"/>',
  square: '<rect x="5" y="5" width="14" height="14" rx="1"/>',
  x: '<line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>',
  refresh: '<polyline points="23 4 23 10 17 10"/><polyline points="1 20 1 14 7 14"/><path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/>',
  search: '<circle cx="11" cy="11" r="7"/><line x1="21" y1="21" x2="16.65" y2="16.65"/>',
  grid: '<rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/>',
  play: '<polygon points="6 4 19 12 6 20 6 4"/>',
  left: '<polyline points="15 18 9 12 15 6"/>',
  right: '<polyline points="9 18 15 12 9 6"/>',
  down: '<polyline points="6 9 12 15 18 9"/>',
  monitor: '<rect x="2" y="3" width="20" height="14" rx="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/>',
  app: '<rect x="3" y="4" width="18" height="16" rx="2"/><line x1="3" y1="9" x2="21" y2="9"/>',
  mouse: '<path d="M3 3l7.07 16.97 2.51-7.39 7.39-2.51L3 3z"/><path d="M13 13l6 6"/>',
  clipboard: '<path d="M16 4h2a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h2"/><rect x="8" y="2" width="8" height="4" rx="1"/>',
  folder: '<path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/>',
  file: '<path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"/><polyline points="13 2 13 9 20 9"/>',
  chat: '<path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/>',
  camera: '<path d="M23 19a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4l2-3h6l2 3h4a2 2 0 0 1 2 2z"/><circle cx="12" cy="13" r="4"/>',
  record: '<circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="4" fill="currentColor"/>',
  power: '<path d="M18.36 6.64a9 9 0 1 1-12.73 0"/><line x1="12" y1="2" x2="12" y2="12"/>',
  lock: '<rect x="4" y="11" width="16" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/>',
  full: '<path d="M8 3H5a2 2 0 0 0-2 2v3m18 0V5a2 2 0 0 0-2-2h-3m0 18h3a2 2 0 0 0 2-2v-3M3 16v3a2 2 0 0 0 2 2h3"/>',
  fit: '<polyline points="4 14 10 14 10 20"/><polyline points="20 10 14 10 14 4"/><line x1="14" y1="10" x2="21" y2="3"/><line x1="3" y1="21" x2="10" y2="14"/>',
  up: '<line x1="12" y1="19" x2="12" y2="5"/><polyline points="5 12 12 5 19 12"/>',
  upload: '<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/>',
  download: '<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/>',
  toRight: '<line x1="5" y1="12" x2="19" y2="12"/><polyline points="12 5 19 12 12 19"/>',
  toLeft: '<line x1="19" y1="12" x2="5" y2="12"/><polyline points="12 19 5 12 12 5"/>',
  sliders: '<line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/>',
  shield: '<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>',
  shieldOk: '<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/><polyline points="8.5 12 11 14.5 15.5 9.5"/>',
  user: '<path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/>',
  users: '<path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M23 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/>',
  copy: '<rect x="9" y="9" width="13" height="13" rx="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>',
  keyboard: '<rect x="2" y="6" width="20" height="12" rx="2"/><path d="M6 10h.01M10 10h.01M14 10h.01M18 10h.01M7 14h10"/>',
  pause: '<rect x="6" y="4" width="4" height="16"/><rect x="14" y="4" width="4" height="16"/>',
  check: '<polyline points="20 6 9 17 4 12"/>',
  alert: '<path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>',
  eye: '<path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/>',
  link: '<path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"/><path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"/>',
  send: '<line x1="22" y1="2" x2="11" y2="13"/><polygon points="22 2 15 22 11 13 2 9 22 2"/>',
  zap: '<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>',
  server: '<rect x="2" y="2" width="20" height="8" rx="2"/><rect x="2" y="14" width="20" height="8" rx="2"/><line x1="6" y1="6" x2="6.01" y2="6"/><line x1="6" y1="18" x2="6.01" y2="18"/>',
  wifi: '<path d="M5 12.55a11 11 0 0 1 14.08 0"/><path d="M1.42 9a16 16 0 0 1 21.16 0"/><path d="M8.53 16.11a6 6 0 0 1 6.95 0"/><line x1="12" y1="20" x2="12.01" y2="20"/>',
  list: '<line x1="8" y1="6" x2="21" y2="6"/><line x1="8" y1="12" x2="21" y2="12"/><line x1="8" y1="18" x2="21" y2="18"/><line x1="3" y1="6" x2="3.01" y2="6"/><line x1="3" y1="12" x2="3.01" y2="12"/><line x1="3" y1="18" x2="3.01" y2="18"/>',
  calendar: '<rect x="3" y="4" width="18" height="18" rx="2"/><line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/><line x1="3" y1="10" x2="21" y2="10"/>',
  film: '<rect x="2" y="2" width="20" height="20" rx="2.18"/><line x1="7" y1="2" x2="7" y2="22"/><line x1="17" y1="2" x2="17" y2="22"/><line x1="2" y1="12" x2="22" y2="12"/><line x1="2" y1="7" x2="7" y2="7"/><line x1="2" y1="17" x2="7" y2="17"/><line x1="17" y1="17" x2="22" y2="17"/><line x1="17" y1="7" x2="22" y2="7"/>',
  edit: '<path d="M12 20h9"/><path d="M16.5 3.5a2.12 2.12 0 0 1 3 3L7 19l-4 1 1-4L16.5 3.5z"/>',
  trash: '<polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/>',
  info: '<circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/>',
  win: '<rect x="3" y="3" width="8" height="8" fill="currentColor" stroke="none"/><rect x="13" y="3" width="8" height="8" fill="currentColor" stroke="none"/><rect x="3" y="13" width="8" height="8" fill="currentColor" stroke="none"/><rect x="13" y="13" width="8" height="8" fill="currentColor" stroke="none"/>',
  tab: '<polyline points="15 6 21 12 15 18"/><line x1="3" y1="12" x2="21" y2="12"/><line x1="21" y1="5" x2="21" y2="19"/>',
  more: '<circle cx="12" cy="12" r="1"/><circle cx="19" cy="12" r="1"/><circle cx="5" cy="12" r="1"/>',
  globe: '<circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/>',
  hdd: '<line x1="22" y1="12" x2="2" y2="12"/><path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/><line x1="6" y1="16" x2="6.01" y2="16"/>',
  image: '<rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/>',
  clock: '<circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>'
};

function icon(name) {
  return '<svg class="i" viewBox="0 0 24 24">' + (ICONS[name] || '') + '</svg>';
}

function abs(style, inner) {
  return '<div style="position:absolute;' + style + '">' + (inner || '') + '</div>';
}

function box(l, t, w, h, text, ng) {
  return '<div class="box' + (ng ? ' ng' : '') + '" style="left:' + l + '%;top:' + t + '%;width:' + w + '%;height:' + h + '%"><span>' + text + '</span></div>';
}

function scene(kind, ok) {
  let s = '';
  if (kind === 'chip') {
    s += abs('inset:0;background:radial-gradient(ellipse at 50% 45%,#3b4048 0,#16191E 75%)');
    s += abs('left:26%;top:25%;width:5%;height:50%;background:repeating-linear-gradient(#c9ccd1 0 6%,transparent 6% 12%)');
    s += abs('left:69%;top:25%;width:5%;height:50%;background:repeating-linear-gradient(#c9ccd1 0 6%,transparent 6% 12%)');
    s += abs('left:31%;top:20%;width:38%;height:60%;background:#2a2d33;border-radius:2%;box-shadow:0 0 0 .4cqw #4a4f57;display:flex;align-items:center;justify-content:center;color:#6b7280;font-size:2.2cqw', 'HS-8601');
    s += box(23, 15, 54, 70, ok ? '芯片定位 0.986' : '偏移 0.42 mm', !ok);
    s += '<div class="cross"></div>';
  } else if (kind === 'pcb') {
    s += abs('inset:0;background:#0d3b2c;background-image:repeating-linear-gradient(90deg,rgba(255,255,255,.05) 0 1px,transparent 1px 7%),repeating-linear-gradient(0deg,rgba(255,255,255,.05) 0 1px,transparent 1px 10%)');
    s += abs('left:10%;top:30%;width:80%;height:.5cqw;background:#b08d3c');
    s += abs('left:10%;top:68%;width:80%;height:.5cqw;background:#b08d3c');
    s += abs('left:48%;top:10%;width:.5cqw;height:80%;background:#b08d3c');
    for (let r = 0; r < 3; r++) {
      for (let c = 0; c < 5; c++) {
        s += abs('left:' + (12 + c * 16) + '%;top:' + (18 + r * 25) + '%;width:7%;height:12%;background:#d8c27a;border-radius:15%');
      }
    }
    s += box(10, 15, 11, 18, '焊点 OK');
    s += box(42, 40, 11, 18, ok ? '焊点 OK' : '少锡', !ok);
    s += box(74, 65, 11, 18, '焊点 OK');
  } else if (kind === 'glue') {
    s += abs('inset:0;background:#202327');
    s += abs('left:16%;top:14%;width:68%;height:72%;background:#8f969d;border-radius:4%');
    s += abs('left:23%;top:24%;width:54%;height:52%;border:1.3cqw solid rgba(250,240,190,.92);border-radius:7%');
    if (!ok) s += abs('left:72%;top:42%;width:6%;height:14%;background:#8f969d');
    s += box(19, 20, 12, 14, '胶宽 1.21 mm');
    s += ok ? box(68, 40, 12, 18, '胶宽 1.18 mm') : box(68, 40, 12, 18, '断胶', true);
  } else {
    s += abs('inset:0;background:#2a2622');
    s += abs('left:14%;top:10%;width:72%;height:80%;background:#c49a6c;border-radius:1%');
    s += abs('left:30%;top:24%;width:40%;height:52%;background:#f8f8f6;border-radius:1%');
    s += abs('left:35%;top:32%;width:30%;height:26%;background:repeating-linear-gradient(90deg,#111 0 .3cqw,transparent .3cqw .6cqw,#111 .6cqw .8cqw,transparent .8cqw 1.3cqw,#111 1.3cqw 1.8cqw,transparent 1.8cqw 2.1cqw)');
    s += abs('left:30%;top:61%;width:40%;text-align:center;font-size:1.8cqw;color:#222', '6 901234 567892');
    s += box(33, 29, 34, 39, ok ? '条码 OK' : '条码不可读', !ok);
  }
  return s;
}

function cursorSvg() {
  return '<svg viewBox="0 0 24 24"><path d="M4 2 L4 20 L9 15.5 L12.5 22 L15.5 20.5 L12 14 L19 14 Z" fill="#fff" stroke="#000" stroke-width="1.3" stroke-linejoin="round"/></svg>';
}

function screen(el) {
  const kind = el.dataset.v || 'chip';
  const ok = el.dataset.ok !== '0';
  const run = el.dataset.run !== '0';
  const st = el.dataset.t || '方案 A12';
  const okN = el.dataset.okn || '12,830';
  const ngN = el.dataset.ngn || '21';
  let h = '';
  h += '<div class="r-top"><span class="r-logo"></span><b>VisionPlatform</b><span class="r-m">文件&nbsp;&nbsp;方案&nbsp;&nbsp;工具&nbsp;&nbsp;视图</span>';
  h += '<span class="r-m">' + st + '</span>';
  h += '<span class="r-cnt" style="margin-left:auto">OK <b style="color:#52C41A">' + okN + '</b>&nbsp;&nbsp;NG <b style="color:#FF4D4F">' + ngN + '</b></span>';
  h += '<span class="r-pill' + (run ? '' : ' stop') + '" style="margin-left:0">' + (run ? '运行中' : '已停止') + '</span></div>';
  h += '<div class="r-body"><div class="r-img">' + scene(kind, ok) + '<div class="r-cam">CAM1 · 2448×2048</div></div>';
  h += '<div class="r-side"><div class="r-res' + (ok ? '' : ' ng') + '">' + (ok ? 'OK' : 'NG') + '</div>';
  h += '<div class="r-kv"><span>良率</span><b>99.84%</b></div><div class="r-kv"><span>节拍</span><b>0.82 s</b></div><div class="r-kv"><span>耗时</span><b>312 ms</b></div>';
  h += '<div class="r-line" style="width:90%"></div><div class="r-line" style="width:70%"></div><div class="r-line" style="width:82%"></div><div class="r-line" style="width:55%"></div></div></div>';
  if (el.dataset.task === '1') {
    h += '<div class="r-task"><i class="win"></i><i></i><i style="background:#2563EB"></i><i></i><span class="st">' + (el.dataset.clock || '14:03') + '</span></div>';
  }
  if (el.dataset.cursor) {
    const p = el.dataset.cursor.split(',');
    h += '<div class="cursor" style="left:' + p[0] + '%;top:' + p[1] + '%">' + cursorSvg() + '</div>';
  }
  el.insertAdjacentHTML('afterbegin', h);
}

document.querySelectorAll('.rs').forEach(screen);
document.querySelectorAll('i[data-i]').forEach(function (e) { e.outerHTML = icon(e.dataset.i); });
