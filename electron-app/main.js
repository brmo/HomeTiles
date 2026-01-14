const { app, BrowserWindow, ipcMain, Tray, Menu } = require('electron');
const path = require('path');
const fs = require('fs');
const os = require('os');
const si = require('systeminformation');
const WebSocket = require('ws');
const Store = require('electron-store');
const { spawn } = require('child_process');

const DEFAULT_METRICS = {
  cpu_load: true,
  cpu_temp: false,
  gpu_temp: false,
  gpu_load: false,
  mem_used: true,
  mem_pct: true,
  uptime: true
};

const DEFAULT_SIMCONNECT = {
  enabled: false,
  host: '127.0.0.1',
  port: 13375
};

const DEFAULT_SIM_VAR_LIST = [];

const DEFAULT_SIM_METRICS = {};

const SIMVAR_DEFAULTS = {
  'AIRSPEED INDICATED': { unit: 'knots', type: 'float64' },
  'AIRSPEED TRUE': { unit: 'knots', type: 'float64' },
  'AIRSPEED MACH': { unit: 'mach', type: 'float64' },
  'GPS GROUND SPEED': { unit: 'knots', type: 'float64' },
  'PLANE ALTITUDE': { unit: 'feet', type: 'float64' },
  'INDICATED ALTITUDE': { unit: 'feet', type: 'float64' },
  'VERTICAL SPEED': { unit: 'feet per minute', type: 'float64' },
  'PLANE HEADING DEGREES MAGNETIC': { unit: 'degrees', type: 'float64' },
  'PLANE HEADING DEGREES TRUE': { unit: 'degrees', type: 'float64' },
  'FUEL TOTAL QUANTITY': { unit: 'gallons', type: 'float64' },
  'SIM ON GROUND': { unit: 'bool', type: 'int32' }
};

// keysender für schnelle Tastensimulation
const { Hardware } = require('keysender');
const kb = new Hardware(null); // null = kein spezifisches Fenster, globale Eingabe

// Settings Store
const store = new Store({
  defaults: {
    autostart: true,  // Standard: Autostart aktiviert
    tab5_ip: '192.168.2.235',
    metrics: DEFAULT_METRICS,
    pc_metrics_enabled: true,
    key_output_enabled: true,
    simconnect: DEFAULT_SIMCONNECT,
    sim_vars: DEFAULT_SIM_VAR_LIST,
    sim_metrics: DEFAULT_SIM_METRICS
  }
});

let mainWindow = null;
let tray = null;
let ws = null;
let reconnectTimer = null;
let pingInterval = null;
let connectionAttempts = 0;
let metricsInterval = null;
let lastCpuSample = null;
let metricsInFlight = false;
let simWs = null;
let simReconnectTimer = null;
let simConnected = false;
let lastSimSensors = [];
let lastSimSensorsRaw = [];
let simBridgeProcess = null;
let simBridgeConfig = null;

const startHidden = process.argv.includes('--hidden');

// Tab5 WebSocket Verbindung
let TAB5_IP = store.get('tab5_ip'); // Aus Settings laden
const TAB5_PORT = 8081;
const METRICS_INTERVAL_MS = 5000;
const SIMCONNECT_RECONNECT_MS = 2000;
const lastMetricValues = {};

