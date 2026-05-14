document.addEventListener('DOMContentLoaded', () => {
    // --- State ---
    let ws;
    let windyMap;
    let mapLoaded = false;
    let gaugeValue = 0;
    const elements = {
        predictorGauge: document.getElementById('predictorGauge'),
        predictorVal: document.getElementById('predictorVal'),
        navItems: document.querySelectorAll('.nav-item'),
        pages: document.querySelectorAll('.page'),
        statusDot: document.querySelector('.status-dot')
    };

    // --- Performance: requestAnimationFrame for Smooth animations ---
    function animate() {
        // AI Gauge smooth pulse/update
        const targetOffset = 534 - (gaugeValue / 100) * 534;
        const currentOffset = parseFloat(elements.predictorGauge.style.strokeDashoffset) || 534;
        const easeOffset = currentOffset + (targetOffset - currentOffset) * 0.1;
        elements.predictorGauge.style.strokeDashoffset = easeOffset;
        elements.predictorVal.innerText = `${Math.round(gaugeValue)}%`;

        // Water ripple movement
        const ripple = document.querySelector('.water-ripple');
        if(ripple) {
            const time = Date.now() * 0.001;
            const x = Math.sin(time) * 2;
            const y = Math.cos(time) * 2;
            ripple.style.transform = `translate(${x}%, ${y}%) scale(1.1)`;
        }

        requestAnimationFrame(animate);
    }
    requestAnimationFrame(animate);

    // --- Navigation & Lazy Loading ---
    elements.navItems.forEach(item => {
        item.addEventListener('click', () => {
            const pageId = item.getAttribute('data-page');
            elements.navItems.forEach(ni => ni.classList.remove('active'));
            item.classList.add('active');
            elements.pages.forEach(p => p.classList.remove('active'));
            document.getElementById(pageId).classList.add('active');

            if(pageId === 'meteo' && !mapLoaded) {
                lazyLoadWindy();
            }
            vibrate(20);
        });
    });

    function lazyLoadWindy() {
        const link = document.createElement('link');
        link.rel = 'stylesheet';
        link.href = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.css';
        document.head.appendChild(link);

        const script = document.createElement('script');
        script.src = 'https://unpkg.com/leaflet@1.9.4/dist/leaflet.js';
        script.onload = () => {
            setTimeout(initMap, 500);
        };
        document.head.appendChild(script);
        mapLoaded = true;
    }

    function initMap() {
        const mapContainer = document.getElementById('windyMap');
        mapContainer.innerHTML = ''; // Remove skeleton

        windyMap = L.map('windyMap', { zoomControl: false, attributionControl: false }).setView([45.03, 38.97], 10);
        L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', { maxZoom: 19 }).addTo(windyMap);

        document.querySelectorAll('.layer-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                document.querySelectorAll('.layer-btn').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                vibrate(30);
            });
        });
    }

    // --- Haptics ---
    function vibrate(pattern) {
        if ("vibrate" in navigator) navigator.vibrate(pattern);
    }

    // --- Telemetry & WebSocket ---
    function connect() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const host = window.location.host || '192.168.4.1';
        ws = new WebSocket(`${protocol}//${host}/ws`);

        ws.onopen = () => {
            elements.statusDot.className = 'status-dot connected';
        };

        ws.onclose = () => {
            elements.statusDot.className = 'status-dot disconnected';
            setTimeout(connect, 3000);
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if(data.type === 'base_data') {
                    if(data.temp) document.getElementById('dash-temp').innerText = data.temp.toFixed(1);
                } else if(data.type === 'rod_data') {
                    const card = document.getElementById(`rod-${data.id}`);
                    if(card) {
                        const glow = card.querySelector('.rod-glow');
                        glow.style.height = `${data.bite}%`;
                        if(data.bite > 70) {
                            card.classList.add('bite');
                            vibrate([100, 50, 100]);
                        } else {
                            card.classList.remove('bite');
                        }
                    }
                }
            } catch(e) { console.error("WS Parse Error", e); }
        };
    }

    // --- Profile Chart ---
    function initProfileChart() {
        const ctx = document.getElementById('profileChart').getContext('2d');
        new Chart(ctx, {
            type: 'line',
            data: {
                labels: ['0m', '500m', '1000m', '3000m', '5000m', '10000m'],
                datasets: [{
                    label: 'Temp (°C)',
                    data: [24, 21, 18, 2, -15, -50],
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
                },
                plugins: { legend: { display: false } }
            }
        });
    }

    // Offline Simulation
    setInterval(() => {
        if(!ws || ws.readyState !== WebSocket.OPEN) {
            gaugeValue = 60 + Math.random() * 20;
            if(Math.random() > 0.8) {
                const id = Math.random() > 0.5 ? 1 : 2;
                const bite = 80 + Math.random() * 20;
                const card = document.getElementById(`rod-${id}`);
                if(card) {
                    card.classList.add('bite');
                    card.querySelector('.rod-glow').style.height = `${bite}%`;
                    setTimeout(() => card.classList.remove('bite'), 1500);
                }
            }
        }
    }, 2000);

    initProfileChart();
    connect();
});
