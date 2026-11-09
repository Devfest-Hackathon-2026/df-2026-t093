import AVFoundation
import SwiftUI
import Shared

// 画面。データは Live（Api.swift）から読む。サーバー未設定・未接続のときは下の固定データ（ハリボテ）を表示する。
// 録音の有無・接続状態・最終同期は BLE の実データ（UiState）。Android の ProductUi.kt と同じ内容。

let pendantTeal = Color(red: 0, green: 0.475, blue: 0.42)

struct Line { let time, who, text: String }
struct Day: Identifiable {
    let date, summary, audio: String
    let lines: [Line]
    var id: String { date }
}

let defaultDays = [
    Day(date: "11/9（月）", summary: "朝食は完食。午後は膝の痛みを2回訴えた。夜は穏やかに家族と通話。",
        audio: "ADPCM 32 kbps ・ 5時間12分 ・ 約 72 MB", lines: [
            Line(time: "07:42", who: "ご本人", text: "今日は、ちょっと膝が痛いねえ。"),
            Line(time: "07:43", who: "ヘルパー", text: "湿布を貼りましょうか。朝ごはんの後でいいですか？"),
            Line(time: "14:10", who: "ご本人", text: "さっきから、また膝がずきずきする。"),
            Line(time: "19:30", who: "家族", text: "お父さん、薬は飲んだ？"),
            Line(time: "19:31", who: "ご本人", text: "飲んだよ。大丈夫。")]),
    Day(date: "11/8（日）", summary: "終日落ち着いて過ごした。夕方に物忘れの訴えが1回。",
        audio: "ADPCM 32 kbps ・ 4時間40分 ・ 約 65 MB", lines: [
            Line(time: "10:05", who: "ご本人", text: "今日は、何曜日だったかな。"),
            Line(time: "10:06", who: "家族", text: "日曜日だよ。散歩に行こうか。"),
            Line(time: "17:20", who: "ご本人", text: "鍵をどこに置いたか、忘れてしまった。")]),
]

let defaultConcerns = ["膝の痛みの訴え: 週5日（朝・午後に多い）", "物忘れの訴え: 週2回（鍵・曜日）", "服薬の確認: 声かけに「飲んだ」と返答 6/7日"]

private struct InfoCard: View {
    let title, main, sub: String
    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title).font(.subheadline)
            Text(main).font(.title2)
            Text(sub).font(.caption)
        }
        .frame(maxWidth: .infinity, alignment: .leading).padding(16)
        .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
    }
}

struct HomeView: View {
    @EnvironmentObject var model: AppModel
    @ObservedObject private var live = Live.shared
    var body: some View {
        let s = model.state
        let rec = s.info?.recording ?? true
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                if let e = live.error { Text(e).font(.caption).foregroundColor(.red) }
                HStack(spacing: 16) {
                    // 本体のラッチインジケーター（録音中は赤、停止は黒）と同じ見た目
                    Circle().fill(rec ? Color(red: 0.84, green: 0, blue: 0) : .black).frame(width: 56, height: 56)
                    VStack(alignment: .leading) {
                        Text(rec ? "録音中" : "停止中").font(.title2.bold())
                        Text(rec ? "ペンダントの前面が赤く表示されています" : "ペンダントの前面は黒です")
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading).padding(16)
                .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
                InfoCard(title: "同期", main: status(s), sub: s.lastSync.map { "最後: \(utc($0.atMs))" } ?? "次回: 約2時間ごと")
                InfoCard(title: "電源", main: "ソーラー発電中", sub: "蓄電 82 % ・ 非常用電池 満")
                InfoCard(title: "端末に保存済み", main: "3日分のうち 1.2日", sub: "古い順に自動で整理します")
                Text("今日のハイライト").font(.headline)
                ForEach(live.days[0].summary.split(separator: "。").map(String.init), id: \.self) { Text("・\($0)") }
            }.padding()
        }
    }
}

