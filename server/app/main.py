import os
import secrets
from contextlib import asynccontextmanager

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from app.db import init_db
from app.routers import assistant, clinic, days, device


@asynccontextmanager
async def lifespan(app: FastAPI):
    init_db()
    if os.getenv("SEED_DEMO") == "1":
        from sqlmodel import Session

        from app.db import engine
        from app.seed import seed

        with Session(engine) as s:
            seed(s)
    yield


app = FastAPI(title="ks-server (LifeOS Pendant)", lifespan=lifespan)
API_TOKEN = os.getenv("API_TOKEN")
PUBLIC = {"/health", "/docs", "/openapi.json"}


@app.middleware("http")
async def bearer_auth(request: Request, call_next):
    """デモ用の共有トークン。ponytail: 本番は Firebase Auth の ID トークン検証に置き換える。"""
    if API_TOKEN and request.url.path not in PUBLIC:
        got = request.headers.get("authorization", "")
        if not secrets.compare_digest(got, f"Bearer {API_TOKEN}"):
            return JSONResponse({"detail": "Unauthorized"}, status_code=401)
    return await call_next(request)


app.include_router(device.router)
app.include_router(assistant.router)
app.include_router(days.router)
app.include_router(clinic.router)


@app.get("/health")
def health():
    return {"status": "ok"}
