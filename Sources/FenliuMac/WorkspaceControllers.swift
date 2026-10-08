import AppKit
import FenliuCore
import WebKit

@MainActor
final class WorkspaceViewController: NSViewController {
    private let store: PlanStore
    private let stationDirectory: StationDirectory
    private var selected: WorkspaceSelection = .plans
    private var currentChild: NSViewController?
    private lazy var portalController = OfficialPortalViewController()
    var onCreatePlan: (() -> Void)?
    var onDataChanged: (() -> Void)?

    init(store: PlanStore, stationDirectory: StationDirectory) {
        self.store = store
        self.stationDirectory = stationDirectory
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { nil }

    override func loadView() {
        view = NSView()
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.windowBackgroundColor.cgColor
        render()
    }

    func show(_ selection: WorkspaceSelection) {
        selected = selection
        guard isViewLoaded else { return }
        render()
    }

    private func render() {
        if let currentChild {
            currentChild.view.removeFromSuperview()
            currentChild.removeFromParent()
        }

        let child: NSViewController
        switch selected {
        case .plans:
            child = PlansOverviewViewController(store: store, stationDirectory: stationDirectory)
            (child as? PlansOverviewViewController)?.onCreatePlan = { [weak self] in self?.onCreatePlan?() }
            (child as? PlansOverviewViewController)?.onOpenPlan = { [weak self] in self?.show(.plan($0)) }
            (child as? PlansOverviewViewController)?.onDeletePlan = { [weak self] planID in
                self?.store.removePlan(planID)
                self?.onDataChanged?()
                self?.render()
            }
        case .portal:
            portalController.onReturnToPlan = { [weak self] in self?.show(.plan($0)) }
            child = portalController
        case let .plan(planID):
            guard let plan = store.plans.first(where: { $0.id == planID }) else {
                selected = .plans
                render()
                return
            }
            let detail = PlanDetailViewController(plan: plan, stationDirectory: stationDirectory)
            detail.onAddCandidate = { [weak self] in self?.addCandidate(to: plan) }
            detail.onQuery = { [weak self] candidate in self?.openPortal(for: candidate, in: plan) }
            detail.onStatusChanged = { [weak self] candidate, status in
                self?.store.updateStatus(status, candidateID: candidate.id, in: plan.id)
                self?.onDataChanged?()
                self?.render()
            }
            detail.onRemoveCandidate = { [weak self] candidateID in
                self?.store.removeCandidate(candidateID, from: plan.id)
                self?.onDataChanged?()
                self?.render()
            }
            child = detail
        }

        addChild(child)
        currentChild = child
        child.view.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(child.view)
        NSLayoutConstraint.activate([
            child.view.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            child.view.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            child.view.topAnchor.constraint(equalTo: view.topAnchor),
            child.view.bottomAnchor.constraint(equalTo: view.bottomAnchor)
        ])
    }

    private func addCandidate(to plan: TravelPlan) {
        guard let form = PlanForm.present(
            title: "添加备选方案",
            stations: stationDirectory.stations,
            planTitle: plan.title,
            defaults: plan.candidates.first
        ) else { return }
        store.addCandidate(form.value, to: plan.id)
        onDataChanged?()
        render()
    }

    private func openPortal(for candidate: TripCandidate, in plan: TravelPlan) {
        store.updateStatus(.checked, candidateID: candidate.id, in: plan.id)
        portalController.setContext(PortalContext(
            planID: plan.id,
            planTitle: plan.title,
            candidate: candidate,
            fromStationCode: stationDirectory.telecode(for: candidate.fromStation),
            toStationCode: stationDirectory.telecode(for: candidate.toStation)
        ))
        onDataChanged?()
        show(.portal)
    }
}

@MainActor
private final class PlansOverviewViewController: NSViewController {
    private let store: PlanStore
    private let stationDirectory: StationDirectory
    var onCreatePlan: (() -> Void)?
    var onOpenPlan: ((UUID) -> Void)?
    var onDeletePlan: ((UUID) -> Void)?

