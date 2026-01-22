// HttpClient 实现：为网络请求提供超时、重试、取消等高级能力
#include "http_client.h"

#include "logger.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace App
{

// 取消令牌构造函数
RequestToken::RequestToken(QObject *parent)
	: QObject(parent)
{
}

// 将令牌标记为已取消，并通知绑定的请求中止
void RequestToken::cancel()
{
	if (cancelledFlag)
		return;
	cancelledFlag = true;
	emit cancelled();
}

// 查询是否已取消
bool RequestToken::isCancelled() const
{
	return cancelledFlag;
}

// 构造 HttpClient，配置默认重定向策略
HttpClient::HttpClient(QObject *parent)
	: QObject(parent)
{
	manager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

// 设置默认请求头
void HttpClient::setDefaultHeaders(const QMap<QByteArray, QByteArray> &headers)
{
	defaultHeaders = headers;
}

// 设置全局 User-Agent
void HttpClient::setUserAgent(const QByteArray &ua)
{
	userAgent = ua;
}

// 配置是否允许自动重定向
void HttpClient::setFollowRedirects(bool enabled)
{
	followRedirects = enabled;
	if (followRedirects)
		manager.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
	else
		manager.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
}

// 将默认头与请求头合并写入 QNetworkRequest
void HttpClient::applyHeaders(QNetworkRequest &request, const HttpRequestOptions &options)
{
	for (auto it = defaultHeaders.cbegin(); it != defaultHeaders.cend(); ++it)
		request.setRawHeader(it.key(), it.value());
	for (auto it = options.headers.cbegin(); it != options.headers.cend(); ++it)
		request.setRawHeader(it.key(), it.value());
	if (!userAgent.isEmpty())
		request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
}

// 发起单次请求，返回可供上层调用取消的令牌
QSharedPointer<RequestToken> HttpClient::send(const HttpRequestOptions &options, const HttpCallback &callback)
{
	QSharedPointer<RequestToken> token = QSharedPointer<RequestToken>::create();
	sendOnce(options, token, callback);
	return token;
}

// 发起带重试逻辑的请求，支持指数退避与取消
QSharedPointer<RequestToken> HttpClient::sendWithRetry(const HttpRequestOptions &options, int maxRetries, int baseDelayMs, const HttpCallback &callback)
{
	QSharedPointer<RequestToken> token = QSharedPointer<RequestToken>::create();
	QSharedPointer<bool> finished = QSharedPointer<bool>::create(false);
	QSharedPointer<int> attempt = QSharedPointer<int>::create(0);
	auto retryFn = QSharedPointer<std::function<void()>>::create();
	*retryFn = [this, options, maxRetries, baseDelayMs, callback, token, finished, attempt, retryFn]() {
		// 已经完成则不再处理
		if (*finished)
			return;
		// 若请求被取消，直接回调取消错误
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
		// 执行一次真实请求，并在回调中决定是否重试
		sendOnce(options, token, [this, maxRetries, baseDelayMs, callback, token, finished, attempt, retryFn](Result<HttpResponse> result) {
			// 不需要重试或已达上限 / 已取消，直接结束
			if (!isRetryable(result) || *attempt >= maxRetries || token->isCancelled())
			{
				if (*finished)
					return;
				*finished = true;
				callback(result);
				return;
			}
			// 增加重试次数
			(*attempt)++;
			int base = baseDelayMs > 0 ? baseDelayMs : 0;
			// 指数退避系数，最大放大到 16 倍
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

// 执行一次实际网络请求，供单次请求和重试流程复用
void HttpClient::sendOnce(const HttpRequestOptions &options, const QSharedPointer<RequestToken> &token, const HttpCallback &callback)
{
	// URL 不合法时立即返回错误，避免发送错误请求
	if (!options.url.isValid())
	{
		Error e;
		e.category = ErrorCategory::Network;
		e.code = -1;
		e.message = QStringLiteral("Invalid URL");
		callback(Result<HttpResponse>::failure(e));
		return;
	}

	// 若请求在发送前就已被取消，直接返回取消错误
	if (token && token->isCancelled())
	{
		Error e;
		e.category = ErrorCategory::Network;
		e.code = -2;
		e.message = QStringLiteral("Request cancelled");
		callback(Result<HttpResponse>::failure(e));
		return;
	}

	// 构造 QNetworkRequest 并写入请求头
	QNetworkRequest request(options.url);
	applyHeaders(request, options);

	// 根据 method 选择 GET/POST/PUT
	QNetworkReply *reply = nullptr;
	const QByteArray method = options.method.isEmpty() ? QByteArray("GET") : options.method.toUpper();
	if (method == "POST")
		reply = manager.post(request, options.body);
	else if (method == "PUT")
		reply = manager.put(request, options.body);
	else
		reply = manager.get(request);

	// 启用请求级别的超时控制
	int timeoutMs = options.timeoutMs > 0 ? options.timeoutMs : 15000;
	QTimer *timer = new QTimer(reply);
	timer->setSingleShot(true);
	QObject::connect(timer, &QTimer::timeout, reply, [reply]() {
		if (reply->isRunning())
			reply->abort();
	});
	timer->start(timeoutMs);

	// 将取消令牌与当前 reply 绑定，保证取消操作能立即中止请求
	if (token)
	{
		QObject::connect(token.data(), &RequestToken::cancelled, reply, [reply]() {
			if (reply->isRunning())
				reply->abort();
		});
	}

	// 统一处理请求完成（成功或失败）逻辑
	QObject::connect(reply, &QNetworkReply::finished, reply, [reply, timer, callback]() {
		timer->stop();

		HttpResponse response;
		response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const auto headerList = reply->rawHeaderList();
		for (const QByteArray &name : headerList)
			response.headers.insert(name, reply->rawHeader(name));
		response.body = reply->readAll();

		// 将 Qt 的网络错误转换为统一的 Error 对象
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

// 判断一次请求结果是否可以进入重试逻辑
bool HttpClient::isRetryable(const Result<HttpResponse> &result) const
{
	// 对于成功结果，仅在 5xx 时视为可重试
	if (result.ok)
	{
		const HttpResponse &resp = result.value;
		return resp.statusCode >= 500 && resp.statusCode < 600;
	}
	// 对失败结果，仅对网络类错误进行重试
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
