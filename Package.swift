// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "FenliuMac",
    platforms: [.macOS(.v14)],
    products: [
        .library(name: "FenliuCore", targets: ["FenliuCore"]),
        .executable(name: "FenliuMac", targets: ["FenliuMac"]),
        .executable(name: "FenliuMacChecks", targets: ["FenliuMacChecks"])
    ],
    targets: [
        .target(name: "FenliuCore"),
        .executableTarget(name: "FenliuMac", dependencies: ["FenliuCore"]),
        .executableTarget(
            name: "FenliuMacChecks",
            dependencies: ["FenliuCore"],
            path: "Tests/FenliuMacTests"
        )
    ]
)