struct LogsView: View {
    @ObservedObject private var live = Live.shared
    @State private var tier = 0
    var body: some View {
        VStack(spacing: 8) {
            Picker("", selection: $tier) { Text("文字起こし").tag(0); Text("要約").tag(1); Text("生データ").tag(2) }
                .pickerStyle(.segmented).padding(.horizontal)
            ScrollView {
                VStack(spacing: 8) {
                    ForEach(live.days) { d in
                        VStack(alignment: .leading, spacing: 4) {
                            Text(d.date).font(.headline)
                            switch tier {
                            case 0: ForEach(d.lines, id: \.time) { Text("\($0.time)  \($0.who)：\($0.text)") }
                            case 1: Text(d.summary)
                            default: Text(d.audio)
                            }
                        }
                        .frame(maxWidth: .infinity, alignment: .leading).padding(12)
                        .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
                    }
                }.padding(.horizontal)
            }
        }.padding(.top)
    }
}

struct ClinicView: View {
    @ObservedObject private var live = Live.shared
    @State private var busy = false
    @State private var err: String?
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                Text("次回診療向けサマリー").font(.title2)
                Text("対象期間: \(live.period)（山田 太郎さん）").font(.caption)
                VStack(alignment: .leading, spacing: 6) {
                    Text("気になる点").font(.headline)
                    ForEach(live.concerns, id: \.self) { Text("・\($0)") }
                }
                .frame(maxWidth: .infinity, alignment: .leading).padding(16)
                .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
                Text("※ 会話ログからの自動要約です。診断ではありません。").font(.caption)
                if Api.enabled {
                    Button(busy ? "Gemini で作成中…" : "記録から作り直す（Gemini）") {
                        Task { busy = true; do { try await live.generateClinic(); err = nil } catch { err = error.localizedDescription }; busy = false }
                    }.buttonStyle(.borderedProminent).tint(pendantTeal).disabled(busy || !live.cloud)
                    if !live.cloud { Text("設定でクラウド送信をオンにすると使えます").font(.caption) }
                    if let err { Text(err).font(.caption).foregroundColor(.red) }
                }
            }.padding()
        }
    }
}

struct PrivacyView: View {
    @ObservedObject private var live = Live.shared
    @State private var err: String?
    @State private var confirm = false
    @State private var dev = false
    // サーバーにつながっていればその設定を切り替える（Gemini に送るかどうかはサーバー側でも判定する）
    private func set(_ on: Bool) {
        guard Api.enabled else { live.cloud = on; return }
        Task { do { try await live.setCloud(on); err = nil } catch { err = error.localizedDescription } }
    }

    var body: some View {
        let cloud = live.cloud
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                Text("プライバシー").font(.title2)
                VStack(alignment: .leading, spacing: 8) {
                    Toggle(isOn: Binding(get: { cloud }, set: { if $0 { confirm = true } else { set(false) } })) {
                        VStack(alignment: .leading) {
                            Text("クラウドに送信する").font(.headline)
                            Text(cloud ? "オン: 文字起こしテキストを Gemini API に送ります" : "オフ: すべてこの端末の中で処理します（初期値）").font(.caption)
                        }
                    }
                    if cloud { Text("外部サーバーに会話の内容が渡ります。ご本人・ご家族の同意を確認してください。").foregroundColor(.red) }
                    if let err { Text(err).font(.caption).foregroundColor(.red) }
                }
                .padding(16).background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
                Toggle(isOn: $live.readAloud) {
                    VStack(alignment: .leading) {
                        Text("返答を読み上げる").font(.headline)
                        Text("要約タブの返答を声でお伝えします").font(.caption)
                    }
                }
                .padding(16).background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
                VStack(alignment: .leading) {
                    Text("録音していることの表示").font(.headline)
                    Text("録音中はペンダント前面が赤になります。電源が切れると黒になり、周りの方にも分かります。").font(.caption)
                }
                .frame(maxWidth: .infinity, alignment: .leading).padding(16)
                .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
                Button("開発者向け画面（BLE 同期・受信ログ）") { dev = true }
            }.padding()
        }
        .sheet(isPresented: $dev) { DevView() }
        .alert("クラウド送信をオンにしますか？", isPresented: $confirm) {
            Button("理解してオンにする") { set(true) }
            Button("やめる", role: .cancel) {}
        } message: { Text("会話の文字起こしが外部のサーバーに送信されます。第三者の声が含まれる場合があります。") }
    }
}

