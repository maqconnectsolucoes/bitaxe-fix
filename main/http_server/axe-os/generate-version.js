const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

// The root version.txt (if present) is also what ESP-IDF reads for PROJECT_VER,
// embedded into the firmware's esp_app_desc_t and compared against this value
// by system.c's mismatch check - keep both in lockstep instead of two
// independent `git describe` calls that can drift apart.
const rootVersionPath = path.join(__dirname, '..', '..', '..', 'version.txt');
const version = fs.existsSync(rootVersionPath)
  ? fs.readFileSync(rootVersionPath, 'utf8').trim()
  : require('child_process').execSync('git describe --tags --always --dirty').toString().trim();
const buildTime = new Date().toISOString();

const distRoot = path.join(__dirname, 'dist', 'axe-os');

const versionTxtPath = path.join(distRoot, 'version.txt');
fs.writeFileSync(versionTxtPath, version);
console.log(`Generated ${versionTxtPath} with version ${version}`);

// Also drop a small JSON file the running app can fetch at runtime, so the UI
// can show "is this the build I just made?" without digging into a sub-page.
// Written straight into dist (same as version.txt above) so it always reflects
// the build that just ran, regardless of when Angular's own asset-copy step ran.
//
// This script runs after the gzipper/only-gzip steps, which already turned the
// placeholder src/assets/build-info.json into build-info.json.gz - and the
// firmware serves the .gz when both exist. Overwrite the gzipped copy too (and
// drop the raw one), otherwise every www.bin reports version "dev".
const buildInfoPath = path.join(distRoot, 'assets', 'build-info.json');
const buildInfo = JSON.stringify({ version, buildTime }, null, 2);
fs.mkdirSync(path.dirname(buildInfoPath), { recursive: true });
fs.writeFileSync(`${buildInfoPath}.gz`, zlib.gzipSync(Buffer.from(buildInfo), { level: 9 }));
fs.rmSync(buildInfoPath, { force: true });
console.log(`Generated ${buildInfoPath}.gz with version ${version} built at ${buildTime}`);
