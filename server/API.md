# Pendant API 仕様

`server/`（FastAPI）が提供する HTTP API。アプリ（`pendant/mobile`）の各画面に対応する。

## 共通

| 項目 | 内容 |
|---|---|
| ベース URL | `https://pendant-api-756406909128.asia-northeast1.run.app`（Cloud Run、東京） |
| 認証 | 全エンドポイントで `Authorization: Bearer <API_TOKEN>` が必要（`/health` と `/docs` を除く） |
| 形式 | リクエストもレスポンスも JSON（UTF-8）。POST / PUT / PATCH には `Content-Type: application/json` を付ける |
| キー名 | **snake_case**（例: `cloud_enabled`、`audio_duration_sec`） |
| 日付 | `YYYY-MM-DD`（例: `2026-11-09`） |
| 時刻 | `HH:MM:SS`（例: `07:42:00`）。送るときは `07:42` でもよい |
| 日時 | ISO 8601、**UTC**（例: `2026-11-09T14:13:29.198593Z`）。末尾の `Z` が付かないことがあるが、その場合も UTC として扱う |
| null | 値がない項目も、キーは省略せずに `null` で返す |

### サーバーのデータについての注意（デモ構成）

- DB は SQLite で、Cloud Run のコンテナ内にある。**再起動・再デプロイのたびに、デモデータ（11/8・11/9 の会話など）に戻る。**
- 戻るとクラウド送信もオフになる。AI 生成を使う前に、`PATCH /settings` でオンにすること。
- 利用者の区別はない（ペンダント 1 台・患者 1 人の前提）。
- しばらくアクセスがないと停止し、次の 1 回目の応答に数秒かかる（コールドスタート）。

### エラー

本文は `{"detail": ...}` の形。

| ステータス | 意味 | `detail` |
|---|---|---|
| 401 | トークンがない、または違う | `"Unauthorized"` |
| 404 | 対象がない | 文字列（例: `"Day not found"`） |
| 409 | クラウド送信がオフなのに、AI 生成を呼んだ | `"クラウド送信がオフです (PATCH /settings で有効化)"` |
| 422 | 入力が不正 | 文字列、または FastAPI の検証エラーの配列（`[{"loc": [...], "msg": "...", ...}]`） |
| 502 | Gemini の呼び出しに失敗した | `"Gemini error (model=...): ..."` |
| 503 | サーバーに Gemini が設定されていない | `"Gemini が未設定です"` |

---

## 画面とエンドポイントの対応

| 画面 | 使うエンドポイント |
|---|---|
| ホーム | `GET /status`。今日のハイライトは `GET /days?limit=1` の `summary` を「。」で区切る |
| 要約 | `GET /days?limit=1`（本日の要約）、`GET /assistant/suggestions`、`GET` / `POST /assistant/messages`、`GET /memos` |
| ログ | `GET /days` |
| 診療 | `GET /clinic-summaries/latest`、`POST /clinic-summaries/generate` |
| 設定 | `GET` / `PATCH /settings` |

---

## ホーム: ペンダントの状態

### `GET /status`

```json
{
  "recording": true,
  "last_sync_at": null,
  "power_source": "solar",
  "battery_percent": 82,
  "backup_battery": "full",
  "stored_days": 1.2,
  "capacity_days": 3.0,
  "updated_at": "2026-11-09T14:06:13.550163Z"
}
```

| キー | 型 | 説明 |
|---|---|---|
| `recording` | bool | 録音中か |
| `last_sync_at` | 日時 \| null | 最後に BLE 同期した日時 |
| `power_source` | string \| null | `"solar"` / `"coin"` など |
| `battery_percent` | int \| null | 蓄電量（0〜100） |
| `backup_battery` | string \| null | 非常用電池の状態（`"full"` など） |
| `stored_days` / `capacity_days` | number \| null | 端末に保存済みの日数 / 保存できる日数（「3日分のうち 1.2日」） |

### `PATCH /status`

