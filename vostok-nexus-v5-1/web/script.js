document.addEventListener('DOMContentLoaded', () => {
    /* Инициализация состояния */
    let ws;
    let windyMap;
    let gaugeValue = 0;
    const elements = {
        predictorGauge: document.getElementById('predictorGauge'),
        predictorVal: document.getElementById('predictorVal'),
        navItems: document.querySelectorAll('.nav-item'),
        pages: document.querySelectorAll('.page'),
        statusDot: document.querySelector('.status-dot')
    };

    /* WebSocket и Обработка событий */
    function connect() {
        const host = window.location.host || '192.168.4.1';
        ws = new WebSocket(`ws://${host}/ws`);

        ws.onopen = () => { elements.statusDot.className = 'status-dot connected'; };
        ws.onclose = () => { elements.statusDot.className = 'status-dot disconnected'; setTimeout(connect, 3000); };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.type === 'rod_data') {
                    updateRodUI(data);
                } else if (data.type === 'gps_data') {
                    updateGPS(data);
                }
            } catch(e) { console.error("Ошибка парсинга", e); }
        };
    }

    function updateRodUI(data) {
        const card = document.getElementById(`rod-${data.id}`);
        const status = document.getElementById(`rod-${data.id}-status`);

        if (data.bite > 50) {
            if (!card.classList.contains('bite')) {
                card.classList.add('bite');
                status.innerText = "BITE!";
                vibrateBite(data.id);
            }
        } else {
            card.classList.remove('bite');
            status.innerText = "READY";
        }
    }

    function updateGPS(data) {
        if (windyMap && data.lat && data.lon) {
            windyMap.setView([data.lat, data.lon], 13);
            console.log("GPS Fix: Map Centered");
        }
    }

    /* Вибрация (Haptics) */
    function vibrateBite(rodId) {
        if ("vibrate" in navigator) {
            // Разные паттерны для разных удочек
            const pattern = rodId === 1 ? [100, 50, 100, 50, 200] : [200, 100, 200, 100, 300];
            navigator.vibrate(pattern);
        }
    }

    /* Анимация Gauge */
    function updateGauge(val) {
        gaugeValue = val;
        const offset = 534 - (val / 100) * 534;
        elements.predictorGauge.style.strokeDashoffset = offset;
        elements.predictorVal.innerText = `${Math.round(val)}%`;
    }

    /* Карта Windy (Leaflet Base) */
    function initMap() {
        windyMap = L.map('windyMap', { zoomControl: false }).setView([45.03, 38.97], 10);
        L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png').addTo(windyMap);
    }

    /* Вертикальный профиль (Chart.js) */
    function initProfileChart() {
        const ctx = document.getElementById('meteoProfileChart').getContext('2d');
        new Chart(ctx, {
            type: 'line',
            data: {
                labels: ['0m', '100m', '500m', '1km', '3km', '5km', '10km'],
                datasets: [{
                    label: 'Температура (°C)',
                    data: [24, 23, 20, 18, 5, -15, -50],
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
                    x: { grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#94a3b8' } },
                    y: { grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#94a3b8' } }
                }
            }
        });
    }

    /* Навигация */
    elements.navItems.forEach(item => {
        item.addEventListener('click', () => {
            const pageId = item.getAttribute('data-page');
            elements.navItems.forEach(ni => ni.classList.remove('active'));
            item.classList.add('active');
            elements.pages.forEach(p => p.classList.remove('active'));
            document.getElementById(pageId).classList.add('active');
            if (pageId === 'meteo' && windyMap) {
                setTimeout(() => windyMap.invalidateSize(), 200);
            }
        });
    });

    initMap();
    initProfileChart();
    connect();

    setInterval(() => {
        updateGauge(65 + Math.random() * 25);
    }, 3000);
});
