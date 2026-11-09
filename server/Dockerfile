FROM python:3.12-slim
COPY --from=ghcr.io/astral-sh/uv:0.13.1 /uv /bin/uv

WORKDIR /app
ENV UV_COMPILE_BYTECODE=1 UV_LINK_MODE=copy PYTHONUNBUFFERED=1
COPY pyproject.toml uv.lock ./
RUN uv sync --frozen --no-dev --no-install-project
COPY app ./app

# Cloud Run のファイルは再起動で消える (デモ用。データを残すなら Cloud SQL に)
ENV DATABASE_URL=sqlite:////tmp/app.db PATH="/app/.venv/bin:$PATH"
RUN useradd -m app
USER app
CMD ["sh", "-c", "exec uvicorn app.main:app --host 0.0.0.0 --port ${PORT:-8080}"]
