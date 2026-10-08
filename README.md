# 分流 Mac

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