    init(store: PlanStore, stationDirectory: StationDirectory) {
        self.store = store
        self.stationDirectory = stationDirectory
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { nil }

    override func loadView() {
        let root = NSView()
        root.wantsLayer = true
        root.layer?.backgroundColor = NSColor.windowBackgroundColor.cgColor
        view = root

        let title = NSTextField(labelWithString: "我的行程")
        title.font = .systemFont(ofSize: 22, weight: .semibold)
        let subtitle = NSTextField(labelWithString: "备选方案与本地状态")
        subtitle.font = .systemFont(ofSize: 12)
        subtitle.textColor = .secondaryLabelColor
        let create = ClosureButton(title: "新建行程", toolTip: nil) { [weak self] in self?.onCreatePlan?() }
        create.image = NSImage(systemSymbolName: "plus", accessibilityDescription: nil)
        create.imagePosition = .imageLeading
        create.bezelStyle = .rounded

        let heading = NSStackView(views: [title, NSView(), create])
        heading.orientation = .horizontal
        heading.alignment = .centerY
        heading.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(heading)

        let detail = NSStackView(views: [subtitle])
        detail.orientation = .vertical
        detail.alignment = .leading
        detail.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(detail)

        let metricText = "行程  \(store.plans.count)      备选方案  \(store.plans.reduce(0) { $0 + $1.candidates.count })      车站字典  \(stationDirectory.stations.count) 站"
        let metrics = NSTextField(labelWithString: metricText)
        metrics.font = .systemFont(ofSize: 12, weight: .medium)
        metrics.textColor = .secondaryLabelColor
        metrics.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(metrics)

        let separator = NSBox()
        separator.boxType = .separator
        separator.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(separator)

        NSLayoutConstraint.activate([
            heading.leadingAnchor.constraint(equalTo: root.leadingAnchor, constant: 28),
            heading.trailingAnchor.constraint(equalTo: root.trailingAnchor, constant: -28),
            heading.topAnchor.constraint(equalTo: root.topAnchor, constant: 28),
            detail.leadingAnchor.constraint(equalTo: heading.leadingAnchor),
            detail.topAnchor.constraint(equalTo: heading.bottomAnchor, constant: 5),
            metrics.leadingAnchor.constraint(equalTo: heading.leadingAnchor),
            metrics.topAnchor.constraint(equalTo: detail.bottomAnchor, constant: 23),
            separator.leadingAnchor.constraint(equalTo: heading.leadingAnchor),
            separator.trailingAnchor.constraint(equalTo: heading.trailingAnchor),
            separator.topAnchor.constraint(equalTo: metrics.bottomAnchor, constant: 15)
        ])

        if store.plans.isEmpty {
            let empty = NSTextField(wrappingLabelWithString: "创建一次出行，再逐步加入日期、车站和车次备选。")
            empty.font = .systemFont(ofSize: 13)
            empty.textColor = .secondaryLabelColor
            empty.alignment = .center
            empty.translatesAutoresizingMaskIntoConstraints = false
            root.addSubview(empty)
            NSLayoutConstraint.activate([
                empty.centerXAnchor.constraint(equalTo: root.centerXAnchor),
                empty.centerYAnchor.constraint(equalTo: root.centerYAnchor, constant: 18),
                empty.widthAnchor.constraint(lessThanOrEqualToConstant: 420)
            ])
        } else {
            let list = NSStackView()
            list.orientation = .vertical
            list.alignment = .leading
            list.spacing = 0
            list.translatesAutoresizingMaskIntoConstraints = false
            root.addSubview(list)
            NSLayoutConstraint.activate([
                list.leadingAnchor.constraint(equalTo: heading.leadingAnchor),
                list.trailingAnchor.constraint(equalTo: heading.trailingAnchor),
                list.topAnchor.constraint(equalTo: separator.bottomAnchor, constant: 4),
                list.bottomAnchor.constraint(lessThanOrEqualTo: root.bottomAnchor, constant: -24)
            ])

            for plan in store.plans {
                let candidate = plan.candidates.first
                let date = candidate?.travelDate.formatted(date: .abbreviated, time: .omitted) ?? "未设日期"
                let route = candidate?.routeTitle ?? "无备选方案"
                let openButton = ClosureButton(
                    title: "\(plan.title)     \(route) · \(date)     \(plan.candidates.count) 个备选",
                    toolTip: plan.title
                ) { [weak self] in self?.onOpenPlan?(plan.id) }
                openButton.alignment = .left
                openButton.isBordered = false
                openButton.bezelStyle = .regularSquare
                openButton.heightAnchor.constraint(equalToConstant: 46).isActive = true
                let deleteButton = ClosureButton(title: "", toolTip: "删除行程") { [weak self] in
                    guard let self, self.confirmDeletion() else { return }
                    self.onDeletePlan?(plan.id)
                }
                deleteButton.image = NSImage(systemSymbolName: "trash", accessibilityDescription: nil)
                deleteButton.isBordered = false
                let row = NSStackView(views: [openButton, deleteButton])
                row.orientation = .horizontal
                row.alignment = .centerY
                row.spacing = 8
                row.translatesAutoresizingMaskIntoConstraints = false
                openButton.widthAnchor.constraint(equalTo: row.widthAnchor, constant: -42).isActive = true
                row.widthAnchor.constraint(equalTo: list.widthAnchor).isActive = true
                list.addArrangedSubview(row)
                let rule = NSBox()
                rule.boxType = .separator
                list.addArrangedSubview(rule)
            }
        }
    }

    private func confirmDeletion() -> Bool {
        let alert = NSAlert()
        alert.messageText = "删除整个行程及其备选方案？"
        alert.alertStyle = .warning
        alert.addButton(withTitle: "删除")
        alert.addButton(withTitle: "取消")
        return alert.runModal() == .alertFirstButtonReturn
    }
}

@MainActor
private final class PlanDetailViewController: NSViewController {
    private let plan: TravelPlan
    private let stationDirectory: StationDirectory
    var onAddCandidate: (() -> Void)?
    var onQuery: ((TripCandidate) -> Void)?
    var onStatusChanged: ((TripCandidate, CandidateStatus) -> Void)?
    var onRemoveCandidate: ((UUID) -> Void)?

