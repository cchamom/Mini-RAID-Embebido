const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
const ws = new WebSocket(`${protocol}//${window.location.host}`);

const serialTerminal = document.getElementById('serialTerminal');
const clearSerialBtn = document.getElementById('clearSerial');
const raidStatusEl = document.getElementById('raidStatus');
const raidBanner = document.getElementById('raidBanner');

const tempEl = document.getElementById('temp');
const humEl = document.getElementById('hum');
const sdAEl = document.getElementById('sdA');
const sdBEl = document.getElementById('sdB');
const ramEl = document.getElementById('ram');

const tableA = document.getElementById('tableA');
const tableB = document.getElementById('tableB');

let firstA = true;
let firstB = true;

const ctx = document.getElementById('realtimeChart').getContext('2d');
const realtimeChart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [
            {
                label: 'Temperatura (°C)',
                borderColor: '#f85149',
                backgroundColor: 'rgba(248, 81, 73, 0.05)',
                data: [],
                borderWidth: 2,
                tension: 0.2,
                fill: true,
                yAxisID: 'y'
            },
            {
                label: 'Humedad (%)',
                borderColor: '#38bdf8',
                backgroundColor: 'rgba(56, 189, 248, 0.05)',
                data: [],
                borderWidth: 2,
                tension: 0.2,
                fill: true,
                yAxisID: 'y1'
            }
        ]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: { labels: { color: '#c9d1d9', font: { family: 'monospace', size: 11 } } }
        },
        scales: {
            x: { ticks: { color: '#8b949e', font: { family: 'monospace', size: 10 } }, grid: { color: '#30363d' } },
            y: { type: 'linear', position: 'left', ticks: { color: '#f85149', font: { family: 'monospace', size: 10 } }, grid: { color: '#30363d' } },
            y1: { type: 'linear', position: 'right', ticks: { color: '#38bdf8', font: { family: 'monospace', size: 10 } }, grid: { drawOnChartArea: false } }
        }
    }
});

ws.onopen = () => appendTerminal('[SISTEMA] Conectado al servidor WebSocket.', 'sys-msg');

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);

    if (data.type === 'raw_serial') {
        let cssClass = '';
        if (data.line.includes('ERROR') || data.line.includes('FAIL')) cssClass = 'err-msg';
        else if (data.line.includes('WARN')) cssClass = 'warn-msg';
        else if (data.line.includes('INIT') || data.line.includes('OK')) cssClass = 'sys-msg';
        appendTerminal(`[${data.timestamp}] ${data.line}`, cssClass);
    } 
    else if (data.type === 'telemetry') {
        updateDashboard(data);
    }
};

function updateDashboard(data) {
    tempEl.textContent = `${data.temp.toFixed(1)} °C`;
    humEl.textContent = `${data.hum.toFixed(1)} %`;
    ramEl.textContent = `${data.ram} B`;

    updateBadge(sdAEl, data.sdA, "UNIDAD ACTIVA (SD-A)", "DESCONECTADA");
    updateBadge(sdBEl, data.sdB, "ESPEJO OK (SD-B)", "DESCONECTADA");

    if (data.sdA && data.sdB) {
        raidStatusEl.textContent = "ÓPTIMO (ESPEJO SINCRONIZADO)";
        raidBanner.style.borderLeftColor = "var(--ok)";
    } else if (data.sdA || data.sdB) {
        raidStatusEl.textContent = "MODO DEGRADADO (UNA UNIDAD ACTIVA)";
        raidBanner.style.borderLeftColor = "var(--accent)";
    } else {
        raidStatusEl.textContent = "CRÍTICO (SIN ALMACENAMIENTO)";
        raidBanner.style.borderLeftColor = "var(--fail)";
    }

    if (realtimeChart.data.labels.length > 20) {
        realtimeChart.data.labels.shift();
        realtimeChart.data.datasets[0].data.shift();
        realtimeChart.data.datasets[1].data.shift();
    }
    realtimeChart.data.labels.push(data.timestamp);
    realtimeChart.data.datasets[0].data.push(data.temp);
    realtimeChart.data.datasets[1].data.push(data.hum);
    realtimeChart.update('none');

    // Llenar tabla SD-A solo si está activa (true)
    if (data.sdA && tableA) {
        if (firstA) { tableA.innerHTML = ''; firstA = false; }
        const rowA = document.createElement('tr');
        rowA.innerHTML = `<td>${data.ms}</td><td>${data.seq}</td><td>${data.temp.toFixed(1)}</td><td>${data.hum.toFixed(1)}</td>`;
        tableA.prepend(rowA);
    }

    // Llenar tabla SD-B solo si está activa (true)
    if (data.sdB && tableB) {
        if (firstB) { tableB.innerHTML = ''; firstB = false; }
        const rowB = document.createElement('tr');
        rowB.innerHTML = `<td>${data.ms}</td><td>${data.seq}</td><td>${data.temp.toFixed(1)}</td><td>${data.hum.toFixed(1)}</td>`;
        tableB.prepend(rowB);
    }
}

function updateBadge(element, isConnected, okText, failText) {
    if (!element) return;
    if (isConnected) {
        element.textContent = okText;
        element.className = "status-badge status-ok";
    } else {
        element.textContent = failText;
        element.className = "status-badge status-fail";
    }
}

function appendTerminal(text, className = '') {
    const p = document.createElement('p');
    if (className) p.className = className;
    p.textContent = text;
    serialTerminal.appendChild(p);
    serialTerminal.scrollTop = serialTerminal.scrollHeight;
}

clearSerialBtn.addEventListener('click', () => {
    serialTerminal.innerHTML = '<p class="sys-msg">[SISTEMA] Terminal limpiada.</p>';
});