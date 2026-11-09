from contextlib import asynccontextmanager

from fastapi import FastAPI

from app.db import init_db
from app.routers import assistant, clinic, days, device


@asynccontextmanager
async def lifespan(app: FastAPI):
    init_db()
    yield


app = FastAPI(title="ks-server (LifeOS Pendant)", lifespan=lifespan)
app.include_router(device.router)
app.include_router(assistant.router)
app.include_router(days.router)
app.include_router(clinic.router)


@app.get("/health")
def health():
    return {"status": "ok"}
