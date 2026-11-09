# anam — The Living Anamnesis

高齢の方の日常の声を 24 時間記録するペンダントと、その記録から「次の診察で先生に伝えること」を整えるアプリ。医療・介護の現場で、ご本人が診察室で思い出せない・言いそびれる生活の様子（アナムネ＝問診・生活歴）を、医師やケアの担い手に届ける。

DevFest GDGoC Japan Hackathon 2026 の提出物。

```text
ペンダント（録音・保存）──BLE、約2時間ごと──▶ スマホアプリ（anam）──HTTPS──▶ pendant-api（Cloud Run）──▶ Gemini（Vertex AI）
  赤/黒の表示で録音中を示す            要約・相談・報告書・設定              記録・メモ・設定を保存       要約・診療サマリー・声の聞き取り
```

## できること

| 画面 | 内容 |
|---|---|
| ホーム | ペンダントの録音状態（本体の赤/黒の表示と同じ見た目）、同期・電源・保存量、今日のハイライト |
| 要約 | 本日の要約と、**声で相談**。大きな「押して話す」ボタンで話すと、音声をそのまま Gemini に渡して聞き取り・返答し、返答を読み上げる。相談で出た「診察で伝えるメモ」がたまる |
| 要約 → 医師向け報告書 | チャットではなく、メモ・記録の所見・本日の様子を 1 枚にまとめ、OS の共有シートで医師に渡す |
| ログ | 日ごとの文字起こし・要約・録音データの情報 |
| 診療 | 次回診療向けの「気になる点」。記録から Gemini で作り直せる |
| 設定 | 「クラウドに送信する」（初期値オフ）、「返答を読み上げる」、開発者向けの BLE 同期画面 |

起動時に、ロゴ（ペンダントに A を刻んだ形）が描かれる約 2 秒のアニメーションが出る。

## Google の技術

- **Gemini（Vertex AI 経由）**: 1 日の要約、診療向けサマリー（気になる点）、要約タブの相談、**声の相談（音声を直接渡して、聞き取りと返答を 1 回で得る）**。
- **Cloud Run**: pendant-api（FastAPI）をデプロイ。

## プライバシーの考え方

- **クラウド送信は初期値オフ。** オンにするときは確認ダイアログで同意を取る。オフの間、サーバーは Gemini に何も送らない（声の相談は 409 で断る）。
- 録音した相談の声は、送信後に端末から消す。
- ペンダントは録音中に前面が赤く、電源が切れると黒になる。周りの人にも録音していることが分かる。
- 診断はしない。画面と報告書に「自動要約であり診断ではない」と明記する。

## リポジトリの構成

