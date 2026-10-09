# 分流 Mac

当前默认使用基于 [云映](https://github.com/zezeez/12306-yunying) 的本地自用版，源码固定在 `61650343505309234ed226ca76a240644df39b05`，位于 `Vendor/Yunying`。旧 Swift 行程工作台仍保留，数据与新版本互不覆盖。两个版本是独立应用，不自动迁移登录信息或行程。

## 云映本地版

保留上游的密码/扫码登录、余票查询、候选车次/席别/乘客、定时查询、自动提交订单、候补及通知功能。登录密码存入 macOS 钥匙串，仅本机使用、不通过 iCloud 同步；取消“记住密码”会删除本应用保存的登录项。配置与缓存通过 Qt 的 `QSettings` / `QStandardPaths` 写在本机，使用独立的 `FenliuLocal/FenliuYunying` 命名空间。

购票请求由本机直连 12306，不经过自有后端或应用代理。支付 HTML 使用应用内 WKWebView，支付网站由对应支付机构处理；不自动启动外部浏览器或支付 App，不自动付款。没有作者服务器的启动更新请求或第三方公网 IP 查询。时间同步、邮件与微信通知默认关闭；手动开启这些可选功能会连接所配置的第三方服务，通知可能包含行程和订单信息。邮件的 SMTP 密码与微信推送 key 仍沿用上游本地存储方式，当前建议保持通知关闭。

CDN 源码保留但不启用：上游需要改版 Qt 的 `setIpAddress`，标准 Qt 无此接口。没有添加验证码、排队或限流绕过。自动下单沿用上游内部接口，接口是否仍可用必须实测，不能保证抢票成功。

安装构建依赖并打包：

```sh
brew install qtbase qtcharts qtmultimedia
sh scripts/package-yunying.sh
open "dist/分流云映.app"
```

打包包含 Qt 运行库、GPLv3 和上游归属说明，使用 ad-hoc 本地签名，无需另装 Qt 即可打开。当前脚本为本机架构和本机 macOS 版本构建，不是跨系统版本/双架构发行包，也没有 Apple 公证。当前机器的 Homebrew Qt 构建要求 macOS 27。

离线检查（使用内存模拟钥匙串，不读取真实凭证，不登录、下单或支付）：

```sh
sh scripts/check-yunying.sh
```

许可证仍为 GPLv3，见 `Vendor/Yunying/LICENSE` 与 `ORIGIN.md`。本地自用不要求公开发布源码；日后分发此衍生版仍须履行适用的 GPL 义务，不能把换界面视为可闭源分发的依据。

## 旧 Swift 版

面向 macOS 的本地行程与备选方案工作台。方案数据保存在用户本机的 `~/Library/Application Support/FenliuMac/plans.json`；12306 官方购票页面使用内嵌 `WKWebView`，由用户电脑直接访问，登录状态由 WebKit 本机网站数据存储管理。

首版包含多行程、日期/站点/车次/席别候选、优先级、手工状态管理、官方车站字典同步与本地缓存，以及应用内的 12306 官方购票页面。用户点“按方案查询”后，应用尝试将当前方案填入官方页面并发起一次查询；不做后台轮询、不自动提交订单，也不绕过验证码或排队。

## 构建

需要 macOS 14+ 和 Swift 6。运行开发版：

```sh
swift run --build-system native
```

运行本地数据与车站解析检查：

```sh
swift run --build-system native FenliuMacChecks
```

打包为 ad-hoc 签名的 `.app`：

```sh
sh scripts/package-app.sh
```

## 数据边界

- 行程、偏好和状态只写入本机 Application Support 目录。
- 车站字典从 12306 官方静态资源直连下载并缓存到本机。
- 购票页面运行在应用内的 WebKit 中，导航被限制在 `12306.cn` 子域。
- 登录状态由 WebKit 本机网站数据存储管理；购票查询参数会直接发送给 12306。
- 不包含自有后端、遥测、凭证转发或余票轮询服务。

原生余票查询接口尚未验证为稳定可用，后续需先核实官方支持范围、请求限制及登录/验证流程，再决定是否增加受控查询能力。
