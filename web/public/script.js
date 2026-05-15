document.addEventListener('DOMContentLoaded', () => {
    const navItems = document.querySelectorAll('.nav-item');
    const tabs = document.querySelectorAll('.tab-content');

    // Navigation Logic
    navItems.forEach(item => {
        item.addEventListener('click', () => {
            const target = item.dataset.tab;

            navItems.forEach(i => i.classList.remove('active'));
            item.classList.add('active');

            tabs.forEach(tab => {
                tab.classList.remove('active');
                if (tab.id === `tab-${target}`) tab.classList.add('active');
            });

            if (target === 'meteo') {
                setTimeout(initMap, 100);
            }
        });
    });

    // Gauge Initialization
    const initGauge = () => {
        const ctx = document.getElementById('predictorGauge').getContext('2d');
        const gradient = ctx.createLinearGradient(0, 0, 200, 0);
        gradient.addColorStop(0, '#7dd3fc');
        gradient.addColorStop(1, '#d4ff8f');

        window.gaugeChart = new Chart(ctx, {
            type: 'doughnut',
            data: {
                datasets: [{
                    data: [88, 12],
                    backgroundColor: [gradient, 'rgba(255,255,255,0.05)'],
                    borderWidth: 0,
                    circumference: 180,
                    rotation: 270,
                }]
            },
            options: {
                cutout: '85%',
                plugins: { legend: { display: false }, tooltip: { enabled: false } },
                animation: { animateRotate: true }
            }
        });
    };

    // Leaflet Map Initialization
    let map;
    const initMap = () => {
        if (map) return;
        map = L.map('map', { zoomControl: false }).setView([45.0, 39.0], 10);
        L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
            attribution: '&copy; OpenStreetMap contributors'
        }).addTo(map);

        // Simulated wind layer
        const circle = L.circle([45.0, 39.0], {
            color: '#d4ff8f',
            fillColor: '#d4ff8f',
            fillOpacity: 0.1,
            radius: 5000
        }).addTo(map);
    };

    // WebSocket / Simulation
    const simulateData = () => {
        setInterval(() => {
            const rod1 = document.getElementById('rod-1');
            const random = Math.random();
            if (random > 0.95) {
                rod1.classList.add('bite');
                if (window.navigator.vibrate) window.navigator.vibrate([100, 50, 100]);
                setTimeout(() => rod1.classList.remove('bite'), 2000);
            }

            const predictPct = document.getElementById('predict-pct');
            const newVal = 80 + Math.floor(Math.random() * 15);
            predictPct.innerText = newVal;
            if (window.gaugeChart) {
                window.gaugeChart.data.datasets[0].data = [newVal, 100 - newVal];
                window.gaugeChart.update('none');
            }
        }, 3000);
    };

    initGauge();
    simulateData();

    console.log("VOSTOK NEXUS v5.1 Engine Loaded");
});

// Profile Chart
const initProfile = () => {
    const ctx = document.getElementById('meteoProfileChart').getContext('2d');
    const table = document.getElementById('profile-rows');

    const labels = ['0', '100', '500', '1000', '3000', '5000', '10000'];
    const temp = [22, 20, 15, 10, -5, -20, -50];
    const wind = [5, 8, 12, 15, 25, 40, 60];

    new Chart(ctx, {
        type: 'line',
        data: {
            labels: labels,
            datasets: [
                { label: 'Temp', data: temp, borderColor: '#7dd3fc', tension: 0.4 },
                { label: 'Wind', data: wind, borderColor: '#d4ff8f', tension: 0.4 }
            ]
        },
        options: {
            indexAxis: 'y',
            plugins: { legend: { display: false } },
            scales: { x: { display: false }, y: { grid: { color: 'rgba(255,255,255,0.05)' } } }
        }
    });

    labels.forEach((l, i) => {
        const row = document.createElement('div');
        row.className = 'row';
        row.innerHTML = `<span>${l}m</span><span>${temp[i]}°</span><span>${wind[i]}m/s</span>`;
        table.appendChild(row);
    });
};

// Fishing logic
const openFishID = () => {
    document.getElementById('fish-id-modal').classList.add('active');
    setTimeout(() => {
        document.getElementById('scan-status').innerText = 'FISH IDENTIFIED: NORTHERN PIKE (3.2kg)';
    }, 3000);
};

const closeFishID = () => {
    document.getElementById('fish-id-modal').classList.remove('active');
};

// Timeline
const addBite = () => {
    const tl = document.getElementById('bite-timeline');
    const point = document.createElement('div');
    point.className = 'bite-point';
    tl.prepend(point);
};

// Call inits on tab change or load
document.querySelector('[data-tab="profile"]').addEventListener('click', initProfile);
setInterval(addBite, 10000);
