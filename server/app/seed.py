"""ProductUi.kt の固定データ (ハリボテ) と同じ内容を DB に入れる。

    uv run python -m app.seed
"""

import datetime as dt

from sqlmodel import Session

from app.db import engine, init_db
from app.models import ClinicSummary, Day, DeviceStatus, Line, Settings

DAYS = [
    (dt.date(2026, 11, 9), "朝食は完食。午後は膝の痛みを2回訴えた。夜は穏やかに家族と通話。",
     5 * 3600 + 12 * 60, 72_000_000, [
         ("07:42", "ご本人", "今日は、ちょっと膝が痛いねえ。"),
         ("07:43", "ヘルパー", "湿布を貼りましょうか。朝ごはんの後でいいですか？"),
         ("14:10", "ご本人", "さっきから、また膝がずきずきする。"),
         ("19:30", "家族", "お父さん、薬は飲んだ？"),
         ("19:31", "ご本人", "飲んだよ。大丈夫。"),
     ]),
    (dt.date(2026, 11, 8), "終日落ち着いて過ごした。夕方に物忘れの訴えが1回。",
     4 * 3600 + 40 * 60, 65_000_000, [
         ("10:05", "ご本人", "今日は、何曜日だったかな。"),
         ("10:06", "家族", "日曜日だよ。散歩に行こうか。"),
         ("17:20", "ご本人", "鍵をどこに置いたか、忘れてしまった。"),
     ]),
]


def seed(session: Session) -> None:
    for date, summary, dur, size, lines in DAYS:
        if session.get(Day, date):
            continue
        session.add(Day(
            date=date, summary=summary,
            audio_codec="ADPCM", audio_bitrate_kbps=32,
            audio_duration_sec=dur, audio_size_bytes=size,
            lines=[Line(day_date=date, time=dt.time.fromisoformat(t), who=w, text=x) for t, w, x in lines],
        ))
    if not session.get(DeviceStatus, 1):
        session.add(DeviceStatus(
            recording=True, power_source="solar", battery_percent=82,
            backup_battery="full", stored_days=1.2, capacity_days=3,
        ))
    if not session.get(Settings, 1):
        session.add(Settings())
    if session.get(ClinicSummary, 1) is None:
        session.add(ClinicSummary(
            patient_name="山田 太郎",
            period_start=dt.date(2026, 11, 2), period_end=dt.date(2026, 11, 9),
            concerns=[
                "膝の痛みの訴え: 週5日（朝・午後に多い）",
                "物忘れの訴え: 週2回（鍵・曜日）",
                "服薬の確認: 声かけに「飲んだ」と返答 6/7日",
            ],
        ))
    session.commit()


if __name__ == "__main__":
    init_db()
    with Session(engine) as s:
        seed(s)
    print("seeded")
