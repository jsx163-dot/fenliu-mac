import AppKit
import Combine
import FenliuCore

@main
struct FenliuMacEntry {
    @MainActor
    static func main() {
        let application = NSApplication.shared
        let delegate = FenliuAppDelegate()
        application.setActivationPolicy(.regular)
        application.delegate = delegate
        application.run()
        withExtendedLifetime(delegate) {}
    }
}

@MainActor
final class FenliuAppDelegate: NSObject, NSApplicationDelegate {
    private let store = PlanStore()
    private let stationDirectory = StationDirectory()
    private var windowController: MainWindowController?

    func applicationDidFinishLaunching(_ notification: Notification) {
        installMainMenu()
        let controller = MainWindowController(store: store, stationDirectory: stationDirectory)
        windowController = controller
        controller.showWindow(nil)
        controller.window?.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

    private func installMainMenu() {
        let mainMenu = NSMenu()
        let appMenuItem = NSMenuItem()
        appMenuItem.title = "分流"
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "关于分流", action: #selector(NSApplication.orderFrontStandardAboutPanel(_:)), keyEquivalent: "")
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: "退出分流", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        appMenuItem.submenu = appMenu
        mainMenu.addItem(appMenuItem)

        let editMenuItem = NSMenuItem()
        let editMenu = NSMenu(title: "编辑")
        editMenu.addItem(withTitle: "撤销", action: NSSelectorFromString("undo:"), keyEquivalent: "z")
        editMenu.addItem(withTitle: "重做", action: NSSelectorFromString("redo:"), keyEquivalent: "Z")
        editMenu.addItem(.separator())
        editMenu.addItem(withTitle: "剪切", action: #selector(NSText.cut(_:)), keyEquivalent: "x")
        editMenu.addItem(withTitle: "拷贝", action: #selector(NSText.copy(_:)), keyEquivalent: "c")
        editMenu.addItem(withTitle: "粘贴", action: #selector(NSText.paste(_:)), keyEquivalent: "v")
        editMenu.addItem(withTitle: "全选", action: #selector(NSText.selectAll(_:)), keyEquivalent: "a")
        editMenuItem.submenu = editMenu
        mainMenu.addItem(editMenuItem)
        NSApp.mainMenu = mainMenu
    }
}

@MainActor
final class MainWindowController: NSWindowController {
    private let store: PlanStore
    private let stationDirectory: StationDirectory
    private let sidebarController: SidebarViewController
    private let workspaceController: WorkspaceViewController
    private var subscriptions = Set<AnyCancellable>()

    init(store: PlanStore, stationDirectory: StationDirectory) {
        self.store = store
        self.stationDirectory = stationDirectory
        sidebarController = SidebarViewController(store: store, stationDirectory: stationDirectory)
        workspaceController = WorkspaceViewController(store: store, stationDirectory: stationDirectory)

        let splitController = NSSplitViewController()
        let sidebarItem = NSSplitViewItem(sidebarWithViewController: sidebarController)
        sidebarItem.minimumThickness = 220
        sidebarItem.maximumThickness = 290
        sidebarItem.canCollapse = false
        splitController.addSplitViewItem(sidebarItem)
        splitController.addSplitViewItem(NSSplitViewItem(viewController: workspaceController))

        let window = NSWindow(contentViewController: splitController)
        window.title = "分流"
        window.setContentSize(NSSize(width: 1180, height: 780))
        window.minSize = NSSize(width: 930, height: 620)
        window.center()
        window.toolbarStyle = .unified
        window.isReleasedWhenClosed = false

        super.init(window: window)

        sidebarController.onSelection = { [weak self] in self?.workspaceController.show($0) }
        sidebarController.onCreatePlan = { [weak self] in self?.createPlan() }
        workspaceController.onCreatePlan = { [weak self] in self?.createPlan() }
        workspaceController.onDataChanged = { [weak self] in self?.sidebarController.reload() }
        stationDirectory.$stations
            .receive(on: RunLoop.main)
            .sink { [weak sidebarController] _ in sidebarController?.reload() }
            .store(in: &subscriptions)
        store.$persistenceError
            .receive(on: RunLoop.main)
            .sink { [weak sidebarController] _ in sidebarController?.reload() }
            .store(in: &subscriptions)
        stationDirectory.$lastRefreshFailed
            .receive(on: RunLoop.main)
            .sink { [weak sidebarController] _ in sidebarController?.reload() }
            .store(in: &subscriptions)
        sidebarController.reload()
        workspaceController.show(.plans)
    }

    required init?(coder: NSCoder) { nil }

    private func createPlan() {
        guard let candidate = PlanForm.present(
            title: "新建行程",
            stations: stationDirectory.stations
        ) else { return }
        let planID = store.createPlan(title: candidate.title, candidate: candidate.value)
        sidebarController.reload()
        sidebarController.select(.plan(planID))
    }

}

@MainActor
final class SidebarViewController: NSViewController {
    private let store: PlanStore
    private let stationDirectory: StationDirectory
    private let stack = NSStackView()
    private let stationStatus = NSTextField(labelWithString: "正在同步车站…")
    var onSelection: ((WorkspaceSelection) -> Void)?
    var onCreatePlan: (() -> Void)?

    init(store: PlanStore, stationDirectory: StationDirectory) {
        self.store = store
        self.stationDirectory = stationDirectory
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) { nil }

    override func loadView() {
        let effect = NSVisualEffectView()
        effect.material = .sidebar
        effect.blendingMode = .behindWindow
        effect.state = .active

        stack.orientation = .vertical
        stack.alignment = .leading
        stack.spacing = 8
        stack.translatesAutoresizingMaskIntoConstraints = false

        let scroll = NSScrollView()
        scroll.hasVerticalScroller = true
        scroll.autohidesScrollers = true
        scroll.drawsBackground = false
        scroll.documentView = stack
        scroll.translatesAutoresizingMaskIntoConstraints = false
        effect.addSubview(scroll)
        NSLayoutConstraint.activate([
            scroll.leadingAnchor.constraint(equalTo: effect.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: effect.trailingAnchor),
            scroll.topAnchor.constraint(equalTo: effect.topAnchor),
            scroll.bottomAnchor.constraint(equalTo: effect.bottomAnchor),
            stack.leadingAnchor.constraint(equalTo: scroll.contentView.leadingAnchor, constant: 14),
            stack.trailingAnchor.constraint(equalTo: scroll.contentView.trailingAnchor, constant: -14),
            stack.topAnchor.constraint(equalTo: scroll.contentView.topAnchor, constant: 18),
            stack.widthAnchor.constraint(equalTo: scroll.contentView.widthAnchor, constant: -28),
            stack.heightAnchor.constraint(greaterThanOrEqualTo: scroll.contentView.heightAnchor)
        ])
        view = effect
        reload()
    }

    func reload() {
        guard isViewLoaded else { return }
        stack.arrangedSubviews.forEach {
            stack.removeArrangedSubview($0)
            $0.removeFromSuperview()
        }

        let title = NSTextField(labelWithString: "分流")
        title.font = .systemFont(ofSize: 22, weight: .semibold)
        title.setContentHuggingPriority(.required, for: .horizontal)
        let add = ClosureButton(title: "＋", toolTip: "新建行程") { [weak self] in self?.onCreatePlan?() }
        add.bezelStyle = .accessoryBarAction
        let header = NSStackView(views: [title, NSView(), add])
        header.orientation = .horizontal
        header.alignment = .centerY
        header.spacing = 6
        stack.addArrangedSubview(header)
        header.widthAnchor.constraint(equalTo: stack.widthAnchor).isActive = true

        addNavigationRow("我的行程", symbol: "tram.fill", selection: .plans)
        addNavigationRow("12306 购票", symbol: "ticket", selection: .portal)

        let section = NSTextField(labelWithString: "行程方案")
        section.font = .systemFont(ofSize: 11, weight: .medium)
        section.textColor = .secondaryLabelColor
        section.maximumNumberOfLines = 1
        stack.addArrangedSubview(section)

        for plan in store.plans {
            let count = plan.candidates.count
            let row = ClosureButton(
                title: "\(plan.title)\n\(count) 个备选",
                toolTip: plan.title
            ) { [weak self] in self?.onSelection?(.plan(plan.id)) }
            row.alignment = .left
            row.font = .systemFont(ofSize: 12)
            row.bezelStyle = .regularSquare
            row.isBordered = false
            row.wantsLayer = true
            row.layer?.cornerRadius = 5
            row.contentTintColor = .labelColor
            row.setContentHuggingPriority(.defaultLow, for: .horizontal)
            stack.addArrangedSubview(row)
            row.widthAnchor.constraint(equalTo: stack.widthAnchor).isActive = true
        }

        if let persistenceError = store.persistenceError {
            let error = NSTextField(wrappingLabelWithString: persistenceError)
            error.font = .systemFont(ofSize: 10)
            error.textColor = .systemRed
            stack.addArrangedSubview(error)
        }

        if stationDirectory.stations.isEmpty && stationDirectory.lastRefreshFailed {
            let retry = ClosureButton(title: "车站同步失败 · 重试", toolTip: "重新下载 12306 车站字典") { [weak self] in
                guard let self else { return }
                Task { await self.stationDirectory.refresh() }
            }
            retry.isBordered = false
            retry.alignment = .left
            retry.font = .systemFont(ofSize: 10)
            retry.contentTintColor = .secondaryLabelColor
            stack.addArrangedSubview(retry)
            retry.widthAnchor.constraint(equalTo: stack.widthAnchor).isActive = true
        } else {
            stationStatus.font = .systemFont(ofSize: 10)
            stationStatus.textColor = .tertiaryLabelColor
            stationStatus.stringValue = stationDirectory.stations.isEmpty
                ? "正在同步车站…"
                : "车站字典 · \(stationDirectory.stations.count) 站"
            stack.addArrangedSubview(stationStatus)
        }
    }

    func select(_ selection: WorkspaceSelection) {
        onSelection?(selection)
    }

    private func addNavigationRow(_ title: String, symbol: String, selection: WorkspaceSelection) {
        let button = ClosureButton(title: title, toolTip: nil) { [weak self] in
            self?.onSelection?(selection)
        }
        button.image = NSImage(systemSymbolName: symbol, accessibilityDescription: nil)
        button.imagePosition = .imageLeading
        button.alignment = .left
        button.bezelStyle = .rounded
        stack.addArrangedSubview(button)
        button.widthAnchor.constraint(equalTo: stack.widthAnchor).isActive = true
    }

}

@MainActor
final class ClosureButton: NSButton {
    private var handler: (() -> Void)?

    init(title: String, toolTip: String?, handler: @escaping () -> Void) {
        self.handler = handler
        super.init(frame: .zero)
        self.title = title
        self.toolTip = toolTip
        target = self
        action = #selector(invokeHandler)
    }

    required init?(coder: NSCoder) { nil }

    func setHandler(_ handler: @escaping () -> Void) {
        self.handler = handler
    }

    @objc private func invokeHandler() { handler?() }
}
