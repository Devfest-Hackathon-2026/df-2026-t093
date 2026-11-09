from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status
from sqlmodel import Session, select

from app.db import get_session
from app.models import ClinicSummary, ClinicSummaryCreate, ClinicSummaryRead

router = APIRouter(prefix="/clinic-summaries", tags=["clinic"])

SessionDep = Annotated[Session, Depends(get_session)]


@router.post("", response_model=ClinicSummaryRead, status_code=status.HTTP_201_CREATED)
def create_summary(payload: ClinicSummaryCreate, session: SessionDep):
    if payload.period_end < payload.period_start:
        raise HTTPException(status.HTTP_422_UNPROCESSABLE_CONTENT, "period_end must be >= period_start")
    s = ClinicSummary.model_validate(payload)
    session.add(s)
    session.commit()
    session.refresh(s)
    return s


@router.get("/latest", response_model=ClinicSummaryRead)
def latest_summary(session: SessionDep):
    s = session.exec(select(ClinicSummary).order_by(ClinicSummary.id.desc())).first()
    if s is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "No summary yet")
    return s


@router.get("/{summary_id}", response_model=ClinicSummaryRead)
def get_summary(summary_id: int, session: SessionDep):
    s = session.get(ClinicSummary, summary_id)
    if s is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Summary not found")
    return s