| パス | 内容 |
|---|---|
| `pendant/mobile/` | アプリ（Android: Kotlin / Compose、iOS: SwiftUI、共通の同期ロジック: Kotlin Multiplatform）。[README](pendant/mobile/README.md) |
| `pendant/fw/` | ペンダントのファームウェア（nRF54L15、Zephyr / NCS） |
| `pendant/hw/` | ペンダントの基板（KiCad）。Standard（CR2477）と Premium（Solar＋蓄電） |
| `pendant/docs/spec/` | ペンダントの仕様（電源・音声・保存・同期形式・RF・筐体など） |
| `pendant/brand/` | ロゴ（SVG）とアプリアイコン |
| `export/`、`app/`、`report/` | 話者照合（ECAPA-TDNN を INT8 化してスマホで動かす）の PoC。[下記](#話者照合-pocecapa-tdnn) |
| `server/`（**`server` ブランチ**） | pendant-api。API 仕様は `server/API.md`、デプロイは `server/deploy.sh` |

## アプリを動かす

1. サーバーの URL とトークンを置く（**Git に入れない**。トークンはチームから受け取る）。
   - Android: `pendant/mobile/local.properties` に `pendant.api.base=https://…` と `pendant.api.token=…`
   - iOS: `pendant/mobile/iosApp/Sources/Secrets.plist` に `ApiBase` と `ApiToken`（文字列）
   - 置かなければ、固定のデモデータと台本の返答で動く（サーバー無しで画面を見られる）。
2. ビルドする。
   ```bash
   cd pendant/mobile && source env.sh && ./gradlew :androidApp:assembleDebug   # APK: androidApp/build/outputs/apk/debug/
   ```
   iOS は `pendant/mobile/iosApp/iosApp.xcodeproj` を Xcode で開いて実行する（Kotlin の共通部分はビルド時に自動で作られる）。
3. 声の相談を使うときは、設定タブで「クラウドに送信する」をオンにする。

## デモの流れ（例）

1. 起動アニメーション → ホームで「録音中」（ペンダントの赤い表示と同じ）を見せる。
2. 要約タブで「押して話す」→「最近、膝が痛くて夜も眠れないの。先生に伝えておいてほしい」と話す → 返答が読み上げられ、メモに残る。
3. 「医師向け報告書にまとめる」→ 共有シートで送る。
4. 診療タブで「記録から作り直す（Gemini）」。
5. 終わったら、サーバーに残った会話とメモを消しておく（相談の文脈に混ざるため）。
   ```bash
   curl -X DELETE -H "Authorization: Bearer $TOKEN" $URL/assistant/messages
   ```

## 既知の制約

- サーバーの DB はデモ用（Cloud Run の `/tmp` に置いた SQLite）。再デプロイ・再起動で初期データに戻り、「クラウドに送信する」もオフに戻る。
- ペンダント本体（Premium）は試作段階で、電源・録音時間の実測は済んでいない。デモのアプリはサーバーのデモデータを表示する。
- 話者照合（ECAPA）は PoC で、モバイルアプリにはまだ組み込んでいない。
- Android の声の相談は、実機での録音から返答までの通しを確認していない（iOS Simulator では確認済み）。

---

## 話者照合 PoC（ECAPA-TDNN）

SpeechBrain `spkrec-ecapa-voxceleb` を ONNX + INT8 に変換し、Flutter（ONNX Runtime）で
「登録音声と今の発話が同一人物か」をオンデバイスで判定する。

### 構成

```
export/                 Python: 変換・量子化・評価
  export_onnx.py        生波形→埋め込み を1本の ONNX に（Fbank もグラフに同梱）
  prepare_data.py       評価用 LibriSpeech test-clean 抽出（40話者×12発話）
  quantize_eval.py      INT8 量子化（dynamic / static QDQ）＋ EER・速度比較 → app/assets にコピー
  check_ort115.py       Flutter 同梱の ORT 1.15.1 でロードできるか確認
app/                    Flutter アプリ（Android / iOS）
  lib/speaker_verifier.dart  ORT 推論・コサイン類似度・PCM変換・簡易VAD
  lib/main.dart              登録（3回）→ 照合 UI、閾値スライダー
```

### 再生成手順

```bash
cd export
uv sync                       # torch 等で約1.3GB
uv run python export_onnx.py  # models/ecapa_fp32.onnx
uv run python prepare_data.py
uv run python quantize_eval.py
## 任意: 古い ORT での互換確認（新しい glibc では GLIBC_TUNABLES が必要）
GLIBC_TUNABLES=glibc.rtld.execstack=2 uv run --no-project --python 3.11 \
  --with onnxruntime==1.15.1 --with "numpy<2" --with soundfile python check_ort115.py

cd ../app
flutter run   # 実機推奨（マイクを使うため）
```

### 結果（LibriSpeech test-clean、キャリブ話者を除く 31話者433発話 / 先頭4秒、x86 CPU 1スレッド）

| model | size | EER | EER閾値 | fp32との cos | 3秒音声の推論 |
|---|---|---|---|---|---|
| fp32 | 84.1 MB | 0.80 % | 0.327 | 1.000 | 226 ms |
| int8 dynamic | 22.0 MB | 0.78 % | 0.334 | 0.995 | 320 ms |
| **int8 static (QDQ)** ← 採用 | 25.8 MB | 0.78 % | 0.329 | 0.996 | **46 ms** |

※ 評価セットは小さく、条件の良い英語の読み上げ音声。実運用では日本語・実機マイク・雑音環境での再評価と閾値調整が必須。

### ハマりどころ（対処済み）

- **ASP のマスク長計算まで量子化すると可変長入力で壊れる** → 量子化対象を Conv/MatMul に限定
- **ORT 1.31 の QDQ 融合が最終 fc Conv でクラッシュ** → そのノードだけ FP32 のまま
- **量子化ツールが付ける未使用 opset（ai.onnx.ml v5 等）で ORT 1.15 がロード拒否** → 未使用の opset 宣言を削除
- 前処理（DFT conv / log-mel）は FP32 のまま（量子化すると精度が落ちるため）
