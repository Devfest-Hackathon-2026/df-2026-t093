"""DB の記録から Gemini 用のプロンプトを組み立てて、結果を返す。

すべて Settings.cloud_enabled が true のときだけ呼ぶこと (require_cloud)。
"""

import datetime as dt

from fastapi import HTTPException, status
from pydantic import BaseModel, Field
from sqlmodel import Session, select

from app import llm
from app.models import AssistantMessage, Day, Memo, Settings

JST = dt.timezone(dt.timedelta(hours=9))


def require_cloud(session: Session) -> None:
    s = session.get(Settings, 1)
    if s is None or not s.cloud_enabled:
        raise HTTPException(status.HTTP_409_CONFLICT, "クラウド送信がオフです (PATCH /settings で有効化)")
    if not llm.enabled():
        raise HTTPException(status.HTTP_503_SERVICE_UNAVAILABLE, "Gemini が未設定です")


def cloud_ready(session: Session) -> bool:
    s = session.get(Settings, 1)
    return bool(s and s.cloud_enabled and llm.enabled())


def _day_text(day: Day) -> str:
    lines = "\n".join(f"{l.time:%H:%M} {l.who}: {l.text}" for l in day.lines)
    return f"## {day.date.isoformat()}\n要約: {day.summary or '(なし)'}\n{lines or '(会話なし)'}"


def _days(session: Session, start: dt.date, end: dt.date) -> list[Day]:
    stmt = select(Day).where(Day.date >= start, Day.date <= end).order_by(Day.date)
    return list(session.exec(stmt).all())


# ---------- 1日の要約 ----------


class DaySummaryOut(BaseModel):
    summary: str = Field(description="その日の様子を2〜3文で。各文は「。」で終える")


def summarize_day(day: Day) -> str:
    prompt = (
        "次の1日分の会話記録から、家族が読む「今日の要約」を書いてください。"
        "食事・痛みや体調の訴え・服薬・物忘れ・気分など、介護で気にすべき点を優先してください。\n\n"
        + _day_text(day)
    )
    return llm.generate_json(prompt, DaySummaryOut).summary


# ---------- 診療向けサマリー ----------


class ConcernsOut(BaseModel):
    concerns: list[str] = Field(
        description="医師に伝えるべき点。各項目は「項目名: 頻度や時間帯などの事実」の形で1行。最大6項目"
    )


def clinic_concerns(session: Session, start: dt.date, end: dt.date) -> list[str]:
    days = _days(session, start, end)
    if not days:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "対象期間の記録がありません")
    prompt = (
        f"{start}〜{end} の会話記録から、次回の診察で医師に伝えるべき点をまとめてください。"
        "回数や日数は記録から数えられる範囲で書いてください。\n\n"
        + "\n\n".join(_day_text(d) for d in days)
    )
    return llm.generate_json(prompt, ConcernsOut).concerns


# ---------- 要約タブのアシスタント ----------


class AssistantOut(BaseModel):
    answer: str = Field(description="利用者への返答。3文程度")
    memo: str | None = Field(
        description="「診察で伝えるメモ」に追加する1行 (30字程度)。追加するものがなければ null"
    )


class VoiceOut(AssistantOut):
    transcript: str = Field(description="音声の質問を聞き取った文。聞き取れなければ空文字")


def assistant_reply(session: Session, question: str) -> tuple[str, str | None]:
    out = llm.generate_json(_assistant_context(session) + f"\n\n# 今回の質問\n{question}", AssistantOut)
    return out.answer, (out.memo or None)


def assistant_voice(session: Session, audio: bytes, mime: str) -> tuple[str, str, str | None]:
    """音声の質問を Gemini に直接渡す。聞き取り (transcript) と返答を1回の呼び出しで得る。"""
    prompt = _assistant_context(session) + (
        "\n\n# 今回の質問\n添付の音声です。高齢のご本人が話していることがあります。"
        "聞き取った質問を transcript に書き、それに答えてください。"
        "聞き取れなかったときは transcript を空にし、answer でもう一度ゆっくり話してほしいと伝えてください。"
        "返答は読み上げられるので、記号や箇条書きを使わず、話し言葉で短く答えてください。"
    )
    out = llm.generate_json(prompt, VoiceOut, audio=(audio, mime))
    return out.transcript.strip(), out.answer, (out.memo or None)


def _assistant_context(session: Session) -> str:
    today = dt.datetime.now(JST).date()
    days = _days(session, today - dt.timedelta(days=7), today)
    history = session.exec(
        select(AssistantMessage).order_by(AssistantMessage.id.desc()).limit(20)
    ).all()
    memos = session.exec(select(Memo).order_by(Memo.id)).all()
    return (
        "ご本人やご家族が「次の診察で先生に何を伝えるか」をあなたと相談しています。"
        "直近の記録を根拠に答え、伝えるとよい点があれば memo に1行で入れてください。"
        "すでにメモにある内容は memo に入れないでください。\n\n"
        "# 直近7日の記録\n" + ("\n\n".join(_day_text(d) for d in days) or "(なし)") + "\n\n"
        "# 診察で伝えるメモ (現在)\n" + ("\n".join(f"- {m.text}" for m in memos) or "(なし)") + "\n\n"
        "# これまでの会話\n"
        + "\n".join(f"{'利用者' if m.mine else 'あなた'}: {m.text}" for m in reversed(history))
    )
