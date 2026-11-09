import SwiftUI
import Shared

func utc(_ ms: Int64?) -> String {
    guard let ms else { return "時刻不明" }
    let f = DateFormatter()
    f.locale = Locale(identifier: "en_US_POSIX"); f.timeZone = TimeZone(identifier: "UTC"); f.dateFormat = "yyyy-MM-dd HH:mm:ss'Z'"
    return f.string(from: Date(timeIntervalSince1970: Double(ms) / 1000))
}

func status(_ s: UiState) -> String {
    switch s.conn {
    case .unregistered: return "未登録"
    case .scanning: return "scan 中"
    case .connecting: return "接続中"
    case .syncing: return "同期中（\(s.accepted) 件受理）"
    case .idle: return "待機"
    case .error: return "エラー: \(s.error.map { ControllerKt.errorText(k: $0) } ?? "")"
    default: return ""
    }
}

struct ContentView: View {
    @EnvironmentObject var model: AppModel

    var body: some View {
        VStack(spacing: 0) {
            if model.state.settings.demo { Text("DEMO").frame(maxWidth: .infinity).padding(8).background(Color.red).foregroundColor(.white) }
            TabView {
                HomeView().tabItem { Label("ホーム", systemImage: "house") }
                SummaryView().tabItem { Label("要約", systemImage: "sparkles") }
                LogsView().tabItem { Label("ログ", systemImage: "text.bubble") }
                ClinicView().tabItem { Label("診療", systemImage: "cross.case") }
                PrivacyView().tabItem { Label("設定", systemImage: "gearshape") }
            }.tint(pendantTeal)
        }
    }
}

struct SyncView: View {
    @EnvironmentObject var model: AppModel
    var body: some View {
        let s = model.state
        ScrollView {
            VStack(alignment: .leading, spacing: 8) {
                Text("機器").font(.headline)
                if let d = s.device { Text("\(d.name)\n\(d.id)") } else {
                    Text("未登録")
                    Button("機器を探す") { model.controller.scanForRegistration() }
                    ForEach(s.found, id: \.id) { f in Button("登録: \(f.name) (\(f.id))") { model.controller.register(item: f) } }
                }
                Text("状態: \(status(s))").font(.headline)
                if s.error != nil { Text(s.errorDetail).font(.caption) }
                if let i = s.info {
                    Text("ペンダント: 録音中=\(i.recording ? "はい" : "いいえ") boot=\(i.boot) tail=\(i.tail) next=\(i.next) acked=\(i.acked) uptime=\(i.uptimeS)s RTC=\(i.rtcMs == 0 ? "未設定" : utc(i.rtcMs))")
                }
                Text("端末に受理済みの最大 seq: \(s.durableMax)")
                if let l = s.lastSync { Text("最後の同期: \(utc(l.atMs)) \(l.ok ? "成功" : "失敗") (\(l.text)) \(l.bytes) B") }
                if s.device != nil { Button("今すぐ同期") { model.controller.syncNow() } }
            }.padding().frame(maxWidth: .infinity, alignment: .leading)
        }
    }
}

struct ListView: View {
    @EnvironmentObject var model: AppModel
    var body: some View {
        let s = model.state
        if s.device == nil { Text("機器が登録されていません") } else {
            List(s.rows, id: \.id) { r in
                HStack(alignment: .top, spacing: 8) {
                    Text("#\(r.seq)").frame(width: 64, alignment: .leading)
                    Text(r.kind).frame(width: 64, alignment: .leading)
                    VStack(alignment: .leading) { Text(utc(r.utcMs?.int64Value)); if !r.detail.isEmpty { Text(r.detail).font(.caption) } }
                }
            }
        }
    }
}

struct SettingsView: View {
    @EnvironmentObject var model: AppModel
    @State private var confirm = false
    var body: some View {
        let st = model.state.settings
        Form {
            Toggle("demo モード（BLE を使わない）", isOn: Binding(get: { st.demo }, set: { model.save(Settings(demo: $0, periodSec: st.periodSec, setTime: st.setTime)) }))
            Picker("同期の周期", selection: Binding(get: { Int(st.periodSec) }, set: { model.save(Settings(demo: st.demo, periodSec: Int32($0), setTime: st.setTime)) })) {
                ForEach([30, 60, 300], id: \.self) { Text("\($0) s").tag($0) }
            }.pickerStyle(.segmented)
            Toggle("同期のたびに時刻を合わせる", isOn: Binding(get: { st.setTime }, set: { model.save(Settings(demo: st.demo, periodSec: st.periodSec, setTime: $0)) }))
            Button("登録した機器を忘れる") { confirm = true }.disabled(model.state.device == nil)
            Text("忘れても、その機器のログは端末に残ります。同じ機器をもう一度登録すると、続きから同期します。").font(.caption)
            Text("OTA: 未対応（後続）")
            Text("アプリ \(Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "") / shared \(model.state.sharedVersion)")
        }
        .alert("登録を解除します。ログは残ります。", isPresented: $confirm) {
            Button("忘れる", role: .destructive) { model.controller.forget() }
            Button("やめる", role: .cancel) {}
        }
    }
}
