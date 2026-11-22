// Browser tests for the web pages (web/): node tests/web/run.js
//
// Serves the real page files with fixture /status and API answers, opens them
// in headless Chromium (Playwright) and checks the cards, tabs and controls.
// Needs the playwright package and its Chromium (CI installs both).
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');
const {chromium} = require('playwright');

const WEB = path.join(__dirname, '..', '..', 'web');
const FIX = path.join(__dirname, 'fixtures');
const fixture = f => JSON.parse(fs.readFileSync(path.join(FIX, f), 'utf8'));

// What the fake device serves; changed by the tests
const device = {status: fixture('status_door.json'), script: '', posts: []};

const server = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://x');
  const send = (code, type, body) => { res.writeHead(code, {'Content-Type': type}); res.end(body); };
  const json = o => send(200, 'application/json', JSON.stringify(o));
  if (req.method === 'POST') {
    device.posts.push(url.pathname + url.search);
    return json({ok: true, result: 'DONE', detail: 'got ' + url.pathname});
  }
  switch (url.pathname) {
    case '/style.css': return send(200, 'text/css', fs.readFileSync(path.join(WEB, 'style.css')));
    case '/app.js': return send(200, 'application/javascript', fs.readFileSync(path.join(WEB, 'app.js')));
    case '/device.js': return send(200, 'application/javascript', device.script);
    case '/status': return json(device.status);
    case '/api/wifi': return json(fixture('wifi.json'));
    case '/api/wifi/test': return json({results: []});
    case '/api/wifi/scan': return json({status: 'done', networks: [
      {ssid: 'home', rssi: -50, channel: 9, bssid: '11:22:33:44:55:66', auth: 'WPA2', connected: true},
      {ssid: 'other', rssi: -80, channel: 1, bssid: '22:33:44:55:66:77', auth: 'WPA2'}]});
    case '/': case '/wifi': case '/extras':
      return send(200, 'text/html', fs.readFileSync(path.join(WEB, 'page.html')));
    default: return send(404, 'text/plain', 'not found');
  }
});

let checks = 0, failures = 0;
function check(cond, what) {
  checks++;
  if (!cond) { failures++; console.log('FAIL ' + what); }
}

