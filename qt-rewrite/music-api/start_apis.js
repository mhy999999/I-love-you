const path = require('path');
const fs = require('fs');

console.log('Starting Music APIs...');

const ncmPort = Number.parseInt(process.env.NCM_PORT || '30490', 10);
const qqPort = Number.parseInt(process.env.QQ_PORT || process.env.PORT || '3200', 10);

// 1. Start Netease Cloud Music API (Port 30490)
try {
  console.log(`-> Starting Netease Cloud Music API on port ${ncmPort}...`);
  // Verify if module exists
  try {
    require.resolve('netease-cloud-music-api-alger/server');
  } catch (e) {
    console.error('netease-cloud-music-api-alger not found. Please run npm install.');
    throw e;
  }
  
  const neteaseServer = require('netease-cloud-music-api-alger/server');
  neteaseServer.serveNcmApi({
    port: ncmPort,
    checkVersion: true,
  });
} catch (err) {
  console.error('Failed to start Netease Cloud Music API:', err);
}

// 2. Start QQ Music API (Port 3200)
try {
  console.log(`-> Starting QQ Music API on port ${qqPort}...`);
  const qqMusicApiDir = path.join(__dirname, 'QQMusicApi');
  
  // Set environment variable for port
  process.env.PORT = String(qqPort);
  
  // QQMusicApi/bin/www is the entry point
  const qqEntry = path.join(qqMusicApiDir, 'bin', 'www');
  
  if (fs.existsSync(qqEntry)) {
    require(qqEntry);
  } else {
    console.error('QQMusicApi entry point not found at:', qqEntry);
  }
} catch (err) {
  console.error('Failed to start QQ Music API:', err);
}