// Scan Code zu Robot Key Mapping
// Based on USB HID Usage Tables
const SCANCODE_MAP = {
  // Letters
  0x04: 'a', 0x05: 'b', 0x06: 'c', 0x07: 'd', 0x08: 'e', 0x09: 'f',
  0x0A: 'g', 0x0B: 'h', 0x0C: 'i', 0x0D: 'j', 0x0E: 'k', 0x0F: 'l',
  0x10: 'm', 0x11: 'n', 0x12: 'o', 0x13: 'p', 0x14: 'q', 0x15: 'r',
  0x16: 's', 0x17: 't', 0x18: 'u', 0x19: 'v', 0x1A: 'w', 0x1B: 'x',
  0x1C: 'y', 0x1D: 'z',

  // Numbers (Main Keyboard)
  0x1E: '1', 0x1F: '2', 0x20: '3', 0x21: '4', 0x22: '5',
  0x23: '6', 0x24: '7', 0x25: '8', 0x26: '9', 0x27: '0',

  // Special Keys
  0x28: 'enter', 0x29: 'escape', 0x2A: 'backspace', 0x2B: 'tab', 0x2C: 'space',
  0x2D: '-', 0x2E: '=', 0x2F: '[', 0x30: ']',
  0x31: '\\', 0x33: ';', 0x34: '\'', 0x35: '`',
  0x36: ',', 0x37: '.', 0x38: '/', 0x39: 'capsLock',

  // Function Keys
  0x3A: 'f1', 0x3B: 'f2', 0x3C: 'f3', 0x3D: 'f4', 0x3E: 'f5', 0x3F: 'f6',
  0x40: 'f7', 0x41: 'f8', 0x42: 'f9', 0x43: 'f10', 0x44: 'f11', 0x45: 'f12',

  // System Keys
  0x46: 'printScreen', 0x47: 'scrollLock', 0x48: 'pause', 0x49: 'insert',
  0x4A: 'home', 0x4B: 'pageUp', 0x4C: 'delete', 0x4D: 'end', 0x4E: 'pageDown',
  0x4F: 'right', 0x50: 'left', 0x51: 'down', 0x52: 'up',

  // Numpad
  0x53: 'numLock', 0x54: 'divide', 0x55: 'multiply', 0x56: 'subtract', 0x57: 'add',
  0x58: 'enter', 0x59: 'num1', 0x5A: 'num2', 0x5B: 'num3', 0x5C: 'num4', 0x5D: 'num5',
  0x5E: 'num6', 0x5F: 'num7', 0x60: 'num8', 0x61: 'num9', 0x62: 'num0', 0x63: 'decimal',

  // Media & Extra
  0x7F: 'audioMute', 0x80: 'audioVolUp', 0x81: 'audioVolDown',
  0xE0: 'controlLeft', 0xE1: 'shiftLeft', 0xE2: 'altLeft', 0xE3: 'metaLeft',
  0xE4: 'controlRight', 0xE5: 'shiftRight', 0xE6: 'altRight', 0xE7: 'metaRight'
};

function normalizeMetricsConfig(raw) {
  const cfg = { ...DEFAULT_METRICS };
  if (!raw || typeof raw !== 'object') return cfg;
  for (const key of Object.keys(cfg)) {
    if (raw[key] !== undefined) {
      cfg[key] = !!raw[key];
    }
  }
  return cfg;
}

function getMetricsConfig() {
  return normalizeMetricsConfig(store.get('metrics', {}));
}

function setMetricsConfig(raw) {
  const cfg = normalizeMetricsConfig(raw);
  store.set('metrics', cfg);
  return cfg;
}

function getPcMetricsEnabled() {
  return !!store.get('pc_metrics_enabled', true);
}

function setPcMetricsEnabled(enabled) {
  const next = !!enabled;
  store.set('pc_metrics_enabled', next);
  if (next) {
    startMetricsLoop();
  } else {
    stopMetricsLoop();
  }
  return next;
}

function getKeyOutputEnabled() {
  return !!store.get('key_output_enabled', true);
}

function setKeyOutputEnabled(enabled) {
  const next = !!enabled;
  store.set('key_output_enabled', next);
  return next;
}

function normalizeSimConfig(raw) {
  const cfg = { ...DEFAULT_SIMCONNECT };
  if (!raw || typeof raw !== 'object') return cfg;
  if (raw.enabled !== undefined) cfg.enabled = !!raw.enabled;
  if (raw.host) cfg.host = String(raw.host);
  if (raw.port !== undefined) {
    const parsed = Number(raw.port);
    if (Number.isFinite(parsed) && parsed > 0) {
      cfg.port = Math.floor(parsed);
    }
  }
  return cfg;
}

function getSimConfig() {
  return normalizeSimConfig(store.get('simconnect', {}));
}

function setSimConfig(raw) {
  const cfg = normalizeSimConfig(raw);
  store.set('simconnect', cfg);
  return cfg;
}

function isLocalBridgeConfig(cfg) {
  const host = (cfg.host || '').toLowerCase();
  return host === '127.0.0.1' || host === 'localhost';
}

function getBridgeExecutablePath() {
  if (app.isPackaged) {
    const packagedPath = path.join(process.resourcesPath, 'bridge', 'SimConnectBridge.exe');
    return fs.existsSync(packagedPath) ? packagedPath : null;
  }
  const candidates = [
    path.join(__dirname, '..', 'simconnect-bridge', 'bin', 'Release', 'net8.0', 'SimConnectBridge.exe'),
    path.join(__dirname, '..', 'simconnect-bridge', 'bin', 'Debug', 'net8.0', 'SimConnectBridge.exe')
  ];
  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }
  return null;
}

