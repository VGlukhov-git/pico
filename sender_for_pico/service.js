const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

const port = new SerialPort({ path: '/dev/tty.usbmodem2101', baudRate: 115200 });
const parser = port.pipe(new ReadlineParser({ delimiter: '\r\n' }))

port.on('data', (data) => {
  console.log(`Received from Pico: ${data.toString()}`);
});

port.on('open', () => {
  console.log(`Serial port  opened.`);
  setInterval(() => {
    port.write('Hello from Node.js!\n');
  }, 10);
});

port.on('error', (err) => {
  console.error('Serial port error:', err.message);
});

