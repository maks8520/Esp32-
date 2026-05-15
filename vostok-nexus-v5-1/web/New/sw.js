/* ═══════════════════════════════════════════════════════════
   VOSTOK NEXUS v5.1 — sw.js (Service Worker)
   Стратегия: Cache-First для статики, Network-First для API
   ═══════════════════════════════════════════════════════════ */

'use strict';

const CACHE_NAME    = 'vostok-nexus-v5.1';
const CACHE_STATIC  = 'vostok-static-v5.1';
const CACHE_FONTS   = 'vostok-fonts-v5.1';

/* Файлы для предварительного кеширования */
const STATIC_ASSETS = [
  './index.html',
  './styles.css',
  './script.js',
  './manifest.json',
];

/* Внешние ресурсы (шрифты, иконки) */
const FONT_ORIGINS = [
  'https://fonts.googleapis.com',
  'https://fonts.gstatic.com',
];

/* ─── INSTALL: предзагрузка статики ─── */
self.addEventListener('install', event => {
  console.log('[SW] Installing VOSTOK NEXUS v5.1...');
  event.waitUntil(
    caches.open(CACHE_STATIC)
      .then(cache => cache.addAll(STATIC_ASSETS))
      .then(() => self.skipWaiting())
      .catch(err => console.warn('[SW] Install error:', err))
  );
});

/* ─── ACTIVATE: очистка старых кешей ─── */
self.addEventListener('activate', event => {
  console.log('[SW] Activating v5.1...');
  event.waitUntil(
    caches.keys().then(keys =>
      Promise.all(
        keys
          .filter(k => ![CACHE_STATIC, CACHE_FONTS].includes(k))
          .map(k => {
            console.log('[SW] Deleting old cache:', k);
            return caches.delete(k);
          })
      )
    ).then(() => self.clients.claim())
  );
});

/* ─── FETCH: стратегия обслуживания ─── */
self.addEventListener('fetch', event => {
  const { request } = event;
  const url = new URL(request.url);

  /* Пропускаем non-GET и chrome-extension */
  if (request.method !== 'GET' || url.protocol === 'chrome-extension:') return;

  /* Шрифты Google — Cache-First с долгим TTL */
  if (FONT_ORIGINS.some(o => url.origin.includes(o.replace('https://', '')))) {
    event.respondWith(cacheFirst(request, CACHE_FONTS));
    return;
  }

  /* Open-Meteo API — Network-First (свежие данные важнее) */
  if (url.hostname === 'api.open-meteo.com') {
    event.respondWith(networkFirst(request, CACHE_NAME));
    return;
  }

  /* Статические файлы приложения — Cache-First */
  event.respondWith(cacheFirst(request, CACHE_STATIC));
});

/* ─── Стратегии ─── */

/** Cache-First: берём из кеша, если нет — с сети и кешируем */
async function cacheFirst(request, cacheName) {
  const cached = await caches.match(request);
  if (cached) return cached;

  try {
    const response = await fetch(request);
    if (response.ok) {
      const cache = await caches.open(cacheName);
      cache.put(request, response.clone());
    }
    return response;
  } catch {
    return offlineFallback(request);
  }
}

/** Network-First: пробуем сеть, при ошибке — кеш */
async function networkFirst(request, cacheName) {
  try {
    const response = await fetch(request);
    if (response.ok) {
      const cache = await caches.open(cacheName);
      cache.put(request, response.clone());
    }
    return response;
  } catch {
    const cached = await caches.match(request);
    return cached || offlineFallback(request);
  }
}

/** Оффлайн-заглушка */
function offlineFallback(request) {
  if (request.destination === 'document') {
    return caches.match('./index.html');
  }
  return new Response('', { status: 503, statusText: 'Offline' });
}

/* ─── Push Notifications (для будущего ESP32 интеграции) ─── */
self.addEventListener('push', event => {
  if (!event.data) return;
  const data = event.data.json();
  self.registration.showNotification(data.title || '🎣 VOSTOK NEXUS', {
    body: data.body || 'Поклёвка!',
    icon: 'https://img.icons8.com/neon/192/anchor.png',
    badge: 'https://img.icons8.com/neon/96/anchor.png',
    vibrate: [100, 50, 100, 50, 200],
    tag: 'vostok-bite',
    renotify: true,
    data: { url: data.url || './' },
  });
});

self.addEventListener('notificationclick', event => {
  event.notification.close();
  event.waitUntil(
    clients.openWindow(event.notification.data.url || './')
  );
});

console.log('[SW] VOSTOK NEXUS v5.1 Service Worker loaded ✅');
