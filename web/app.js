// app.js - the esp32-iot-common web pages (iot-platform architecture §6.5).
//
// Every page is the same shell (page.html); this script draws it from /status.
// The library provides the Home page (Device Identity, the device's sensor
// cards, System Status, Network Settings, MQTT Status) and the WiFi page. A
// device adds its own sensor cards, pages, tabs and controls in /device.js:
//
//   IOT.setup({title: 'ESP32 Garage', icon: '&#x1F697;', sensors: 'Garage Doors'});
//   IOT.card('garage_door', (d, st) => `<div class='sensor'>...</div>`);
//   IOT.page({path: '/sim', tab: 'Simulator', show: st => st.device.simulation,
//             html: () => `<div id='doors'></div>`, load: async st => { ... }});
//
// Controls (HTML strings, handled here): IOT.button(), IOT.toggle(), IOT.form().
'use strict';

const $ = s => document.querySelector(s);
const esc = s => String(s == null ? '' : s).replace(/[&<>"']/g,
  c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'}[c]));
const n = v => v == null ? '' : v;

function badge(s, small) {
  return `<span class='status ${small ? 'small ' : ''}${esc(String(s).toLowerCase())}'>${esc(s)}</span>`;
}
// One "Label: value" line; value is HTML (escape text with esc()).
function kv(label, value) {
  return `<div><span class='label'>${label}:</span><span class='value'>${value}</span></div>`;
}
function yes(ok, good, bad) {
  return ok ? `<span class='connected'>${good} &#10003;</span>` : `<span class='disconnected'>${bad} &#10007;</span>`;
}
function dur(s) {
  s = Math.max(0, Math.floor(s));
  const d = Math.floor(s / 86400), h = Math.floor(s / 3600) % 24, m = Math.floor(s / 60) % 60;
  return (d ? d + 'd ' : '') + (h ? h + 'h ' : '') + (m ? m + 'm ' : '') + (s % 60) + 's';
}
// "bus_voltage" -> "Bus voltage"
const human = k => k.charAt(0).toUpperCase() + k.slice(1).replace(/_/g, ' ');

const IOT = {
  title: '', icon: '', sensors: 'Sensors',
  pages: [], cards: {}, banners: [], actions: {},
  status: null, current: null, timer: null,

  // Device settings: title, icon (HTML entity), sensors (heading of the sensor cards)
  setup(o) { Object.assign(this, o); },
  // A page: {path, tab, html() -> static HTML (drawn once), load(st) (every refresh),
  //          refresh (ms, default 2000), show(st) -> false hides the tab}
  page(p) { this.pages.push(p); },
  // A card for sensors of one "type" in /status: fn(sensor, status) -> HTML
  card(type, fn) { this.cards[type] = fn; },
  // A banner shown on every page: fn(status) -> HTML or ''
  banner(fn) { this.banners.push(fn); },
  // A named function for a button that is not a plain POST: IOT.button(label, null, {action: name})
  action(name, fn) { this.actions[name] = fn; },

  // --- Controls -------------------------------------------------------------

  // Button that POSTs to url, shows the result and refreshes the page.
  // o.confirm: true or a question to ask first; o.cls: CSS class; o.action: run a named function instead.
  button(label, url, o = {}) {
    const ask = o.confirm ? ` data-confirm='${esc(o.confirm === true ? label + '?' : o.confirm)}'` : '';
    const what = o.action ? ` data-action='${esc(o.action)}'` : ` data-post='${esc(url)}'`;
    return `<button${what}${ask}${o.cls ? ` class='${esc(o.cls)}'` : ''}>${esc(label)}</button>`;
  },
  // On/off button: shows "label: ON" (highlighted) or "label: off"; posts offUrl when on, onUrl when off.
  toggle(label, on, onUrl, offUrl, o = {}) {
    return this.button(`${label}: ${on ? 'ON' : 'off'}`, on ? offUrl : onUrl, Object.assign({cls: on ? 'on' : ''}, o));
  },
  // Form: a table of fields and a submit button that POSTs url?name=value&...
  // field: {name, label, type: 'text'|'number'|'select'|'checkbox', value, min, max, step, options,
  //         maxlength, placeholder, width, text (after the input)} or {label, items: [field, ...]} for one row.
  // o.submit: button text (default 'Apply'); o.confirm as for button(); o.extra: HTML after the button.
  form(id, fields, url, o = {}) {
    const input = f => {
      const fid = `${id}-${f.name}`, st = f.width ? ` style='width:${esc(f.width)}'` : '';
      let h;
      if (f.type === 'select') {
        h = `<select id='${fid}' name='${esc(f.name)}'>` + f.options.map(v =>
          `<option${String(v) === String(n(f.value)) ? ' selected' : ''}>${esc(v)}</option>`).join('') + '</select>';
      } else if (f.type === 'checkbox') {
        h = `<label><input type='checkbox' id='${fid}' name='${esc(f.name)}'${f.value ? ' checked' : ''}> ${esc(n(f.text))}</label>`;
        return h;
      } else {
        const a = ['min', 'max', 'step', 'maxlength', 'placeholder'].filter(k => f[k] != null)
          .map(k => ` ${k}='${esc(f[k])}'`).join('');
        h = `<input id='${fid}' name='${esc(f.name)}' type='${f.type || 'text'}' value='${esc(n(f.value))}'${a}${st}>`;
      }
      return h + (f.text ? ` <span class='muted'>${f.text}</span>` : '');
    };
    const rows = fields.map(f => `<tr><th>${esc(n(f.label))}</th><td>${(f.items || [f]).map(input).join(' ')}</td></tr>`);
    const ask = o.confirm ? ` data-confirm='${esc(o.confirm === true ? (o.submit || 'Apply') + '?' : o.confirm)}'` : '';
    rows.push(`<tr><th></th><td><button data-form='${esc(id)}' data-url='${esc(url)}'${ask}>${esc(o.submit || 'Apply')}</button>` +
      `${o.extra || ''}</td></tr>`);
    return `<table class='form' id='${esc(id)}'>${rows.join('')}</table>`;
  },
  // The form's fields as URL parameters (checkbox: 1 or 0)
  formValues(id) {
    const q = new URLSearchParams();
    document.querySelectorAll(`#${id} [name]`).forEach(e => q.append(e.name, e.type === 'checkbox' ? (e.checked ? 1 : 0) : e.value));
    return q;
  },
  // Set the form's fields from the device's current values, unless the user is editing (or force)
  fill(id, values, force) {
    const t = $('#' + id);
    if (!t || (t.dataset.dirty && !force)) return;
    Object.entries(values).forEach(([k, v]) => {
      const e = t.querySelector(`[name='${k}']`);
      if (!e) return;
      if (e.type === 'checkbox') e.checked = !!v; else e.value = n(v);
    });
  },

  // POST and show the device's answer {ok, result, detail} in the message line
  async post(url) {
    let j = {};
    try {
      const r = await fetch(url, {method: 'POST'});
      try { j = await r.json(); } catch (e) { j = {ok: false, detail: r.status + ' ' + r.statusText}; }
    } catch (e) {
      j = {ok: false, detail: 'No answer from the device'};
    }
    $('#msg').textContent = (j.result ? j.result + ': ' : '') + (j.detail || '');
    return j;
  },
  async getJson(url) { return (await fetch(url)).json(); },

  // --- Pages ----------------------------------------------------------------

  start() {
    const all = [HOME_PAGE, WIFI_PAGE].concat(this.pages);
    this.current = all.find(p => p.path === location.pathname) || HOME_PAGE;
    this.all = all;
    document.title = this.title || 'ESP32';
    $('#title').innerHTML = (this.icon ? this.icon + ' ' : '') + esc(this.title || '');
    $('#page').innerHTML = this.current.html ? this.current.html() : '';
    document.addEventListener('click', e => this.click(e));
    document.addEventListener('input', e => {
      const t = e.target.closest('table.form');
      if (t) t.dataset.dirty = '1';
    });
    this.refresh();
  },
  tabs(st) {
    const tabs = this.all.filter(p => p.tab && (!p.show || p.show(st)))
      .map(p => [p.path, p.tab]).concat([['/status', 'JSON']]);
    $('#tabs').innerHTML = tabs.map(([path, label]) =>
      `<a href='${esc(path)}'${path === this.current.path ? " class='active'" : ''}>${esc(label)}</a>`).join('');
  },
  async refresh() {
    clearTimeout(this.timer);
    try {
      const st = await this.getJson('/status');
      this.status = st;
      if (!this.title) $('#title').innerHTML = (this.icon ? this.icon + ' ' : '') + esc(st.device.name);
      this.tabs(st);
      $('#banner').innerHTML = this.banners.map(b => b(st) || '').join('');
      if (this.current.load) await this.current.load(st);
      $('#footer').textContent = 'Last updated: ' + st.device.uptime + 's';
    } catch (e) {
      $('#msg').textContent = 'No answer from the device';
      if (window.console) console.warn(e);
    }
    this.timer = setTimeout(() => this.refresh(), this.current.refresh || 2000);
  },
  async click(e) {
    const b = e.target.closest('button[data-post],button[data-form],button[data-action]');
    if (!b) return;
    if (b.dataset.confirm && !confirm(b.dataset.confirm)) return;
    if (b.dataset.action) {
      await this.actions[b.dataset.action](b);
      return;
    }
    if (b.dataset.form) {
      const id = b.dataset.form;
      const j = await this.post(b.dataset.url + '?' + this.formValues(id));
      if (j.ok) delete $('#' + id).dataset.dirty;
    } else {
      await this.post(b.dataset.post);
    }
    this.refresh();
  },

  // --- Common cards ---------------------------------------------------------

  sensorCard(s, st) {
    if (this.cards[s.type]) return this.cards[s.type](s, st);
    // Default card: name, state badge, every other plain field, then the topic
    let h = `<div class='sensor'><span class='label'>${esc(s.name || s.id || s.type)}:</span>` +
      (s.state != null ? badge(s.state) : '');
    Object.entries(s).forEach(([k, v]) => {
      if (['name', 'type', 'state', 'topic', 'id'].includes(k) || v === null || typeof v === 'object') return;
      h += kv(esc(human(k)), esc(v));
    });
    if (s.topic) h += kv('MQTT Topic', esc(s.topic));
    return h + '</div>';
  },
};

const HOME_PAGE = {
  path: '/', tab: 'Home',
  html: () => `<h2>Device Identity</h2><div class='info' id='ident'></div>
<h2 id='sensh'></h2><div id='sensors'></div>
<h2>System Status</h2><div class='info' id='sys'></div>
<h2>Network Settings</h2><div class='info' id='net'></div>
<h2>MQTT Status</h2><div class='info' id='mqtt'></div>`,
  load(st) {
    const v = st.device, w = st.network, m = st.mqtt, sensors = st.sensors || [];
    $('#ident').innerHTML = kv('Device Name', esc(v.name)) + kv('Chip Model', esc(v.chip)) + kv('Board', esc(v.board)) +
      kv('Firmware', esc(v.firmware) + (v.build ? ' (' + esc(v.build) + ')' : ''));
    $('#sensh').textContent = sensors.length ? `${IOT.sensors} (${sensors.length})` : '';
    $('#sensors').innerHTML = sensors.map(s => IOT.sensorCard(s, st)).join('');
    $('#sys').innerHTML = kv('CPU Speed', v.cpu_speed + ' MHz') +
      kv('Current Time', v.time_synced ? esc(v.time) : 'not synced') +
      kv('Time Sync', yes(v.time_synced, 'Synced', 'Not Synced')) + kv('Uptime', dur(v.uptime)) +
      kv('Last Reset', esc(v.reset_reason)) +
      kv('Reconnects', `WiFi ${v.wifi_reconnects}, MQTT ${Math.max(0, m.connects - 1)}`) +
      kv('Free Memory', `${v.free_heap} bytes (lowest ${v.min_free_heap})`);
    $('#net').innerHTML = kv('WiFi SSID', esc(w.ssid)) + kv('Signal Quality', w.rssi + ' dBm') +
      kv('IP Address', esc(v.ip)) + kv('MAC Address', esc(v.mac)) + kv('mDNS Host Name', esc(w.hostname) + '.local') +
      kv('Access Point', esc(w.bssid) + ' (channel ' + w.channel + ')');
    let mq = kv('MQTT Status', yes(m.connected, 'Connected', 'Disconnected') +
      (!m.connected && m.disconnected_s != null ? ` for ${dur(m.disconnected_s)}` : ''));
    if (m.failed_attempts) mq += kv('Failed Attempts', m.failed_attempts);
    if (m.last_error) mq += kv('Last Error', esc(m.last_error) + ` (at uptime ${dur(m.last_error_uptime)})`);
    mq += kv('MQTT Broker', esc(m.broker)) + kv('MQTT Port', m.port) + kv('Username', esc(m.user)) +
      kv('Status Topic', esc(m.status_topic)) + kv('Telemetry Topic', esc(m.telemetry_topic));
    $('#mqtt').innerHTML = mq;
  },
};

// --- WiFi page (moved from esp32-garage; the API is in iot_web.h) ------------

const TX_POWERS = ['19.5', '19', '18.5', '17', '15', '13', '11', '8.5', '7', '5', '2'];

const WIFI_PAGE = {
  path: '/wifi', tab: 'WiFi',
  html: () => `<h2>WiFi Status</h2><div class='info' id='live'></div>
<h2>Roaming</h2><div class='info'><div id='roam'></div>${IOT.button('Check now', '/api/wifi/roam')}</div>
<h2>Since Last Telemetry</h2><div class='info' id='win'></div>
<h2>Signal, Last 10 Minutes</h2><div class='info'><canvas id='graph' width='740' height='200'></canvas></div>
<h2>Reception Test</h2>
<div class='info'>
<p class='muted'>x / y: feet from the property origin (may be negative); height: feet above the ground.
Swap antennas at the SMA end with the power off.</p>
${IOT.form('wtest', [
    {name: 'antenna', label: 'Antenna', value: 'antenna-1', maxlength: 32},
    {name: 'position', label: 'Position', maxlength: 32, placeholder: 'e.g. kitchen'},
    {label: 'x, y, height (ft)', items: ['x_ft', 'y_ft', 'height_ft'].map(k =>
      ({name: k, type: 'number', step: 0.5, width: '5em'}))},
    {label: 'Orientation', items: [
      {name: 'orientation', type: 'select', options: ['', 'vertical', 'horizontal', 'angled']},
      {name: 'environment', type: 'select', options: ['', 'indoor', 'outdoor']}]},
    {name: 'note', label: 'Note', maxlength: 64, width: '20em'},
    {name: 'seconds', label: 'Seconds', type: 'number', value: 60, min: 10, max: 600, width: '5em'},
  ], '/api/wifi/test', {submit: 'Start'})}
<div id='testrun' class='muted'></div>
</div>
<h2>Settings (Until Restart)</h2>
<div class='info'>${IOT.form('wcfg', [
    {name: 'tx_power', label: 'TX Power', type: 'select', options: TX_POWERS, text: 'dBm'},
    {name: 'sleep', label: 'Modem Sleep', type: 'checkbox', text: 'on'},
  ], '/api/wifi/config')}</div>
<h2>Test Results (Last 10)</h2><div class='info scroll'><table id='results'></table></div>
<h2>Nearby Networks</h2><div class='info'>${IOT.button('Scan', null, {action: 'scan'})}<table id='scan'></table></div>`,

  async load() {
    const w = await IOT.getJson('/api/wifi?history=1');
    $('#live').innerHTML = kv('WiFi SSID', esc(w.ssid)) + kv('Signal Quality', w.rssi + ' dBm') +
      kv('Access Point', esc(w.bssid) + ' (channel ' + w.channel + ')') + kv('TX Power', w.tx_power_dbm + ' dBm') +
      kv('Modem Sleep', w.modem_sleep ? 'on' : 'off') + kv('IP Address', `${esc(w.ip)} (gateway ${esc(w.gateway)})`);
    IOT.fill('wcfg', {tx_power: String(w.tx_power_dbm), sleep: w.modem_sleep});
    const ro = w.roaming;
    if (ro) {
      const next = ro.checking ? 'checking now...' : 'next check in ' + dur(ro.next_check_s);
      $('#roam').innerHTML = kv('Roaming', ro.enabled ? `every ${dur(ro.interval_s)}, move if another access point is ` +
          `${ro.margin_db} dB stronger; ${next}` : 'off') +
        kv('Checks', `${ro.checks}, roams ${ro.roams}` + (ro.last_roam_epoch ?
          ' (last ' + new Date(ro.last_roam_epoch * 1000).toLocaleString() + ')' : '')) +
        kv('Last Check', esc(ro.last_result));
    }
    const s = w.window;
    $('#win').innerHTML = kv('RSSI', `${s.rssi} dBm (min ${s.rssi_min}, max ${s.rssi_max}, sd ${s.rssi_std})`) +
      (s.ping_sent ? kv('Ping', `${s.ping_ms} ms (max ${s.ping_ms_max}), loss ${(s.ping_loss * 100).toFixed(1)} %`) :
        kv('Ping', 'not measured yet (only during tests and while this page is open)')) + kv('Samples', s.samples);
    WIFI_PAGE.draw(w.history || []);
    const t = w.test;
    $('#testrun').textContent = t.running ? (t.scanning ? `Scanning access points for test ${t.id}...` :
      `Test ${t.id} "${t.label}" at "${t.position}": ${t.elapsed_s} of ${t.seconds} s`) : '';
    const r = await IOT.getJson('/api/wifi/test');
    const when = t => t.epoch ? new Date(t.epoch * 1000).toLocaleString([], {month: 'numeric', day: 'numeric',
      hour: '2-digit', minute: '2-digit'}) : 'uptime ' + dur(t.uptime_s);
    $('#results').innerHTML = '<tr><th>#</th><th>Time</th><th>Antenna</th><th>Position</th><th>x</th><th>y</th><th>h</th>' +
      '<th>RSSI</th><th>p10</th><th>sd</th><th>loss %</th><th>ping ms</th><th>p95</th><th>disc</th><th>roam</th>' +
      '<th>AP</th><th>mesh APs</th><th>same ch</th></tr>' + r.results.map(t => `<tr><td>${t.id}</td><td>${when(t)}</td>` +
      `<td>${esc(t.antenna)}${t.orientation ? ' (' + esc(t.orientation) + ')' : ''}</td>` +
      `<td>${esc(t.position)}${t.environment ? ' (' + esc(t.environment) + ')' : ''}` +
      `${t.note ? '<br><span class="muted">' + esc(t.note) + '</span>' : ''}</td>` +
      `<td>${n(t.x_ft)}</td><td>${n(t.y_ft)}</td><td>${n(t.height_ft)}</td><td>${t.rssi}</td><td>${t.rssi_p10}</td>` +
      `<td>${t.rssi_std}</td><td>${(t.ping_loss * 100).toFixed(1)}</td><td>${t.ping_ms}</td><td>${t.ping_ms_p95}</td>` +
      `<td>${t.disconnects}</td><td>${t.ap_changes}</td><td>${esc(t.bssid)} ch ${t.channel}</td>` +
      `<td>${t.scanned ? t.own_aps.map(a => esc(a.bssid.slice(-5)) + ' ' + a.rssi).join('<br>') : '-'}</td>` +
      `<td>${t.scanned ? t.same_channel_others + (t.same_channel_others ? ' (' + t.same_channel_strongest + ')' : '') : '-'}</td>` +
      '</tr>').join('');
  },

  draw(h) {
    const c = $('#graph'), x = c.getContext('2d'), W = c.width, H = c.height;
    x.clearRect(0, 0, W, H);
    const lo = -95, hi = -25, y = v => H - (v - lo) / (hi - lo) * H;
    x.font = '11px Arial'; x.fillStyle = '#666'; x.strokeStyle = '#ddd'; x.lineWidth = 1;
    for (let v = -90; v <= -30; v += 10) {
      x.beginPath(); x.moveTo(30, y(v)); x.lineTo(W, y(v)); x.stroke(); x.fillText(v, 0, y(v) + 4);
    }
    if (!h.length) return;
    x.strokeStyle = '#2196F3'; x.lineWidth = 2; x.beginPath();
    h.forEach((v, i) => { const px = 30 + (W - 30) * (i + 600 - h.length) / 600; i ? x.lineTo(px, y(v)) : x.moveTo(px, y(v)); });
    x.stroke();
  },
};

IOT.action('scan', async () => {
  await fetch('/api/wifi/scan?start=1');
  $('#scan').innerHTML = '<tr><td>Scanning...</td></tr>';
  for (let i = 0; i < 20; i++) {
    await new Promise(r => setTimeout(r, 1000));
    const s = await IOT.getJson('/api/wifi/scan');
    if (s.status === 'done') {
      s.networks.sort((a, b) => b.rssi - a.rssi);
      $('#scan').innerHTML = '<tr><th>SSID</th><th>RSSI</th><th>Channel</th><th>BSSID</th><th>Security</th></tr>' +
        s.networks.map(x => `<tr><td>${esc(x.ssid) || '<i>hidden</i>'}${x.connected ? ' &#10003;' : ''}</td>` +
        `<td>${x.rssi}</td><td>${x.channel}</td><td>${esc(x.bssid)}</td><td>${esc(x.auth)}</td></tr>`).join('');
      return;
    }
  }
});
