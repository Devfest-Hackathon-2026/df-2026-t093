from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status
from sqlmodel import Session, delete, select

from app import ai, assistant
from app.db import get_session
from app.models import (
    AskRequest,
    AskResponse,
    AssistantMessage,
    AssistantMessageRead,
    Memo,
    MemoCreate,
    MemoRead,
)

router = APIRouter(tags=["assistant"])

SessionDep = Annotated[Session, Depends(get_session)]


def _memos(session: Session) -> list[Memo]:
    return list(session.exec(select(Memo).order_by(Memo.id)).all())


def _add_memo(session: Session, text: str) -> None:
    if session.exec(select(Memo).where(Memo.text == text)).first() is None:
        session.add(Memo(text=text))


# ---------- 会話 ----------


@router.get("/assistant/suggestions", response_model=list[str])
def suggestions():
    """入力欄の上に並べる提案チップ。"""
    return [s.chip for s in assistant.SCRIPTS]


@router.get("/assistant/messages", response_model=list[AssistantMessageRead])
def list_messages(session: SessionDep):
    """会話の履歴。空なら最初の挨拶だけを返す (保存はしない)。"""
    msgs = session.exec(select(AssistantMessage).order_by(AssistantMessage.id)).all()
    if not msgs:
        return [AssistantMessage(id=0, mine=False, text=assistant.GREETING)]
    return msgs


@router.post("/assistant/messages", response_model=AskResponse, status_code=status.HTTP_201_CREATED)
def ask(payload: AskRequest, session: SessionDep):
    """クラウド送信がオンで Gemini が設定済みなら Gemini、そうでなければアプリと同じ台本で返す。"""
    if ai.cloud_ready(session):
        answer_text, memo_text = ai.assistant_reply(session, payload.text)
    else:
        answer_text, memo_text = assistant.reply(payload.text)
    q = AssistantMessage(mine=True, text=payload.text)
    a = AssistantMessage(mine=False, text=answer_text)
    session.add(q)
    session.add(a)
    if memo_text:
        _add_memo(session, memo_text)
    session.commit()
    session.refresh(q)
    session.refresh(a)
    return AskResponse(
        question=AssistantMessageRead.model_validate(q),
        answer=AssistantMessageRead.model_validate(a),
        memos=[MemoRead.model_validate(m) for m in _memos(session)],
    )


@router.delete("/assistant/messages", status_code=status.HTTP_204_NO_CONTENT)
def clear_messages(session: SessionDep):
    session.exec(delete(AssistantMessage))
    session.commit()


# ---------- 診察で伝えるメモ ----------


@router.get("/memos", response_model=list[MemoRead])
def list_memos(session: SessionDep):
    return _memos(session)


@router.post("/memos", response_model=list[MemoRead], status_code=status.HTTP_201_CREATED)
def create_memo(payload: MemoCreate, session: SessionDep):
    """手でメモを足す。同じ文面がすでにあれば何もしない。追加後の一覧を返す。"""
    _add_memo(session, payload.text)
    session.commit()
    return _memos(session)


@router.delete("/memos/{memo_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_memo(memo_id: int, session: SessionDep):
    m = session.get(Memo, memo_id)
    if m is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Memo not found")
    session.delete(m)
    session.commit()
