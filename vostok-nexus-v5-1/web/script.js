document.addEventListener('DOMContentLoaded', () => {
    const state = {
        ws: null,
        currentPage: 'dashboard',
        predictor: 0,
        lastUpdate: 0
    };

    // Navigation
    const navItems = document.querySelectorAll('.nav-item');
    const pages = document.querySelectorAll('.page');

    navItems.forEach(item => {
        item.addEventListener('click', () => {
            const pageId = item.dataset.page;
            navItems.forEach(n => n.classList.remove('active'));
            item.classList.add('active');
            pages.forEach(p => p.classList.remove('active'));
            document.getElementById(pageId).classList.add('active');

            if (pageId === 'meteo') initMap();
            if (pageId === 'profile') initProfile();
        });
    });

    // WebSocket with Throttling (Phase 2)
    function connectWS() {
        const host = window.location.host || '192.168.4.1';
        state.ws = new WebSocket(`ws://${host}/ws`);

        state.ws.onopen = () => {
            document.querySelector('.status-dot').className = 'status-dot connected';
            document.getElementById('ws-status').innerText = 'ПОДКЛЮЧЕНО';
            document.getElementById('ws-status').style.color = '#d4ff8f';
        };

        state.ws.onclose = () => {
            document.querySelector('.status-dot').className = 'status-dot disconnected';
            document.getElementById('ws-status').innerText = 'ПЕРЕПОДКЛЮЧЕНИЕ...';
            document.getElementById('ws-status').style.color = '#ef4444';
            setTimeout(connectWS, 3000);
        };

        state.ws.onmessage = (e) => {
            const now = Date.now();
            if (now - state.lastUpdate < 20) return; // 50Hz cap for safety
            state.lastUpdate = now;

            requestAnimationFrame(() => {
                try {
                    const data = JSON.parse(e.data);
                    handleMessage(data);
                } catch (err) {}
            });
        };
    }

    function handleMessage(data) {
        if (data.type === 'rod_data') {
            updateRod(data);
        }
    }

    function updateRod(data) {
        const { id, bite, accel } = data;
        const card = document.getElementById(`rod-${id}`);
        const accelEl = document.getElementById(`rod-${id}-accel`);
        const statusEl = document.getElementById(`rod-${id}-status`);

        if (accelEl) accelEl.innerText = accel.toFixed(1);

        if (bite > 60) {
            card.classList.add('bite');
            statusEl.innerText = 'ПОКЛЕВКА!';
            statusEl.style.color = '#d4ff8f';
            // Phase 3: Haptic Strike
            if (window.navigator.vibrate) window.navigator.vibrate([150, 50, 150]);
        } else {
            card.classList.remove('bite');
            statusEl.innerText = 'ОЖИДАНИЕ';
            statusEl.style.color = 'inherit';
        }

        updatePredictor(bite);
    }

    function updatePredictor(val) {
        state.predictor = Math.min(100, Math.max(0, val));
        document.getElementById('predictorVal').innerText = `${state.predictor}%`;
        const offset = 534 - (state.predictor / 100 * 534);
        document.getElementById('predictorGauge').style.strokeDashoffset = offset;
    }

    // ECharts Vertical Profile (Phase 3)
    let profileChart = null;
    function initProfile() {
        if (profileChart) return;
        const chartDom = document.getElementById('profileChart');
        profileChart = echarts.init(chartDom, 'dark');
        const option = {
            backgroundColor: 'transparent',
            title: { text: 'Вертикальный Профиль', textStyle: { color: '#d4ff8f', fontSize: 14 } },
            tooltip: { trigger: 'axis' },
            legend: { data: ['Темп.', 'Ветер'], top: 30 },
            grid: { left: '3%', right: '4%', bottom: '3%', containLabel: true },
            xAxis: { type: 'value', splitLine: { lineStyle: { color: 'rgba(255,255,255,0.05)' } } },
            yAxis: { type: 'category', data: ['0м', '500м', '1км', '3км', '5км', '10км'], axisLabel: { color: '#7dd3fc' } },
            series: [
                { name: 'Темп.', type: 'line', data: [24, 18, 12, 0, -15, -45], color: '#7dd3fc', smooth: true },
                { name: 'Ветер', type: 'line', data: [3, 8, 15, 25, 45, 80], color: '#d4ff8f', smooth: true }
            ]
        };
        profileChart.setOption(option);
    }

    let map = null;
    function initMap() {
        if (map) return;
        map = L.map('windyMap', { zoomControl: false }).setView([45.0, 39.0], 10);
        L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png').addTo(map);
    }

    connectWS();
});
