#pragma once

#include <QByteArray>
#include <QMap>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSharedPointer>
#include <QUrl>

#include <functional>

#include "core_types.h"

namespace App
{

class RequestToken : public QObject
{
	Q_OBJECT

public:
	explicit RequestToken(QObject *parent = nullptr);
	void cancel();
	bool isCancelled() const;

signals:
	void cancelled();

private:
	bool cancelledFlag = false;
};

struct HttpRequestOptions
{
	QUrl url;
	QByteArray method = "GET";
	QMap<QByteArray, QByteArray> headers;
	QByteArray body;
	int timeoutMs = 15000;
};

struct HttpResponse
{
	int statusCode = 0;
	QByteArray body;
	QMap<QByteArray, QByteArray> headers;
};

using HttpCallback = std::function<void(Result<HttpResponse>)>;

class HttpClient : public QObject
{
	Q_OBJECT

public:
	explicit HttpClient(QObject *parent = nullptr);

	void setDefaultHeaders(const QMap<QByteArray, QByteArray> &headers);
	void setUserAgent(const QByteArray &ua);
	void setFollowRedirects(bool enabled);

	QSharedPointer<RequestToken> send(const HttpRequestOptions &options, const HttpCallback &callback);
	QSharedPointer<RequestToken> sendWithRetry(const HttpRequestOptions &options, int maxRetries, int baseDelayMs, const HttpCallback &callback);

private:
	QNetworkAccessManager manager;
	QMap<QByteArray, QByteArray> defaultHeaders;
	QByteArray userAgent;
	bool followRedirects = true;

	void applyHeaders(QNetworkRequest &request, const HttpRequestOptions &options);
	void sendOnce(const HttpRequestOptions &options, const QSharedPointer<RequestToken> &token, const HttpCallback &callback);
	bool isRetryable(const Result<HttpResponse> &result) const;
};

}

