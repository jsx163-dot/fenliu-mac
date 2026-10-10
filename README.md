# 分流购票app

当前默认使用基于 [云映](https://github.com/zezeez/12306-yunying) 的本地自用版，源码固定在 `61650343505309234ed226ca76a240644df39b05`，位于 `Vendor/Yunying`。旧 Swift 行程工作台仍保留，数据与新版本互不覆盖。两个版本是独立应用，不自动迁移登录信息或行程。

主窗口底部保留“赞助开发”栏和明确标注的广告展示位，常规查询与智能备选共用。当前尚未配置实际收款码或广告素材，仅显示本地占位，不加载第三方内容、广告 SDK 或追踪服务，不自动打开外部网站。改名不改变既有配置命名空间、Bundle ID 或钥匙串服务标识。

界面采用统一浅色样式，登录、设置和起售查询位于顶部；常规抢票偏好、运行记录与网络时延使用紧凑底栏，智能备选页不显示无关的旧控制面板。图标来自 Lucide，随应用本地打包并保留许可证。支付窗口统一原生外框，支付机构网页保持其原始界面。

## 云映本地版

保留上游的密码/扫码登录、余票查询、候选车次/席别/乘客、定时查询、自动提交订单、候补及通知功能。登录密码存入 macOS 钥匙串，仅本机使用、不通过 iCloud 同步；取消“记住密码”会删除本应用保存的登录项。配置与缓存通过 Qt 的 `QSettings` / `QStandardPaths` 写在本机，使用独立的 `FenliuLocal/FenliuYunying` 命名空间。

购票请求由本机直连 12306，不经过自有后端或应用代理。支付 HTML 使用应用内 WKWebView，支付网站由对应支付机构处理；不自动启动外部浏览器或支付 App，不自动付款。没有作者服务器的启动更新请求或第三方公网 IP 查询。时间同步、邮件与微信通知默认关闭；手动开启这些可选功能会连接所配置的第三方服务，通知可能包含行程和订单信息。邮件的 SMTP 密码与微信推送 key 仍沿用上游本地存储方式，当前建议保持通知关闭。

CDN 源码保留但不启用：上游需要改版 Qt 的 `setIpAddress`，标准 Qt 无此接口。没有添加验证码、排队或限流绕过。自动下单沿用上游内部接口，接口是否仍可用必须实测，不能保证抢票成功。

### 智能备选

主窗口的“智能备选”视图支持城市或指定车站、可排除的同城站点、席别优先顺序、全部乘客的总预算与出发时间范围。城市归属读取官方车站字典，不用站名猜测；卧铺方案默认软卧、硬卧，允许同车拆两段，整程方案优先。每段必须满足全部选中乘客人数，不拆开同行乘客。第一版没有换乘、三段票、短买后补票、自动付款或云端 AI。

搜索使用本机直连的只读接口，与原抢票回调隔离，串行查询间隔至少三秒，查询失败或验证要求会暂停。结果逐步出现；未检查的组合不代表无票，报价缺失的方案不能下单。一次搜索为一轮，搜索完成后可重新查询，不保证覆盖全球最优方案。

确认完整行程后才会重新核实并逐段下单。第一段待付款时不会购买第二段；点击“支付本段订单”在应用内打开对应支付页面，付款后点击“核实订单并继续”。只有官方订单明确确认已支付，才会核实并推进下一段。报价或本段席别变化时，可以通过“重新确认余程”明确接受原允许席别内的新方案，已支付的前段不会重买。分段不能同时锁票，可能换铺或只买到一段；程序不自动退票、取消、改买其他路线或重试结果不明确的订单。

偏好及必要任务进度仅保存本机，不额外保存身份证、登录凭证或购票令牌。重启后需重新选择原乘客并核实订单，不自动恢复下单。新流程不调用 SMTP、Server酱、作者更新或云端服务；付款仍需访问用户选择的支付机构。旧流程的可选通知和既有 Cookie 缓存行为未在此功能中改写。

所有自动化检查使用假数据与隔离配置。真实登录、下单、订单状态和支付接口仍需用户在应用中验证；官方字段不完整或状态不明确时会停止自动推进，不承诺出票成功。

### 构建与检查

兼容构建目标为 **Intel：macOS 10.15+；Apple Silicon：macOS 11+**。Apple Silicon 没有 macOS 10.x；更早的 Intel 10.x 不在本轮兼容目标内。使用 Qt 官方 6.2.4 双架构 SDK，不使用按当前系统编译的 Homebrew Qt。此版本 Qt 已较旧，不能视为仍有安全维护的最新运行库。

需要 Command Line Tools、Python 3.9+ 和 Qt 官方 SDK。通过 aqtinstall 获取 SDK 并打包：

```sh
python3 -m venv .build-yunying/aqt-venv
.build-yunying/aqt-venv/bin/pip install aqtinstall==3.3.0
.build-yunying/aqt-venv/bin/aqt install-qt mac desktop 6.2.4 clang_64 \
  -O .build-yunying/qt-compat -m qtcharts qtmultimedia \
  --archives qtbase qtcharts qtmultimedia qtsvg qttools
QT_BIN="$PWD/.build-yunying/qt-compat/6.2.4/macos/bin" sh scripts/package-yunying.sh
open "dist/分流购票app.app"
```

默认构建包含 `x86_64` 和 `arm64` 的 Universal 应用，也可设置 `MAC_ARCHS=x86_64` 或 `MAC_ARCHS=arm64` 生成单架构包。`MAC_SDK` 可指定本机已安装的 SDK 名称，SDK 版本不等于最低运行系统版本。打包脚本在构建前检查 QtCore，打包后检查所有 Mach-O 文件的架构、最低系统版本及应用内依赖，拒绝虚假降低 plist 要求或依赖本机 Homebrew 库的包。

推荐使用 Xcode 13/14 的工具链和 SDK。过新的 SDK 可能移除 Qt 6.2 需要的 AGL 框架，新的 libc++ 也可能不再支持 10.15。本机如已有旧 SDK，可用 `MAC_SDK=macosx11.1` 选择；若当前工具链缺少配套 C++ 头文件，还需通过 `MAC_CXX_HEADERS` 指定兼容的 libc++ 源码 `include` 目录（其上一级须保留 `LICENSE.TXT`，会一并打包），不能只降低版本标记忽略这些问题。

本机兼容构建使用 macOS 11.1 SDK 和 [LLVM libc++ 12.0.1 头文件](https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.1/libcxx-12.0.1.src.tar.xz)，不额外打包系统 libc++ 动态库。Qt 6.2 的部署工具通过 `scripts/compat-tools/otool` 检查单个架构以兼容其旧输出解析逻辑，之后独立校验器会检查整个应用的两个架构。

打包包含 Qt 运行库、GPLv3 和上游归属说明，使用 ad-hoc 本地签名，没有 Apple 公证，无需用户另装 Qt。上述版本是构建兼容目标，仍需对应系统实机验证。GitHub 的 `v0.4.0-local.1` 旧安装包仍为 arm64/macOS 27+，修改构建脚本不会改变已发布文件。

离线检查（使用内存模拟钥匙串，不读取真实凭证，不登录、下单或支付）：

```sh
python3 -B -m unittest discover -s Tests -p 'test_*.py' -v
QT_BIN="$PWD/.build-yunying/qt-compat/6.2.4/macos/bin" sh scripts/check-yunying.sh
```

离线检查也默认构建双架构，`TEST_ARCH=x86_64` 可在安装 Rosetta 的 Apple Silicon Mac 上运行 Intel 检查；这不能代替 macOS 10.15 实机测试。检查和打包的临时构建目录相互隔离。

本轮本机验证：兼容校验器 11/11；Universal 包及解压后的 21 个 Mach-O 文件、依赖与签名通过；仅使用随包运行库的离线副本中 MacChecks 11/11、JourneyChecks 42/42、UiChecks 9/10。剩余 UI 断言要求窗口 1440×900，但当前屏幕实际限制为 1440×895，此历史检查问题未在兼容任务中扩展修改。Intel 执行环境、macOS 10.15/11 实机和真实购票流程尚未验证，不保证出票。

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
