/* ═══════════════════════════════════════════════════════════
   VOSTOK NEXUS v5.1 ULTRA PREMIUM — script.js
   ═══════════════════════════════════════════════════════════ */

'use strict';

/* ─── Constants ─── */
const STORAGE_KEY = 'vostok_nexus_v5_catches';

const HEIGHTS   = [0,100,200,300,400,500,600,700,800,900,1000,2000,4000,6000,8000,10000];
const COMPASS16 = ['С','ССВ','СВ','ВСВ','В','ВЮВ','ЮВ','ЮВЮ','Ю','ЮЗЮ','ЮЗ','ЗЮЗ','З','ЗСЗ','СЗ','СЗС'];

/* ─── Application State ─── */
const STATE = {
  view:     'dashboard',
  atmoMode: 'table',
  layer:    'mix',
  dirMode:  'from',
  predPct:  78,
  predDir:  1,
  catches:  [],
};

/* ═══════════════════════════════════════════════════════════
   STORAGE
   ═══════════════════════════════════════════════════════════ */
function loadFromStorage() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw) {
      const data = JSON.parse(raw);
      STATE.catches = Array.isArray(data.catches) ? data.catches : [];
    }
  } catch (e) {
    console.warn('[VOSTOK] Storage load error:', e);
    STATE.catches = [];
  }
}

function saveToStorage() {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify({ catches: STATE.catches }));
  } catch (e) {
    console.warn('[VOSTOK] Storage save error:', e);
  }
}

function clearAllData() {
  if (!confirm('⚠️ Очистить все данные? Это необратимо!')) return;
  STATE.catches = [];
  localStorage.removeItem(STORAGE_KEY);
  renderCatchJournal();
  updateDataStats();
  showToast('✓ Данные очищены');
}

function updateDataStats() {
  const count  = STATE.catches.length;
  const sizeKB = (new Blob([JSON.stringify(STATE.catches)]).size / 1024).toFixed(1);
  setEl('data-stats', count);
  setEl('data-size', `~${sizeKB} KB`);
}

/* ═══════════════════════════════════════════════════════════
   NAVIGATION
   ═══════════════════════════════════════════════════════════ */
const VIEW_ORDER = { dashboard: 0, meteo: 1, fishing: 2, map: 3, settings: 4 };

function switchView(name) {
  document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
  document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));

  document.getElementById('view-' + name).classList.add('active');
  document.querySelectorAll('.nav-btn')[VIEW_ORDER[name]].classList.add('active');

  STATE.view = name;

  if (name === 'meteo') {
    renderAtmoTable();
    setTimeout(drawAtmoGraph, 80);
  }
  if (name === 'fishing')  renderCatchJournal();
  if (name === 'settings') updateDataStats();
}

/* ═══════════════════════════════════════════════════════════
   TOAST
   ═══════════════════════════════════════════════════════════ */
let _toastTimer;
function showToast(msg) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.classList.add('show');
  clearTimeout(_toastTimer);
  _toastTimer = setTimeout(() => el.classList.remove('show'), 3000);
}

/* ═══════════════════════════════════════════════════════════
   HELPERS
   ═══════════════════════════════════════════════════════════ */
function setEl(id, val) {
  const el = document.getElementById(id);
  if (el) el.textContent = val;
}

function now() {
  return new Date().toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit' });
}

/* ═══════════════════════════════════════════════════════════
   CLOCK & LIVE DATA
   ═══════════════════════════════════════════════════════════ */
function updateClock() {
  const t = new Date().toLocaleTimeString('ru-RU', {
    hour: '2-digit', minute: '2-digit', second: '2-digit'
  });
  setEl('gps-coords', `54.7842°N 56.0342°E · ${t}`);
  setEl('dash-time', `Полевой комплекс · ${t}`);
}

function simulateLiveData() {
  const noise = () => (Math.random() - 0.5) * 0.4;
  setEl('r1-temp', (14.2 + noise()).toFixed(1) + '°C');
  setEl('r2-temp', (13.8 + noise()).toFixed(1) + '°C');
  setEl('r2-vib',  (0.08 + Math.random() * 0.08).toFixed(2) + 'g');

  const r2bar = document.getElementById('r2-bar');
  if (r2bar) r2bar.style.width = Math.min(10 + Math.random() * 6, 15) + '%';

  setEl('m-qnh',  String(Math.round(1013 + noise() * 2)));
  setEl('m-temp', '+' + (18.4 + noise()).toFixed(1) + '°');
}