    init(plan: TravelPlan, stationDirectory: StationDirectory) {
        self.plan = plan
        self.stationDirectory = stationDirectory
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { nil }

    override func loadView() {
        let root = NSView()
        root.wantsLayer = true
        root.layer?.backgroundColor = NSColor.windowBackgroundColor.cgColor
        view = root

        let title = NSTextField(labelWithString: plan.title)
        title.font = .systemFont(ofSize: 22, weight: .semibold)
        let subtitle = NSTextField(labelWithString: "\(plan.candidates.count) 个出行备选")
        subtitle.font = .systemFont(ofSize: 12)
        subtitle.textColor = .secondaryLabelColor
        let addButton = ClosureButton(title: "添加备选", toolTip: nil) { [weak self] in self?.onAddCandidate?() }
        addButton.image = NSImage(systemSymbolName: "plus", accessibilityDescription: nil)
        addButton.imagePosition = .imageLeading
        addButton.bezelStyle = .rounded

        let heading = NSStackView(views: [title, NSView(), addButton])
        heading.orientation = .horizontal
        heading.alignment = .centerY
        heading.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(heading)

        let summary = NSStackView(views: [subtitle])
        summary.orientation = .vertical
        summary.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(summary)

        let header = NSStackView(views: [
            NSTextField(labelWithString: "日期 / 站点"),
            NSTextField(labelWithString: "车次 / 席别"),
            NSTextField(labelWithString: "人数"),
            NSTextField(labelWithString: "优先级"),
            NSTextField(labelWithString: "状态"),
            NSTextField(labelWithString: "操作")
        ])
        header.orientation = .horizontal
        header.distribution = .fillProportionally
        header.spacing = 12
        header.translatesAutoresizingMaskIntoConstraints = false
        header.subviews.compactMap { $0 as? NSTextField }.forEach {
            $0.font = .systemFont(ofSize: 10, weight: .medium)
            $0.textColor = .secondaryLabelColor
        }
        root.addSubview(header)

        let list = NSStackView()
        list.orientation = .vertical
        list.alignment = .leading
        list.spacing = 0
        list.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(list)

        let scroll = NSScrollView()
        scroll.documentView = list
        scroll.hasVerticalScroller = true
        scroll.drawsBackground = false
        scroll.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(scroll)

        let footer = NSTextField(labelWithString: "本机保存方案  ·  官方查询在应用内打开")
        footer.font = .systemFont(ofSize: 10)
        footer.textColor = .tertiaryLabelColor
        footer.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(footer)

        NSLayoutConstraint.activate([
            heading.leadingAnchor.constraint(equalTo: root.leadingAnchor, constant: 26),
            heading.trailingAnchor.constraint(equalTo: root.trailingAnchor, constant: -26),
            heading.topAnchor.constraint(equalTo: root.topAnchor, constant: 26),
            summary.leadingAnchor.constraint(equalTo: heading.leadingAnchor),
            summary.topAnchor.constraint(equalTo: heading.bottomAnchor, constant: 5),
            header.leadingAnchor.constraint(equalTo: heading.leadingAnchor),
            header.trailingAnchor.constraint(equalTo: heading.trailingAnchor),
            header.topAnchor.constraint(equalTo: summary.bottomAnchor, constant: 22),
            scroll.leadingAnchor.constraint(equalTo: heading.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: heading.trailingAnchor),
            scroll.topAnchor.constraint(equalTo: header.bottomAnchor, constant: 9),
            scroll.bottomAnchor.constraint(equalTo: footer.topAnchor, constant: -10),
            footer.leadingAnchor.constraint(equalTo: heading.leadingAnchor),
            footer.bottomAnchor.constraint(equalTo: root.bottomAnchor, constant: -16),
            list.leadingAnchor.constraint(equalTo: scroll.contentView.leadingAnchor),
            list.trailingAnchor.constraint(equalTo: scroll.contentView.trailingAnchor),
            list.topAnchor.constraint(equalTo: scroll.contentView.topAnchor),
            list.widthAnchor.constraint(equalTo: scroll.contentView.widthAnchor)
        ])

        for candidate in plan.candidates.sorted(by: { $0.priority > $1.priority }) {
            let row = CandidateRowView(candidate: candidate)
            row.onQuery = { [weak self] in self?.onQuery?(candidate) }
            row.onStatusChange = { [weak self] status in
                self?.onStatusChanged?(candidate, status)
            }
            row.onRemove = { [weak self] in
                guard let self, self.confirmDeletion() else { return }
                self.onRemoveCandidate?(candidate.id)
            }
            row.widthAnchor.constraint(equalTo: list.widthAnchor).isActive = true
            list.addArrangedSubview(row)
            let rule = NSBox()
            rule.boxType = .separator
            list.addArrangedSubview(rule)
        }
    }

    private func confirmDeletion() -> Bool {
        let alert = NSAlert()
        alert.messageText = "删除这条备选方案？"
        alert.alertStyle = .warning
        alert.addButton(withTitle: "删除")
        alert.addButton(withTitle: "取消")
        return alert.runModal() == .alertFirstButtonReturn
    }
}

@MainActor
private final class CandidateRowView: NSView {
    let candidate: TripCandidate
    var onQuery: (() -> Void)?
    var onStatusChange: ((CandidateStatus) -> Void)?
    var onRemove: (() -> Void)?