/// 開発用の既存画面（BLE 同期・受信ログ・設定）。
struct DevView: View {
    @State private var sub = 0
    var body: some View {
        VStack {
            Picker("", selection: $sub) { Text("同期").tag(0); Text("一覧").tag(1); Text("設定").tag(2) }
                .pickerStyle(.segmented).padding()
            switch sub {
            case 0: SyncView()
            case 1: ListView()
            default: SettingsView()
            }
            Spacer(minLength: 0)
        }
    }
}

// 「先生に伝えたいこと」をAIと相談するアシスタント。サーバーにつながっていればサーバーが返す
// （クラウドがオンなら Gemini、オフなら同じ台本）。未設定・失敗時はアプリ内の台本で返す。
private struct Script { let chip, answer, memo: String }
private struct Msg: Identifiable { let id = UUID(); let mine: Bool; let text: String }

private let scripts = [
    Script(chip: "何を伝えるべき？", answer: "膝の痛みが今週5日あります。痛む時間帯（朝・午後）と、湿布で和らいだかを伝えると診察が早く進みます。物忘れ（鍵・曜日）も週2回ありました。", memo: "膝の痛み: 週5日・朝と午後・湿布で対応"),
    Script(chip: "膝の痛みを詳しく", answer: "朝食前後と14時ごろに訴えがあり、「ずきずき」という表現でした。痛みの強さを10段階で本人に聞いておくと、先生に伝えやすくなります。", memo: "痛みの表現: 「ずきずき」（強さは未確認）"),
    Script(chip: "服薬は大丈夫？", answer: "19:31に「飲んだよ」と返答があり、今週は6/7日で確認できました。確認できなかった1日は、先生にそのまま伝えましょう。", memo: "服薬: 6/7日確認（1日不明）"),
]

struct SummaryView: View {
    @State private var msgs = [Msg(mine: false, text: "下の大きなボタンを押して、先生に伝えたいことや気になることを話してください。話し終わったら、もう一度押します。")]
    @ObservedObject private var live = Live.shared
    @State private var input = ""
    @State private var report = false
    @State private var busy = false
    @State private var typing = false
    @State private var recording = false
    @State private var secs = 0
    @State private var recorder = Recorder()

    private func reply(_ a: String) { msgs.append(Msg(mine: false, text: a)); live.say(a) }

    private func local(_ a: String, _ m: String) {
        reply(a)
        if !live.memo.contains(m) { live.memo.append(m) }
    }

    private func ask(_ q: String, _ a: String, _ m: String) {
        msgs.append(Msg(mine: true, text: q))
        guard Api.enabled else { return local(a, m) }
        Task {
            busy = true
            do { reply(try await live.ask(q)) } catch { local(a + "（サーバーに接続できないため台本で返答）", m) }
            busy = false
        }
    }

    private func micTapped() {
        if recording { return stopAndSend() }
        guard Api.enabled, live.cloud else { return reply("声で相談するには、設定で「クラウドに送信する」をオンにしてください。") }
        Task {
            guard await AVAudioApplication.requestRecordPermission() else { return reply("マイクの使用が許可されていません。設定アプリで許可してください。") }
            live.hush()
            do { try recorder.start() } catch { return reply("録音を始められませんでした。") }
            secs = 0; recording = true
            while recording { // 1 回は最大 60 秒
                try? await Task.sleep(for: .seconds(1))
                if recording { secs += 1; if secs >= 60 { stopAndSend() } }
            }
        }
    }

    private func stopAndSend() {
        guard recording else { return }
        recording = false
        let wav = recorder.stop()
        guard wav.count > 44 + 16_000 else { return reply("短くて聞き取れませんでした。もう一度、ボタンを押してお話しください。") } // 0.5 秒未満
        Task {
            busy = true
            do {
                let (q, a) = try await live.askVoice(wav)
                msgs.append(Msg(mine: true, text: q)); reply(a)
            } catch { reply("うまく届きませんでした。もう一度お話しください。（\(error.localizedDescription)）") }
            busy = false
        }
    }

