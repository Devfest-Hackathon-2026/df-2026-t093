# server/ — LifeOS Pendant アプリ向けバックエンド

FastAPI + SQLite（SQLModel）。`pendant/mobile` の画面（ホーム / 要約 / ログ / 診療 / 設定）の固定データ（`ProductUi.kt` / `ProductViews.swift`）に合わせた API。アプリ側は未接続（現状アプリは INTERNET 権限を持たない設計）。

```bash
cd server
uv sync
uv run python -m app.seed               # アプリの固定データと同じデモデータを投入（./app.db）
uv run uvicorn app.main:app --reload    # http://localhost:8000/docs
uv run pytest
```

| 画面 | エンドポイント |
|---|---|
| ホーム | `GET` / `PATCH /status` |
| 要約 | `GET /assistant/suggestions`、`GET` / `POST` / `DELETE /assistant/messages`、`GET` / `POST /memos`、`DELETE /memos/{id}` |
| ログ | `GET /days`、`GET` / `PUT` / `DELETE /days/{YYYY-MM-DD}`、`POST /days/{date}/lines` |
| 診療 | `POST /clinic-summaries`、`GET /clinic-summaries/latest`、`GET /clinic-summaries/{id}` |
| 設定 | `GET` / `PATCH /settings`（クラウド送信のオン/オフ） |
| AI 生成 | `POST /days/{date}/summarize`、`POST /clinic-summaries/generate` |

## Gemini

`app/llm.py`（SDK 呼び出し）と `app/ai.py`（プロンプト）。**`/settings` の `cloud_enabled` が true のときだけ** Gemini に送る。

- `POST /days/{date}/summarize`: その日の文字起こしから要約を作って保存
- `POST /clinic-summaries/generate` `{patient_name, period_start, period_end}`: 期間の記録から「気になる点」を作って保存
- `POST /assistant/messages`: クラウドオンなら直近7日の記録・メモ・会話履歴を踏まえて Gemini が返答。オフならアプリと同じ台本
- クラウドがオフだと生成系は 409、Gemini 未設定だと 503

ローカルで試す: `GEMINI_API_KEY=... uv run uvicorn app.main:app --reload`（モデルは `GEMINI_MODEL`、既定 `gemini-2.5-flash`）

## 認証（デモ用）

環境変数 `API_TOKEN` を設定すると、`/health` と `/docs` 以外は `Authorization: Bearer <API_TOKEN>` が必要。ponytail: 本番は Firebase Auth の ID トークン検証に置き換える。

## Cloud Run へのデプロイ

```bash
PROJECT=<GCP プロジェクト ID> bash deploy.sh   # gcloud が必要。最後に URL と API_TOKEN を表示
```

- `gcloud run deploy --source .`（Cloud Build で `Dockerfile` をビルド）、リージョン `asia-northeast1`
- Gemini は Vertex AI 経由（サービスアカウント認証、API キー不要）。`deploy.sh` が `roles/aiplatform.user` を付与する
- **DB は SQLite を `/tmp` に置くデモ構成**。起動時に `SEED_DEMO=1` でデモデータを入れ、再起動すると書き込んだ内容は消える。インスタンス数は 1 に固定。データを残すなら Cloud SQL を作って `DATABASE_URL` を差し替える
