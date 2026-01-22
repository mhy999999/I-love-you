// HTTP 客户端封装：统一超时、重试、取消与错误处理
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

// 请求取消令牌，用于在 UI 层主动终止正在进行的请求
class RequestToken : public QObject
{
	Q_OBJECT

public:
	explicit RequestToken(QObject *parent = nullptr);
	// 标记请求为已取消，并发出 cancelled 信号
	void cancel();
	// 查询当前请求是否已被取消
	bool isCancelled() const;

signals:
	// 当请求被取消时发出，用于通知底层中止网络操作
	void cancelled();

private:
	bool cancelledFlag = false;
};

// 单次 HTTP 请求配置
struct HttpRequestOptions
{
	QUrl url;
	QByteArray method = "GET";
	QMap<QByteArray, QByteArray> headers;
	QByteArray body;
	int timeoutMs = 15000;
};

// HTTP 响应结果（状态码 + body + 响应头）
struct HttpResponse
{
	int statusCode = 0;
	QByteArray body;
	QMap<QByteArray, QByteArray> headers;
};

// 请求完成回调，统一使用 Result<HttpResponse> 表达成功或失败
using HttpCallback = std::function<void(Result<HttpResponse>)>;

// HTTP 客户端，对 QNetworkAccessManager 进行高层封装
class HttpClient : public QObject
{
	Q_OBJECT

public:
	explicit HttpClient(QObject *parent = nullptr);

	// 设置默认请求头，将在每次请求前自动附加
	void setDefaultHeaders(const QMap<QByteArray, QByteArray> &headers);
	// 设置全局 User-Agent
	void setUserAgent(const QByteArray &ua);
	// 控制是否自动跟随重定向
	void setFollowRedirects(bool enabled);

	// 发起单次请求，不带自动重试，返回取消令牌
	QSharedPointer<RequestToken> send(const HttpRequestOptions &options, const HttpCallback &callback);
	// 发起带重试与指数退避的请求，返回取消令牌
	QSharedPointer<RequestToken> sendWithRetry(const HttpRequestOptions &options, int maxRetries, int baseDelayMs, const HttpCallback &callback);

private:
	// 底层网络访问管理器
	QNetworkAccessManager manager;
	// 默认请求头集合
	QMap<QByteArray, QByteArray> defaultHeaders;
	// 全局 User-Agent 文本
	QByteArray userAgent;
	// 是否启用自动重定向
	bool followRedirects = true;

	// 将默认头与调用方指定的头统一写入请求
	void applyHeaders(QNetworkRequest &request, const HttpRequestOptions &options);
	// 实际执行一次请求（不带重试），可被重试逻辑复用
	void sendOnce(const HttpRequestOptions &options, const QSharedPointer<RequestToken> &token, const HttpCallback &callback);
	// 判断一次请求结果是否符合重试条件
	bool isRetryable(const Result<HttpResponse> &result) const;
};

}

