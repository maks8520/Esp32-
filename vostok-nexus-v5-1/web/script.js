document.addEventListener('DOMContentLoaded', () => {
    const state = {
        ws: null,
        currentPage: 'dashboard',
        rodData: { 1: { bite: 0, accel: 9.8 }, 2: { bite: 0, accel: 9.8 } },
        meteo: { temp: 24.5, wind: 3.2 },
        predictor: 0
    };

    const elements = {
        navItems: document.querySelectorAll('.nav-item'),
        pages: document.querySelectorAll('.page'),
        predictorGauge: document.getElementById('predictorGauge'),
        predictorVal: document.getElementById('predictorVal'),
        statusDot: document.querySelector('.status-dot')
    };

    // Navigation logic
    elements.navItems.forEach(item => {
        item.addEventListener('click', () => {
            const pageId = item.dataset.page;
            showPage(pageId);
        });
    });

    function showPage(pageId) {
        state.currentPage = pageId;
        elements.pages.forEach(p => p.classList.remove('active'));
        elements.navItems.forEach(n => n.classList.remove('active'));

        document.getElementById(pageId).classList.add('active');
        document.querySelector(`.nav-item[data-page="${pageId}"]`).classList.add('active');

        if (pageId === 'meteo') initMap();
        if (pageId === 'profile') initProfile();
    }

    // WebSocket implementation
    function connectWS() {
        const host = window.location.host || '192.168.4.1';
        state.ws = new WebSocket(`ws://${host}/ws`);

        state.ws.onopen = () => {
            elements.statusDot.classList.add('connected');
            elements.statusDot.classList.remove('disconnected');
            if(document.getElementById('ws-status')) document.getElementById('ws-status').innerText = 'CONNECTED';
        };

        state.ws.onclose = () => {
            elements.statusDot.classList.add('disconnected');
            elements.statusDot.classList.remove('connected');
            if(document.getElementById('ws-status')) document.getElementById('ws-status').innerText = 'RECONNECTING...';
            setTimeout(connectWS, 3000);
        };

        state.ws.onmessage = (e) => {
            try {
                const data = JSON.parse(e.data);
                handleUpdate(data);
            } catch (err) {
                console.error("WS Parse Error:", err);
            }
        };
    }

    function handleUpdate(data) {
        if (data.type === 'rod_data') {
            updateRodUI(data);
        } else if (data.type === 'meteo') {
            updateMeteoUI(data);
        }
    }

    function updateRodUI(data) {
        const { id, bite, accel } = data;
        const card = document.getElementById(`rod-${id}`);
        const accelEl = document.getElementById(`rod-${id}-accel`);
        const statusEl = document.getElementById(`rod-${id}-status`);

        if (accelEl) accelEl.innerText = accel.toFixed(1);

        if (bite > 60) {
            card.classList.add('bite');
            statusEl.innerText = 'BITE DETECTED!';
            statusEl.style.color = '#d4ff8f';
            if (window.navigator.vibrate) window.navigator.vibrate([200, 100, 200]);
        } else {
            card.classList.remove('bite');
            statusEl.innerText = 'MONITORING';
            statusEl.style.color = 'inherit';
        }

        // AI Predictor logic simulation if not sent from ESP
        updatePredictor(bite);
    }

    function updatePredictor(val) {
        state.predictor = Math.min(100, Math.max(0, val));
        elements.predictorVal.innerText = `${state.predictor}%`;
        // SVG Gauge animation: circumference is 534 (2 * PI * 85)
        const offset = 534 - (state.predictor / 100 * 534);
        elements.predictorGauge.style.strokeDashoffset = offset;
    }

    function updateMeteoUI(data) {
        if (data.temp) document.getElementById('m-temp').innerText = `${data.temp}°C`;
        if (data.wind) document.getElementById('m-wind').innerText = `${data.wind}m/s`;
    }

    // Initializations
    let map = null;
    function initMap() {
        if (map) return;
        map = L.map('windyMap', { zoomControl: false }).setView([55.75, 37.61], 10);
        L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png').addTo(map);
    }

    function initProfile() {
        const ctx = document.getElementById('profileChart').getContext('2d');
        if (window.pChart) window.pChart.destroy();
        window.pChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: ['0m', '500m', '1k', '3k', '5k', '10k'],
                datasets: [{
                    label: 'Temp Profile',
                    data: [24, 20, 15, 5, -10, -45],
                    borderColor: '#7dd3fc',
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: { legend: { display: false } },
                scales: {
                    y: { grid: { color: 'rgba(255,255,255,0.05)' } },
                    x: { grid: { display: false } }
                }
            }
        });
    }

    // Start
    connectWS();

    // Demo simulation
    setInterval(() => {
        if (state.ws && state.ws.readyState !== WebSocket.OPEN) {
            handleUpdate({ type: 'rod_data', id: 1, bite: Math.floor(Math.random() * 40), accel: 9.8 + Math.random() * 0.2 });
            handleUpdate({ type: 'rod_data', id: 2, bite: Math.floor(Math.random() * 40), accel: 9.7 + Math.random() * 0.2 });
        }
    }, 5000);
});