    init(candidate: TripCandidate) {
        self.candidate = candidate
        super.init(frame: .zero)
        build()
    }

    required init?(coder: NSCoder) { nil }

    private func build() {
        let date = NSTextField(labelWithString: candidate.travelDate.formatted(date: .abbreviated, time: .omitted))
        date.font = .systemFont(ofSize: 12, weight: .medium)
        let route = NSTextField(labelWithString: candidate.routeTitle)
        route.font = .systemFont(ofSize: 10)
        route.textColor = .secondaryLabelColor
        route.lineBreakMode = .byTruncatingTail
        let routeColumn = NSStackView(views: [date, route])
        routeColumn.orientation = .vertical
        routeColumn.alignment = .leading
        routeColumn.spacing = 4

        let trains = NSTextField(labelWithString: candidate.trains.isEmpty ? "车次不限" : candidate.trains)
        let seats = NSTextField(labelWithString: candidate.seats.isEmpty ? "席别不限" : candidate.seats)
        seats.font = .systemFont(ofSize: 10)
        seats.textColor = .secondaryLabelColor
        trains.lineBreakMode = .byTruncatingTail
        let preferenceColumn = NSStackView(views: [trains, seats])
        preferenceColumn.orientation = .vertical
        preferenceColumn.alignment = .leading
        preferenceColumn.spacing = 4

        let count = NSTextField(labelWithString: "\(candidate.passengers) 位")
        let priority = NSTextField(labelWithString: "P\(candidate.priority)")
        priority.font = .monospacedSystemFont(ofSize: 11, weight: .semibold)

        let status = NSPopUpButton(frame: .zero, pullsDown: false)
        CandidateStatus.allCases.enumerated().forEach { index, value in
            status.addItem(withTitle: value.rawValue)
            status.item(at: index)?.tag = index
        }
        status.selectItem(at: CandidateStatus.allCases.firstIndex(of: candidate.status) ?? 0)
        status.target = self
        status.action = #selector(statusSelected(_:))

        let query = ClosureButton(title: "在应用内查票", toolTip: nil) { [weak self] in self?.onQuery?() }
        query.image = NSImage(systemSymbolName: "magnifyingglass", accessibilityDescription: nil)
        query.imagePosition = .imageLeading
        query.controlSize = .small
        let remove = ClosureButton(title: "", toolTip: "删除备选") { [weak self] in self?.onRemove?() }
        remove.image = NSImage(systemSymbolName: "trash", accessibilityDescription: nil)
        remove.isBordered = false
        remove.controlSize = .small
        let actions = NSStackView(views: [query, remove])
        actions.orientation = .horizontal
        actions.spacing = 6

        let columns = [routeColumn, preferenceColumn, count, priority, status, actions]
        let row = NSStackView(views: columns)
        row.orientation = .horizontal
        row.alignment = .centerY
        row.distribution = .fill
        row.spacing = 12
        row.translatesAutoresizingMaskIntoConstraints = false
        addSubview(row)
        NSLayoutConstraint.activate([
            row.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 8),
            row.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -8),
            row.topAnchor.constraint(equalTo: topAnchor, constant: 9),
            row.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -9),
            heightAnchor.constraint(greaterThanOrEqualToConstant: 56)
        ])
        routeColumn.widthAnchor.constraint(equalToConstant: 176).isActive = true
        count.widthAnchor.constraint(equalToConstant: 55).isActive = true
        priority.widthAnchor.constraint(equalToConstant: 52).isActive = true
        status.widthAnchor.constraint(equalToConstant: 92).isActive = true
        actions.widthAnchor.constraint(equalToConstant: 172).isActive = true
    }