/* ═══════════════════════════════════════════════════════════
   AI PREDICTOR GAUGE
   ═══════════════════════════════════════════════════════════ */
function animatePredictor() {
  STATE.predPct += STATE.predDir * (0.3 + Math.random() * 0.5);
  if (STATE.predPct > 94) STATE.predDir = -1;
  if (STATE.predPct < 58) STATE.predDir =  1;
  STATE.predPct = Math.max(55, Math.min(97, STATE.predPct));

  const v    = Math.round(STATE.predPct);
  const ring = document.getElementById('gaugeRing');
  setEl('pred-pct', String(v));
  if (ring) ring.setAttribute('stroke-dashoffset', String(465 - (v / 100) * 465));
}

/* ═══════════════════════════════════════════════════════════
   ROD CARDS
   ═══════════════════════════════════════════════════════════ */
function rodClicked(id) {
  showToast(`🎣 Удочка #${id} — нажата`);
}

/* ═══════════════════════════════════════════════════════════
   ATMOSPHERE DATA MODEL
   ═══════════════════════════════════════════════════════════ */
function compass(deg) {
  return COMPASS16[Math.round(deg / 22.5) % 16];
}

function cloudDesc(pct) {
  if (pct <  8)  return 'Ясно';
  if (pct < 25)  return 'Малооблачно';
  if (pct < 55)  return 'Переменная';
  if (pct < 85)  return 'Облачно';
  return 'Пасмурно';
}

function atmoPoint(h) {
  const t  = 12.4 - 0.0063 * h + Math.sin(h / 730) * 1.4;
  const rh = Math.max(18, Math.min(95,
    86 - h * 0.006 + Math.sin((h + 260) / 760) * 18 + Math.sin(h / 2100) * 10
  ));
  const press = 1013 * Math.pow(1 - h * 0.0000226, 5.256);
  const wind  = Math.max(0.8,
    3.8 + h / 880 + Math.sin(h / 900) * 2.6 + Math.max(0, h - 2200) / 1300
  );

  let dir = 232 + h / 53 + Math.sin(h / 1200) * 36;
  if (STATE.dirMode === 'to') dir = (dir + 180) % 360;
  dir = ((dir % 360) + 360) % 360;

  let cloud;
  if      (h <= 1000) cloud = 50 + Math.sin((h + 140) / 260) * 22 + (rh - 65) * 0.55;
  else if (h <  2000) cloud = 42 + Math.sin(h / 430) * 20 + (rh - 58) * 0.42;
  else if (h <  6000) cloud = 35 + Math.sin((h - 1800) / 820) * 28 + (rh - 48) * 0.58;
  else                cloud = 56 + Math.sin(h / 1100) * 24 + (rh - 36) * 0.35;
  cloud = Math.max(0, Math.min(100, cloud));

  return {
    h,
    t:     +t.toFixed(1),
    rh:    Math.round(rh),
    press: +press.toFixed(1),
    wind:  +wind.toFixed(1),
    dir:   Math.round(dir),
    cloud: Math.round(cloud),
  };
}

/* ─── Atmosphere Table ─── */
function renderAtmoTable() {
  const tbody = document.getElementById('atmoTbody');
  if (!tbody) return;

  tbody.innerHTML = HEIGHTS.map(h => {
    const p      = atmoPoint(h);
    const dirRot = STATE.dirMode === 'to' ? p.dir - 90 : p.dir + 90;
    const tenths = Math.max(0, Math.min(10, Math.round(p.cloud / 10)));

    return `<tr>
      <td>${h}м</td>
      <td class="tc-temp">${p.t}°</td>
      <td class="tc-rh">${p.rh}%</td>
      <td class="tc-press">${p.press}</td>
      <td class="tc-wind">${p.wind}</td>
      <td>
        <div class="wind-cell">
          <div class="wind-arrow">
            <svg viewBox="0 0 20 20">
              <path d="M10 3l4 8H6l4-8z" fill="currentColor" transform="rotate(${dirRot},10,10)"/>
            </svg>
          </div>
          <span>${p.dir}° ${compass(p.dir)}</span>
        </div>
      </td>
      <td>
        <div class="cloud-bar-wrap">
          <div class="cloud-bar">
            <div class="cloud-bar-fill" style="width:${p.cloud}%;background:var(--cyan)"></div>
          </div>
          <span class="tc-cloud">${p.cloud}% (${tenths}/10)</span>
        </div>
        <div class="cloud-desc">${cloudDesc(p.cloud)}</div>
      </td>
      <td class="tc-type">
        ${p.cloud < 20 ? '☀️ Ясно' : p.cloud < 50 ? '🌤 Кучевые' : '☁️ Облачно'}
      </td>
    </tr>`;
  }).join('');
}

