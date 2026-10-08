import Foundation
import FenliuCore

@MainActor
@main
struct FenliuMacChecks {
    static func main() throws {
        try parsesOfficialStationDictionaryRecords()
        try rejectsMalformedStationDictionary()
        try planStorePersistsCandidatesLocally()
        print("All FenliuMac checks passed.")
    }

    @MainActor
    static func parsesOfficialStationDictionaryRecords() throws {
        let payload = "var station_names = '@bji|北京|BJP|beijing|bj|2|0357|北京|||@sha|上海|SHH|shanghai|sh|20|0712|上海|||';"
        guard let stations = StationDirectory.parseStationNames(Data(payload.utf8)) else {
            throw CheckFailure(description: "Official station data should parse")
        }

        try expect(stations == [
            Station(name: "北京", telecode: "BJP", pinyin: "beijing", abbreviation: "bj"),
            Station(name: "上海", telecode: "SHH", pinyin: "shanghai", abbreviation: "sh")
        ], "Parsed station records should retain their official codes")
    }

    @MainActor
    static func rejectsMalformedStationDictionary() throws {
        try expect(
            StationDirectory.parseStationNames(Data("no station data".utf8)) == nil,
            "Malformed station data should be rejected"
        )
    }

    @MainActor
    static func planStorePersistsCandidatesLocally() throws {
        let fileURL = FileManager.default.temporaryDirectory
            .appendingPathComponent("fenliu-test-\(UUID().uuidString)")
            .appendingPathComponent("plans.json")
        defer { try? FileManager.default.removeItem(at: fileURL.deletingLastPathComponent()) }

        let store = PlanStore(fileURL: fileURL)
        let candidate = TripCandidate(
            fromStation: "北京",
            toStation: "上海",
            travelDate: Date(timeIntervalSince1970: 1_800_000_000),
            trains: "G1,G3",
            seats: "二等座",
            passengers: 2,
            priority: 3
        )
        let planID = store.createPlan(title: "国庆出行", candidate: candidate)
        store.addCandidate(
            TripCandidate(fromStation: "北京南", toStation: "上海虹桥", travelDate: candidate.travelDate),
            to: planID
        )
        store.updateStatus(.waitlisted, candidateID: candidate.id, in: planID)

        let reopenedStore = PlanStore(fileURL: fileURL)
        guard let reopenedPlan = reopenedStore.plans.first else {
            throw CheckFailure(description: "Saved plan should be present after reopening")
        }

        try expect(reopenedPlan.title == "国庆出行", "Plan title should persist")
        try expect(reopenedPlan.candidates.count == 2, "All candidates should persist")
        try expect(reopenedPlan.candidates.first?.status == .waitlisted, "Candidate status should persist")
        try expect(reopenedPlan.candidates.first?.passengers == 2, "Passenger count should persist")
    }
}

private func expect(_ condition: Bool, _ message: String) throws {
    guard condition else { throw CheckFailure(description: message) }
}

private struct CheckFailure: Error, CustomStringConvertible {
    let description: String
}