アプリが BLE 同期のあとに、ペンダントの状態を送る。**送ったキーだけ更新**する。返り値は `GET /status` と同じ。

```json
{ "recording": true, "last_sync_at": "2026-11-09T05:00:00Z", "battery_percent": 80 }
```

`battery_percent` が 0〜100 の範囲外なら 422。

---

## 設定: クラウド送信

### `GET /settings`

```json
{ "cloud_enabled": false, "cloud_consented_at": null }
```

### `PATCH /settings`

```json
{ "cloud_enabled": true }
```

→ `{"cloud_enabled": true, "cloud_consented_at": "2026-11-09T14:13:14.909817Z"}`

- オンにした時刻が `cloud_consented_at` に入る。オフにすると `null` に戻る。
- **アプリで確認ダイアログ（「理解してオンにする」）を出してから**呼ぶこと。
- オフの間は、サーバーは Gemini に何も送らない。

---

## ログ: 日ごとの記録

### Day オブジェクト

```json
{
  "date": "2026-11-09",
  "summary": "朝食は完食。午後は膝の痛みを2回訴えた。夜は穏やかに家族と通話。",
  "audio_codec": "ADPCM",
  "audio_bitrate_kbps": 32,
  "audio_duration_sec": 18720,
  "audio_size_bytes": 72000000,
  "updated_at": "2026-11-09T13:58:11.982Z",
  "lines": [
    { "id": 1, "time": "07:42:00", "who": "ご本人", "text": "今日は、ちょっと膝が痛いねえ。" },
    { "id": 2, "time": "07:43:00", "who": "ヘルパー", "text": "湿布を貼りましょうか。朝ごはんの後でいいですか？" }
  ]
}
```

- `lines` は時刻順に並ぶ。
- 「生データ」タブの `ADPCM 32 kbps ・ 5時間12分 ・ 約 72 MB` は、`audio_*` からアプリ側で組み立てる。
- `summary` と `audio_*` は `null` のことがある。

### `GET /days`

新しい日付から順に、Day の配列を返す。

| クエリ | 既定 | 説明 |
|---|---|---|
| `since` | なし | この日付以降（その日を含む） |
| `until` | なし | この日付以前（その日を含む） |
| `limit` | 14 | 1〜100 |

### `GET /days/{date}`

1 日分の Day を返す。なければ 404。

### `PUT /days/{date}`

要約や録音情報を作成・更新する。**送ったキーだけ更新**する。返り値は Day。

```json
{ "summary": "…", "audio_codec": "ADPCM", "audio_bitrate_kbps": 32, "audio_duration_sec": 18720, "audio_size_bytes": 72000000 }
```

### `POST /days/{date}/lines` → 201

文字起こしの行をまとめて追加する。その日がなければ作る。返り値は Day（追加後）。

```json
[
  { "time": "07:42", "who": "ご本人", "text": "今日は、ちょっと膝が痛いねえ。" },
  { "time": "07:43", "who": "ヘルパー", "text": "湿布を貼りましょうか。" }
]
```

`who` は 1〜50 文字、`text` は 1〜5000 文字。

### `POST /days/{date}/summarize` 【AI】

その日の `lines` から Gemini で要約を作り、`summary` に保存する。返り値は Day。

- クラウド送信がオフなら 409、`lines` が空なら 422、その日がなければ 404。
- 応答には数秒かかる。

### `DELETE /days/{date}` → 204

---

## 要約タブ: 相談アシスタント

### `GET /assistant/suggestions`

入力欄の上に並べる提案チップの文言。

```json
["何を伝えるべき？", "膝の痛みを詳しく", "服薬は大丈夫？"]
```

### `GET /assistant/messages`

会話の履歴（古い順）。

```json
[
  { "id": 1, "mine": true,  "text": "服薬は大丈夫？", "created_at": "..." },
  { "id": 2, "mine": false, "text": "19:31に「飲んだよ」と返答があり、…", "created_at": "..." }
]
```

