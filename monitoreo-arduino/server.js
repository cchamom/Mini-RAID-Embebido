const express = require('express');
const http = require('http');
const fs = require('fs');
const path = require('path');
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

// Endpoints independientes para descargar el Excel de cada SD
app.get('/download-sda', (req, res) => {
    const filePath = path.join(__dirname, 'datalog_a.csv');
    if (fs.existsSync(filePath)) {
        res.download(filePath, 'datalog_SD_A.csv');
    } else {
        res.status(404).send('El archivo de la SD-A aún no está disponible.');
    }
});

app.get('/download-sdb', (req, res) => {
    const filePath = path.join(__dirname, 'datalog_b.csv');
    if (fs.existsSync(filePath)) {
        res.download(filePath, 'datalog_SD_B.csv');
    } else {
        res.status(404).send('El archivo de la SD-B aún no está disponible.');
    }
});

parser.on('data', (line) => {
    console.log(`[SERIAL] ${line}`);

    broadcast(JSON.stringify({
        type: 'raw_serial',
        timestamp: new Date().toLocaleTimeString(),
        line: line
    }));

    // Detectar cuando el Arduino termina una sincronización física para replicarla en los archivos locales de la PC
    if (line.includes("sincronizada con exito")) {
        const fileA = path.join(__dirname, 'datalog_a.csv');
        const fileB = path.join(__dirname, 'datalog_b.csv');

        if (line.includes("SD-A") && fs.existsSync(fileB)) {
            fs.copyFileSync(fileB, fileA);
            console.log("[SERVIDOR] Sincronización replicada: datalog_a.csv actualizado desde datalog_b.csv");
        } else if (line.includes("SD-B") && fs.existsSync(fileA)) {
            fs.copyFileSync(fileA, fileB);
            console.log("[SERVIDOR] Sincronización replicada: datalog_b.csv actualizado desde datalog_a.csv");
        }
    }

    if (line.startsWith('DATA:')) {
        const rawData = line.replace('DATA:', '');
        const [ms, seq, temp, hum, sdA, sdB, ram] = rawData.split(',');

        const csvLine = `${ms},${seq},${temp},${hum}\n`;

        // Solo escribir en el archivo local si el disco físico reporta estado '1' (activo)
        if (sdA === '1') {
            fs.appendFile(path.join(__dirname, 'datalog_a.csv'), csvLine, () => {});
        }
        if (sdB === '1') {
            fs.appendFile(path.join(__dirname, 'datalog_b.csv'), csvLine, () => {});
        }

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