/* ─── Atmosphere Mode & Layer Switchers ─── */
function switchAtmoMode(mode, btn) {
  STATE.atmoMode = mode;

  document.querySelectorAll('#modeBtns .seg-btn').forEach(b => b.classList.remove('active'));
  btn.classList.add('active');

  document.getElementById('atmoTableView').style.display  = mode === 'table' ? '' : 'none';
  document.getElementById('atmoGraphView').style.display  = mode === 'graph' ? '' : 'none';
  document.getElementById('layerBtns').style.display      = mode === 'graph' ? '' : 'none';

  if (mode === 'graph') setTimeout(drawAtmoGraph, 60);
  else renderAtmoTable();
}

function switchLayer(layer, btn) {
  STATE.layer = layer;
  document.querySelectorAll('#layerBtns .seg-btn').forEach(b => b.classList.remove('active'));
  btn.classList.add('active');
  drawAtmoGraph();
}

function toggleWindDir() {
  STATE.dirMode = STATE.dirMode === 'from' ? 'to' : 'from';
  const btn = document.getElementById('dirToggleBtn');
  btn.classList.toggle('active', STATE.dirMode === 'to');
  btn.textContent = STATE.dirMode === 'to' ? '✓ +180° куда дует' : '+180° куда дует';

  if (STATE.atmoMode === 'table') renderAtmoTable();
  else drawAtmoGraph();

  showToast(STATE.dirMode === 'to'
    ? '→ Показывает куда дует ветер'
    : '← Показывает откуда дует ветер'
  );
}

/* ─── Atmosphere Graph ─── */
function drawAtmoGraph() {
  const cv = document.getElementById('atmoCanvas');
  if (!cv || !cv.offsetParent) return;

  const dpr = Math.min(devicePixelRatio, 2);
  const W   = cv.offsetWidth  * dpr;
  const H   = cv.offsetHeight * dpr;
  cv.width  = W;
  cv.height = H;

  const ctx = cv.getContext('2d');
  ctx.clearRect(0, 0, W, H);

  const PAD_L = 50 * dpr, PAD_R = 16 * dpr;
  const PAD_T = 12 * dpr, PAD_B =  8 * dpr;
  const CW = W - PAD_L - PAD_R;
  const CH = H - PAD_T - PAD_B;

  function hToY(h) {
    const k = Math.log1p(h / 80) / Math.log1p(10000 / 80);
    return PAD_T + (1 - k) * CH;
  }

  function vToX(v, min, max) {
    return PAD_L + Math.max(0, Math.min(1, (v - min) / (max - min))) * CW;
  }

  /* Grid */
  HEIGHTS.forEach(h => {
    const y     = hToY(h);
    const major = h % 1000 === 0;
    ctx.strokeStyle = major ? 'rgba(125,211,252,.15)' : 'rgba(255,255,255,.05)';
    ctx.lineWidth   = major ? 1.2 * dpr : 0.6 * dpr;
    ctx.beginPath();
    ctx.moveTo(PAD_L, y);
    ctx.lineTo(W - PAD_R, y);
    ctx.stroke();
  });

  const pts = HEIGHTS.map(h => atmoPoint(h));

  function drawLine(getter, min, max, color) {
    const data = pts.map(p => ({ x: vToX(getter(p), min, max), y: hToY(p.h) }));
    ctx.strokeStyle = color;
    ctx.lineWidth   = 2.5 * dpr;
    ctx.lineJoin    = 'round';
    ctx.shadowColor = color;
    ctx.shadowBlur  = 10 * dpr;
    ctx.beginPath();
    data.forEach((d, i) => i === 0 ? ctx.moveTo(d.x, d.y) : ctx.lineTo(d.x, d.y));
    ctx.stroke();
    ctx.shadowBlur = 0;
    data.forEach(d => {
      ctx.fillStyle = color;
      ctx.beginPath();
      ctx.arc(d.x, d.y, 3.5 * dpr, 0, Math.PI * 2);
      ctx.fill();
    });
  }

  if (STATE.layer === 'mix'   || STATE.layer === 'wind')  drawLine(p => p.wind,          0, 30,  '#d4ff8f');
  if (STATE.layer === 'mix'   || STATE.layer === 'cloud') drawLine(p => p.cloud,          0, 100, '#7dd3fc');
  if (STATE.layer === 'mix'   || STATE.layer === 'temp')  drawLine(p => p.t + 55,         0, 70,  '#fbbf24');
}

