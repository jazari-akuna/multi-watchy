// Watchy BLE bring-up tool.
// Minimal Web Bluetooth driver for pushing test packets to the watch's
// GATT service. Not for production use.

const SERVICE_UUID = '4e2d0001-6f00-4d1a-9c7b-8f7c2e0a1d3b';
const CHAR_WRITE_UUID = '4e2d0002-6f00-4d1a-9c7b-8f7c2e0a1d3b';
const CHAR_STATE_UUID = '4e2d0003-6f00-4d1a-9c7b-8f7c2e0a1d3b';

const PACKET_SIZE = 64;
const STATE_SIZE = 16;
const TITLE_MAX = 46;

const TAG_TIME_SYNC = 0x01;
const TAG_EVENT     = 0x02;
const TAG_BATCH_END = 0x03;
const TAG_CLEAR     = 0x04;

// Module-local state.
const state = {
    device: null,
    server: null,
    service: null,
    writeChar: null,
    stateChar: null,
};

// --- UI helpers -----------------------------------------------------------

const logEl = document.getElementById('log');
const btnConnect   = document.getElementById('btnConnect');
const btnClear     = document.getElementById('btnClear');
const btnBatch     = document.getElementById('btnBatch');
const btnReadState = document.getElementById('btnReadState');

function log(msg) {
    const ts = new Date().toISOString().slice(11, 23); // HH:MM:SS.sss
    logEl.textContent += `[${ts}] ${msg}\n`;
    logEl.scrollTop = logEl.scrollHeight;
}

function setConnectedUI(connected) {
    btnClear.disabled     = !connected;
    btnBatch.disabled     = !connected;
    btnReadState.disabled = !connected;
    btnConnect.textContent = connected ? 'Reconnect watch' : 'Connect watch';
}

// --- Packet builders ------------------------------------------------------

function makePacket(tag, fillFn) {
    const pkt = new Uint8Array(PACKET_SIZE);
    pkt[0] = tag;
    if (fillFn) fillFn(pkt);
    return pkt;
}

function dv(pkt) {
    return new DataView(pkt.buffer, pkt.byteOffset, pkt.byteLength);
}

function writeInt64LE(pkt, off, val) {
    const v = typeof val === 'bigint' ? val : BigInt(val);
    dv(pkt).setBigInt64(off, v, true);
}

function writeInt32LE(pkt, off, val) {
    dv(pkt).setInt32(off, val | 0, true);
}

function writeTitle(pkt, off, s) {
    const encoded = new TextEncoder().encode(s);
    const bytes = encoded.slice(0, TITLE_MAX);
    pkt.set(bytes, off);
    // Remainder of the 46-byte title field is already zero (null-padded).
}

function buildTimeSync(utcSeconds, gmtOffsetSec) {
    return makePacket(TAG_TIME_SYNC, (pkt) => {
        writeInt64LE(pkt, 8, utcSeconds);
        writeInt32LE(pkt, 16, gmtOffsetSec);
    });
}

function buildEvent({ flags, startUtc, endUtc, title }) {
    return makePacket(TAG_EVENT, (pkt) => {
        pkt[1] = flags & 0xff;
        writeInt64LE(pkt, 2, startUtc);
        writeInt64LE(pkt, 10, endUtc);
        writeTitle(pkt, 18, title);
    });
}

function buildBatchEnd() {
    return makePacket(TAG_BATCH_END);
}

function buildClear() {
    return makePacket(TAG_CLEAR);
}

// --- BLE transport --------------------------------------------------------

async function writePacket(pkt, label) {
    if (!state.writeChar) throw new Error('not connected');
    await state.writeChar.writeValueWithResponse(pkt);
    log(`${label} sent (${pkt.length} B)`);
}

async function connect() {
    if (!('bluetooth' in navigator)) {
        log('ERROR: Web Bluetooth not available in this browser.');
        return;
    }
    try {
        log('Requesting device...');
        const device = await navigator.bluetooth.requestDevice({
            filters: [{ services: [SERVICE_UUID] }],
        });
        log(`Picked: ${device.name || '(unnamed)'} [${device.id}]`);

        device.addEventListener('gattserverdisconnected', () => {
            log('Disconnected.');
            setConnectedUI(false);
            state.device = null;
            state.server = null;
            state.service = null;
            state.writeChar = null;
            state.stateChar = null;
        });

        log('Connecting GATT...');
        const server = await device.gatt.connect();
        const service = await server.getPrimaryService(SERVICE_UUID);
        const writeChar = await service.getCharacteristic(CHAR_WRITE_UUID);
        const stateChar = await service.getCharacteristic(CHAR_STATE_UUID);

        state.device = device;
        state.server = server;
        state.service = service;
        state.writeChar = writeChar;
        state.stateChar = stateChar;

        log('Connected. Characteristics resolved.');
        setConnectedUI(true);
    } catch (err) {
        log(`Connect failed: ${err.message || err}`);
    }
}

async function sendClear() {
    try {
        await writePacket(buildClear(), 'CLEAR');
    } catch (err) {
        log(`CLEAR failed: ${err.message || err}`);
    }
}

async function sendTestBatch() {
    try {
        const nowSec = Math.floor(Date.now() / 1000);
        // getTimezoneOffset is minutes WEST of UTC; invert for "offset east" seconds.
        const gmtOffsetSec = -new Date().getTimezoneOffset() * 60;

        log(`Batch: nowUtc=${nowSec} gmtOffsetSec=${gmtOffsetSec}`);
        await writePacket(buildTimeSync(nowSec, gmtOffsetSec), 'TIME_SYNC');

        const events = [
            { title: 'Design review',   offsetH: 1 },
            { title: 'Lunch with team', offsetH: 2 },
            { title: 'Gym',             offsetH: 3 },
        ];
        for (let i = 0; i < events.length; i++) {
            const ev = events[i];
            const start = nowSec + ev.offsetH * 3600;
            const end   = start + 1800; // 30-minute slots
            const pkt = buildEvent({
                flags: 0,
                startUtc: start,
                endUtc: end,
                title: ev.title,
            });
            await writePacket(pkt, `EVENT ${i + 1}/${events.length} ("${ev.title}")`);
        }

        await writePacket(buildBatchEnd(), 'BATCH_END');

        // Follow up with a state read so the user sees eventCount update.
        await readState();
    } catch (err) {
        log(`Batch failed: ${err.message || err}`);
    }
}

async function readState() {
    try {
        if (!state.stateChar) throw new Error('not connected');
        const v = await state.stateChar.readValue();
        if (v.byteLength < STATE_SIZE) {
            log(`STATE short read: ${v.byteLength} B`);
            return;
        }
        const lastSyncUtc   = v.getBigInt64(0, true);
        const eventCount    = v.getUint16(8, true);
        const schemaVersion = v.getUint16(10, true);
        const when = lastSyncUtc === 0n
            ? '(never)'
            : new Date(Number(lastSyncUtc) * 1000).toISOString();
        log(`STATE: lastSyncUtc=${lastSyncUtc} (${when}) eventCount=${eventCount} schemaVersion=${schemaVersion}`);
    } catch (err) {
        log(`Read STATE failed: ${err.message || err}`);
    }
}

// --- Wire up --------------------------------------------------------------

btnConnect.addEventListener('click', connect);
btnClear.addEventListener('click', sendClear);
btnBatch.addEventListener('click', sendTestBatch);
btnReadState.addEventListener('click', readState);

log('Ready. Press DOWN twice on the watch to advertise, then Connect.');
