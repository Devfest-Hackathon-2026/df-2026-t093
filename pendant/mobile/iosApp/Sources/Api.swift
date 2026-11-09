import Foundation

// pendant-api（origin/server の server/）の呼び出し。Android の Api.kt と同じ動き。
// URL とトークンは Sources/Secrets.plist の ApiBase / ApiToken（Git に入れない）。無ければ固定データ（ハリボテ）のまま動く。

enum Api {
    private static let secrets = Bundle.main.url(forResource: "Secrets", withExtension: "plist")
        .flatMap { NSDictionary(contentsOf: $0) as? [String: String] } ?? [:]
    static let base = secrets["ApiBase"] ?? ""
    static let token = secrets["ApiToken"] ?? ""
    static var enabled: Bool { !base.isEmpty && !token.isEmpty }

    static func call<T: Decodable>(_ method: String, _ path: String, _ body: [String: Any]? = nil) async throws -> T {
        var r = URLRequest(url: URL(string: base.trimmingCharacters(in: CharacterSet(charactersIn: "/")) + path)!)
        r.httpMethod = method
        r.timeoutInterval = 30 // Gemini は数秒かかる。止まったら台本に切り替える
        r.setValue("Bearer \(token)", forHTTPHeaderField: "Authorization")
        if let body {
            r.httpBody = try JSONSerialization.data(withJSONObject: body)
            r.setValue("application/json", forHTTPHeaderField: "Content-Type")
        }
        let (data, resp) = try await URLSession.shared.data(for: r)
        let code = (resp as? HTTPURLResponse)?.statusCode ?? 0
        guard code < 400 else { throw ApiError(message: "HTTP \(code) " + String((String(data: data, encoding: .utf8) ?? "").prefix(150))) }
        return try JSONDecoder().decode(T.self, from: data)
    }
}

struct ApiError: LocalizedError {
    let message: String
    var errorDescription: String? { message }
}

private struct LineDTO: Decodable { let time, who, text: String }
private struct DayDTO: Decodable {
    let date: String
    let summary: String?
    let audio_codec: String?
    let audio_bitrate_kbps, audio_duration_sec, audio_size_bytes: Int?
    let lines: [LineDTO]
}
private struct SettingsDTO: Decodable { let cloud_enabled: Bool }
private struct MemoDTO: Decodable { let text: String }
private struct ClinicDTO: Decodable { let period_start, period_end: String; let concerns: [String] }
private struct AskDTO: Decodable {
    struct M: Decodable { let text: String }
    let answer: M
    let memos: [MemoDTO]
}

/// 画面が読むデータ。サーバーから読めたら置き換え、読めない・未設定なら固定データのまま。
@MainActor final class Live: ObservableObject {
    static let shared = Live()
    @Published var days = defaultDays
    @Published var concerns = defaultConcerns
    @Published var period = "11/2〜11/9"
    @Published var cloud = false
    @Published var memo: [String] = []
    @Published var error: String?
    private var latest = "2026-11-09"

    func load() async {
        guard Api.enabled else { return }
        do {
            let ds = (try await Api.call("GET", "/days") as [DayDTO]).sorted { $0.date > $1.date }
            if let first = ds.first { days = ds.map(Self.day); latest = first.date }
            cloud = (try await Api.call("GET", "/settings") as SettingsDTO).cloud_enabled
            memo = (try await Api.call("GET", "/memos") as [MemoDTO]).map(\.text)
            if let c = try? await Api.call("GET", "/clinic-summaries/latest") as ClinicDTO { clinic(c) } // まだ無ければ 404
            error = nil
        } catch {
            self.error = "サーバーに接続できません。固定データを表示しています（\(error.localizedDescription)）"
        }
    }

    /// 要約タブの相談。返答を返し、メモはサーバーの一覧で置き換える。クラウドがオフならサーバーが台本で返す。
    func ask(_ q: String) async throws -> String {
        let o: AskDTO = try await Api.call("POST", "/assistant/messages", ["text": q])
        memo = o.memos.map(\.text)
        return o.answer.text
    }

    func setCloud(_ on: Bool) async throws {
        cloud = (try await Api.call("PATCH", "/settings", ["cloud_enabled": on]) as SettingsDTO).cloud_enabled
    }

    /// 直近 7 日の記録から Gemini で「気になる点」を作る（クラウドがオンのときだけ）。
    func generateClinic() async throws {
        let start = Calendar(identifier: .gregorian).date(byAdding: .day, value: -7, to: Self.iso.date(from: latest)!)!
        clinic(try await Api.call("POST", "/clinic-summaries/generate",
                                  ["patient_name": "山田 太郎", "period_start": Self.iso.string(from: start), "period_end": latest]))
    }

    private func clinic(_ c: ClinicDTO) {
        concerns = c.concerns
        period = "\(Self.md(c.period_start))〜\(Self.md(c.period_end))"
    }

    private static let iso: DateFormatter = {
        let f = DateFormatter()
        f.calendar = Calendar(identifier: .gregorian); f.locale = Locale(identifier: "en_US_POSIX")
        f.timeZone = TimeZone(identifier: "Asia/Tokyo"); f.dateFormat = "yyyy-MM-dd"
        return f
    }()

    private static let jp: DateFormatter = {
        let f = DateFormatter()
        f.calendar = Calendar(identifier: .gregorian); f.locale = Locale(identifier: "ja_JP")
        f.timeZone = TimeZone(identifier: "Asia/Tokyo"); f.dateFormat = "M/d（E）"
        return f
    }()

    private static func md(_ s: String) -> String {
        let p = s.split(separator: "-").compactMap { Int($0) }
        return p.count == 3 ? "\(p[1])/\(p[2])" : s
    }

    private static func day(_ d: DayDTO) -> Day {
        let sec = d.audio_duration_sec ?? 0
        let audio = "\(d.audio_codec ?? "") \(d.audio_bitrate_kbps ?? 0) kbps ・ \(sec / 3600)時間\(sec % 3600 / 60)分 ・ 約 \((d.audio_size_bytes ?? 0) / 1_000_000) MB"
        let summary = d.summary ?? ""
        return Day(date: iso.date(from: d.date).map(jp.string) ?? d.date,
                   summary: summary.isEmpty ? "（要約はまだありません）" : summary, audio: audio,
                   lines: d.lines.map { Line(time: String($0.time.prefix(5)), who: $0.who, text: $0.text) })
    }
}
