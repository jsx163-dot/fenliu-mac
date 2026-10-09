# Upstream And Local Changes

- Source: https://github.com/zezeez/12306-yunying
- Branch: master
- Revision: 61650343505309234ed226ca76a240644df39b05
- Upstream version: 0.4.0
- Imported on: 2026-10-09
- License: GPL-3.0, see LICENSE; bundled libraries retain their notices.

This directory is a local-use derivative, not an independent closed-source
implementation. Keep the upstream license and attribution. Private use does
not require publishing source; distributing a derivative requires meeting
the applicable GPL obligations.

macOS adaptations:

- Independent application and data identity: com.fenliu.yunying.local.
- Login credentials use the macOS Keychain, not fixed-key SM4 storage.
  Protocol-level SM4 encoding for requests to 12306 remains unchanged.
- Payment HTML opens in an in-app, ephemeral WKWebView, not an external browser
  or a temporary HTML file. Payment redirects are restricted to HTTPS official
  railway and supported payment-provider domains. External app schemes are not
  launched automatically.
- No startup request to the author's update server. NTP synchronization defaults
  to off. No third-party public-IP lookup. Optional notifications remain opt-in.
- CDN source remains present, but HAS_CDN is disabled: upstream depends on a
  nonstandard Qt setIpAddress API. Stock Qt does not support it.
- Release builds suppress upstream debug output, which can contain account or
  order data. There is no application backend or application-managed proxy.

See the root README and scripts/package-yunying.sh for builds. Online login,
ordering, CAPTCHA/SMS verification and payment success are not covered by the
offline checks. This is not an official 12306 client, and internal interfaces
may change. No CAPTCHA, queue, or server rate-limit bypass is added.