/* ═══════════════════════════════════════════════════════════
   FORM VALIDATION
   ═══════════════════════════════════════════════════════════ */
function validateFish() {
  const el  = document.getElementById('in-fish');
  const ok  = el.value.trim().length >= 2;
  el.classList.toggle('error',   !ok);
  el.classList.toggle('success',  ok);
  return ok;
}

function validateWeight() {
  const el  = document.getElementById('in-weight');
  const msg = document.getElementById('val-weight');
  const v   = parseFloat(el.value);
  const ok  = !isNaN(v) && v >= 0.1 && v <= 100;
  el.classList.toggle('error',   !ok);
  el.classList.toggle('success',  ok);
  if (msg) msg.innerHTML = ok
    ? '<span class="validation-success">✓ OK</span>'
    : '<span class="validation-error">❌ Вес 0.1–100 кг</span>';
  return ok;
}

function validateLength() {
  const el  = document.getElementById('in-length');
  const msg = document.getElementById('val-length');
  const v   = parseFloat(el.value);
  const ok  = el.value === '' || (!isNaN(v) && v >= 5 && v <= 300);
  el.classList.toggle('error',   !ok);
  el.classList.toggle('success',  ok && el.value !== '');
  if (msg) msg.innerHTML = !ok
    ? '<span class="validation-error">❌ Длина 5–300 см</span>'
    : '';
  return ok;
}

/* ═══════════════════════════════════════════════════════════
   CATCH JOURNAL
   ═══════════════════════════════════════════════════════════ */
function renderCatchJournal() {
  const el = document.getElementById('catch-journal');
  if (!el) return;

  if (STATE.catches.length === 0) {
    el.innerHTML = `<div style="text-align:center;padding:20px;color:var(--text-3);font-size:13px">
      Улов пока не записан 🎣
    </div>`;
  } else {
    el.innerHTML = STATE.catches.map((c, i) => `
      <div class="catch-entry" style="animation:slideInDown ${0.2 + i * 0.05}s ease-out">
        <div class="catch-icon">🐟</div>
        <div class="catch-info">
          <div class="catch-name">${c.fish}</div>
          <div class="catch-meta">${c.bait} · ${c.time}</div>
        </div>
      </div>
    `).join('');
  }

  setEl('catch-count', STATE.catches.length + ' записей');
}

function saveCatch() {
  if (!validateFish() || !validateWeight() || !validateLength()) {
    showToast('⚠️ Проверьте поля формы');
    return;
  }

  const fish   = document.getElementById('in-fish').value.trim();
  const weight = document.getElementById('in-weight').value.trim();
  const length = document.getElementById('in-length').value.trim();
  const bait   = document.getElementById('in-bait').value.trim() || '—';
  const label  = `${fish} ${weight}кг` + (length ? ` (${length}см)` : '');

  STATE.catches.unshift({ fish: label, bait, time: now() });
  saveToStorage();
  renderCatchJournal();
  updateDataStats();

  /* reset form */
  ['in-fish', 'in-weight', 'in-length', 'in-bait'].forEach(id => {
    const el = document.getElementById(id);
    if (el) { el.value = ''; el.classList.remove('error', 'success'); }
  });
  ['val-weight', 'val-length'].forEach(id => setEl(id, ''));

  showToast('✅ Улов записан!');
}

