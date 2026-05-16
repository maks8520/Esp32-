document.addEventListener('DOMContentLoaded', () => {
    // 1. Инициализация Карты (Leaflet)
    // Координаты по умолчанию: Яровое
    const map = L.map('map').setView([52.9275, 78.5828], 13);
    
    // Темная тема для карты (работает при наличии интернета на клиенте)
    L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
        attribution: 'VOSTOK NEXUS',
        maxZoom: 19
    }).addTo(map);

    // Маркер Базовой Станции
    const baseMarker = L.marker([52.9275, 78.5828]).addTo(map)
        .bindPopup('<b>VOSTOK NEXUS</b><br>Базовая станция').openPopup();

    // 2. Инициализация Графика (Chart.js)
    const ctx = document.getElementById('weatherChart').getContext('2d');
    const weatherChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [], // Время
            datasets: [
                {
                    label: 'Ветер (м/с)',
                    borderColor: '#7dd3fc',
                    backgroundColor: 'rgba(125, 211, 252, 0.1)',
                    data: [],
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4
                },
                {
                    label: 'Аномалия ИИ (%)',
                    borderColor: '#d4ff8f',
                    backgroundColor: 'transparent',
                    data: [],
                    borderWidth: 2,
                    borderDash: [5, 5],
                    tension: 0.4
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: { ticks: { color: '#9ca3af' }, grid: { color: 'rgba(255,255,255,0.05)' } },
                y: { ticks: { color: '#9ca3af' }, grid: { color: 'rgba(255,255,255,0.05)' } }
            },
            plugins: {
                legend: { labels: { color: '#f3f4f6' } }
            }
        }
    });

    // 3. WebSocket Соединение
    const wsUrl = `ws://${window.location.hostname}/ws`;
    window.wsClient = new WebSocket(wsUrl);

    window.wsClient.onopen = () => {
        console.log("WebSocket подключен!");
        if (typeof updateWebSocketStatus === 'function') updateWebSocketStatus(true);
    };

    window.wsClient.onclose = () => {
        console.log("WebSocket отключен!");
        if (typeof updateWebSocketStatus === 'function') updateWebSocketStatus(false);
    };

    window.wsClient.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            console.log("Данные с базы:", data);

            // Если пришли GPS данные от NEO-7M
            if (data.gps && data.gps.lat && data.gps.lon) {
                const newLat = parseFloat(data.gps.lat);
                const newLon = parseFloat(data.gps.lon);
                baseMarker.setLatLng([newLat, newLon]);
                map.panTo([newLat, newLon]);
                document.getElementById('anomaly-value').innerText = `SAT: ${data.gps.satellites}`;
            }

            // Обновление графика (если есть данные телеметрии)
            if (data.telemetry) {
                const now = new Date().toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
                
                weatherChart.data.labels.push(now);
                weatherChart.data.datasets[0].data.push(data.telemetry.wind);
                weatherChart.data.datasets[1].data.push(data.telemetry.ai_prob);

                // Храним только последние 10 точек на графике
                if (weatherChart.data.labels.length > 10) {
                    weatherChart.data.labels.shift();
                    weatherChart.data.datasets[0].data.shift();
                    weatherChart.data.datasets[1].data.shift();
                }
                weatherChart.update();
            }
        } catch (e) {
            console.error("Ошибка парсинга JSON:", e);
        }
    };
});