- `mine`: `true` が利用者の発言、`false` がアシスタントの発言（アプリの `Msg.mine` と同じ）。
- 履歴が空のときは、最初の挨拶 1 件（`id: 0`、`mine: false`）だけを返す。この挨拶は保存されない。

### `POST /assistant/messages` → 201 【AI】

質問を送り、返答を受け取る。チップを押したときも、その文言をそのまま `text` に入れる。

```json
{ "text": "最近の様子で先生に伝えることは？" }
```

```json
{
  "question": { "id": 1, "mine": true,  "text": "最近の様子で先生に伝えることは？", "created_at": "..." },
  "answer":   { "id": 2, "mine": false, "text": "直近の記録では、11月9日に朝と午後の2回…", "created_at": "..." },
  "memos": [
    { "id": 1, "text": "11/9の朝と午後に膝が痛む・ずきずきすると訴えがあった", "created_at": "..." }
  ]
}
```

- **クラウド送信オン**: Gemini が、直近 7 日の記録・メモ・会話履歴を踏まえて答える。数秒かかる。
- **クラウド送信オフ**: アプリと同じ台本で即答する。チップの文言と完全に一致すれば台本の答え、それ以外は「メモに追加しました」と返す。この場合 409 にはならない。
- `memos` は**追加後のメモ全件**。アプリはメモの表示を、これで丸ごと置き換えればよい。
- `text` は 1〜2000 文字。

### `DELETE /assistant/messages` → 204

会話をリセットする（メモは残る）。

---

## 要約タブ: 診察で伝えるメモ

### `GET /memos`

```json
[{ "id": 1, "text": "膝の痛み: 週5日・朝と午後・湿布で対応", "created_at": "..." }]
```

### `POST /memos` → 201

```json
{ "text": "夜よく眠れていない" }
```

返り値は追加後のメモ全件。同じ文面がすでにあれば追加しない（エラーにはならない）。`text` は 1〜500 文字。

### `DELETE /memos/{id}` → 204

なければ 404。

---

## 診療: 次回診療向けサマリー

### ClinicSummary オブジェクト

```json
{
  "id": 2,
  "patient_name": "山田 太郎",
  "period_start": "2026-11-02",
  "period_end": "2026-11-09",
  "concerns": [
    "膝の痛み: 11月9日の朝（7時台）と午後（14時台）に計2回、痛みの訴えがありました。",
    "物忘れ・日時の確認: 11月8日の午前（10時台）に曜日を尋ねる発言があり、…"
  ],
  "created_at": "2026-11-09T14:13:36.322629Z"
}
```

画面の「対象期間: 11/2〜11/9（山田 太郎さん）」と、「気になる点」の箇条書きに対応する。

### `GET /clinic-summaries/latest`

一番新しいサマリーを返す。1 件もなければ 404。

### `POST /clinic-summaries/generate` → 201 【AI】

期間内の記録から、Gemini で「気になる点」を作って保存する。

```json
{ "patient_name": "山田 太郎", "period_start": "2026-11-02", "period_end": "2026-11-09" }
```

- クラウド送信がオフなら 409、期間内に記録がなければ 404、`period_end < period_start` なら 422。
- 応答には数秒かかる。

### `POST /clinic-summaries` → 201

AI を使わず、手で作ったサマリーを保存する。本文は `generate` の内容に `"concerns": ["…"]` を足したもの。

### `GET /clinic-summaries/{id}`

---

## その他

| エンドポイント | 説明 |
|---|---|
| `GET /health` | 認証不要。`{"status": "ok"}` |
| `GET /docs` | Swagger UI（自動生成。トークンを送れないので、画面から API を試すことはできない） |

## curl での確認例

```bash
U=https://pendant-api-756406909128.asia-northeast1.run.app
H="Authorization: Bearer <API_TOKEN>"
curl -s -H "$H" $U/status
curl -s -X PATCH -H "$H" -H "Content-Type: application/json" -d '{"cloud_enabled": true}' $U/settings
curl -s -X POST  -H "$H" -H "Content-Type: application/json" -d '{"text": "何を伝えるべき？"}' $U/assistant/messages
```