async function startSimBridge(cfg) {
  if (!cfg || !cfg.enabled) return;
  if (!isLocalBridgeConfig(cfg)) return;

  if (simBridgeProcess && simBridgeConfig &&
      simBridgeConfig.host === cfg.host && simBridgeConfig.port === cfg.port) {
    return;
  }

  await stopSimBridge();

  const exePath = getBridgeExecutablePath();
  if (!exePath) {
    log('SimConnect bridge executable not found. Build/publish it first.');
    return;
  }

  const configPath = writeSimBridgeConfigFile(getSimVarList());
  const args = ['--host', cfg.host, '--port', String(cfg.port), '--config', configPath];
  simBridgeConfig = { host: cfg.host, port: cfg.port, configPath };
  simBridgeProcess = spawn(exePath, args, {
    cwd: path.dirname(exePath),
    windowsHide: true
  });

  log(`Starting SimConnect bridge: ${exePath}`);

  simBridgeProcess.stdout.on('data', (data) => {
    const text = data.toString().trim();
    if (text.length) log(text);
  });

  simBridgeProcess.stderr.on('data', (data) => {
    const text = data.toString().trim();
    if (text.length) log(text);
  });

  simBridgeProcess.on('exit', (code) => {
    log(`SimConnect bridge exited (${code ?? 'unknown'})`);
    simBridgeProcess = null;
    simBridgeConfig = null;
  });
}

function stopSimBridge() {
  return new Promise((resolve) => {
    if (!simBridgeProcess) {
      resolve();
      return;
    }
    const proc = simBridgeProcess;
    simBridgeProcess = null;
    simBridgeConfig = null;

    const timeout = setTimeout(() => {
      try {
        proc.kill('SIGKILL');
      } catch {
        // Ignore.
      }
      resolve();
    }, 2000);

    proc.once('exit', () => {
      clearTimeout(timeout);
      resolve();
    });

    try {
      proc.kill();
    } catch {
      clearTimeout(timeout);
      resolve();
    }
  });
}

function getSimVarDefaults(simvar) {
  const key = String(simvar || '').trim().toUpperCase();
  return SIMVAR_DEFAULTS[key] || null;
}

function parseSimVarType(raw) {
  const value = String(raw || '').trim().toLowerCase();
  if (!value) return null;
  if (value === 'int' || value === 'int32' || value === 'bool') return 'int32';
  if (value === 'string' || value === 'string32') return 'string32';
  if (value === 'string256') return 'string256';
  return 'float64';
}

function normalizeSimVarType(raw, simvar) {
  const parsed = parseSimVarType(raw);
  if (parsed) return parsed;
  const fallback = getSimVarDefaults(simvar);
  const fallbackType = fallback ? parseSimVarType(fallback.type) : null;
  return fallbackType || 'float64';
}

function normalizeSimVarUnit(simvar, rawUnit) {
  const unit = String(rawUnit || '').trim();
  if (unit) return unit;
  const fallback = getSimVarDefaults(simvar);
  return fallback ? fallback.unit || '' : '';
}

function normalizeSimVarDecimals(raw) {
  if (raw === null || raw === undefined || raw === '') return null;
  const parsed = Number(raw);
  if (!Number.isFinite(parsed)) return null;
  const rounded = Math.floor(parsed);
  if (rounded < 0) return null;
  return Math.min(6, rounded);
}

function normalizeSimVarList(raw) {
  if (!Array.isArray(raw)) return [];
  const result = [];
  const seen = new Set();
  raw.forEach((entry) => {
    if (!entry || typeof entry !== 'object') return;
    const entityId = String(entry.entity_id || entry.entityId || '').trim();
    const simvar = String(entry.simvar || entry.simVar || '').trim();
    if (!entityId || !simvar) return;
    const key = entityId.toLowerCase();
    if (seen.has(key)) return;
    seen.add(key);
    const name = String(entry.name || simvar).trim();
    const unit = normalizeSimVarUnit(simvar, entry.unit);
    const type = normalizeSimVarType(entry.type, simvar);
    const decimals = normalizeSimVarDecimals(entry.decimals);
    result.push({
      entity_id: entityId,
      name: name || simvar,
      simvar,
      unit,
      type,
      ...(decimals === null ? {} : { decimals })
    });
  });
  return result;
}

function getSimVarList() {
  const raw = store.get('sim_vars', []);
  return normalizeSimVarList(raw);
}

function setSimVarList(raw) {
  const list = normalizeSimVarList(raw);
  store.set('sim_vars', list);
  writeSimBridgeConfigFile(list);
  return list;
}

function getSimBridgeConfigPath() {
  return path.join(app.getPath('userData'), 'simconnect-bridge-config.json');
}

function writeSimBridgeConfigFile(list) {
  const configPath = getSimBridgeConfigPath();
  const payload = {
    version: 1,
    simvars: list
  };
  fs.mkdirSync(path.dirname(configPath), { recursive: true });
  fs.writeFileSync(configPath, JSON.stringify(payload, null, 2), 'utf8');
  return configPath;
}

