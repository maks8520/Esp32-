document.addEventListener('DOMContentLoaded', () => {
    const state = {
        ws: null,
        currentPage: 'dashboard',
        rodData: { 1: { bite: 0, accel: 9.8 }, 2: { bite: 0, accel: 9.8 } },
        map: null
    };

    const elements = {
        navItems: document.querySelectorAll('.nav-item'),
        pages: document.querySelectorAll('.page'),
        predictorGauge: document.getElementById('predictorGauge'),
        predictorVal: document.getElementById('predictorVal'),
        statusDot: document.querySelector('.status-dot')
    };

    // WebSocket
    function connectWS() {
        const host = window.location.host || '192.168.4.1';
        state.ws = new WebSocket(`ws://${host}/ws`);

        state.ws.onopen = () => {
            elements.statusDot.classList.replace('disconnected', 'connected');
            document.getElementById('ws-status').innerText = 'CONNECTED';
        };

        state.ws.onclose = () => {
            elements.statusDot.classList.replace('connected', 'disconnected');
            document.getElementById('ws-status').innerText = 'RECONNECTING...';
            setTimeout(connectWS, 3000);
        };

        state.ws.onmessage = (e) => {
            const data = JSON.parse(e.data);
            if (data.type === 'rod_data') {
                handleRodUpdate(data);
            }
        };
    }

    function handleRodUpdate(data) {
        const id = data.id;
        const card = document.getElementById(`rod-${id}`);
        const status = document.getElementById(`rod-${id}-status`);
        const accel = document.getElementById(`rod-${id}-accel`);

        accel.innerText = (9.5 + Math.random()).toFixed(1);

        if (data.bite > 50) {
            card.classList.add('bite');
            status.innerText = 'BITE!';
            if (navigator.vibrate) navigator.vibrate([100, 50, 100]);
        } else {
            card.classList.remove('bite');
            status.innerText = 'READY';
        }
    }

    // Gauge
    function updateGauge(val) {
        const offset = 534 - (val / 100) * 534;
        elements.predictorGauge.style.strokeDashoffset = offset;
        elements.predictorVal.innerText = Math.round(val) + '%';
    }

    // Navigation
    elements.navItems.forEach(btn => {
        btn.addEventListener('click', () => {
            const pageId = btn.dataset.page;
            elements.navItems.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            elements.pages.forEach(p => p.classList.remove('active'));
            document.getElementById(pageId).classList.add('active');

            if (pageId === 'meteo' && !state.map) initMap();
            if (pageId === 'profile') initProfile();
        });
    });

    // Map
    function initMap() {
        state.map = L.map('windyMap', { zoomControl: false }).setView([45.03, 38.97], 10);
        L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png').addTo(state.map);
    }

    // Profile
    function initProfile() {
        const ctx = document.getElementById('profileChart').getContext('2d');
        new Chart(ctx, {
            type: 'line',
            data: {
                labels: ['0m', '500m', '1km', '2km', '5km', '10km'],
                datasets: [{
                    label: 'Temp',
                    borderColor: '#7dd3fc',
                    data: [25, 20, 15, 5, -20, -55],
                    fill: false,
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: { legend: { display: false } },
                scales: {
                    y: { grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#64748b' } },
                    x: { grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#64748b' } }
                }
            }
        });

        const tableBody = document.getElementById('profile-table-body');
        const heights = [0, 500, 1000, 2000, 5000, 10000];
        tableBody.innerHTML = heights.map(h => `
            <tr>
                <td>${h}m</td>
                <td>${(25 - h/180).toFixed(1)}°C</td>
                <td>${(3 + h/2000).toFixed(1)}m/s</td>
            </tr>
        `).join('');
    }

    // Init
    connectWS();
    setInterval(() => updateGauge(70 + Math.random() * 20), 2000);
});