async function main() {
  await new Promise(r => server.listen(0, '127.0.0.1', r));
  const base = `http://127.0.0.1:${server.address().port}`;
  const browser = await chromium.launch();
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
  const text = async sel => (await page.textContent(sel)) || '';
  const open = async (p, ready) => { await page.goto(base + p); await page.waitForSelector(ready); };

  // 1. A device without its own script: the common cards and the default sensor card
  await open('/', '#ident div');
  check(await text('#title') === 'backyard00', 'title falls back to the device name');
  check((await page.$$eval('#tabs a', as => as.map(a => a.textContent))).join(',') === 'Home,WiFi,JSON', 'tabs Home, WiFi, JSON');
  check(await text('#tabs a.active') === 'Home', 'Home tab active');
  const ident = await text('#ident');
  for (const s of ['Device Name:backyard00', 'Chip Model:ESP32-D0WD-V3 rev 3', 'Board:ESP32 DEVKITV1 (ESP-WROOM-32)',
    'Firmware:261122-0552 (244d03e)']) check(ident.includes(s), 'identity: ' + s);
  check(await text('#sensh') === 'Sensors (1)', 'sensor heading');
  const card = await text('#sensors');
  check(card.includes('Backyard Gate:CLOSED') && card.includes('Pin:18') && card.includes('MQTT Topic:sensor/backyard/gate01'),
    'default sensor card: name, state, other fields, topic');
  check(await page.$('#sensors .status.closed') !== null, 'state badge colored by state');
  const sys = await text('#sys');
  for (const s of ['CPU Speed:80 MHz', 'Current Time:2026-11-21T21:55:12', 'Time Sync:Synced', 'Uptime:1h 2m 5s',
    'Last Reset:SOFTWARE', 'Reconnects:WiFi 0, MQTT 1', 'Free Memory:143832 bytes (lowest 128288)'])
    check(sys.includes(s), 'system: ' + s);
  const net = await text('#net');
  for (const s of ['WiFi SSID:home', 'Signal Quality:-54 dBm', 'IP Address:192.168.1.140', 'MAC Address:AA:BB:CC:DD:EE:FF',
    'mDNS Host Name:backyard00.local', 'Access Point:11:22:33:44:55:66 (channel 9)']) check(net.includes(s), 'network: ' + s);
  check(!net.includes('WiFi Status') && !net.includes('RSSI'), 'no WiFi Status or separate RSSI line');
  const mq = await text('#mqtt');
  for (const s of ['MQTT Status:Connected', 'MQTT Broker:192.168.1.112', 'MQTT Port:8883', 'Username:backyard00',
    'Status Topic:sensor/backyard/status', 'Telemetry Topic:telemetry/backyard/device']) check(mq.includes(s), 'mqtt: ' + s);
  check(!mq.includes('Encryption') && !mq.includes('Client ID') && !mq.includes('Last Error') && !mq.includes('Failed'),
    'no encryption, client id or error lines when all is well');
  check(/Last updated: \d+s/.test(await text('#footer')), 'footer');

  // 2. A device script: title, banner, custom cards, controls, MQTT trouble lines
  device.status = fixture('status_garage.json');
  device.script = fs.readFileSync(path.join(FIX, 'device_garage.js'), 'utf8');
  await open('/', '#card-left');
  check((await text('#title')).includes('ESP32 Test Garage'), 'title from IOT.setup');
  check(await page.$('#simbanner') !== null, 'banner from IOT.banner');
  check(await text('#sensh') === 'Garage Doors (2)', 'sensor heading from IOT.setup');
  check(await page.$$eval('.door-card', e => e.length) === 2, 'custom card for each garage_door');
  check((await page.$$eval('#tabs a', as => as.map(a => a.textContent))).join(',') === 'Home,WiFi,Extras,JSON', 'device tab shown');
  const mq2 = await text('#mqtt');
  check(mq2.includes('Disconnected') && mq2.includes('for 2m 5s'), 'disconnected for ...');
  check(mq2.includes('Failed Attempts:3') && mq2.includes('Last Error:connect failed (rc=-2) (at uptime 1m 1s)'), 'error lines');
  check((await text('#sys')).includes('Current Time:not synced'), 'time not synced');

  device.posts = [];
  page.once('dialog', d => d.dismiss());
  await page.click('#card-left button[data-post$="/open"]');
  await page.waitForTimeout(200);
  check(device.posts.length === 0, 'confirm dismissed: nothing sent');
  page.once('dialog', d => { check(d.message() === 'Open the door?', 'confirm question'); d.accept(); });
  await page.click('#card-left button[data-post$="/open"]');
  await page.waitForFunction(() => document.querySelector('#msg').textContent.includes('DONE'));
  check(device.posts.includes('/api/door/left/open'), 'button POSTs its url');
  check(await text('#msg') === 'DONE: got /api/door/left/open', 'result shown in the message line');
  check(await text('#card-left button.on') === 'Light: ON', 'toggle shows ON when on');
  await page.click('#card-left button.on');
  await page.waitForTimeout(200);
  check(device.posts.includes('/api/door/left/light_off'), 'toggle that is on posts its off url');
  await page.click('#card-right button[data-post$="light_on"]');
  await page.waitForTimeout(200);
  check(device.posts.includes('/api/door/right/light_on'), 'toggle that is off posts its on url');

  // 3. A device page: form filled from the device, kept while editing, submitted as parameters
  await open('/extras', '#cfg');
  await page.waitForFunction(() => document.querySelector('#cfg-baud').value === '9600');
  check(await text('#tabs a.active') === 'Extras', 'device page tab active');
  check(await page.$eval('#cfg-format', e => e.value) === '8E1' && await page.$eval('#cfg-mqtt', e => e.checked), 'form filled');
  await page.fill('#cfg-baud', '19200');
  await page.waitForFunction(() => /loads [3-9]/.test(document.querySelector('#loads').textContent));
  check(await page.$eval('#cfg-baud', e => e.value) === '19200', 'an edited form is not overwritten by refreshes');
  device.posts = [];
  await page.click('#cfg button[data-form]');
  await page.waitForTimeout(300);
  check(device.posts.includes('/api/bus/config?baud=19200&format=8E1&mqtt=1'), 'form posts its fields');
  await page.click('button[data-action="hello"]');
  check(await text('#msg') === 'hello action', 'action button runs its function');
  device.status = Object.assign({}, device.status, {device: Object.assign({}, device.status.device, {simulation: false})});
  await page.goto(base + '/');
  await page.waitForSelector('#card-left');
  check(!(await page.$$eval('#tabs a', as => as.map(a => a.textContent))).includes('Extras'), 'show(st) false hides the tab');
  check(await page.$('#simbanner') === null, 'banner hidden when its function returns nothing');

  // 4. The WiFi page
  device.status = fixture('status_door.json');
  device.script = '';
  await open('/wifi', '#live div');
  check(await text('#tabs a.active') === 'WiFi', 'WiFi tab active');
  check((await text('#live')).includes('Signal Quality:-60 dBm'), 'WiFi status');
  check((await text('#roam')).includes('every 2h 0s, move if another access point is 8 dB stronger'), 'roaming');
  check(await page.$eval('#wcfg-tx_power', e => e.value) === '15' && await page.$eval('#wcfg-sleep', e => e.checked),
    'settings filled from the device');
  device.posts = [];
  await page.fill('#wtest-position', 'kitchen');
  await page.click('#wtest button[data-form]');
  await page.waitForTimeout(300);
  check(device.posts.some(p => p.startsWith('/api/wifi/test?antenna=antenna-1&position=kitchen&x_ft=&y_ft=&height_ft=') &&
    p.endsWith('&seconds=60')), 'reception test form');
  await page.click('button[data-post="/api/wifi/roam"]');
  await page.waitForTimeout(200);
  check(device.posts.includes('/api/wifi/roam'), 'Check now');
  await page.click('button[data-action="scan"]');
  await page.waitForFunction(() => document.querySelector('#scan').textContent.includes('other'), null, {timeout: 5000});
  check((await text('#scan')).indexOf('home') < (await text('#scan')).indexOf('other'), 'scan sorted by signal');

  check(errors.length === 0, 'no script errors: ' + errors.join(' | '));
  await browser.close();
  server.close();
  console.log(`web pages: ${checks} checks, ${failures} failures`);
  process.exit(failures ? 1 : 0);
}

main().catch(e => { console.error(e); process.exit(1); });