function normalizeSimMetricsConfig(raw) {
  const cfg = { ...DEFAULT_SIM_METRICS };
  if (!raw || typeof raw !== 'object') return cfg;
  for (const [key, value] of Object.entries(raw)) {
    cfg[key] = !!value;
  }
  return cfg;
}

function getSimMetricsConfig() {
  return normalizeSimMetricsConfig(store.get('sim_metrics', {}));
}

function setSimMetricsConfig(raw) {
  const cfg = normalizeSimMetricsConfig(raw);
  store.set('sim_metrics', cfg);
  return cfg;
}

function isSimMetricEnabled(config, entityId) {
  if (!entityId) return false;
  if (Object.prototype.hasOwnProperty.call(config, entityId)) {
    return !!config[entityId];
  }
  return true;
}

function filterSimSensors(sensors) {
  if (!Array.isArray(sensors) || sensors.length === 0) return [];
  const allowed = new Set(getSimVarList().map((entry) => entry.entity_id));
  const cfg = getSimMetricsConfig();
  return sensors.filter((sensor) => allowed.has(sensor.entity_id) && isSimMetricEnabled(cfg, sensor.entity_id));
}

function roundSensorValue(value, decimals) {
  if (typeof value !== 'number' || !Number.isFinite(value)) return value;
  let digits = 2;
  if (typeof decimals === 'number' && Number.isFinite(decimals)) {
    digits = Math.max(0, Math.min(6, Math.floor(decimals)));
  }
  const factor = 10 ** digits;
  return Math.round(value * factor) / factor;
}

function getMetricValue(entityId, value, decimals) {
  if (value === null || value === undefined) {
    if (Object.prototype.hasOwnProperty.call(lastMetricValues, entityId)) {
      return lastMetricValues[entityId];
    }
    return '--';
  }
  const rounded = roundSensorValue(value, decimals);
  lastMetricValues[entityId] = rounded;
  return rounded;
}

function updateMetricsPreview(sensors) {
  if (!mainWindow || !mainWindow.webContents) return;
  const payload = Array.isArray(sensors) ? sensors : [];
  mainWindow.webContents.send('metrics-update', payload);
}

function updateSimPreview(sensors) {
  if (!mainWindow || !mainWindow.webContents) return;
  const payload = Array.isArray(sensors) ? sensors : [];
  mainWindow.webContents.send('sim-update', payload);
}

function buildSimVarDecimalsMap() {
  const map = new Map();
  getSimVarList().forEach((entry) => {
    if (!entry || !entry.entity_id) return;
    if (entry.decimals === null || entry.decimals === undefined) return;
    map.set(entry.entity_id, entry.decimals);
  });
  return map;
}

function normalizeIncomingSensors(rawSensors, decimalsMap) {
  if (!Array.isArray(rawSensors)) return [];
  return rawSensors
    .map((sensor) => {
      if (!sensor || typeof sensor !== 'object') return null;
      const entityId = sensor.entity_id || sensor.entityId;
      if (!entityId) return null;
      const name = sensor.name || entityId;
      const unit = sensor.unit || '';
      const decimals = decimalsMap ? decimalsMap.get(entityId) : null;
      const value = getMetricValue(entityId, sensor.value, decimals);
      return { entity_id: entityId, name, unit, value };
    })
    .filter(Boolean);
}

function normalizeTemperature(value) {
  if (!Number.isFinite(value)) return null;
  if (value <= 0) return null;
  return value;
}

function normalizePercent(value) {
  if (!Number.isFinite(value)) return null;
  return Math.max(0, Math.min(100, value));
}

function getCpuTempValue(cpuTemp) {
  if (!cpuTemp) return null;
  const main = normalizeTemperature(cpuTemp.main);
  if (main !== null) return main;
  if (Array.isArray(cpuTemp.cores) && cpuTemp.cores.length) {
    const values = cpuTemp.cores
      .map((v) => normalizeTemperature(v))
      .filter((v) => v !== null);
    if (values.length) {
      const sum = values.reduce((acc, v) => acc + v, 0);
      return sum / values.length;
    }
  }
  return null;
}

function pickGpuController(graphics) {
  if (!graphics || !Array.isArray(graphics.controllers)) return null;
  return graphics.controllers.find((c) => {
    const temp = normalizeTemperature(c.temperatureGpu);
    const load = normalizePercent(Number(c.utilizationGpu));
    return temp !== null || load !== null;
  }) || null;
}

