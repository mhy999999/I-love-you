const server = require('netease-cloud-music-api-alger/server')
const path = require('path')

async function start() {
  try {
    // Determine the path to the default modules
    const serverPath = require.resolve('netease-cloud-music-api-alger/server')
    const defaultModulePath = path.join(path.dirname(serverPath), 'module')
    
    const special = {
      'daily_signin.js': '/daily_signin',
      'fm_trash.js': '/fm_trash',
      'personal_fm.js': '/personal_fm',
    }
    
    // Load default modules
    const modules = await server.getModulesDefinitions(defaultModulePath, special)
    
    // Add custom module for /user/setCookie
    modules.push({
      identifier: 'user_setCookie',
      route: '/user/setCookie',
      module: async (query, request) => {
        const cookie = query.data || (request.body && request.body.data) || '';
        console.log('[User SetCookie] Received cookie:', cookie.substring(0, 20) + '...');
        
        return {
          status: 200,
          body: {
            code: 200,
            message: 'Cookie received and saved',
            success: true,
            profile: {
                userId: 'qq_music_user',
                nickname: 'QQ Music User',
                avatarUrl: '', 
                signature: 'Logged in via QQ Music Cookie',
                vipType: 0
            },
            cookie: cookie
          }
        }
      }
    })

    // Start the server with the combined modules
    const port = process.env.PORT || 30490
    await server.serveNcmApi({
      port: Number(port),
      checkVersion: true,
      moduleDefs: modules
    })
    
    console.log(`Netease Cloud Music API server started on port ${port} with custom /user/setCookie route`)
  } catch (error) {
    console.error('Failed to start server:', error)
  }
}

start()
