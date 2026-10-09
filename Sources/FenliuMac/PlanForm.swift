import AppKit
import FenliuCore

struct PlanFormResult {
    var title: String
    var value: TripCandidate
}

@MainActor
enum PlanForm {
    static func present(
        title: String,
        stations: [Station],
        planTitle: String = "",
        defaults: TripCandidate? = nil
    ) -> PlanFormResult? {
        let alert = NSAlert()
        alert.messageText = title
        alert.alertStyle = .informational
        alert.addButton(withTitle: defaults == nil ? "创建行程" : "添加备选")
        alert.addButton(withTitle: "取消")

        let form = PlanFormAccessory(stations: stations, planTitle: planTitle, defaults: defaults)
        alert.accessoryView = form.view
        guard alert.runModal() == .alertFirstButtonReturn else { return nil }
        return form.result
    }
}

@MainActor
private final class PlanFormAccessory {
    private let titleField = NSTextField()
    private let fromCombo = NSComboBox()
    private let toCombo = NSComboBox()
    private let datePicker = NSDatePicker()
    private let trainField = NSTextField()
    private let seatField = NSTextField()
    private let passengerField = NSTextField()
    private let priorityField = NSTextField()
    private let noteField = NSTextField()
    let view: NSView

    var result: PlanFormResult {
        let candidate = TripCandidate(
            fromStation: fromCombo.stringValue.trimmingCharacters(in: .whitespacesAndNewlines),
            toStation: toCombo.stringValue.trimmingCharacters(in: .whitespacesAndNewlines),
            travelDate: datePicker.dateValue,
            trains: trainField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines),
            seats: seatField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines),
            passengers: max(1, min(10, Int(passengerField.stringValue) ?? 1)),
            priority: max(1, min(5, Int(priorityField.stringValue) ?? 1)),
            note: noteField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        )
        return PlanFormResult(title: titleField.stringValue, value: candidate)
    }

    init(stations: [Station], planTitle: String, defaults: TripCandidate?) {
        let root = NSView(frame: NSRect(x: 0, y: 0, width: 510, height: defaults == nil ? 280 : 250))
        view = root
        [titleField, fromCombo, toCombo, datePicker, trainField, seatField, passengerField, priorityField, noteField]
            .forEach { $0.controlSize = .regular }

        titleField.placeholderString = "例如：国庆回家（可选）"
        titleField.stringValue = planTitle
        titleField.isHidden = defaults != nil
        configureCombo(fromCombo, stations: stations)
        configureCombo(toCombo, stations: stations)

        datePicker.datePickerStyle = .textField
        datePicker.datePickerElements = [.yearMonthDay]
        datePicker.minDate = .now
        datePicker.dateValue = defaults?.travelDate ?? (Calendar.current.date(byAdding: .day, value: 1, to: .now) ?? .now)

        trainField.placeholderString = "不限，可填 G1,G3"
        seatField.placeholderString = "不限，例如二等座"
        passengerField.placeholderString = "1"
        passengerField.stringValue = String(defaults?.passengers ?? 1)
        passengerField.widthAnchor.constraint(equalToConstant: 70).isActive = true
        priorityField.placeholderString = "1-5"
        priorityField.stringValue = String(defaults?.priority ?? 1)
        priorityField.widthAnchor.constraint(equalToConstant: 70).isActive = true
        noteField.placeholderString = "可接受邻近车站等"

        fromCombo.stringValue = defaults?.fromStation ?? ""
        toCombo.stringValue = defaults?.toStation ?? ""
        trainField.stringValue = defaults?.trains ?? ""
        seatField.stringValue = defaults?.seats ?? ""
        noteField.stringValue = defaults?.note ?? ""

        let rows: [NSView] = [
            fieldBlock("行程名称", control: titleField),
            pairedFields(("出发站", fromCombo), ("到达站", toCombo)),
            pairedFields(("乘车日期", datePicker), ("乘车人数", passengerField)),
            pairedFields(("优先车次", trainField), ("席别偏好", seatField)),
            pairedFields(("优先级 1-5", priorityField), ("备注", noteField))
        ]

        let stack = NSStackView(views: rows)
        stack.orientation = .vertical
        stack.alignment = .leading
        stack.spacing = 10
        stack.translatesAutoresizingMaskIntoConstraints = false
        root.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: root.leadingAnchor),
            stack.trailingAnchor.constraint(equalTo: root.trailingAnchor),
            stack.topAnchor.constraint(equalTo: root.topAnchor, constant: 4),
            stack.bottomAnchor.constraint(lessThanOrEqualTo: root.bottomAnchor, constant: -4)
        ])
        stack.widthAnchor.constraint(equalTo: root.widthAnchor).isActive = true
    }

    private func configureCombo(_ combo: NSComboBox, stations: [Station]) {
        combo.placeholderString = "站名或拼音"
        combo.usesDataSource = false
        combo.completes = true
        combo.addItems(withObjectValues: stations.map(\.name))
    }

    private func fieldBlock(_ title: String, control: NSView) -> NSView {
        let label = NSTextField(labelWithString: title)
        label.font = .systemFont(ofSize: 10)
        label.textColor = .secondaryLabelColor
        let stack = NSStackView(views: [label, control])
        stack.orientation = .vertical
        stack.alignment = .leading
        stack.spacing = 4
        control.widthAnchor.constraint(greaterThanOrEqualToConstant: 120).isActive = true
        return stack
    }

    private func pairedFields(_ first: (String, NSView), _ second: (String, NSView)) -> NSView {
        let firstBlock = fieldBlock(first.0, control: first.1)
        let secondBlock = fieldBlock(second.0, control: second.1)
        let row = NSStackView(views: [firstBlock, secondBlock])
        row.orientation = .horizontal
        row.alignment = .top
        row.spacing = 14
        row.distribution = .fillEqually
        firstBlock.widthAnchor.constraint(equalTo: secondBlock.widthAnchor).isActive = true
        row.widthAnchor.constraint(equalToConstant: 510).isActive = true
        return row
    }
}
