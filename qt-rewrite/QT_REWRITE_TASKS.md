# Qt 二次开发任务清单（含 Git 提交计划）

> 目标：基于现有项目“获取音乐”的逻辑进行二次开发，将实现语言切换为 Qt/C++，并通过更稳健的工程化手段规避旧项目的关键 bug（竞态、解析脆弱、错误处理不统一等）。

## 约定

- 分支：`qt-rewrite`
- 提交格式：`<type>(scope): <message>`，例如：`feat(provider): add kuwo search`
- 每个提交必须：可编译、可启动（至少启动到主界面或跑完核心单测）
- 提交粒度：一个提交只做一件事（新增一个模块 / 打通一条链路 / 修复一类问题）
- 里程碑标签：
  - `v0.1`：能搜 + 能播（最小闭环）
  - `v0.2`：+ 歌词/封面/歌单（体验完整）
  - `v1.0`：可打包发布

---

## 0. 准备与基准对照（让迁移不跑偏）

1. `chore(repo): init qt-rewrite branch and ignore rules`
   - 新建分支
   - 补充 `.gitignore`（Qt/CMake/构建产物）
   - 初始化基础目录（不引入业务）

2. `chore(ci): add build workflow for windows`（可选但强烈建议）
   - 配置自动构建（干净环境可编译）

3. `test(baseline): add golden cases for music fetching`
   - 建立“对照用例”文件：关键词搜索、指定 songId 获取详情、获取播放 URL、获取歌词（若支持）
   - 固化为 JSON 快照，Qt 版后续必须对齐

4. `docs(baseline): record api contracts and error taxonomy`（不想写文档可改为代码内常量）
   - 固定字段含义、可空性、错误分类（网络/解析/鉴权/上游变更）

---

## 1. Qt 工程骨架（先能跑起来）

5. `chore(qt): add CMake Qt6 app skeleton`
   - Qt6 + CMake 最小可运行应用（窗口/入口/资源）

6. `refactor(core): add shared data models`
   - 统一模型：`Song / Artist / Album / PlayUrl / Lyric / Playlist`
   - 统一 `Result<T>` / `Error`（code、message、可选 detail）

7. `feat(core): add logging and diagnostics switches`
   - 统一日志入口（可开关 debug/info/error）

---

## 2. 网络层与通用能力（把“容易出 bug 的点”集中封装）

8. `feat(net): add HttpClient wrapper with timeout and headers`
   - `QNetworkAccessManager` 封装：超时、默认 headers、gzip、UA、重定向策略

9. `feat(net): add retry and backoff policy`
   - 限定可重试错误（超时/部分 5xx）
   - 指数退避 + 重试次数上限

10. `feat(net): add request cancellation tokens`
    - UI 切换/重复搜索可取消旧请求，避免竞态导致“错歌/错歌词”

11. `feat(core): add json helpers and tolerant parsing`
    - 统一 JSON 读取工具（字段缺失/类型漂移时可诊断）

12. `feat(cache): add memory cache for provider responses`
    - 搜索/详情短期缓存，减少请求；后续再加磁盘缓存

---

## 3. Provider 接口设计（“二次开发”的核心骨架）

13. `feat(provider): define IProvider interface`
    - `search() / songDetail() / playUrl() / lyric()` 等接口签名
    - Provider 注册机制（按名称选择：如 `netease/qq/kuwo`）

14. `feat(provider): add ProviderManager and fallback strategy`
    - 同一能力多源可切换
    - 失败自动 fallback（可配置）

---

## 4. 最小闭环（v0.1：能搜到、能播）

> 建议先选旧项目里最稳定/最常用的一个源作为 MVP（网易/QQ/酷我其一），避免一开始全做导致验证困难。

15. `feat(provider-<x>): implement search`
    - 搜索返回：id、name、artists、album、duration、source

16. `feat(provider-<x>): implement song detail`
    - 详情补齐：封面、专辑信息、可选码率列表等

17. `feat(provider-<x>): implement playUrl fetch`
    - 处理跳转、签名、cookie、referer 等旧项目涉及的规则

18. `feat(player): add basic playback with QtMultimedia`
    - `QMediaPlayer`：播放/暂停/停止/进度/音量/错误回调

19. `feat(ui): add minimal UI for search and play`
    - 搜索框 + 列表 + 播放控制（最小可用）

20. `test(e2e): add smoke run for search->playUrl`
    - 关键词 → 取第一首 → 获取 playUrl 成功（至少拿到 URL）

**打标签：** `v0.1`

---

## 5. 体验补全（v0.2：歌词 / 封面 / 歌单）

21. `feat(provider-<x>): implement lyric fetch`
    - 支持纯文本 / 逐行时间戳格式，统一返回结构

22. `feat(ui): add lyric view and sync timer`
    - 播放进度驱动歌词高亮

23. `feat(provider-<x>): implement cover image fetch`
    - 封面 URL 与缓存策略（配合磁盘缓存）

24. `feat(cache): add disk cache for images and lyrics`
    - 缓存目录、容量上限、LRU 清理

25. `feat(provider): add playlist detail and tracks`（若旧项目有）
    - 歌单详情、分页拉取曲目

26. `feat(ui): add playlist page and queue import`
    - 选择歌单 → 导入播放队列

**打标签：** `v0.2`

---

## 6. 并发/稳定性专项（用工程手段消掉旧 bug）

27. `fix(net): guard against race conditions in multi-search`
    - 连续输入/快速切换源时，只展示最新请求结果

28. `fix(provider): harden parsers against upstream schema changes`
    - 字段变更时降级策略（备用字段/兜底值），错误可诊断

29. `feat(net): add rate limit and circuit breaker`
    - 被风控/频繁失败时进入冷却，避免无限重试

30. `feat(core): add metrics counters for failures`
    - 聚合统计失败类型，便于定位“接口变更 vs 网络问题”

31. `test(regression): add cases reproducing old-project bugs`
    - 每个已知 bug 一条用例：输入 → 期望输出/期望错误码

---

## 7. 多 Provider 扩展（按源逐个交付）

对每个新源建议按固定提交组推进（保持可审查性）：

- `feat(provider-<y>): implement search`
- `feat(provider-<y>): implement song detail`
- `feat(provider-<y>): implement playUrl fetch`
- `feat(provider-<y>): implement lyric fetch`（可选）
- `test(provider-<y>): add golden cases`

---

## 8. 发布与安装（v1.0）

32. `chore(release): add versioning and build artifacts`
    - 版本号、构建产物命名规则

33. `chore(release): add windeployqt packaging script`
    - 打包脚本（zip/installer 二选一）

34. `feat(app): add settings for provider order and proxy`
    - 源优先级、代理、缓存大小、日志级别

35. `test(release): add pre-release checklist runner`
    - 一键自检：编译、单测、冒烟用例、资源完整性

**打标签：** `v1.0`

