const server = require('netease-cloud-music-api-alger/server')

const port = Number.parseInt(process.env.NCM_PORT || '30490', 10)
server.serveNcmApi({
  port,
  checkVersion: true,
})
console.log(`Netease Cloud Music API server started on port ${port}`)