    @objc private func statusSelected(_ sender: NSPopUpButton) {
        let index = sender.indexOfSelectedItem
        guard CandidateStatus.allCases.indices.contains(index) else { return }
        onStatusChange?(CandidateStatus.allCases[index])
    }
}

@MainActor
private final class OfficialPortalViewController: NSViewController, WKNavigationDelegate, WKUIDelegate {
    private let webView = WKWebView(frame: .zero)
    private let contextLabel = NSTextField(labelWithString: "官方页面")
    private let queryButton = ClosureButton(title: "按方案查询", toolTip: "将行程填入 12306 官方查询并发起一次查询") {}
    private let statusLabel = NSTextField(labelWithString: "查询由本机直连 12306；请在官方页面完成登录或验证。")
    private var context: PortalContext?
    private var hasLoaded = false
    var onReturnToPlan: ((UUID) -> Void)?

    override func loadView() {
        let root = NSView()
        root.wantsLayer = true
        root.layer?.backgroundColor = NSColor.windowBackgroundColor.cgColor
        view = root

        let title = NSTextField(labelWithString: "12306 官方购票")
        title.font = .systemFont(ofSize: 17, weight: .semibold)
        contextLabel.font = .systemFont(ofSize: 11)
        contextLabel.textColor = .secondaryLabelColor
        contextLabel.maximumNumberOfLines = 2
        let backButton = ClosureButton(title: "返回行程", toolTip: nil) { [weak self] in
            guard let self, let id = self.context?.planID else { return }
            self.onReturnToPlan?(id)
        }
        backButton.image = NSImage(systemSymbolName: "arrow.left", accessibilityDescription: nil)
        backButton.imagePosition = .imageLeading
        let refreshButton = ClosureButton(title: "", toolTip: "刷新官方页面") { [weak self] in self?.webView.reload() }
        refreshButton.image = NSImage(systemSymbolName: "arrow.clockwise", accessibilityDescription: nil)
        refreshButton.isBordered = false
        queryButton.bezelStyle = .rounded
        queryButton.image = NSImage(systemSymbolName: "magnifyingglass", accessibilityDescription: nil)
        queryButton.imagePosition = .imageLeading
        queryButton.isEnabled = false
        queryButton.setHandler { [weak self] in self?.queryCurrentCandidate() }

        let titleBlock = NSStackView(views: [title, contextLabel])
        titleBlock.orientation = .vertical
        titleBlock.alignment = .leading
        titleBlock.spacing = 4
        let header = NSStackView(views: [titleBlock, NSView(), backButton, refreshButton, queryButton])
        header.orientation = .horizontal
        header.alignment = .centerY
        header.spacing = 12
        header.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(header)

        statusLabel.font = .systemFont(ofSize: 10)
        statusLabel.textColor = .tertiaryLabelColor
        statusLabel.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(statusLabel)

        webView.navigationDelegate = self
        webView.uiDelegate = self
        webView.allowsBackForwardNavigationGestures = true
        webView.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(webView)

        NSLayoutConstraint.activate([
            header.leadingAnchor.constraint(equalTo: root.leadingAnchor, constant: 18),
            header.trailingAnchor.constraint(equalTo: root.trailingAnchor, constant: -18),
            header.topAnchor.constraint(equalTo: root.topAnchor, constant: 12),
            webView.leadingAnchor.constraint(equalTo: root.leadingAnchor),
            webView.trailingAnchor.constraint(equalTo: root.trailingAnchor),
            webView.topAnchor.constraint(equalTo: header.bottomAnchor, constant: 10),
            webView.bottomAnchor.constraint(equalTo: statusLabel.topAnchor, constant: -8),
            statusLabel.leadingAnchor.constraint(equalTo: root.leadingAnchor, constant: 16),
            statusLabel.trailingAnchor.constraint(equalTo: root.trailingAnchor, constant: -16),
            statusLabel.bottomAnchor.constraint(equalTo: root.bottomAnchor, constant: -8)
        ])

        if !hasLoaded {
            webView.load(URLRequest(url: URL(string: "https://kyfw.12306.cn/otn/leftTicket/init")!))
            hasLoaded = true
        }
        if let context {
            contextLabel.stringValue = contextDescription(context)
        }
    }

