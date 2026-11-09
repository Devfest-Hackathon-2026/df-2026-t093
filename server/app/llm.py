"""Gemini 呼び出し (google-genai SDK)。

認証は SDK の環境変数に任せる:
- Cloud Run (推奨): GOOGLE_GENAI_USE_VERTEXAI=true, GOOGLE_CLOUD_PROJECT, GOOGLE_CLOUD_LOCATION
  → サービスアカウントで Vertex AI を使う。API キー不要。
- ローカル: GEMINI_API_KEY
"""

import logging
import os
from functools import lru_cache

from pydantic import BaseModel

log = logging.getLogger(__name__)

MODEL = os.getenv("GEMINI_MODEL", "gemini-3.8-flash")

SYSTEM = (
    "あなたは高齢者の在宅介護を手伝うアシスタントです。"
    "入力はペンダント型レコーダーで録音した会話の文字起こしです。"
    "話者は「ご本人」「ヘルパー」「家族」などです。"
    "日本語で、やさしく簡潔に書いてください。"
    "記録にない事実を作らないでください。診断や病名の推測はしないでください。"
)


def enabled() -> bool:
    return bool(
        os.getenv("GEMINI_API_KEY")
        or os.getenv("GOOGLE_API_KEY")
        or os.getenv("GOOGLE_GENAI_USE_VERTEXAI", "").lower() in ("1", "true")
    )


@lru_cache
def _client():
    from google import genai

    return genai.Client()


def generate_json[T: BaseModel](prompt: str, schema: type[T], audio: tuple[bytes, str] | None = None) -> T:
    """audio=(データ, MIME) を渡すと、プロンプトと一緒に音声も Gemini に渡す。"""
    from fastapi import HTTPException, status
    from google.genai import types

    try:
        r = _client().models.generate_content(
            model=MODEL,
            contents=prompt if audio is None else [prompt, types.Part.from_bytes(data=audio[0], mime_type=audio[1])],
            config=types.GenerateContentConfig(
                system_instruction=SYSTEM,
                response_mime_type="application/json",
                response_schema=schema,
                temperature=0.3,
            ),
        )
        return schema.model_validate_json(r.text)
    except Exception as e:
        log.exception("Gemini call failed (model=%s)", MODEL)
        raise HTTPException(
            status.HTTP_502_BAD_GATEWAY, f"Gemini error (model={MODEL}): {type(e).__name__}: {e}"[:500]
        ) from e