function exportCatches() {
  if (STATE.catches.length === 0) {
    showToast('⚠️ Нечего экспортировать');
    return;
  }
  let csv = 'Рыба,Приманка,Время\n';
  STATE.catches.forEach(c => {
    csv += `"${c.fish}","${c.bait}","${c.time}"\n`;
  });
  const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
  const link = document.createElement('a');
  link.href     = URL.createObjectURL(blob);
  link.download = `vostok_ulovy_${Date.now()}.csv`;
  link.click();
  showToast('✓ Файл скачан');
}

/* ═══════════════════════════════════════════════════════════
   AI FISH ID MODAL
   ═══════════════════════════════════════════════════════════ */
const FISH_DB = [
  { name: 'Щука обыкновенная',   weight: '1.5–2.1 кг', conf: 94 },
  { name: 'Окунь речной',        weight: '280–450 г',   conf: 87 },
  { name: 'Карп обыкновенный',   weight: '2.0–3.2 кг',  conf: 91 },
  { name: 'Язь',                 weight: '600–900 г',   conf: 85 },
  { name: 'Лещ',                 weight: '800–1.4 кг',  conf: 89 },
];

function openFishIDModal() {
  document.getElementById('fishIDModal').classList.add('active');
  showToast('📸 Инициализация сканера...');
  setTimeout(simulateFishID, 2200);
}

function closeFishIDModal() {
  document.getElementById('fishIDModal').classList.remove('active');
}

function startScanFish() {
  const scanner = document.getElementById('fishScanner');
  const result  = document.getElementById('fishResult');
  scanner.classList.add('active');
  result.classList.remove('show');
  showToast('📸 Сканируем...');
  setTimeout(simulateFishID, 2500);
}

function simulateFishID() {
  const fish    = FISH_DB[Math.floor(Math.random() * FISH_DB.length)];
  const scanner = document.getElementById('fishScanner');
  const result  = document.getElementById('fishResult');

  scanner.classList.remove('active');
  result.classList.add('show');

  setEl('fishResultName',   fish.name);
  setEl('fishResultWeight', '~' + fish.weight);
  setEl('fishResultConf',   'Уверенность: ' + fish.conf + '%');
  setEl('fishResultMeta',   'По размеру относительно монеты (d=24.26mm). Характер чешуи и окраска подтверждают вид.');

  showToast('✅ Рыба определена: ' + fish.name);
}

function confirmFishResult() {
  const name   = document.getElementById('fishResultName').textContent;
  const weight = document.getElementById('fishResultWeight').textContent
    .replace('~', '').split('–')[0].trim();

  document.getElementById('in-fish').value   = name;
  document.getElementById('in-weight').value = weight;
  validateFish();
  validateWeight();

  closeFishIDModal();
  showToast('✓ Результат применён! Нажмите «Сохранить»');
}

/* ═══════════════════════════════════════════════════════════
   SYNC (demo)
   ═══════════════════════════════════════════════════════════ */
function syncData() {
  showToast('🔄 Синхронизация...');
  setTimeout(() => showToast('✅ Данные обновлены'), 1600);
}

/* ═══════════════════════════════════════════════════════════
   SERVICE WORKER REGISTRATION
   ═══════════════════════════════════════════════════════════ */
function registerSW() {
  if ('serviceWorker' in navigator) {
    navigator.serviceWorker.register('./sw.js')
      .then(() => console.log('[VOSTOK] SW registered'))
      .catch(e  => console.warn('[VOSTOK] SW error:', e));
  }
}

/* ═══════════════════════════════════════════════════════════
   BOOT
   ═══════════════════════════════════════════════════════════ */
document.addEventListener('DOMContentLoaded', () => {
  loadFromStorage();
  renderCatchJournal();
  updateDataStats();
  updateClock();
  renderAtmoTable();

  /* Live intervals */
  setInterval(updateClock,       1000);
  setInterval(animatePredictor,  2800);
  setInterval(simulateLiveData,  1400);

  /* PWA */
  registerSW();

  console.log('[VOSTOK NEXUS v5.1] ✅ Boot complete');
});
