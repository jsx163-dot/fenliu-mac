import Foundation

public enum CandidateStatus: String, Codable, CaseIterable, Identifiable {
    case planned = "待查询"
    case checked = "已查看官网"
    case waitlisted = "已候补"
    case booked = "已购票"

    public var id: String { rawValue }
}

public struct TripCandidate: Codable, Equatable, Identifiable {
    public var id: UUID
    public var fromStation: String
    public var toStation: String
    public var travelDate: Date
    public var trains: String
    public var seats: String
    public var passengers: Int
    public var priority: Int
    public var status: CandidateStatus
    public var note: String

    public init(
        id: UUID = UUID(),
        fromStation: String,
        toStation: String,
        travelDate: Date,
        trains: String = "",
        seats: String = "",
        passengers: Int = 1,
        priority: Int = 1,
        status: CandidateStatus = .planned,
        note: String = ""
    ) {
        self.id = id
        self.fromStation = fromStation
        self.toStation = toStation
        self.travelDate = travelDate
        self.trains = trains
        self.seats = seats
        self.passengers = passengers
        self.priority = priority
        self.status = status
        self.note = note
    }

    public var routeTitle: String {
        "\(fromStation) → \(toStation)"
    }
}

public struct TravelPlan: Codable, Equatable, Identifiable {
    public var id: UUID
    public var title: String
    public var createdAt: Date
    public var candidates: [TripCandidate]

    public init(id: UUID = UUID(), title: String, createdAt: Date = .now, candidates: [TripCandidate]) {
        self.id = id
        self.title = title
        self.createdAt = createdAt
        self.candidates = candidates
    }
}

public struct PortalContext {
    public var planID: UUID
    public var planTitle: String
    public var candidate: TripCandidate
    public var fromStationCode: String?
    public var toStationCode: String?

    public init(
        planID: UUID,
        planTitle: String,
        candidate: TripCandidate,
        fromStationCode: String? = nil,
        toStationCode: String? = nil
    ) {
        self.planID = planID
        self.planTitle = planTitle
        self.candidate = candidate
        self.fromStationCode = fromStationCode
        self.toStationCode = toStationCode
    }
}

public enum WorkspaceSelection: Hashable {
    case plans
    case portal
    case plan(UUID)
}
