import datetime as dt
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlmodel import Session, select

from app import ai
from app.db import get_session
from app.models import Day, DayRead, DayUpsert, Line, LineCreate, now

router = APIRouter(prefix="/days", tags=["logs"])

SessionDep = Annotated[Session, Depends(get_session)]


def _get_or_404(session: Session, date: dt.date) -> Day:
    day = session.get(Day, date)
    if day is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Day not found")
    return day


@router.get("", response_model=list[DayRead])
def list_days(
    session: SessionDep,
    since: dt.date | None = None,
    until: dt.date | None = None,
    limit: Annotated[int, Query(ge=1, le=100)] = 14,
):
    """新しい日付順。ログタブの一覧用。"""
    stmt = select(Day)
    if since is not None:
        stmt = stmt.where(Day.date >= since)
    if until is not None:
        stmt = stmt.where(Day.date <= until)
    return session.exec(stmt.order_by(Day.date.desc()).limit(limit)).all()


@router.get("/{date}", response_model=DayRead)
def get_day(date: dt.date, session: SessionDep):
    return _get_or_404(session, date)


@router.put("/{date}", response_model=DayRead)
def upsert_day(date: dt.date, payload: DayUpsert, session: SessionDep):
    """要約・録音情報を作成または更新する (送ったフィールドだけ更新)。"""
    day = session.get(Day, date) or Day(date=date)
    day.sqlmodel_update(payload.model_dump(exclude_unset=True))
    day.updated_at = now()
    session.add(day)
    session.commit()
    session.refresh(day)
    return day


@router.post("/{date}/lines", response_model=DayRead, status_code=status.HTTP_201_CREATED)
def add_lines(date: dt.date, payload: list[LineCreate], session: SessionDep):
    """文字起こしの行をまとめて追加する。その日がなければ作る。"""
    day = session.get(Day, date) or Day(date=date)
    for line in payload:
        day.lines.append(Line.model_validate(line, update={"day_date": date}))
    day.updated_at = now()
    session.add(day)
    session.commit()
    session.refresh(day)
    return day


@router.post("/{date}/summarize", response_model=DayRead)
def summarize_day(date: dt.date, session: SessionDep):
    """その日の文字起こしから Gemini で要約を作って保存する (クラウド送信オンのときだけ)。"""
    ai.require_cloud(session)
    day = _get_or_404(session, date)
    if not day.lines:
        raise HTTPException(status.HTTP_422_UNPROCESSABLE_CONTENT, "文字起こしがありません")
    day.summary = ai.summarize_day(day)
    day.updated_at = now()
    session.add(day)
    session.commit()
    session.refresh(day)
    return day


@router.delete("/{date}", status_code=status.HTTP_204_NO_CONTENT)
def delete_day(date: dt.date, session: SessionDep):
    session.delete(_get_or_404(session, date))
    session.commit()
