// 这是一个聚合启动脚本，用于同时启动网易云音乐 API 和 QQ 音乐 API
// 它们将运行在同一个 Node.js 进程中，但监听不同的端口，从而节省资源。

const path = require('path');

console.log('正在启动双音乐 API 服务...');

const ncmPort = Number.parseInt(process.env.NCM_PORT || '30490', 10);
const qqPort = Number.parseInt(process.env.QQ_PORT || process.env.PORT || '3200', 10);

// --- 启动网易云音乐 API (Port: 30490) ---
try {
  console.log('-> 正在加载网易云音乐 API...');
  const musicApiDir = path.join(__dirname, 'music-api');
  process.env.NCM_PORT = String(ncmPort);
  require(path.join(musicApiDir, 'start_server.js'));
} catch (err) {
  console.error('启动网易云音乐 API 失败:', err);
}

// --- 启动 QQ 音乐 API (Port: 3200) ---
try {
  console.log('-> 正在加载 QQ 音乐 API...');
  const qqMusicApiDir = path.join(__dirname, 'music-api', 'QQMusicApi');
  
  // 设置端口
  process.env.PORT = String(qqPort);
  
  // 直接引入 bin/www 启动脚本
  require(path.join(qqMusicApiDir, 'bin', 'www'));
  
} catch (err) {
  console.error('启动 QQ 音乐 API 失败:', err);
}

console.log('双 API 服务启动指令已发送!');
console.log('请等待各个服务输出启动成功日志...');
