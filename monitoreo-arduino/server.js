const express = require('express');
const http = require('http');
const { WebSocketServer, WebSocket } = require('ws');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

const app = express();
const server = http.createServer(app);
const wss = new WebSocketServer({ server });

app.use(express.static('public'));

const PUERTO_ARDUINO = 'COM4'; // Ajustar según tu puerto asignado en Windows

const port = new SerialPort({
    path: PUERTO_ARDUINO,
    baudRate: 115200,
    autoOpen: false
});

const parser = port.pipe(new ReadlineParser({ delimiter: '\r\n' }));

port.open((err) => {
    if (err) {
        return console.error(`[ERROR] No se pudo abrir el puerto ${PUERTO_ARDUINO}: `, err.message);
    }
    console.log(`[CONECTADO] Servidor escuchando Arduino en ${PUERTO_ARDUINO}`);
});

parser.on('data', (line) => {
    console.log(`[SERIAL] ${line}`);

    // Transmitir todo al Monitor Serial de la Web
    broadcast(JSON.stringify({
        type: 'raw_serial',
        timestamp: new Date().toLocaleTimeString(),
        line: line
    }));

    // Procesar tramas de datos estructuradas
    if (line.startsWith('DATA:')) {
        const rawData = line.replace('DATA:', '');
        const [ms, seq, temp, hum, sdA, sdB, ram] = rawData.split(',');

        const payload = JSON.stringify({
            type: 'telemetry',
            timestamp: new Date().toLocaleTimeString(),
            ms,
            seq,
            temp: parseFloat(temp),
            hum: parseFloat(hum),
            sdA: sdA === '1',
            sdB: sdB === '1',
            ram
        });

        broadcast(payload);
    }
});

function broadcast(data) {
    wss.clients.forEach((client) => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(data);
        }
    });
}

const PORT = 3001;
server.listen(PORT, () => {
    console.log(`Servidor de Monitoreo RAID 1 activo en http://localhost:${PORT}`);
});