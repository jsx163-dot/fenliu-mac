import Foundation
import Combine

public struct Station: Codable, Hashable, Identifiable {
    public var name: String
    public var telecode: String
    public var pinyin: String
    public var abbreviation: String

    public var id: String { telecode }

    public init(name: String, telecode: String, pinyin: String, abbreviation: String) {
        self.name = name
        self.telecode = telecode
        self.pinyin = pinyin
        self.abbreviation = abbreviation
    }
}

@MainActor
public final class StationDirectory: ObservableObject {
    @Published public private(set) var stations: [Station] = []
    @Published public private(set) var isRefreshing = false
    @Published public private(set) var lastRefreshFailed = false

    private let cacheURL: URL
    private let sourceURL = URL(string: "https://kyfw.12306.cn/otn/resources/js/framework/station_name.js")!

    public init(cacheURL: URL? = nil) {
        if let cacheURL {
            self.cacheURL = cacheURL
        } else {
            let supportDirectory = FileManager.default.urls(
                for: .applicationSupportDirectory,
                in: .userDomainMask
            )[0].appendingPathComponent("FenliuMac", isDirectory: true)
            self.cacheURL = supportDirectory.appendingPathComponent("stations.json")
        }
        loadCache()
        Task { await refresh() }
    }

    public func search(_ text: String) -> [Station] {
        let query = text.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard query.count >= 1 else { return [] }
        return stations.lazy
            .filter {
                $0.name.localizedStandardContains(query)
                    || $0.pinyin.localizedStandardContains(query)
                    || $0.abbreviation.localizedStandardContains(query)
                    || $0.telecode.localizedStandardContains(query)
            }
            .prefix(8)
            .map { $0 }
    }

    public func telecode(for stationName: String) -> String? {
        stations.first { $0.name == stationName }?.telecode
    }

    public func refresh() async {
        guard !isRefreshing else { return }
        isRefreshing = true
        defer { isRefreshing = false }

        do {
            var request = URLRequest(url: sourceURL)
            request.timeoutInterval = 20
            request.setValue("FenliuMac/1.0", forHTTPHeaderField: "User-Agent")
            let (data, response) = try await URLSession.shared.data(for: request)
            guard let response = response as? HTTPURLResponse,
                  (200..<300).contains(response.statusCode),
                  let parsed = Self.parseStationNames(data), !parsed.isEmpty
            else {
                lastRefreshFailed = true
                return
            }

            stations = parsed
            lastRefreshFailed = false
            saveCache(parsed)
        } catch {
            lastRefreshFailed = true
        }
    }

    public static func parseStationNames(_ data: Data) -> [Station]? {
        guard let source = String(data: data, encoding: .utf8),
              let firstQuote = source.firstIndex(of: "'"),
              let lastQuote = source.lastIndex(of: "'"),
              firstQuote < lastQuote
        else { return nil }

        let payload = source[source.index(after: firstQuote)..<lastQuote]
        var result: [Station] = []
        var seen = Set<String>()

        for record in payload.split(separator: "@", omittingEmptySubsequences: true) {
            let fields = record.split(separator: "|", omittingEmptySubsequences: false)
            guard fields.count >= 5 else { continue }
            let name = String(fields[1])
            let telecode = String(fields[2])
            guard !name.isEmpty, !telecode.isEmpty, seen.insert(telecode).inserted else { continue }
            result.append(Station(
                name: name,
                telecode: telecode,
                pinyin: String(fields[3]),
                abbreviation: String(fields[4])
            ))
        }
        return result
    }

    private func loadCache() {
        guard let data = try? Data(contentsOf: cacheURL),
              let cached = try? JSONDecoder().decode([Station].self, from: data)
        else { return }
        stations = cached
    }

    private func saveCache(_ stations: [Station]) {
        do {
            try FileManager.default.createDirectory(
                at: cacheURL.deletingLastPathComponent(),
                withIntermediateDirectories: true
            )
            let data = try JSONEncoder().encode(stations)
            try data.write(to: cacheURL, options: .atomic)
        } catch {
            lastRefreshFailed = true
        }
    }
}
