const ctx = document.getElementById('realtimeChart').getContext('2d');

// Configuración de la gráfica de líneas
const chart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [
            { label: 'Temperatura (°C)', data: [], borderColor: '#f87171', backgroundColor: 'rgba(248, 113, 113, 0.1)', fill: true, tension: 0.3 },
            { label: 'Humedad (%)', data: [], borderColor: '#38bdf8', backgroundColor: 'rgba(56, 189, 248, 0.1)', fill: true, tension: 0.3 }
        ]
    },
    options: {
        responsive: true,
        scales: {
            x: { ticks: { color: '#94a3b8' }, grid: { color: '#334155' } },
            y: { ticks: { color: '#94a3b8' }, grid: { color: '#334155' } }
        },
        plugins: { legend: { labels: { color: '#f8fafc' } } }
    }
});

const ws = new WebSocket(`ws://${location.host}`);
const terminal = document.getElementById('serialTerminal');
const excelTableBody = document.getElementById('excelTableBody');
const clearSerialBtn = document.getElementById('clearSerial');

let isFirstRow = true;

clearSerialBtn.addEventListener('click', () => {
    terminal.innerHTML = '';
});

function addSerialLog(text) {
    const p = document.createElement('p');
    if (text.includes('[ALARMA]') || text.includes('fallo')) p.className = 'err-msg';
    else if (text.includes('[REBUILD]') || text.includes('sincronizacion')) p.className = 'warn-msg';
    else if (text.includes('[SISTEMA]') || text.includes('DATA:')) p.className = 'sys-msg';
    
    p.innerText = text;
    terminal.appendChild(p);
    terminal.scrollTop = terminal.scrollHeight;
}

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);

    // 1. Mostrar todo en el Monitor Serial en Vivo
    if (data.type === 'raw_serial') {
        addSerialLog(`[${data.timestamp}] ${data.line}`);
    }

    // 2. Procesar Telemetría y Tabla Excel
    if (data.type === 'telemetry') {
        // Actualizar métricas principales
        document.getElementById('temp').innerText = `${data.temp.toFixed(1)} °C`;
        document.getElementById('hum').innerText = `${data.hum.toFixed(0)} %`;
        document.getElementById('ram').innerText = `${data.ram} B`;

        // Estado de cada SD
        const elSdA = document.getElementById('sdA');
        const elSdB = document.getElementById('sdB');

        elSdA.innerText = data.sdA ? 'OPERATIVA (ESPEJO)' : 'FALLA / EXTRAÍDA';
        elSdA.className = `status-badge ${data.sdA ? 'status-ok' : 'status-fail'}`;

        elSdB.innerText = data.sdB ? 'OPERATIVA (ESPEJO)' : 'FALLA / EXTRAÍDA';
        elSdB.className = `status-badge ${data.sdB ? 'status-ok' : 'status-fail'}`;

        // Banner Global RAID 1
        const raidBanner = document.getElementById('raidBanner');
        const raidStatus = document.getElementById('raidStatus');

        if (data.sdA && data.sdB) {
            raidStatus.innerText = 'SALUDABLE (RAID 1 OK)';
            raidStatus.style.color = '#34d399';
            raidBanner.style.borderColor = '#059669';
        } else if (data.sdA || data.sdB) {
            raidStatus.innerText = 'MODO DEGRADADO (TOLERANTE A FALLOS)';
            raidStatus.style.color = '#fbbf24';
            raidBanner.style.borderColor = '#d97706';
        } else {
            raidStatus.innerText = 'CRÍTICO - SIN ALMACENAMIENTO';
            raidStatus.style.color = '#f87171';
            raidBanner.style.borderColor = '#dc2626';
        }

        // Gráfica en tiempo real
        if (chart.data.labels.length >= 15) {
            chart.data.labels.shift();
            chart.data.datasets[0].data.shift();
            chart.data.datasets[1].data.shift();
        }

        chart.data.labels.push(data.timestamp);
        chart.data.datasets[0].data.push(data.temp);
        chart.data.datasets[1].data.push(data.hum);
        chart.update();

        // Agregar fila a la tabla virtual del Excel (datalog.csv)
        if (isFirstRow) {
            excelTableBody.innerHTML = '';
            isFirstRow = false;
        }

        const tr = document.createElement('tr');
        const raidMirrorTag = (data.sdA && data.sdB) 
            ? '<span class="tag-ok">✔ Guardado en SD-A y SD-B</span>' 
            : '<span class="tag-deg">⚠ Guardado solo en tarjeta activa</span>';

        tr.innerHTML = `
            <td>${data.ms}</td>
            <td><strong>#${data.seq}</strong></td>
            <td>${data.temp.toFixed(1)} °C</td>
            <td>${data.hum.toFixed(0)} %</td>
            <td>${raidMirrorTag}</td>
        `;

        // Insertar siempre al principio para ver el dato más reciente arriba
        excelTableBody.insertBefore(tr, excelTableBody.firstChild);

        // Limitar a máximo 20 filas en pantalla
        if (excelTableBody.children.length > 20) {
            excelTableBody.removeChild(excelTableBody.lastChild);
        }
    }
};