function getCpuUsagePercent() {
  const cpus = os.cpus();
  let idle = 0;
  let total = 0;

  for (const cpu of cpus) {
    idle += cpu.times.idle;
    total += cpu.times.user + cpu.times.nice + cpu.times.sys + cpu.times.idle + cpu.times.irq;
  }

  if (!lastCpuSample) {
    lastCpuSample = { idle, total };
    return null;
  }

  const idleDiff = idle - lastCpuSample.idle;
  const totalDiff = total - lastCpuSample.total;
  lastCpuSample = { idle, total };

  if (totalDiff <= 0) return null;

  let usage = (1 - idleDiff / totalDiff) * 100;
  if (!Number.isFinite(usage)) return null;
  usage = Math.max(0, Math.min(100, usage));
  return usage;
}

async function buildPcMetricsPayload() {
  const config = getMetricsConfig();
  const sensors = [];
  const tasks = [];
  let cpuTemp = null;
  let graphics = null;

  if (config.cpu_temp) {
    tasks.push(
      si.cpuTemperature()
        .then((data) => { cpuTemp = data; })
        .catch(() => {})
    );
  }

  if (config.gpu_temp || config.gpu_load) {
    tasks.push(
      si.graphics()
        .then((data) => { graphics = data; })
        .catch(() => {})
    );
  }

  if (config.cpu_load) {
    const cpuLoad = getCpuUsagePercent();
    const value = cpuLoad !== null ? Number(cpuLoad.toFixed(1)) : null;
    sensors.push({
      entity_id: 'pc.cpu_load',
      name: 'CPU Load',
      unit: '%',
      value: getMetricValue('pc.cpu_load', value)
    });
  }

  if (config.mem_used || config.mem_pct) {
    const totalMem = os.totalmem();
    const freeMem = os.freemem();
    const usedMem = totalMem - freeMem;
    const usedGb = usedMem / (1024 ** 3);
    const usedPct = totalMem > 0 ? (usedMem / totalMem) * 100 : 0;

    if (config.mem_used) {
      sensors.push({
        entity_id: 'pc.mem_used',
        name: 'RAM Used',
        unit: 'GB',
        value: getMetricValue('pc.mem_used', Number(usedGb.toFixed(2)))
      });
    }

    if (config.mem_pct) {
      sensors.push({
        entity_id: 'pc.mem_pct',
        name: 'RAM Usage',
        unit: '%',
        value: getMetricValue('pc.mem_pct', Number(usedPct.toFixed(1)))
      });
    }
  }

  if (config.uptime) {
    sensors.push({
      entity_id: 'pc.uptime',
      name: 'PC Uptime',
      unit: 's',
      value: getMetricValue('pc.uptime', Math.floor(os.uptime()))
    });
  }

  if (tasks.length) {
    await Promise.all(tasks);
  }

  if (config.cpu_temp) {
    const temp = getCpuTempValue(cpuTemp);
    const value = temp !== null ? Number(temp.toFixed(1)) : null;
    sensors.push({
      entity_id: 'pc.cpu_temp',
      name: 'CPU Temp',
      unit: 'C',
      value: getMetricValue('pc.cpu_temp', value)
    });
  }

  if (config.gpu_temp || config.gpu_load) {
    const gpu = pickGpuController(graphics);
    const gpuTemp = gpu ? normalizeTemperature(gpu.temperatureGpu) : null;
    const gpuLoad = gpu ? normalizePercent(Number(gpu.utilizationGpu)) : null;

    if (config.gpu_temp) {
      const value = gpuTemp !== null ? Number(gpuTemp.toFixed(1)) : null;
      sensors.push({
        entity_id: 'pc.gpu_temp',
        name: 'GPU Temp',
        unit: 'C',
        value: getMetricValue('pc.gpu_temp', value)
      });
    }

    if (config.gpu_load) {
      const value = gpuLoad !== null ? Number(gpuLoad.toFixed(1)) : null;
      sensors.push({
        entity_id: 'pc.gpu_load',
        name: 'GPU Load',
        unit: '%',
        value: getMetricValue('pc.gpu_load', value)
      });
    }
  }

  updateMetricsPreview(sensors);
  if (!sensors.length) return null;
  return { type: 'pc_metrics', sensors };
}

function sendSensorsToTab5(sensors) {
  if (!Array.isArray(sensors) || sensors.length === 0) return;
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type: 'pc_metrics', sensors }));
  }
}

async function sendPcMetrics() {
  if (!getPcMetricsEnabled()) return;
  if (metricsInFlight) return;
  metricsInFlight = true;
  try {
    const payload = await buildPcMetricsPayload();
    if (!payload) return;
    sendSensorsToTab5(payload.sensors);
  } catch (err) {
    log(`Metrics send error: ${err.message}`);
  } finally {
    metricsInFlight = false;
  }
}

