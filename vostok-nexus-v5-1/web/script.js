document.addEventListener('DOMContentLoaded', () => {
    /* Navigation */
    const navItems = document.querySelectorAll('.nav-item');
    const pages = document.querySelectorAll('.page');

    navItems.forEach(item => {
        item.addEventListener('click', () => {
            const pageId = item.getAttribute('data-page');

            navItems.forEach(ni => ni.classList.remove('active'));
            item.classList.add('active');

            pages.forEach(p => p.classList.remove('active'));
            document.getElementById(pageId).classList.add('active');
        });
    });

    /* WebSocket Connection */
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
                handleData(data);
            } catch(e) { console.error("WS JSON Parse Error", e); }
        };
    }

    function handleData(data) {
        if (data.type === 'base_data') {
            if(document.getElementById('dash-temp')) document.getElementById('dash-temp').innerText = `${data.temp.toFixed(1)}°C`;
            if(document.getElementById('meteo-press')) document.getElementById('meteo-press').innerText = `${data.press.toFixed(1)}hPa`;
            if(document.getElementById('meteo-hum')) document.getElementById('meteo-hum').innerText = `${data.hum.toFixed(0)}%`;
        } else if (data.type === 'rod_data') {
            const rodId = data.id;
            const card = document.getElementById(`rod-${rodId}`);
            if (card) {
                const glow = card.querySelector('.rod-glow');
                const intensity = Math.min(100, Math.sqrt(data.ax**2 + data.ay**2 + data.az**2) * 5);
                glow.style.height = `${intensity}%`;

                if (data.bite > 50) {
                    card.classList.add('bite');
                    card.querySelector('.rod-status').innerText = "BITE!";
                } else {
                    card.classList.remove('bite');
                    card.querySelector('.rod-status').innerText = "IDLE";
                }
            }
        }
    }

    /* Gauge Drawing */
    const gaugeCanvas = document.getElementById('biteGauge');
    if (gaugeCanvas) {
        const ctx = gaugeCanvas.getContext('2d');
        gaugeCanvas.width = 280;
        gaugeCanvas.height = 280;

        function drawGauge(value) {
            ctx.clearRect(0, 0, 280, 280);

            // Outer Ring
            ctx.beginPath();
            ctx.arc(140, 140, 120, 0.8 * Math.PI, 0.2 * Math.PI);
            ctx.strokeStyle = 'rgba(255,255,255,0.1)';
            ctx.lineWidth = 15;
            ctx.lineCap = 'round';
            ctx.stroke();

            // Progress Ring
            ctx.beginPath();
            const endAngle = 0.8 * Math.PI + (value / 100) * 1.4 * Math.PI;
            ctx.arc(140, 140, 120, 0.8 * Math.PI, endAngle);
            const gradient = ctx.createLinearGradient(0, 0, 280, 0);
            gradient.addColorStop(0, '#7dd3fc');
            gradient.addColorStop(1, '#d4ff8f');
            ctx.strokeStyle = gradient;
            ctx.lineWidth = 15;
            ctx.lineCap = 'round';
            ctx.shadowBlur = 15;
            ctx.shadowColor = '#d4ff8f';
            ctx.stroke();
            ctx.shadowBlur = 0;
        }

        let gaugeVal = 0;
        setInterval(() => {
            gaugeVal = 60 + Math.random() * 30;
            drawGauge(gaugeVal);
            document.querySelector('.predictor-value').innerText = `${Math.round(gaugeVal)}%`;
        }, 3000);
    }

    /* AI Fish ID Logic */
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

    /* Charts */
    const profileCanvas = document.getElementById('verticalProfile');
    if (profileCanvas) {
        const ctx = profileCanvas.getContext('2d');
        new Chart(ctx, {
            type: 'line',
            data: {
                labels: ['0m', '500m', '1000m', '3000m', '5000m', '10000m'],
                datasets: [{
                    label: 'Temperature (°C)',
                    data: [25, 20, 15, 0, -20, -50],
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
                    y: { grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#94a3b8' } },
                    x: { grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#94a3b8' } }
                },
                plugins: { legend: { display: false } }
            }
        });
    }

    /* Simulation for UI Testing if not connected */
    setInterval(() => {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            handleData({
                type: 'base_data',
                temp: 22 + Math.random() * 5,
                press: 1010 + Math.random() * 10,
                hum: 40 + Math.random() * 20
            });

            handleData({
                type: 'rod_data',
                id: 1,
                ax: Math.random() * 5,
                ay: Math.random() * 5,
                az: 9.8,
                bite: Math.random() * 100
            });
            handleData({
                type: 'rod_data',
                id: 2,
                ax: Math.random() * 5,
                ay: Math.random() * 5,
                az: 9.8,
                bite: Math.random() * 100
            });
        }
    }, 2000);

    connect();
});
