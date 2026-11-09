import pytest
from fastapi.testclient import TestClient
from sqlmodel import Session, SQLModel, create_engine
from sqlmodel.pool import StaticPool

from app.db import get_session
from app.main import app
from app.seed import seed


@pytest.fixture
def engine():
    engine = create_engine(
        "sqlite://", connect_args={"check_same_thread": False}, poolclass=StaticPool
    )
    SQLModel.metadata.create_all(engine)
    return engine


@pytest.fixture
def client(engine):
    def override():
        with Session(engine) as session:
            yield session

    app.dependency_overrides[get_session] = override
    yield TestClient(app)
    app.dependency_overrides.clear()


def test_health(client):
    assert client.get("/health").json() == {"status": "ok"}


def test_days_flow(client):
    r = client.post("/days/2026-11-09/lines", json=[
        {"time": "14:10", "who": "ご本人", "text": "膝がずきずきする。"},
        {"time": "07:42", "who": "ご本人", "text": "膝が痛いねえ。"},
    ])
    assert r.status_code == 201
    lines = r.json()["lines"]
    assert [l["time"] for l in lines] == ["07:42:00", "14:10:00"]  # 時刻順

    r = client.put("/days/2026-11-09", json={"summary": "膝の痛みを2回訴えた。", "audio_codec": "ADPCM"})
    assert r.json()["summary"] == "膝の痛みを2回訴えた。"
    assert len(r.json()["lines"]) == 2

    # 部分更新: summary は残る
    r = client.put("/days/2026-11-09", json={"audio_duration_sec": 100})
    assert r.json()["summary"] == "膝の痛みを2回訴えた。"

    client.put("/days/2026-11-08", json={"summary": "落ち着いて過ごした。"})
    assert [d["date"] for d in client.get("/days").json()] == ["2026-11-09", "2026-11-08"]
    assert len(client.get("/days", params={"since": "2026-11-09"}).json()) == 1

    assert client.delete("/days/2026-11-09").status_code == 204
    assert client.get("/days/2026-11-09").status_code == 404


def test_status_and_settings(client):
    assert client.get("/status").json()["recording"] is False
    r = client.patch("/status", json={"recording": True, "battery_percent": 82})
    assert r.json()["recording"] is True and r.json()["battery_percent"] == 82
    assert client.patch("/status", json={"battery_percent": 120}).status_code == 422

    s = client.get("/settings").json()
    assert s == {"cloud_enabled": False, "cloud_consented_at": None}
    s = client.patch("/settings", json={"cloud_enabled": True}).json()
    assert s["cloud_enabled"] and s["cloud_consented_at"]
    s = client.patch("/settings", json={"cloud_enabled": False}).json()
    assert s["cloud_consented_at"] is None


def test_clinic(client):
    assert client.get("/clinic-summaries/latest").status_code == 404
    body = {"patient_name": "山田 太郎", "period_start": "2026-11-02", "period_end": "2026-11-09",
            "concerns": ["膝の痛み"]}
    assert client.post("/clinic-summaries", json=body).status_code == 201
    assert client.get("/clinic-summaries/latest").json()["concerns"] == ["膝の痛み"]
    bad = body | {"period_end": "2026-11-01"}
    assert client.post("/clinic-summaries", json=bad).status_code == 422


def test_seed(client, engine):
    with Session(engine) as s:
        seed(s)
        seed(s)  # 2回呼んでも重複しない
    days = client.get("/days").json()
    assert len(days) == 2 and len(days[0]["lines"]) == 5
    assert client.get("/status").json()["battery_percent"] == 82
    assert client.get("/clinic-summaries/latest").json()["patient_name"] == "山田 太郎"


def test_assistant(client):
    chips = client.get("/assistant/suggestions").json()
    assert chips == ["何を伝えるべき？", "膝の痛みを詳しく", "服薬は大丈夫？"]

    hist = client.get("/assistant/messages").json()
    assert len(hist) == 1 and hist[0]["mine"] is False  # 挨拶

    r = client.post("/assistant/messages", json={"text": "服薬は大丈夫？"})
    assert r.status_code == 201
    body = r.json()
    assert body["question"]["mine"] is True and "6/7日" in body["answer"]["text"]
    assert [m["text"] for m in body["memos"]] == ["服薬: 6/7日確認（1日不明）"]

    # 同じチップをもう一度: メモは増えない
    body = client.post("/assistant/messages", json={"text": "服薬は大丈夫？"}).json()
    assert len(body["memos"]) == 1

    # 自由入力はそのままメモになる
    body = client.post("/assistant/messages", json={"text": "夜よく眠れていない"}).json()
    assert body["memos"][-1]["text"] == "夜よく眠れていない"
    assert len(client.get("/assistant/messages").json()) == 6

    memo_id = body["memos"][0]["id"]
    assert client.delete(f"/memos/{memo_id}").status_code == 204
    assert len(client.get("/memos").json()) == 1
    assert len(client.post("/memos", json={"text": "手で追加"}).json()) == 2

    assert client.delete("/assistant/messages").status_code == 204
    assert len(client.get("/assistant/messages").json()) == 1
    assert client.post("/assistant/messages", json={"text": ""}).status_code == 422
