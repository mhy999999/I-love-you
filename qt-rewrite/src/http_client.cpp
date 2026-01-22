#include "http_client.h"

#include "logger.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace App
{

RequestToken::RequestToken(QObject *parent)
	: QObject(parent)
{
}

void RequestToken::cancel()
{
	if (cancelledFlag)
		return;
	cancelledFlag = true;
	emit cancelled();
}

bool RequestToken::isCancelled() const
{
	return cancelledFlag;
}

HttpClient::HttpClient(QObject *parent)
	: QObject(parent)
{
	manager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

void HttpClient::setDefaultHeaders(const QMap<QByteArray, QByteArray> &headers)
{
	defaultHeaders = headers;
}

void HttpClient::setUserAgent(const QByteArray &ua)
{
	userAgent = ua;
}

void HttpClient::setFollowRedirects(bool enabled)
{
	followRedirects = enabled;
	if (followRedirects)
		manager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
	else
		manager.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
}

void HttpClient::applyHeaders(QNetworkRequest &request, const HttpRequestOptions &options)
{
	for (auto it = defaultHeaders.cbegin(); it != defaultHeaders.cend(); ++it)
		request.setRawHeader(it.key(), it.value());
	for (auto it = options.headers.cbegin(); it != options.headers.cend(); ++it)
		request.setRawHeader(it.key(), it.value());
	if (!userAgent.isEmpty())
		request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
}

QSharedPointer<RequestToken> HttpClient::send(const HttpRequestOptions &options, const HttpCallback &callback)
{
	QSharedPointer<RequestToken> token = QSharedPointer<RequestToken>::create();
	sendOnce(options, token, callback);
	return token;
}

QSharedPointer<RequestToken> HttpClient::sendWithRetry(const HttpRequestOptions &options, int maxRetries, int baseDelayMs, const HttpCallback &callback)
{
	QSharedPointer<RequestToken> token = QSharedPointer<RequestToken>::create();
	QSharedPointer<bool> finished = QSharedPointer<bool>::create(false);
	QSharedPointer<int> attempt = QSharedPointer<int>::create(0);
	auto retryFn = QSharedPointer<std::function<void()>>::create();
	*retryFn = [this, options, maxRetries, baseDelayMs, callback, token, finished, attempt, retryFn]() {
		if (*finished)
			return;
		if (token->isCancelled())
		{
			if (!*finished)
			{
				*finished = true;
				Error e;
				e.category = ErrorCategory::Network;
				e.code = -2;
				e.message = QStringLiteral("Request cancelled");
				callback(Result<HttpResponse>::failure(e));
			}
			return;
		}
		sendOnce(options, token, [this, maxRetries, baseDelayMs, callback, token, finished, attempt, retryFn](Result<HttpResponse> result) {
			if (!isRetryable(result) || *attempt >= maxRetries || token->isCancelled())
			{
				if (*finished)
					return;
				*finished = true;
				callback(result);
				return;
			}
			(*attempt)++;
			int base = baseDelayMs > 0 ? baseDelayMs : 0;
			qint64 factor = 1;
			for (int i = 1; i < *attempt; ++i)
			{
				factor *= 2;
				if (factor > 16)
				{
					factor = 16;
					break;
				}
			}
			int delay = base * static_cast<int>(factor);
			QTimer::singleShot(delay, this, [retryFn]() {
				(*retryFn)();
			});
		});
	};
	(*retryFn)();
	return token;
}

void HttpClient::sendOnce(const HttpRequestOptions &options, const QSharedPointer<RequestToken> &token, const HttpCallback &callback)
{
	if (!options.url.isValid())
	{
		Error e;
		e.category = ErrorCategory::Network;
		e.code = -1;
		e.message = QStringLiteral("Invalid URL");
		callback(Result<HttpResponse>::failure(e));
		return;
	}

	if (token && token->isCancelled())
	{
		Error e;
		e.category = ErrorCategory::Network;
		e.code = -2;
		e.message = QStringLiteral("Request cancelled");
		callback(Result<HttpResponse>::failure(e));
		return;
	}

	QNetworkRequest request(options.url);
	request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, followRedirects);
	applyHeaders(request, options);

	QNetworkReply *reply = nullptr;
	const QByteArray method = options.method.isEmpty() ? QByteArray("GET") : options.method.toUpper();
	if (method == "POST")
		reply = manager.post(request, options.body);
	else if (method == "PUT")
		reply = manager.put(request, options.body);
	else
		reply = manager.get(request);

	int timeoutMs = options.timeoutMs > 0 ? options.timeoutMs : 15000;
	QTimer *timer = new QTimer(reply);
	timer->setSingleShot(true);
	QObject::connect(timer, &QTimer::timeout, reply, [reply]() {
		if (reply->isRunning())
			reply->abort();
	});
	timer->start(timeoutMs);

	if (token)
	{
		QObject::connect(token.data(), &RequestToken::cancelled, reply, [reply]() {
			if (reply->isRunning())
				reply->abort();
		});
	}

	QObject::connect(reply, &QNetworkReply::finished, reply, [reply, timer, callback]() {
		timer->stop();

		HttpResponse response;
		response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const auto headerList = reply->rawHeaderList();
		for (const QByteArray &name : headerList)
			response.headers.insert(name, reply->rawHeader(name));
		response.body = reply->readAll();

		if (reply->error() != QNetworkReply::NoError)
		{
			Error e;
			e.category = ErrorCategory::Network;
			e.code = static_cast<int>(reply->error());
			e.message = reply->errorString();
			e.detail = QString::number(response.statusCode);
			callback(Result<HttpResponse>::failure(e));
		}
		else
		{
			callback(Result<HttpResponse>::success(response));
		}

		reply->deleteLater();
	});
}

bool HttpClient::isRetryable(const Result<HttpResponse> &result) const
{
	if (result.ok)
	{
		const HttpResponse &resp = result.value;
		return resp.statusCode >= 500 && resp.statusCode < 600;
	}
	const Error &e = result.error;
	if (e.category != ErrorCategory::Network)
		return false;
	int code = e.code;
	if (code == static_cast<int>(QNetworkReply::TimeoutError))
		return true;
	if (code == static_cast<int>(QNetworkReply::TemporaryNetworkFailureError))
		return true;
	if (code == static_cast<int>(QNetworkReply::UnknownNetworkError))
		return true;
	if (code == static_cast<int>(QNetworkReply::NetworkSessionFailedError))
		return true;
	return false;
}

}
