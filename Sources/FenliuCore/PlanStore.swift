import Foundation
import Combine

@MainActor
public final class PlanStore: ObservableObject {
    @Published public private(set) var plans: [TravelPlan] = []
    @Published public private(set) var persistenceError: String?

    private let fileURL: URL

    public init(fileURL: URL? = nil) {
        if let fileURL {
            self.fileURL = fileURL
        } else {
            let supportDirectory = FileManager.default.urls(
                for: .applicationSupportDirectory,
                in: .userDomainMask
            )[0].appendingPathComponent("FenliuMac", isDirectory: true)
            self.fileURL = supportDirectory.appendingPathComponent("plans.json")
        }
        load()
    }

    @discardableResult
    public func createPlan(title: String, candidate: TripCandidate) -> UUID {
        let trimmedTitle = title.trimmingCharacters(in: .whitespacesAndNewlines)
        let plan = TravelPlan(
            title: trimmedTitle.isEmpty ? candidate.routeTitle : trimmedTitle,
            candidates: [candidate]
        )
        plans.insert(plan, at: 0)
        persist()
        return plan.id
    }

    public func addCandidate(_ candidate: TripCandidate, to planID: UUID) {
        guard let index = plans.firstIndex(where: { $0.id == planID }) else { return }
        plans[index].candidates.append(candidate)
        persist()
    }

    public func updateStatus(_ status: CandidateStatus, candidateID: UUID, in planID: UUID) {
        guard let planIndex = plans.firstIndex(where: { $0.id == planID }),
              let candidateIndex = plans[planIndex].candidates.firstIndex(where: { $0.id == candidateID })
        else { return }

        plans[planIndex].candidates[candidateIndex].status = status
        persist()
    }

    public func removeCandidate(_ candidateID: UUID, from planID: UUID) {
        guard let index = plans.firstIndex(where: { $0.id == planID }) else { return }
        plans[index].candidates.removeAll { $0.id == candidateID }
        persist()
    }

    public func removePlan(_ planID: UUID) {
        plans.removeAll { $0.id == planID }
        persist()
    }

    private func load() {
        guard FileManager.default.fileExists(atPath: fileURL.path) else { return }
        do {
            let data = try Data(contentsOf: fileURL)
            plans = try JSONDecoder().decode([TravelPlan].self, from: data)
        } catch {
            persistenceError = "本地行程文件无法读取：\(error.localizedDescription)"
        }
    }

    private func persist() {
        do {
            try FileManager.default.createDirectory(
                at: fileURL.deletingLastPathComponent(),
                withIntermediateDirectories: true
            )
            let encoder = JSONEncoder()
            encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
            let data = try encoder.encode(plans)
            try data.write(to: fileURL, options: .atomic)
            persistenceError = nil
        } catch {
            persistenceError = "本地保存失败：\(error.localizedDescription)"
        }
    }
}
