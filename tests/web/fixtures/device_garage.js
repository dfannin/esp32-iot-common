// A device script like a garage's, using every extension point of app.js
IOT.setup({title: 'ESP32 Test Garage', icon: '&#x1F697;', sensors: 'Garage Doors'});
IOT.banner(st => st.device.simulation ? "<div class='banner' id='simbanner'>Simulation</div>" : '');
IOT.card('garage_door', d => `<div class='sensor door-card' id='card-${d.id}'><span class='label'>${esc(d.name)}:</span>` +
  badge(d.state) + '<div>' + IOT.button('Open', `/api/door/${d.id}/open`, {confirm: 'Open the door?'}) +
  IOT.toggle('Light', d.light, `/api/door/${d.id}/light_on`, `/api/door/${d.id}/light_off`) + '</div></div>');
let extrasLoads = 0;
IOT.page({
  path: '/extras', tab: 'Extras', show: st => st.device.simulation, refresh: 500,
  html: () => IOT.form('cfg', [
    {name: 'baud', label: 'Baud rate', type: 'number', min: 1200, max: 115200},
    {name: 'format', label: 'Format', type: 'select', options: ['8N1', '8E1']},
    {name: 'mqtt', label: 'MQTT', type: 'checkbox', text: 'publish'},
  ], '/api/bus/config') + "<div id='loads'></div>" + IOT.button('Hello', null, {action: 'hello'}),
  load: st => {
    IOT.fill('cfg', {baud: 9600, format: '8E1', mqtt: true});
    document.querySelector('#loads').textContent = 'loads ' + (++extrasLoads);
  },
});
IOT.action('hello', () => { document.querySelector('#msg').textContent = 'hello action'; });