    func setContext(_ context: PortalContext) {
        self.context = context
        if isViewLoaded {
            contextLabel.stringValue = contextDescription(context)
            queryButton.isEnabled = hasLoaded
        }
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        queryButton.isEnabled = context != nil
        if context != nil { statusLabel.stringValue = "行程已准备好；点击“按方案查询”后，将在官方页面提交一次余票查询。" }
    }

    func webView(
        _ webView: WKWebView,
        decidePolicyFor navigationAction: WKNavigationAction,
        decisionHandler: @escaping @MainActor @Sendable (WKNavigationActionPolicy) -> Void
    ) {
        guard let url = navigationAction.request.url,
              Self.is12306Host(url.host)
        else {
            decisionHandler(.cancel)
            return
        }
        decisionHandler(.allow)
    }

    func webView(
        _ webView: WKWebView,
        createWebViewWith configuration: WKWebViewConfiguration,
        for navigationAction: WKNavigationAction,
        windowFeatures: WKWindowFeatures
    ) -> WKWebView? {
        guard let url = navigationAction.request.url,
              Self.is12306Host(url.host)
        else { return nil }
        webView.load(navigationAction.request)
        return nil
    }

    private func queryCurrentCandidate() {
        guard let context else { return }
        guard let fromCode = context.fromStationCode, let toCode = context.toStationCode else {
            statusLabel.stringValue = "车站字典中找不到站码，请重试同步或选择标准站名。"
            return
        }

        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.dateFormat = "yyyy-MM-dd"
        let input = [
            "fromName": context.candidate.fromStation,
            "toName": context.candidate.toStation,
            "fromCode": fromCode,
            "toCode": toCode,
            "date": formatter.string(from: context.candidate.travelDate)
        ]

        do {
            let jsonData = try JSONSerialization.data(withJSONObject: input, options: [.fragmentsAllowed])
            guard let json = String(data: jsonData, encoding: .utf8) else { throw PortalError.encoding }
            let script = """
            (() => {
              const data = \(json);
              const setValue = (element, value) => {
                if (!element) return false;
                element.value = value;
                element.dispatchEvent(new Event('input', { bubbles: true }));
                element.dispatchEvent(new Event('change', { bubbles: true }));
                return true;
              };
              const ok = setValue(document.getElementById('fromStation'), data.fromCode)
                && setValue(document.getElementById('fromStationText'), data.fromName)
                && setValue(document.getElementById('toStation'), data.toCode)
                && setValue(document.getElementById('toStationText'), data.toName)
                && setValue(document.querySelector('[name="leftTicketDTO.train_date"]'), data.date);
              if (!ok) return 'form-not-ready';
              const button = document.getElementById('query_ticket');
              if (!button) return 'query-button-not-found';
              button.click();
              return 'query-sent';
            })();
            """
            webView.evaluateJavaScript(script) { [weak self] result, error in
                DispatchQueue.main.async {
                    if error != nil {
                        self?.statusLabel.stringValue = "官方页面未接受自动填入，请在下方页面手动输入并查询。"
                    } else if let result = result as? String, result == "query-sent" {
                        self?.statusLabel.stringValue = "已在 12306 官方页面发起查询；结果以官方页面显示为准。"
                    } else {
                        self?.statusLabel.stringValue = "官方页面结构暂不匹配，请在下方页面手动输入并查询。"
                    }
                }
            }
        } catch {
            statusLabel.stringValue = "准备查询参数失败，请在官方页面手动输入并查询。"
        }
    }

    private static func is12306Host(_ host: String?) -> Bool {
        guard let host = host?.lowercased() else { return false }
        return host == "12306.cn" || host.hasSuffix(".12306.cn")
    }

    private func contextDescription(_ context: PortalContext) -> String {
        let trains = context.candidate.trains.isEmpty ? "车次不限" : context.candidate.trains
        let seats = context.candidate.seats.isEmpty ? "席别不限" : context.candidate.seats
        return "\(context.planTitle)  ·  \(context.candidate.routeTitle)  ·  \(context.candidate.travelDate.formatted(date: .long, time: .omitted))  ·  \(trains)  ·  \(seats)  ·  \(context.candidate.passengers) 位"
    }
}

private enum PortalError: Error { case encoding }
