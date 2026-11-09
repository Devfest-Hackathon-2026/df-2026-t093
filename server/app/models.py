"""モバイルアプリ (pendant/mobile, ui-android ブランチ) の ProductUi.kt の固定データに合わせたモデル。

- Day / Line      : ログタブ (文字起こし・要約・生データ) と ホームの「今日のハイライト」
- DeviceStatus    : ホームタブの 録音中/停止中・同期・電源・端末に保存済み
- Settings        : プライバシータブの「クラウドに送信する」
- ClinicSummary   : 診療タブの「次回診療向けサマリー」
"""

import datetime as dt

from sqlalchemy import JSON, Column
from sqlmodel import Field, Relationship, SQLModel


def now() -> dt.datetime:
    return dt.datetime.now(dt.UTC)


# ---------- 会話ログ ----------


class LineBase(SQLModel):
    time: dt.time
    who: str = Field(min_length=1, max_length=50)  # "ご本人" / "ヘルパー" / "家族" など
    text: str = Field(min_length=1, max_length=5000)


class Line(LineBase, table=True):
    id: int | None = Field(default=None, primary_key=True)
    day_date: dt.date = Field(foreign_key="day.date", index=True)
    day: "Day" = Relationship(back_populates="lines")


class LineCreate(LineBase):
    pass


class LineRead(LineBase):
    id: int


class DayBase(SQLModel):
    summary: str | None = Field(default=None, max_length=5000)
    # 生データ (例: "ADPCM 32 kbps ・ 5時間12分 ・ 約 72 MB") の元になる値
    audio_codec: str | None = Field(default=None, max_length=20)
    audio_bitrate_kbps: int | None = Field(default=None, ge=0)
    audio_duration_sec: int | None = Field(default=None, ge=0)
    audio_size_bytes: int | None = Field(default=None, ge=0)


class Day(DayBase, table=True):
    date: dt.date = Field(primary_key=True)
    updated_at: dt.datetime = Field(default_factory=now)
    lines: list[Line] = Relationship(
        back_populates="day",
        sa_relationship_kwargs={
            "order_by": "Line.time",
            "cascade": "all, delete-orphan",
        },
    )


class DayUpsert(DayBase):
    pass


class DayRead(DayBase):
    date: dt.date
    updated_at: dt.datetime
    lines: list[LineRead] = []


# ---------- ペンダントの状態 ----------


class DeviceStatusBase(SQLModel):
    recording: bool = False
    last_sync_at: dt.datetime | None = None
    power_source: str | None = Field(default=None, max_length=20)  # "solar" / "coin" など
    battery_percent: int | None = Field(default=None, ge=0, le=100)  # 蓄電
    backup_battery: str | None = Field(default=None, max_length=20)  # 非常用電池 ("full" など)
    stored_days: float | None = Field(default=None, ge=0)  # 端末に保存済み
    capacity_days: float | None = Field(default=None, ge=0)


class DeviceStatus(DeviceStatusBase, table=True):
    id: int = Field(default=1, primary_key=True)
    updated_at: dt.datetime = Field(default_factory=now)


class DeviceStatusUpdate(SQLModel):
    recording: bool | None = None
    last_sync_at: dt.datetime | None = None
    power_source: str | None = Field(default=None, max_length=20)
    battery_percent: int | None = Field(default=None, ge=0, le=100)
    backup_battery: str | None = Field(default=None, max_length=20)
    stored_days: float | None = Field(default=None, ge=0)
    capacity_days: float | None = Field(default=None, ge=0)


class DeviceStatusRead(DeviceStatusBase):
    updated_at: dt.datetime


# ---------- 設定 ----------


class Settings(SQLModel, table=True):
    id: int = Field(default=1, primary_key=True)
    cloud_enabled: bool = False  # 初期値オフ (アプリと同じ)
    cloud_consented_at: dt.datetime | None = None


class SettingsUpdate(SQLModel):
    cloud_enabled: bool


class SettingsRead(SQLModel):
    cloud_enabled: bool
    cloud_consented_at: dt.datetime | None


# ---------- 診療向けサマリー ----------


class ClinicSummaryBase(SQLModel):
    patient_name: str = Field(min_length=1, max_length=100)
    period_start: dt.date
    period_end: dt.date


class ClinicSummary(ClinicSummaryBase, table=True):
    id: int | None = Field(default=None, primary_key=True)
    concerns: list[str] = Field(default_factory=list, sa_column=Column(JSON, nullable=False))
    created_at: dt.datetime = Field(default_factory=now)


class ClinicSummaryCreate(ClinicSummaryBase):
    concerns: list[str] = Field(default_factory=list, max_length=50)


class ClinicSummaryRead(ClinicSummaryBase):
    id: int
    concerns: list[str]
    created_at: dt.datetime


# ---------- 要約タブ: 先生に伝えることを相談するアシスタント ----------


class AssistantMessage(SQLModel, table=True):
    id: int | None = Field(default=None, primary_key=True)
    mine: bool  # true = 利用者の発言, false = アシスタント (アプリの Msg.mine と同じ)
    text: str
    created_at: dt.datetime = Field(default_factory=now)


class AssistantMessageRead(SQLModel):
    id: int
    mine: bool
    text: str
    created_at: dt.datetime


class AskRequest(SQLModel):
    text: str = Field(min_length=1, max_length=2000)


class Memo(SQLModel, table=True):
    """診察で伝えるメモ。同じ文面は1件だけ (アプリの `if m !in memo` と同じ)。"""

    id: int | None = Field(default=None, primary_key=True)
    text: str = Field(unique=True)
    created_at: dt.datetime = Field(default_factory=now)


class MemoCreate(SQLModel):
    text: str = Field(min_length=1, max_length=500)


class MemoRead(SQLModel):
    id: int
    text: str
    created_at: dt.datetime


class AskResponse(SQLModel):
    question: AssistantMessageRead
    answer: AssistantMessageRead
    memos: list[MemoRead]  # 追加後のメモ一覧