function startMetricsLoop() {
  if (!getPcMetricsEnabled()) return;
  stopMetricsLoop();
  lastCpuSample = null;
  getCpuUsagePercent();
  metricsInterval = setInterval(sendPcMetrics, METRICS_INTERVAL_MS);
  setTimeout(sendPcMetrics, 1000);
}

function stopMetricsLoop() {
  if (metricsInterval) {
    clearInterval(metricsInterval);
    metricsInterval = null;
  }
}

function updateSimStatus(status) {
  if (mainWindow) {
    mainWindow.webContents.send('sim-status', status);
  }
}

function handleSimBridgeMessage(data) {
  let msg = null;
  try {
    msg = JSON.parse(data.toString());
  } catch (err) {
    log(`SimConnect parse error: ${err.message}`);
    return;
  }
  if (!msg || !Array.isArray(msg.sensors)) return;
  const decimalsMap = buildSimVarDecimalsMap();
  const sensors = normalizeIncomingSensors(msg.sensors, decimalsMap);
  if (!sensors.length) return;
  lastSimSensorsRaw = sensors;
  const filtered = filterSimSensors(sensors);
  lastSimSensors = filtered;
  updateSimPreview(sensors);
  sendSensorsToTab5(filtered);
}

async function connectSimBridge() {
  const cfg = getSimConfig();
  if (!cfg.enabled) {
    updateSimStatus('disconnected');
    await stopSimBridge();
    return;
  }
  if (isLocalBridgeConfig(cfg)) {
    await startSimBridge(cfg);
  } else {
    await stopSimBridge();
  }
  if (simWs && (simWs.readyState === WebSocket.OPEN || simWs.readyState === WebSocket.CONNECTING)) {
    return;
  }

  const url = `ws://${cfg.host}:${cfg.port}`;
  log(`Connecting SimConnect bridge: ${url}`);

  simWs = new WebSocket(url, {
    perMessageDeflate: false,
    handshakeTimeout: 5000
  });

  simWs.on('open', () => {
    simConnected = true;
    updateSimStatus('connected');
    log('SimConnect bridge connected');
    if (simReconnectTimer) {
      clearTimeout(simReconnectTimer);
      simReconnectTimer = null;
    }
  });

  simWs.on('message', (data) => {
    handleSimBridgeMessage(data);
  });

  simWs.on('close', () => {
    simConnected = false;
    updateSimStatus('disconnected');
    log('SimConnect bridge disconnected');
    if (!getSimConfig().enabled) return;
    if (simReconnectTimer) return;
    simReconnectTimer = setTimeout(() => {
      simReconnectTimer = null;
      connectSimBridge();
    }, SIMCONNECT_RECONNECT_MS);
  });

  simWs.on('error', (err) => {
    log(`SimConnect bridge error: ${err.message}`);
  });
}

function disconnectSimBridge() {
  if (simReconnectTimer) {
    clearTimeout(simReconnectTimer);
    simReconnectTimer = null;
  }
  if (simWs) {
    simWs.close();
    simWs = null;
  }
  simConnected = false;
  updateSimStatus('disconnected');
}

function sendSimBridgeReload() {
  if (simWs && simWs.readyState === WebSocket.OPEN) {
    simWs.send(JSON.stringify({ command: 'reload' }));
    log('Sent reload command to SimConnect bridge');
  }
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 520,
    height: 700,
    show: false, // Nicht sofort anzeigen
    resizable: false,
    maximizable: false,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false
    }
  });

  // Menüleiste entfernen
  Menu.setApplicationMenu(null);

  mainWindow.loadFile('index.html');

  // Fenster anzeigen wenn bereit (oder im Tray lassen)
  mainWindow.once('ready-to-show', () => {
    if (startHidden) return;
    // Nur anzeigen wenn kein Tray Icon existiert
    if (!tray) {
      mainWindow.show();
    }
  });

  mainWindow.on('close', (event) => {
    if (!app.isQuitting) {
      event.preventDefault();
      mainWindow.hide();
    }
  });

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

function createTray() {
  const iconPath = path.join(__dirname, 'icon.png');

  if (!fs.existsSync(iconPath)) {
    console.log('⚠️ icon.png not found - skipping tray icon');
    return;
  }

  tray = new Tray(iconPath);

  const contextMenu = Menu.buildFromTemplate([
    {
      label: 'Show Window',
      click: () => {
        mainWindow.show();
        mainWindow.focus();
      }
    },
    { type: 'separator' },
    {
      label: 'Quit',
      click: () => {
        app.isQuitting = true;
        app.quit();
      }
    }
  ]);

  tray.setContextMenu(contextMenu);
  tray.setToolTip('Tab5 Game Controls - Click to open');

  // Doppelklick auf Tray Icon öffnet Fenster
  tray.on('double-click', () => {
    mainWindow.show();
    mainWindow.focus();
  });
}

