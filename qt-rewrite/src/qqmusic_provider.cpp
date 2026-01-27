#include "qqmusic_provider.h"
#include "json_utils.h"
#include "logger.h"
#include <QUrlQuery>
#include <QJsonObject>
#include <QJsonDocument>

namespace App
{

QQMusicProvider::QQMusicProvider(HttpClient *httpClient, const QUrl &baseUrl, QObject *parent)
	: IProvider(parent)
	, client(httpClient)
	, apiBase(baseUrl)
{
}

void QQMusicProvider::setCookie(const QString &cookie)
{
	m_cookie = cookie;
}

QString QQMusicProvider::cookie() const
{
	return m_cookie;
}

QUrl QQMusicProvider::buildUrl(const QString &path, const QList<QPair<QString, QString>> &query) const
{
	QUrl url = apiBase;
	url.setPath(path);
	QUrlQuery q;
	for (const auto &pair : query)
	{
		q.addQueryItem(pair.first, pair.second);
	}
	url.setQuery(q);
	return url;
}

QSharedPointer<RequestToken> QQMusicProvider::loginQrKey(const LoginQrKeyCallback &callback)
{
	HttpRequestOptions opts;
	opts.url = buildUrl("/login/qr/qq/key", {});
    Logger::info("QQMusicProvider: Requesting QR key from " + opts.url.toString());

	return client->sendWithRetry(opts, 2, 500, [this, callback](Result<HttpResponse> result) {
		if (!result.ok)
		{
            Logger::error("QQMusicProvider: Failed to get QR key: " + result.error.message);
			callback(Result<LoginQrKey>::failure(result.error));
			return;
		}

        Logger::info("QQMusicProvider: Received QR key response: " + QString::fromUtf8(result.value.body));

		QJsonParseError error;
		QJsonDocument doc = QJsonDocument::fromJson(result.value.body, &error);
		if (error.error != QJsonParseError::NoError)
		{
            Logger::error("QQMusicProvider: JSON parse error: " + error.errorString());
			callback(Result<LoginQrKey>::failure(Error{ErrorCategory::Parser, -1, "JSON parse error", ""}));
			return;
		}

		QJsonObject root = doc.object();
		int code = root.value("result").toInt();
		if (code != 100)
		{
            Logger::error("QQMusicProvider: Upstream error code: " + QString::number(code) + ", msg: " + root.value("errMsg").toString());
			callback(Result<LoginQrKey>::failure(Error{ErrorCategory::UpstreamChange, code, root.value("errMsg").toString(), ""}));
			return;
		}

		QJsonObject data = root.value("data").toObject();
		LoginQrKey key;
		key.unikey = data.value("qrsig").toString();
        
        // Store image for next step
        m_qrImages[key.unikey] = data.value("image").toString();
        Logger::info("QQMusicProvider: Stored QR image for key: " + key.unikey + ", image length: " + QString::number(m_qrImages[key.unikey].length()));

		callback(Result<LoginQrKey>::success(key));
	});
}

QSharedPointer<RequestToken> QQMusicProvider::loginQrCreate(const QString &key, const LoginQrCreateCallback &callback)
{
    Logger::info("QQMusicProvider: Creating QR for key: " + key);
    auto token = QSharedPointer<RequestToken>::create();
    
    // Return cached image immediately
    if (m_qrImages.contains(key)) {
        Logger::info("QQMusicProvider: Found cached image for key");
        LoginQrCreate create;
        create.qrImg = m_qrImages[key];
        create.qrUrl = ""; // No URL for QQ, just image
        callback(Result<LoginQrCreate>::success(create));
    } else {
        Logger::error("QQMusicProvider: QR image not found for key: " + key);
        callback(Result<LoginQrCreate>::failure(Error{ErrorCategory::Unknown, -1, "QR image not found for key", ""}));
    }
    
    return token;
}

QSharedPointer<RequestToken> QQMusicProvider::loginQrCheck(const QString &key, const LoginQrCheckCallback &callback)
{
	HttpRequestOptions opts;
	opts.url = buildUrl("/login/qr/qq/check", {{"qrsig", key}});

	return client->sendWithRetry(opts, 2, 500, [callback](Result<HttpResponse> result) {
		if (!result.ok)
		{
			callback(Result<LoginQrCheck>::failure(result.error));
			return;
		}

		QJsonParseError error;
		QJsonDocument doc = QJsonDocument::fromJson(result.value.body, &error);
		if (error.error != QJsonParseError::NoError)
		{
			callback(Result<LoginQrCheck>::failure(Error{ErrorCategory::Parser, -1, "JSON parse error", ""}));
			return;
		}

		QJsonObject root = doc.object();
		int resultParams = root.value("result").toInt();

		LoginQrCheck check;
		if (resultParams == 100)
		{
			check.code = 803; // Success
			check.message = "Login success";
            
            QJsonObject data = root.value("data").toObject();
            QStringList cookies;
            for (auto it = data.begin(); it != data.end(); ++it) {
                cookies.append(it.key() + "=" + it.value().toString());
            }
            check.cookie = cookies.join("; ");
		}
		else if (resultParams == 101)
		{
			check.code = 801; // Waiting
			check.message = root.value("message").toString();
		}
		else if (resultParams == 102)
		{
			check.code = 802; // Confirming
			check.message = root.value("message").toString();
		}
		else if (resultParams == 103)
		{
			check.code = 800; // Expired
			check.message = root.value("message").toString();
		}
		else
		{
			callback(Result<LoginQrCheck>::failure(Error{ErrorCategory::UpstreamChange, resultParams, root.value("errMsg").toString(), ""}));
			return;
		}

		callback(Result<LoginQrCheck>::success(check));
	});
}

}
