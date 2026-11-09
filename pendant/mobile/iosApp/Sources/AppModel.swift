import Foundation
import Shared

/// Owns the shared Controller and mirrors its state for SwiftUI. Storage roots live in Application Support, are excluded
/// from backup and use NSFileProtectionCompleteUntilFirstUserAuthentication.
final class AppModel: ObservableObject {
    @Published var state: UiState
    let controller: Controller
    private var unsubscribe: (() -> Void)?

    init() {
        let sched = MainScheduler()
        let root = AppModel.storageRoot()
        let d = UserDefaults.standard
        let settings = Settings(demo: d.bool(forKey: "demo"), periodSec: d.object(forKey: "periodSec") as? Int32 ?? 60,
                                setTime: d.object(forKey: "setTime") as? Bool ?? true)
        let fs = PlatformFs()
        let demoRoot = root.appendingPathComponent("pendant-demo").path
        controller = Controller(
            fs: fs, realRoot: root.appendingPathComponent("pendant").path, demoRoot: demoRoot,
            sched: sched,
            makeLink: { demo in demo.boolValue ? Demo.shared.link(sched: sched, fs: fs, root: demoRoot, seed: 7) : CoreBluetoothLink() },
            utcNow: { KotlinLong(value: Int64(Date().timeIntervalSince1970 * 1000)) },
            settings: settings)
        state = controller.state.value as! UiState
        controller.load()
        unsubscribe = controller.subscribe { [weak self] s in DispatchQueue.main.async { self?.state = s } }
    }

    func save(_ s: Settings) {
        let d = UserDefaults.standard
        d.set(s.demo, forKey: "demo"); d.set(Int(s.periodSec), forKey: "periodSec"); d.set(s.setTime, forKey: "setTime")
        controller.updateSettings(s: s)
    }

    private static func storageRoot() -> URL {
        let fm = FileManager.default
        var url = try! fm.url(for: .applicationSupportDirectory, in: .userDomainMask, appropriateFor: nil, create: true)
        for name in ["pendant", "pendant-demo"] {
            var dir = url.appendingPathComponent(name)
            try? fm.createDirectory(at: dir, withIntermediateDirectories: true,
                                    attributes: [.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication])
            var v = URLResourceValues(); v.isExcludedFromBackup = true
            try? dir.setResourceValues(v)
        }
        return url
    }
}

final class MainScheduler: Scheduler {
    func nowMs() -> Int64 { Int64(ProcessInfo.processInfo.systemUptime * 1000) }
    func after(ms: Int64, fn: @escaping () -> Void) -> () -> Void {
        let w = DispatchWorkItem(block: fn)
        DispatchQueue.main.asyncAfter(deadline: .now() + .milliseconds(Int(ms)), execute: w)
        return { w.cancel() }
    }
}

extension Data {
    func kotlin() -> KotlinByteArray {
        let a = KotlinByteArray(size: Int32(count))
        for (i, b) in enumerated() { a.set(index: Int32(i), value: Int8(bitPattern: b)) }
        return a
    }
}

extension KotlinByteArray {
    func data() -> Data {
        var d = Data(count: Int(size))
        for i in 0..<Int(size) { d[i] = UInt8(bitPattern: get(index: Int32(i))) }
        return d
    }
}
