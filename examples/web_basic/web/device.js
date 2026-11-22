// device.js for the web_basic example: a title, a sensor card with a control.
// Embedded into web_device.h with:
//   python3 ../../tools/web_embed.py web_device.h DEVICE web/device.js
IOT.setup({title: 'IoT Example', icon: '&#x1F4A1;', sensors: 'Lamps'});
IOT.card('lamp', s => `<div class='sensor'><span class='label'>${esc(s.name)}:</span>${badge(s.state)}` +
  `<div>${IOT.toggle('Lamp', s.state === 'ON', '/api/lamp?on=1', '/api/lamp?on=0')}</div></div>`);
