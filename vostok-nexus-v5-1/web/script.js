document.addEventListener('DOMContentLoaded', () => {
    // --- State & Config ---
    let ws;
    let map;
    let windyKey = '';
    let mapLoaded = false;
    let currentBiteIntensity = 0;
    let targetBiteIntensity = 0;

    const elements = {
        navItems: document.querySelectorAll('.nav-item'),
        pages: document.querySelectorAll('.page'),
        statusDot: document.querySelector('.status-dot'),
        statusText: document.querySelector('.status-text'),
        predictorGauge: document.getElementById('predictorGauge'),
        confidenceGauge: document.getElementById('confidenceGauge'),
        predictorVal: document.getElementById('predictorVal'),
        confVal: document.getElementById('confVal'),
        windArrow: document.getElementById('windArrow'),
        confirmModal: document.getElementById('confirmModal'),
        fishModal: document.getElementById('fishModal'),
        aiBtn: document.querySelector('.ai-fish-id-btn')
    };

    // --- Performance: requestAnimationFrame for Animations ---
    let lastTime = 0;
    function animate(time) {
        // Smoothly interpolate bite intensity for wobble
        currentBiteIntensity += (targetBiteIntensity - currentBiteIntensity) * 0.1;

        document.querySelectorAll('.rod-card.bite').forEach(card => {
            const rot = Math.sin(time * 0.02) * (currentBiteIntensity / 20);
            const scale = 1 + (currentBiteIntensity / 1000);
            card.style.transform = `rotate(${rot}deg) scale(${scale})`;
        });

        // Water ripple effect movement
        const ripple = document.querySelector('.water-ripple');
        if (ripple) {
            const shiftX = Math.sin(time * 0.001) * 2;
            const shiftY = Math.cos(time * 0.001) * 2;
            ripple.style.transform = `translate(${shiftX}%, ${shiftY}%) scale(1.1)`;
        }

        requestAnimationFrame(animate);
    }
    requestAnimationFrame(animate);

    // --- UX: Haptics Utility ---
    function vibrate(pattern) {
        if ("vibrate" in navigator) {
            navigator.vibrate(pattern);
        }
    }

    // --- Performance: Debounce Utility ---
    function debounce(func, timeout = 300) {
        let timer;
        return (...args) => {
            clearTimeout(timer);
            timer = setTimeout(() => { func.apply(this, args); }, timeout);
        };
    }

    // --- Navigation System ---
    elements.navItems.forEach(item => {
        item.addEventListener('click', () => {
            vibrate(30);
            const pageId = item.getAttribute('data-page');

            elements.navItems.forEach(ni => ni.classList.remove('active'));
            item.classList.add('active');

            elements.pages.forEach(p => p.classList.remove('active'));
            const targetPage = document.getElementById(pageId);
            targetPage.classList.add('active');

            // Performance: Lazy load Windy Map
            if(pageId === 'meteo' && !mapLoaded) {
                lazyLoadMap();
            }
        });
    });

    // --- Windy Map Implementation (Lazy Load) ---
    function lazyLoadMap() {
        const link = document.createElement('link');
        link.rel = 'stylesheet';
        link.href = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.css';
        document.head.appendChild(link);

        const script = document.createElement('script');
        script.src = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.js';
        script.onload = initMap;
        document.head.appendChild(script);
        mapLoaded = true;
    }

    function initMap() {
        setTimeout(() => {
            const loading = document.querySelector('.map-loading-spinner');
            if(loading) loading.style.display = 'none';

            map = L.map('windyMap', {
                zoomControl: false,
                attributionControl: false
            }).setView([45.039, 38.975], 10);

            L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
                maxZoom: 19
            }).addTo(map);

            // Mock layer switching logic
            document.querySelectorAll('.layer-btn').forEach(btn => {
                btn.addEventListener('click', () => {
                    vibrate(20);
                    document.querySelectorAll('.layer-btn').forEach(b => b.classList.remove('active'));
                    btn.classList.add('active');
                    // In real Windy API: map.setOverlay(btn.dataset.layer)
                });
            });
        }, 1000);
    }

    // --- WebSocket & Telemetry ---
    function connect() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const host = window.location.host || '192.168.4.1';
        ws = new WebSocket(`${protocol}//${host}/ws`);

        ws.onopen = () => {
            elements.statusDot.className = 'status-dot connected';
            elements.statusText.innerText = 'CONNECTED';
        };

        ws.onclose = () => {
            elements.statusDot.className = 'status-dot disconnected';
            elements.statusText.innerText = 'DISCONNECTED';
            setTimeout(connect, 3000);
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                handleTelemetry(data);
            } catch(e) { console.error("WS Telemetry Error", e); }
        };
    }

    function handleTelemetry(data) {
        if (data.type === 'config') {
            windyKey = data.windy_key; // Received securely from NVS
            console.log("Config loaded");
        } else if (data.type === 'base_data') {
            updateBaseUI(data);
        } else if (data.type === 'rod_data') {
            updateRodUI(data);
        }
    }

    function updateBaseUI(data) {
        if(document.getElementById('dash-temp')) document.getElementById('dash-temp').innerText = data.temp.toFixed(1);
        const tempBig = document.querySelector('.temp-big');
        if(tempBig) tempBig.innerText = `${data.temp.toFixed(1)}°C`;
    }

    function updateRodUI(data) {
        const rodId = data.id;
        const card = document.getElementById(`rod-${rodId}`);
        if (!card) return;

        const glow = document.getElementById(`rod-${rodId}-glow`);
        const status = document.getElementById(`rod-${rodId}-status`);

        glow.style.height = `${data.bite}%`;

        if (data.bite > 60) {
            if(!card.classList.contains('bite')) vibrate([100, 50, 100]); // UX: Bite Alert
            card.classList.add('bite');
            status.innerText = "STRIKE!";
            targetBiteIntensity = data.bite;
        } else {
            card.classList.remove('bite');
            status.innerText = "READY";
            if (targetBiteIntensity > 0) targetBiteIntensity = 0;
        }
    }

    // --- Settings: Debounced Sliders ---
    const updateSettings = debounce((id, val) => {
        if(ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'setting', id: id, value: val }));
        }
        console.log(`Setting ${id} changed to ${val}`);
    });

    document.querySelectorAll('.debounced-slider').forEach(slider => {
        slider.addEventListener('input', (e) => {
            updateSettings(e.target.id, e.target.value);
        });
    });

    // --- Modals: Confirmations for Destructive Actions ---
    function showConfirm(title, desc, onConfirm) {
        document.getElementById('modal-title').innerText = title;
        document.getElementById('modal-desc').innerText = desc;
        elements.confirmModal.classList.add('active');

        const confirmBtn = elements.confirmModal.querySelector('.btn-confirm');
        const cancelBtn = elements.confirmModal.querySelector('.btn-cancel');

        const cleanup = () => {
            elements.confirmModal.classList.remove('active');
            confirmBtn.onclick = null;
            cancelBtn.onclick = null;
        };

        confirmBtn.onclick = () => { vibrate(50); onConfirm(); cleanup(); };
        cancelBtn.onclick = () => { vibrate(20); cleanup(); };
    }

    document.getElementById('btn-erase').addEventListener('click', () => {
        showConfirm("ERASE ALL LOGS?", "This will permanently wipe the SD card telemetry data.", () => {
            console.log("SD Wipe triggered");
        });
    });

    // --- Charts & Profile (High Fidelity Steps) ---
    function initProfile() {
        const ctx = document.getElementById('verticalProfileChart').getContext('2d');
        // Steps logic: 100m for first 1km, then 2km steps
        const labels = ['0m', '100m', '200m', '300m', '400m', '500m', '600m', '700m', '800m', '900m', '1000m', '3000m', '5000m', '7000m', '9000m', '10000m'];
        const tempPoints = [24, 23.5, 23, 22.5, 22, 21.5, 21, 20.5, 20, 19.5, 19, 5, -10, -25, -45, -55];

        new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Temp',
                    data: tempPoints,
                    borderColor: '#7dd3fc',
                    backgroundColor: 'rgba(125, 211, 252, 0.1)',
                    fill: true,
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: { ticks: { color: '#94a3b8', font: { family: 'JetBrains Mono', size: 9 } } },
                    y: { ticks: { color: '#94a3b8' } }
                },
                plugins: { legend: { display: false } }
            }
        });

        const tableBody = document.getElementById('profileData');
        labels.forEach((l, i) => {
            const row = `<tr><td>${l}</td><td>${tempPoints[i]}°C</td><td>${4 + i}m/s</td><td>${Math.max(0, 100-i*6)}%</td></tr>`;
            tableBody.innerHTML += row;
        });
    }

    // --- AI Modal Simulator ---
    if(elements.aiBtn) {
        elements.aiBtn.addEventListener('click', () => {
            vibrate(50);
            elements.fishModal.classList.add('active');
            const result = elements.fishModal.querySelector('.fish-result');
            const loader = elements.fishModal.querySelector('.scan-loader');

            result.classList.add('hidden');
            loader.style.display = 'block';

            setTimeout(() => {
                vibrate([50, 30, 50]);
                loader.style.display = 'none';
                result.classList.remove('hidden');
            }, 3500);
        });
    }

    document.querySelector('.close-modal').addEventListener('click', () => {
        elements.fishModal.classList.remove('active');
    });

    // --- Init ---
    initProfile();
    connect();

    // High-fidelity simulation for offline testing
    setInterval(() => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            handleTelemetry({ type: 'base_data', temp: 24.2 + (Math.random() - 0.5) });
            handleTelemetry({ type: 'rod_data', id: 1, bite: Math.random() * 100 });
        }
    }, 2000);
});
