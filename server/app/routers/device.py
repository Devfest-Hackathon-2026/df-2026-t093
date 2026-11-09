from typing import Annotated

from fastapi import APIRouter, Depends
from sqlmodel import Session

from app.db import get_session
from app.models import (
    DeviceStatus,
    DeviceStatusRead,
    DeviceStatusUpdate,
    Settings,
    SettingsRead,
    SettingsUpdate,
    now,
)

router = APIRouter(tags=["device"])

SessionDep = Annotated[Session, Depends(get_session)]


def _singleton[T](session: Session, model: type[T]) -> T:
    obj = session.get(model, 1)
    if obj is None:
        obj = model(id=1)
        session.add(obj)
        session.commit()
        session.refresh(obj)
    return obj


@router.get("/status", response_model=DeviceStatusRead)
def get_status(session: SessionDep):
    return _singleton(session, DeviceStatus)


@router.patch("/status", response_model=DeviceStatusRead)
def update_status(payload: DeviceStatusUpdate, session: SessionDep):
    st = _singleton(session, DeviceStatus)
    st.sqlmodel_update(payload.model_dump(exclude_unset=True))
    st.updated_at = now()
    session.add(st)
    session.commit()
    session.refresh(st)
    return st


@router.get("/settings", response_model=SettingsRead)
def get_settings(session: SessionDep):
    return _singleton(session, Settings)


@router.patch("/settings", response_model=SettingsRead)
def update_settings(payload: SettingsUpdate, session: SessionDep):
    s = _singleton(session, Settings)
    if payload.cloud_enabled and not s.cloud_enabled:
        s.cloud_consented_at = now()  # 確認ダイアログで「理解してオンにする」を押した時刻
    elif not payload.cloud_enabled:
        s.cloud_consented_at = None
    s.cloud_enabled = payload.cloud_enabled
    session.add(s)
    session.commit()
    session.refresh(s)
    return s