function connectToTab5() {
  const url = `ws://${TAB5_IP}:${TAB5_PORT}`;

  connectionAttempts++;
  log(`Connecting to Tab5: ${url} (attempt ${connectionAttempts})`);

  ws = new WebSocket(url, {
    perMessageDeflate: false,
    handshakeTimeout: 5000
  });

  ws.on('open', () => {
    log('✅ Connected to Tab5!');
    updateStatus('connected');
    connectionAttempts = 0;

    // Reconnect Timer clearen
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }

    // Ping alle 15 Sekunden senden
    pingInterval = setInterval(() => {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.ping();
      }
    }, 15000);

    if (getPcMetricsEnabled()) {
      startMetricsLoop();
    }
    const filteredSim = filterSimSensors(lastSimSensorsRaw);
    if (filteredSim.length) {
      sendSensorsToTab5(filteredSim);
    }
  });

  ws.on('message', (data) => {
    try {
      const msg = JSON.parse(data.toString());
      handleTab5Message(msg);
    } catch (err) {
      log(`Error parsing message: ${err.message}`);
    }
  });

  ws.on('pong', () => {
    // Server antwortet auf Ping - Verbindung ist aktiv
  });

  ws.on('close', (code, reason) => {
    log(`❌ Disconnected from Tab5 (code: ${code})`);
    updateStatus('disconnected');

    // Cleanup
    if (pingInterval) {
      clearInterval(pingInterval);
      pingInterval = null;
    }
    stopMetricsLoop();
    stopMetricsLoop();
    ws = null;

    // Auto-Reconnect mit exponential backoff (max 30 Sekunden)
    const delay = Math.min(5000 * Math.pow(1.5, Math.min(connectionAttempts, 5)), 30000);
    log(`Reconnecting in ${Math.round(delay / 1000)} seconds...`);

    reconnectTimer = setTimeout(() => {
      connectToTab5();
    }, delay);
  });

  ws.on('error', (err) => {
    log(`WebSocket error: ${err.message}`);
  });
}

function handleTab5Message(msg) {
  log(`📩 Received: ${JSON.stringify(msg)}`);

  if (msg.type === 'button_press') {
    if (getKeyOutputEnabled()) {
      simulateKeyPress(msg.key, msg.modifier);
    }

    // An Renderer senden für UI Update
    if (mainWindow) {
      mainWindow.webContents.send('button-pressed', msg);
    }
  }
}

async function simulateKeyPress(scancode, modifier) {
  const key = SCANCODE_MAP[scancode];

  if (!key) {
    log(`⚠️ Unknown scan code: 0x${scancode.toString(16)}`);
    return;
  }

  const modifiers = [];
  if (modifier & 0x01) modifiers.push('control');
  if (modifier & 0x02) modifiers.push('shift');
  if (modifier & 0x04) modifiers.push('alt');
  if (modifier & 0x08) modifiers.push('command'); // Win/Meta

  log(`⌨️ Processing: ${modifiers.join('+')} ${key} (Code: 0x${scancode.toString(16)})`);

  try {
    // keysender Hardware API
    if (modifiers.length > 0) {
      // Mit Modifiern
      const keyCombo = [...modifiers, key];
      log(`   -> Sending combo: [${keyCombo.join(', ')}]`);
      await kb.keyboard.sendKey(keyCombo);
    } else {
      // Ohne Modifier
      log(`   -> Sending single: ${key}`);
      await kb.keyboard.sendKey(key);
    }
    log(`✅ Success`);
  } catch (err) {
    log(`❌ ERROR Sending Key: ${err.message}`);
    // Prevent App Crash by catching all errors here
  }
}

function log(message) {
  const timestamp = new Date().toLocaleTimeString();
  console.log(`[${timestamp}] ${message}`);

  if (mainWindow) {
    mainWindow.webContents.send('log', { timestamp, message });
  }
}

function updateStatus(status) {
  if (mainWindow) {
    mainWindow.webContents.send('status', status);
  }
}

// IPC Handlers
ipcMain.on('connect', () => {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    connectToTab5();
  }
});

ipcMain.on('disconnect', () => {
  if (ws) {
    // Cleanup Timers
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
    if (pingInterval) {
      clearInterval(pingInterval);
      pingInterval = null;
    }

    // Verbindung schließen
    ws.close();
    ws = null;
    updateStatus('disconnected');
    log('Manually disconnected');
  }
});

ipcMain.on('get-tab5-ip', (event) => {
  event.returnValue = store.get('tab5_ip');
});

ipcMain.on('set-tab5-ip', (event, ip) => {
  TAB5_IP = ip;
  store.set('tab5_ip', ip);  // In Config speichern
  log(`Tab5 IP changed to: ${ip}`);
});

