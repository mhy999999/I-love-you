const axios = require('axios');
const getSign = require('../util/sign');

function hash33(str, h = 0) {
  let hash = BigInt(h);
  for (let i = 0; i < str.length; i++) {
    hash = (hash << 5n) + hash + BigInt(str.charCodeAt(i));
  }
  return Number(hash & 2147483647n);
}

function uuid4() {
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
    const r = Math.random() * 16 | 0;
    const v = c === 'x' ? r : (r & 0x3 | 0x8);
    return v.toString(16);
  });
}

function parseCookies(headers) {
  const cookies = {};
  const setCookie = headers['set-cookie'];
  if (setCookie) {
    setCookie.forEach((cookie) => {
      const parts = cookie.split(';');
      const pair = parts[0].split('=');
      const key = pair[0].trim();
      const value = pair[1] ? pair[1].trim() : '';
      cookies[key] = value;
    });
  }
  return cookies;
}

function cookiesToString(cookies) {
  return Object.keys(cookies)
    .map((k) => `${k}=${cookies[k]}`)
    .join('; ');
}

const login = {
  '/phone/send': async ({req, res, request}) => {
    const {phone, country_code = 86} = req.query;
    if (!phone) {
      return res.send({result: 500, errMsg: 'phone is required'});
    }

    const data = {
      req1: {
        module: 'music.login.LoginServer',
        method: 'SendPhoneAuthCode',
        param: {
          tmeAppid: 'qqmusic',
          phoneNo: String(phone),
          areaCode: String(country_code),
        },
      },
    };

    const sign = getSign(data);
    const url = `https://u6.y.qq.com/cgi-bin/musics.fcg?sign=${sign}&format=json&inCharset=utf8&outCharset=utf-8&data=${encodeURIComponent(
      JSON.stringify(data)
    )}`;

    const result = await request({url});
    if (result && result.req1 && result.req1.code === 0) {
      return res.send({result: 100, data: '验证码已发送'});
    }
    return res.send({
      result: 200,
      errMsg: result && result.req1 && result.req1.data && result.req1.data.errMsg ? result.req1.data.errMsg : '发送失败',
      raw: result,
    });
  },

  '/phone/login': async ({req, res, request}) => {
    const {phone, code, country_code = 86} = req.query;
    if (!phone || !code) {
      return res.send({result: 500, errMsg: 'phone and code are required'});
    }

    const data = {
      req1: {
        module: 'music.login.LoginServer',
        method: 'Login',
        param: {
          code: String(code),
          phoneNo: String(phone),
          areaCode: String(country_code),
          loginMode: 1,
        },
      },
    };

    const sign = getSign(data);
    const url = `https://u6.y.qq.com/cgi-bin/musics.fcg?sign=${sign}&format=json&inCharset=utf8&outCharset=utf-8&data=${encodeURIComponent(
      JSON.stringify(data)
    )}`;

    const result = await request({url});
    if (result && result.req1 && result.req1.code === 0) {
      const cookies = result.req1.data || {};
      Object.keys(cookies).forEach((k) => {
        res.cookie(k, cookies[k], {maxAge: 86400000});
      });
      return res.send({result: 100, data: cookies});
    }
    return res.send({
      result: 200,
      errMsg: result && result.req1 && result.req1.data && result.req1.data.errMsg ? result.req1.data.errMsg : '登录失败',
      raw: result,
    });
  },

  '/qr/qq/key': async ({req, res}) => {
    try {
      const url = 'https://ssl.ptlogin2.qq.com/ptqrshow';
      const params = {
        appid: '716027609',
        e: '2',
        l: 'M',
        s: '3',
        d: '72',
        v: '4',
        t: Math.random(),
        daid: '383',
        pt_3rd_aid: '100497308',
      };

      const response = await axios.get(url, {
        params,
        responseType: 'arraybuffer',
        headers: {'Referer': 'https://xui.ptlogin2.qq.com/'},
      });

      const cookies = parseCookies(response.headers);
      const qrsig = cookies['qrsig'];
      const imageBase64 = Buffer.from(response.data, 'binary').toString('base64');

      return res.send({
        result: 100,
        data: {
          qrsig,
          image: `data:image/png;base64,${imageBase64}`,
        },
      });
    } catch (e) {
      return res.send({result: 500, errMsg: e.message});
    }
  },

  '/qr/qq/check': async ({req, res, request}) => {
    const {qrsig} = req.query;
    if (!qrsig) {
      return res.send({result: 500, errMsg: 'qrsig is required'});
    }

    try {
      const checkUrl = 'https://ssl.ptlogin2.qq.com/ptqrlogin';
      const checkParams = {
        u1: 'https://graph.qq.com/oauth2.0/login_jump',
        ptqrtoken: hash33(qrsig),
        ptredirect: '0',
        h: '1',
        t: '1',
        g: '1',
        from_ui: '1',
        ptlang: '2052',
        action: `0-0-${Date.now()}`,
        js_ver: '20102616',
        js_type: '1',
        pt_uistyle: '40',
        aid: '716027609',
        daid: '383',
        pt_3rd_aid: '100497308',
        has_onekey: '1',
      };

      const checkResp = await axios.get(checkUrl, {
        params: checkParams,
        headers: {
          'Referer': 'https://xui.ptlogin2.qq.com/',
          'Cookie': `qrsig=${qrsig}`,
        },
      });

      const match = String(checkResp.data).match(/ptuiCB\((.*?)\)/);
      if (!match) {
        return res.send({result: 500, errMsg: '解析响应失败'});
      }

      const data = match[1].split(',').map((s) => s.trim().replace(/'/g, ''));
      const code = data[0];

      if (code === '66') return res.send({result: 101, message: '等待扫码'});
      if (code === '67') return res.send({result: 102, message: '正在认证'});
      if (code === '65') return res.send({result: 103, message: '二维码已过期'});

      if (code !== '0') {
        return res.send({result: 200, errMsg: `未知状态码: ${code}`, raw: data});
      }

      const urlWithSigx = data[2];
      const sigxMatch = urlWithSigx.match(/&ptsigx=(.+?)&s_url/);
      const uinMatch = urlWithSigx.match(/&uin=(.+?)&service/);
      if (!sigxMatch || !uinMatch) {
        return res.send({result: 500, errMsg: '解析 sigx 或 uin 失败'});
      }

      const sigx = sigxMatch[1];
      const uin = uinMatch[1];

      const checkSigResp = await axios.get('https://ssl.ptlogin2.graph.qq.com/check_sig', {
        params: {
          uin,
          pttype: '1',
          service: 'ptqrlogin',
          nodirect: '0',
          ptsigx: sigx,
          s_url: 'https://graph.qq.com/oauth2.0/login_jump',
          ptlang: '2052',
          ptredirect: '100',
          aid: '716027609',
          daid: '383',
          j_later: '0',
          low_login_hour: '0',
          regmaster: '0',
          pt_login_type: '3',
          pt_aid: '0',
          pt_aaid: '16',
          pt_light: '0',
          pt_3rd_aid: '100497308',
        },
        headers: {'Referer': 'https://xui.ptlogin2.qq.com/'},
        maxRedirects: 0,
        validateStatus: (s) => s >= 200 && s < 400,
      });

      const checkSigCookies = parseCookies(checkSigResp.headers);
      const p_skey = checkSigCookies['p_skey'];
      if (!p_skey) {
        return res.send({result: 500, errMsg: '获取 p_skey 失败'});
      }

      const g_tk = hash33(p_skey, 5381);
      const authResp = await axios.post(
        'https://graph.qq.com/oauth2.0/authorize',
        new URLSearchParams({
          response_type: 'code',
          client_id: '100497308',
          redirect_uri: 'https://y.qq.com/portal/wx_redirect.html?login_type=1&surl=https://y.qq.com/',
          scope: 'get_user_info,get_app_friends',
          state: 'state',
          switch: '',
          from_ptlogin: '1',
          src: '1',
          update_auth: '1',
          openapi: '1010_1030',
          g_tk: g_tk,
          auth_time: Date.now(),
          ui: uuid4(),
        }).toString(),
        {
          headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
            'Cookie': cookiesToString(checkSigCookies),
          },
          maxRedirects: 0,
          validateStatus: (s) => s >= 200 && s < 400,
        }
      );

      const location = authResp.headers['location'];
      if (!location) {
        return res.send({result: 500, errMsg: '获取 Location 失败'});
      }

      const codeMatch = location.match(/code=(.+?)&/);
      if (!codeMatch) {
        return res.send({result: 500, errMsg: '获取 auth code 失败'});
      }
      const authCode = codeMatch[1];

      const loginData = {
        req1: {
          module: 'QQConnectLogin.LoginServer',
          method: 'QQLogin',
          param: {
            code: authCode,
          },
        },
      };

      const loginSign = getSign(loginData);
      const loginUrl = `https://u6.y.qq.com/cgi-bin/musics.fcg?sign=${loginSign}&format=json&inCharset=utf8&outCharset=utf-8&data=${encodeURIComponent(
        JSON.stringify(loginData)
      )}`;

      const loginResult = await request({url: loginUrl});
      if (loginResult && loginResult.req1 && loginResult.req1.code === 0) {
        const finalCookies = loginResult.req1.data || {};
        Object.keys(finalCookies).forEach((k) => {
          res.cookie(k, finalCookies[k], {maxAge: 86400000});
        });
        return res.send({result: 100, data: finalCookies});
      }

      return res.send({
        result: 200,
        errMsg: loginResult && loginResult.req1 && loginResult.req1.data && loginResult.req1.data.errMsg ? loginResult.req1.data.errMsg : 'QQLogin 失败',
        raw: loginResult,
      });
    } catch (e) {
      return res.send({result: 500, errMsg: e.message});
    }
  },

  '/qr/wx/key': async ({req, res}) => {
    try {
      const url = 'https://open.weixin.qq.com/connect/qrconnect';
      const params = {
        appid: 'wx48db31d50e334801',
        redirect_uri: 'https://y.qq.com/portal/wx_redirect.html?login_type=2&surl=https://y.qq.com/',
        response_type: 'code',
        scope: 'snsapi_login',
        state: 'STATE',
        href: 'https://y.qq.com/mediastyle/music_v17/src/css/popup_wechat.css#wechat_redirect',
      };
      const resp = await axios.get(url, {params});
      const match = String(resp.data).match(/uuid=(.+?)"/);
      if (!match) {
        return res.send({result: 500, errMsg: '获取 uuid 失败'});
      }
      const uuid = match[1];
      const qrUrl = `https://open.weixin.qq.com/connect/qrcode/${uuid}`;

      const imgResp = await axios.get(qrUrl, {responseType: 'arraybuffer'});
      const imageBase64 = Buffer.from(imgResp.data, 'binary').toString('base64');

      return res.send({
        result: 100,
        data: {
          uuid,
          image: `data:image/jpeg;base64,${imageBase64}`,
        },
      });
    } catch (e) {
      return res.send({result: 500, errMsg: e.message});
    }
  },

  '/qr/wx/check': async ({req, res, request}) => {
    const {uuid} = req.query;
    if (!uuid) return res.send({result: 500, errMsg: 'uuid is required'});

    try {
      const url = 'https://lp.open.weixin.qq.com/connect/l/qrconnect';
      const params = {uuid, _: Date.now()};
      const resp = await axios.get(url, {params, headers: {Referer: 'https://open.weixin.qq.com/'}});

      const match = String(resp.data).match(/window\.wx_errcode=(\d+);window\.wx_code='([^']*)'/);
      if (!match) return res.send({result: 500, errMsg: '解析失败'});

      const errcode = match[1];
      const code = match[2];

      if (errcode !== '405') {
        return res.send({result: Number(errcode), message: '等待或未完成', raw: resp.data});
      }

      const data = {
        req1: {
          module: 'music.login.LoginServer',
          method: 'Login',
          param: {
            code: code,
            strAppid: 'wx48db31d50e334801',
          },
        },
      };

      const sign = getSign(data);
      const loginUrl = `https://u6.y.qq.com/cgi-bin/musics.fcg?sign=${sign}&format=json&inCharset=utf8&outCharset=utf-8&data=${encodeURIComponent(
        JSON.stringify(data)
      )}`;

      const loginResult = await request({url: loginUrl});
      if (loginResult && loginResult.req1 && loginResult.req1.code === 0) {
        const finalCookies = loginResult.req1.data || {};
        Object.keys(finalCookies).forEach((k) => {
          res.cookie(k, finalCookies[k], {maxAge: 86400000});
        });
        return res.send({result: 100, data: finalCookies});
      }
      return res.send({
        result: 200,
        errMsg: loginResult && loginResult.req1 && loginResult.req1.data && loginResult.req1.data.errMsg ? loginResult.req1.data.errMsg : 'WX Login 失败',
        raw: loginResult,
      });
    } catch (e) {
      return res.send({result: 500, errMsg: e.message});
    }
  },
};

module.exports = login;