    var body: some View {
        VStack(spacing: 8) {
            VStack(alignment: .leading, spacing: 2) {
                Text("本日の要約  \(live.days[0].date)").font(.headline)
                Text(live.days[0].summary)
            }
            .frame(maxWidth: .infinity, alignment: .leading).padding(12)
            .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
            ScrollViewReader { proxy in
                ScrollView {
                    VStack(spacing: 8) {
                        ForEach(msgs) { m in
                            HStack {
                                if m.mine { Spacer(minLength: 40) }
                                Text(m.text).font(.title3).padding(12)
                                    .background(m.mine ? pendantTeal : Color(red: 0.7, green: 0.87, blue: 0.86), in: RoundedRectangle(cornerRadius: 16))
                                    .foregroundColor(m.mine ? .white : .primary)
                                if !m.mine { Spacer(minLength: 40) }
                            }.id(m.id)
                        }
                        if busy { Text("聞いています…").frame(maxWidth: .infinity, alignment: .leading) }
                        if !live.memo.isEmpty {
                            VStack(alignment: .leading, spacing: 2) {
                                Text("診察で伝えるメモ").font(.headline)
                                ForEach(live.memo, id: \.self) { Text("・\($0)") }
                            }
                            .frame(maxWidth: .infinity, alignment: .leading).padding(12)
                            .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
                            .id("memo")
                        }
                    }
                }
                .onChange(of: msgs.count) { _, _ in withAnimation { proxy.scrollTo(live.memo.isEmpty ? msgs.last?.id as AnyHashable? : "memo", anchor: .bottom) } }
            }
            // 録音中はペンダントの表示と同じ赤にする
            Button(action: micTapped) {
                Text(recording ? "話し終わったら押す（\(secs)秒）" : busy ? "考えています…" : "押して話す")
                    .font(.title2.weight(.semibold)).frame(maxWidth: .infinity, minHeight: 64)
            }
            .buttonStyle(.borderedProminent).buttonBorderShape(.capsule)
            .tint(recording ? Color(red: 0.84, green: 0, blue: 0) : pendantTeal).disabled(busy)
            HStack {
                Button(typing ? "文字入力を閉じる" : "文字で入力する") { typing.toggle() }
                Spacer()
                Button("医師向け報告書にまとめる") { report = true }
            }.font(.subheadline)
            if typing {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack { ForEach(scripts, id: \.chip) { sc in Button(sc.chip) { ask(sc.chip, sc.answer, sc.memo) }.buttonStyle(.bordered).disabled(busy) } }
                }
                HStack {
                    TextField("先生に伝えたいことを相談", text: $input).textFieldStyle(.roundedBorder)
                    Button("送信") { ask(input, "「\(input)」を診察で伝えるメモに追加しました。", input); input = "" }
                        .buttonStyle(.borderedProminent).disabled(busy || input.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
        }.padding()
        .sheet(isPresented: $report) { ReportView(text: reportText(live)) }
        .onDisappear { _ = recorder.stop(); recording = false }
    }
}

/// 医師に渡す報告書。チャットではなく、相談で整理したメモと記録の所見だけを1枚にまとめる。
@MainActor private func reportText(_ live: Live) -> String {
    var s = "【医師向け報告書】\n患者: 山田 太郎さん\n期間: \(live.period)\n\n【伝えたいこと】\n"
    s += live.memo.isEmpty ? "・（相談で整理した内容はまだありません）\n" : live.memo.map { "・\($0)\n" }.joined()
    s += "\n【記録からの所見】\n" + live.concerns.map { "・\($0)\n" }.joined()
    s += "\n【本日の様子 \(live.days[0].date)】\n\(live.days[0].summary)\n\n※ ペンダントの会話ログからの自動要約です。診断ではありません。"
    return s
}

private struct ReportView: View {
    let text: String
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        VStack(spacing: 12) {
            ScrollView { Text(text).frame(maxWidth: .infinity, alignment: .leading) }
            ShareLink(item: text) { Label("医師に共有する", systemImage: "square.and.arrow.up").frame(maxWidth: .infinity) }
                .buttonStyle(.borderedProminent).tint(pendantTeal)
            Button("閉じる") { dismiss() }.buttonStyle(.bordered).frame(maxWidth: .infinity)
        }.padding()
    }
}
