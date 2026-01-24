const server = require('netease-cloud-music-api-alger/server')
server.serveNcmApi({
  port: 30490,
  checkVersion: true,
})
console.log('Netease Cloud Music API server started on port 30490')