ipcMain.on('get-metrics-config', (event) => {
  event.returnValue = getMetricsConfig();
});

ipcMain.on('set-metrics-config', (event, config) => {
  const next = setMetricsConfig(config);
  log(`Metrics config updated: ${JSON.stringify(next)}`);
});

ipcMain.on('get-pc-metrics-enabled', (event) => {
  event.returnValue = getPcMetricsEnabled();
});

ipcMain.on('set-pc-metrics-enabled', (event, enabled) => {
  const next = setPcMetricsEnabled(enabled);
  log(`PC metrics ${next ? 'enabled' : 'disabled'}`);
});

ipcMain.on('get-key-output-enabled', (event) => {
  event.returnValue = getKeyOutputEnabled();
});

ipcMain.on('set-key-output-enabled', (event, enabled) => {
  const next = setKeyOutputEnabled(enabled);
  log(`Key output ${next ? 'enabled' : 'disabled'}`);
});

ipcMain.on('get-sim-config', (event) => {
  event.returnValue = getSimConfig();
});

ipcMain.on('set-sim-config', async (event, config) => {
  const next = setSimConfig(config);
  log(`SimConnect config updated: ${JSON.stringify(next)}`);
  disconnectSimBridge();
  await stopSimBridge();
  if (next.enabled) {
    connectSimBridge();
  }
});

ipcMain.on('get-sim-var-list', (event) => {
  event.returnValue = getSimVarList();
});

ipcMain.on('get-sim-var-draft-list', (event) => {
  event.returnValue = store.get('sim_vars_draft', null);
});

ipcMain.on('set-sim-var-list', (event, list) => {
  const next = setSimVarList(list);
  log(`SimConnect SimVars updated: ${JSON.stringify(next)}`);
  event.returnValue = next;

  // Clear cached values for sim metrics
  for (const key of Object.keys(lastMetricValues)) {
    if (key.startsWith('sim.')) {
      delete lastMetricValues[key];
    }
  }

  // Send reload command to bridge instead of restarting
  sendSimBridgeReload();
});

ipcMain.on('set-sim-var-draft-list', (event, list) => {
  store.set('sim_vars_draft', Array.isArray(list) ? list : []);
  event.returnValue = store.get('sim_vars_draft');
});

ipcMain.on('get-sim-metrics-config', (event) => {
  event.returnValue = getSimMetricsConfig();
});

ipcMain.on('set-sim-metrics-config', (event, config) => {
  const next = setSimMetricsConfig(config);
  log(`SimConnect metrics updated: ${JSON.stringify(next)}`);
  const filtered = filterSimSensors(lastSimSensorsRaw);
  lastSimSensors = filtered;
  updateSimPreview(lastSimSensorsRaw);
  sendSensorsToTab5(filtered);
});

// Autostart Funktionen
function setAutostart(enabled) {
  const loginItem = {
    openAtLogin: enabled,
    openAsHidden: true,
    args: ['--hidden']
  };

  if (!app.isPackaged) {
    loginItem.path = process.execPath;
    loginItem.args = [app.getAppPath(), '--hidden'];
  }

  app.setLoginItemSettings(loginItem);
  store.set('autostart', enabled);
  log(`Autostart ${enabled ? 'enabled' : 'disabled'}`);
}

function getAutostart() {
  return store.get('autostart', true);
}

// IPC Handler für Autostart
ipcMain.on('get-autostart', (event) => {
  event.returnValue = getAutostart();
});

ipcMain.on('set-autostart', (event, enabled) => {
  setAutostart(enabled);
});

// App Lifecycle
app.whenReady().then(() => {
  // Autostart beim ersten Start aktivieren
  const autostartEnabled = getAutostart();
  setAutostart(autostartEnabled);
  writeSimBridgeConfigFile(getSimVarList());

  createTray();    // Tray zuerst erstellen
  createWindow();  // Dann Window (startet minimiert wenn Tray existiert)
  if (getPcMetricsEnabled()) {
    startMetricsLoop();
  }
  if (getSimConfig().enabled) {
    connectSimBridge();
  }

  // Auto-Connect nach 2 Sekunden
  setTimeout(() => {
    connectToTab5();
  }, 2000);
});

app.on('window-all-closed', () => {
  // Nichts tun - App läuft im Tray weiter
});

app.on('activate', () => {
  if (mainWindow === null) {
    createWindow();
  } else {
    mainWindow.show();
  }
});

app.on('before-quit', async () => {
  stopMetricsLoop();
  disconnectSimBridge();
  await stopSimBridge();
  if (ws) {
    ws.close();
  }
});
