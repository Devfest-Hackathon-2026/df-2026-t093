import SwiftUI
import Shared

// ハリボテ: 見た目と画面遷移を決めるための固定データ（Android の ProductUi.kt と同じ内容）。
// 録音の有無・接続状態・最終同期だけ実データ（UiState）。
// ponytail: 保存・文字起こし・要約・共有は未接続。実装するときはここの固定データを差し替える。

let pendantTeal = Color(red: 0, green: 0.475, blue: 0.42)

private struct Line { let time, who, text: String }
private struct Day: Identifiable {
    let date, summary, audio: String
    let lines: [Line]
    var id: String { date }
}

private let days = [
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
    var body: some View {
        let s = model.state
        let rec = s.info?.recording ?? true
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
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
                ForEach(days[0].summary.split(separator: "。").map(String.init), id: \.self) { Text("・\($0)") }
            }.padding()
        }
    }
}

struct LogsView: View {
    @State private var tier = 0
    var body: some View {
        VStack(spacing: 8) {
            Picker("", selection: $tier) { Text("文字起こし").tag(0); Text("要約").tag(1); Text("生データ").tag(2) }
                .pickerStyle(.segmented).padding(.horizontal)
            ScrollView {
                VStack(spacing: 8) {
                    ForEach(days) { d in
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
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                Text("次回診療向けサマリー").font(.title2)
                Text("対象期間: 11/2〜11/9（山田 太郎さん）").font(.caption)
                VStack(alignment: .leading, spacing: 6) {
                    Text("気になる点").font(.headline)
                    Text("・膝の痛みの訴え: 週5日（朝・午後に多い）")
                    Text("・物忘れの訴え: 週2回（鍵・曜日）")
                    Text("・服薬の確認: 声かけに「飲んだ」と返答 6/7日")
                }
                .frame(maxWidth: .infinity, alignment: .leading).padding(16)
                .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
                Text("※ 会話ログからの自動要約です。診断ではありません。").font(.caption)
                Button("医師に共有する") {}.buttonStyle(.borderedProminent)
            }.padding()
        }
    }
}

struct PrivacyView: View {
    @State private var cloud = false
    @State private var confirm = false
    @State private var dev = false
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                Text("プライバシー").font(.title2)
                VStack(alignment: .leading, spacing: 8) {
                    Toggle(isOn: Binding(get: { cloud }, set: { if $0 { confirm = true } else { cloud = false } })) {
                        VStack(alignment: .leading) {
                            Text("クラウドに送信する").font(.headline)
                            Text(cloud ? "オン: 文字起こしテキストを Gemini API に送ります" : "オフ: すべてこの端末の中で処理します（初期値）").font(.caption)
                        }
                    }
                    if cloud { Text("外部サーバーに会話の内容が渡ります。ご本人・ご家族の同意を確認してください。").foregroundColor(.red) }
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
            Button("理解してオンにする") { cloud = true }
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

// 「先生に伝えたいこと」をAIと相談するアシスタント。返答は固定の台本（ハリボテ）。
// ponytail: 実際の応答は未接続。つなぐときは ask() の返答を Gemini 等の呼び出しに差し替える。
private struct Script { let chip, answer, memo: String }
private struct Msg: Identifiable { let id = UUID(); let mine: Bool; let text: String }

private let scripts = [
    Script(chip: "何を伝えるべき？", answer: "膝の痛みが今週5日あります。痛む時間帯（朝・午後）と、湿布で和らいだかを伝えると診察が早く進みます。物忘れ（鍵・曜日）も週2回ありました。", memo: "膝の痛み: 週5日・朝と午後・湿布で対応"),
    Script(chip: "膝の痛みを詳しく", answer: "朝食前後と14時ごろに訴えがあり、「ずきずき」という表現でした。痛みの強さを10段階で本人に聞いておくと、先生に伝えやすくなります。", memo: "痛みの表現: 「ずきずき」（強さは未確認）"),
    Script(chip: "服薬は大丈夫？", answer: "19:31に「飲んだよ」と返答があり、今週は6/7日で確認できました。確認できなかった1日は、先生にそのまま伝えましょう。", memo: "服薬: 6/7日確認（1日不明）"),
]

struct SummaryView: View {
    @State private var msgs = [Msg(mine: false, text: "今日の記録から、先生に伝えるとよさそうな点を見つけました。下のボタンか、自由な文章で相談してください。")]
    @State private var memo: [String] = []
    @State private var input = ""
    @State private var report = false

    private func ask(_ q: String, _ a: String, _ m: String) {
        msgs.append(Msg(mine: true, text: q)); msgs.append(Msg(mine: false, text: a))
        if !memo.contains(m) { memo.append(m) }
    }

    var body: some View {
        VStack(spacing: 8) {
            VStack(alignment: .leading, spacing: 2) {
                Text("本日の要約  \(days[0].date)").font(.headline)
                Text(days[0].summary)
            }
            .frame(maxWidth: .infinity, alignment: .leading).padding(12)
            .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
            ScrollViewReader { proxy in
                ScrollView {
                    VStack(spacing: 8) {
                        ForEach(msgs) { m in
                            HStack {
                                if m.mine { Spacer(minLength: 40) }
                                Text(m.text).padding(10)
                                    .background(m.mine ? pendantTeal : Color(red: 0.7, green: 0.87, blue: 0.86), in: RoundedRectangle(cornerRadius: 16))
                                    .foregroundColor(m.mine ? .white : .primary)
                                if !m.mine { Spacer(minLength: 40) }
                            }.id(m.id)
                        }
                        if !memo.isEmpty {
                            VStack(alignment: .leading, spacing: 2) {
                                Text("診察で伝えるメモ").font(.headline)
                                ForEach(memo, id: \.self) { Text("・\($0)") }
                            }
                            .frame(maxWidth: .infinity, alignment: .leading).padding(12)
                            .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16))
                            .id("memo")
                        }
                    }
                }
                .onChange(of: msgs.count) { _, _ in withAnimation { proxy.scrollTo(memo.isEmpty ? msgs.last?.id as AnyHashable? : "memo", anchor: .bottom) } }
            }
            Button("医師向け報告書にまとめる") { report = true }.buttonStyle(.borderedProminent).tint(pendantTeal).frame(maxWidth: .infinity)
            ScrollView(.horizontal, showsIndicators: false) {
                HStack { ForEach(scripts, id: \.chip) { sc in Button(sc.chip) { ask(sc.chip, sc.answer, sc.memo) }.buttonStyle(.bordered) } }
            }
            HStack {
                TextField("先生に伝えたいことを相談", text: $input).textFieldStyle(.roundedBorder)
                Button("送信") { ask(input, "「\(input)」を診察で伝えるメモに追加しました。（ハリボテ: AIの応答は未接続）", input); input = "" }
                    .buttonStyle(.borderedProminent).disabled(input.trimmingCharacters(in: .whitespaces).isEmpty)
            }
        }.padding()
        .sheet(isPresented: $report) { ReportView(text: reportText(memo)) }
    }
}

/// 医師に渡す報告書。チャットではなく、相談で整理したメモと記録の所見だけを1枚にまとめる。
private func reportText(_ memo: [String]) -> String {
    var s = "【医師向け報告書】\n患者: 山田 太郎さん\n期間: 11/2〜11/9（作成: 11/9）\n\n【伝えたいこと】\n"
    s += memo.isEmpty ? "・（相談で整理した内容はまだありません）\n" : memo.map { "・\($0)\n" }.joined()
    s += "\n【記録からの所見】\n・膝の痛みの訴え: 週5日（朝・午後に多い）\n・物忘れの訴え: 週2回（鍵・曜日）\n・服薬の確認: 声かけに「飲んだ」と返答 6/7日\n"
    s += "\n【本日の様子 \(days[0].date)】\n\(days[0].summary)\n\n※ ペンダントの会話ログからの自動要約です。診断ではありません。"
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
