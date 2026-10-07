const fs = require('fs');
const path = require('path');

// Match ESP-IDF's version.txt so the web and application OTA images can be
// checked as a pair without replacing the upstream Git tag or NVS settings.
const version = fs.readFileSync(path.resolve(__dirname, '../../../version.txt'), 'utf8').trim();
if (!version || Buffer.byteLength(version, 'utf8') > 31) {
  throw new Error('Firmware version must fit the ESP application descriptor');
}

const outputPath = path.join(__dirname, 'dist', 'axe-os', 'version.txt');
fs.writeFileSync(outputPath, version);
fs.writeFileSync(path.join(__dirname, 'dist', 'axe-os', 'build-info.json'), JSON.stringify({
  product: '5tratumFW', version, nodeVersion: process.versions.node, upstream: 'v2.14.2'
}) + '\n');

console.log(`Generated ${outputPath} with version ${version}`);
