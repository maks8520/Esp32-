document.addEventListener('DOMContentLoaded', () => {
    // --- Navigation System ---
    const navItems = document.querySelectorAll('.nav-item');
    const pages = document.querySelectorAll('.page');

    navItems.forEach(item => {
        item.addEventListener('click', () => {
            const pageId = item.getAttribute('data-page');

            navItems.forEach(ni => ni.classList.remove('active'));
            item.classList.add('active');

            pages.forEach(p => p.classList.remove('active'));
            document.getElementById(pageId).classList.add('active');

            // Re-trigger layout for maps/charts if needed
            if(pageId === 'meteo') {
                setTimeout(() => map.invalidateSize(), 200);
            }
        });
    });

    // --- WebSocket & Telemetry ---
    let ws;
    const statusDot = document.querySelector('.status-dot');
    const statusText = document.querySelector('.status-text');

    function connect() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const host = window.location.host || '192.168.4.1';
        ws = new WebSocket(`${protocol}//${host}/ws`);

        ws.onopen = () => {
            statusDot.className = 'status-dot connected';
            statusText.innerText = 'CONNECTED';
        };

        ws.onclose = () => {
            statusDot.className = 'status-dot disconnected';
            statusText.innerText = 'DISCONNECTED';
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
        if (data.type === 'base_data') {
            updateDashboardMeteo(data);
        } else if (data.type === 'rod_data') {
            updateRodUI(data);
        }
    }

    function updateDashboardMeteo(data) {
        if(document.getElementById('dash-temp')) document.getElementById('dash-temp').innerText = data.temp.toFixed(1);
        if(document.getElementById('dash-wind')) document.getElementById('dash-wind').innerText = data.wind ? data.wind.toFixed(1) : '--';

        // Update main meteo tab too
        const tempBig = document.querySelector('.temp-big');
        if(tempBig) tempBig.innerText = `${data.temp.toFixed(1)}°C`;
    }

    function updateRodUI(data) {
        const rodId = data.id;
        const card = document.getElementById(`rod-${rodId}`);
        if (!card) return;

        const glow = document.getElementById(`rod-${rodId}-glow`);
        const status = document.getElementById(`rod-${rodId}-status`);

        // Calculate intensity based on acceleration vector magnitude
        const accelMag = Math.sqrt(data.ax**2 + data.ay**2 + data.az**2);
        const relativeIntensity = Math.min(100, Math.max(0, (accelMag - 9.8) * 20));

        glow.style.height = `${relativeIntensity}%`;

        if (data.bite > 60) {
            card.classList.add('bite');
            status.innerText = "STRIKE!";
        } else {
            card.classList.remove('bite');
            status.innerText = "READY";
        }
    }

    // --- SVG Gauge Controllers ---
    const predictorGauge = document.getElementById('predictorGauge');
    const confidenceGauge = document.getElementById('confidenceGauge');
    const predictorVal = document.getElementById('predictorVal');
    const confVal = document.getElementById('confVal');

    function setGauge(el, percent, circumference) {
        const offset = circumference - (percent / 100) * circumference;
        el.style.strokeDashoffset = offset;
    }

    function updateAIGauges(activity, confidence) {
        setGauge(predictorGauge, activity, 534);
        setGauge(confidenceGauge, confidence, 440);
        predictorVal.innerText = `${Math.round(activity)}%`;
        confVal.innerText = `${Math.round(confidence)}%`;
    }

    // --- Windy Map Implementation ---
    let map;
    function initMap() {
        // Initializing Leaflet map with dark theme
        map = L.map('windyMap', {
            zoomControl: false,
            attributionControl: false
        }).setView([45.039, 38.975], 10); // Default to a fishing spot

        L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
            maxZoom: 19
        }).addTo(map);

        // In a real production environment, we'd use the Windy Leaflet plugin here
        // For this UI, we demonstrate the Windy-style layer switching
        document.querySelectorAll('.layer-btn').forEach(btn => {
            btn.addEventListener('click', () => {
                document.querySelectorAll('.layer-btn').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                // Layer logic would go here
            });
        });
    }

    // --- Vertical Profile Chart ---
    let profileChart;
    function initProfile() {
        const ctx = document.getElementById('verticalProfileChart').getContext('2d');
        const gradient = ctx.createLinearGradient(0, 0, 0, 400);
        gradient.addColorStop(0, 'rgba(125, 211, 252, 0.3)');
        gradient.addColorStop(1, 'rgba(2, 6, 23, 0)');

        profileChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: ['0m', '500m', '1000m', '3000m', '5000m', '8000m', '10000m'],
                datasets: [
                    {
                        label: 'Temperature',
                        data: [24, 21, 18, 5, -12, -35, -55],
                        borderColor: '#7dd3fc',
                        backgroundColor: gradient,
                        fill: true,
                        tension: 0.4,
                        yAxisID: 'y'
                    },
                    {
                        label: 'Wind Speed',
                        data: [4, 6, 12, 25, 45, 80, 110],
                        borderColor: '#d4ff8f',
                        borderDash: [5, 5],
                        fill: false,
                        tension: 0.4,
                        yAxisID: 'y1'
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                interaction: { mode: 'index', intersect: false },
                scales: {
                    y: {
                        type: 'linear', display: true, position: 'left',
                        grid: { color: 'rgba(255,255,255,0.05)' },
                        ticks: { color: '#94a3b8', font: { family: 'JetBrains Mono' } }
                    },
                    y1: {
                        type: 'linear', display: true, position: 'right',
                        grid: { drawOnChartArea: false },
                        ticks: { color: '#d4ff8f', font: { family: 'JetBrains Mono' } }
                    },
                    x: {
                        grid: { color: 'rgba(255,255,255,0.05)' },
                        ticks: { color: '#94a3b8', font: { family: 'JetBrains Mono' } }
                    }
                },
                plugins: { legend: { display: false } }
            }
        });

        // Fill table
        const tableBody = document.getElementById('profileData');
        const heights = ['0m', '500m', '1000m', '3000m', '5000m', '8000m', '10000m'];
        const temps = [24, 21, 18, 5, -12, -35, -55];
        const winds = [4, 6, 12, 25, 45, 80, 110];

        heights.forEach((h, i) => {
            const row = `<tr><td>${h}</td><td>${temps[i]}°C</td><td>${winds[i]}m/s</td><td>${Math.max(0, 100-i*15)}%</td></tr>`;
            tableBody.innerHTML += row;
        });
    }

    // --- Bite Timeline ---
    function initBiteTimeline() {
        const ctx = document.getElementById('biteTimeline').getContext('2d');
        const biteChart = new Chart(ctx, {
            type: 'scatter',
            data: {
                datasets: [{
                    label: 'Strikes',
                    data: [],
                    backgroundColor: '#d4ff8f',
                    pointRadius: 6,
                    pointHoverRadius: 10,
                    showLine: true,
                    borderColor: 'rgba(212, 255, 143, 0.2)'
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: { type: 'linear', position: 'bottom', display: false },
                    y: { min: 0, max: 100, display: false }
                },
                plugins: { legend: { display: false } }
            }
        });

        // Simulating live timeline movement
        let time = 0;
        setInterval(() => {
            time++;
            if(Math.random() > 0.8) {
                biteChart.data.datasets[0].data.push({x: time, y: 30 + Math.random() * 50});
                if(biteChart.data.datasets[0].data.length > 20) biteChart.data.datasets[0].data.shift();
                biteChart.update('none');
            }
        }, 1000);
    }

    // --- AI Modal Controller ---
    const aiBtn = document.querySelector('.ai-fish-id-btn');
    const modal = document.getElementById('fishModal');
    const closeBtn = document.querySelector('.close-modal');
    const fishResult = document.querySelector('.fish-result');

    if(aiBtn) {
        aiBtn.addEventListener('click', () => {
            modal.classList.add('active');
            fishResult.classList.add('hidden');
            setTimeout(() => {
                fishResult.classList.remove('hidden');
            }, 3000);
        });
    }

    if(closeBtn) {
        closeBtn.addEventListener('click', () => {
            modal.classList.remove('active');
        });
    }

    // --- Initialization & Simulation ---
    initMap();
    initProfile();
    initBiteTimeline();
    connect();

    // High-fidelity simulation for offline testing
    setInterval(() => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            const mockBase = {
                type: 'base_data',
                temp: 24.2 + (Math.random() - 0.5),
                wind: 4.5 + (Math.random() * 2),
                press: 1013.2 + (Math.random() * 5)
            };
            handleTelemetry(mockBase);

            handleTelemetry({
                type: 'rod_data', id: 1,
                ax: Math.random() * 2, ay: Math.random() * 2, az: 9.8 + (Math.random() * 5),
                bite: Math.random() * 100
            });
            handleTelemetry({
                type: 'rod_data', id: 2,
                ax: Math.random() * 2, ay: Math.random() * 2, az: 9.8 + (Math.random() * 5),
                bite: Math.random() * 100
            });

            updateAIGauges(75 + Math.random() * 15, 92 + Math.random() * 5);
        }
    }, 2000);
});
