# server/ — LifeOS Pendant アプリ向けバックエンド

FastAPI + SQLite（SQLModel）。`pendant/mobile` の画面（ホーム / 要約 / ログ / 診療 / 設定）の固定データ（`ProductUi.kt` / `ProductViews.swift`）に合わせた API。アプリ側は未接続（現状アプリは INTERNET 権限を持たない設計）。認証なし。

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

アシスタントの返答はアプリと同じ台本（`app/assistant.py` の `reply()`）。外部 LLM につなぐときは `Settings.cloud_enabled` が true のときだけ送ること。
