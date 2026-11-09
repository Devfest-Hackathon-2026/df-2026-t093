#!/usr/bin/env bash
# Cloud Run へデプロイする。使い方: PROJECT=my-gcp-project bash deploy.sh
set -euo pipefail
cd "$(dirname "$0")"

PROJECT="${PROJECT:?PROJECT=<GCP プロジェクト ID> を指定してください}"
REGION="${REGION:-asia-northeast1}"
SERVICE="${SERVICE:-pendant-api}"
GEMINI_LOCATION="${GEMINI_LOCATION:-global}"
API_TOKEN="${API_TOKEN:-$(openssl rand -hex 24)}"

gcloud config set project "$PROJECT" >/dev/null
gcloud services enable run.googleapis.com cloudbuild.googleapis.com \
  artifactregistry.googleapis.com aiplatform.googleapis.com

# 既定の Compute SA に、ソースからのビルド (run.builder) と Vertex AI (aiplatform.user) の権限を付ける
NUM=$(gcloud projects describe "$PROJECT" --format='value(projectNumber)')
for ROLE in roles/run.builder roles/aiplatform.user; do
  gcloud projects add-iam-policy-binding "$PROJECT" \
    --member="serviceAccount:${NUM}-compute@developer.gserviceaccount.com" \
    --role="$ROLE" --condition=None >/dev/null
done

# SQLite (デモ) なのでインスタンスは 1 台に固定する
gcloud run deploy "$SERVICE" --source . --region "$REGION" \
  --allow-unauthenticated --max-instances 1 --memory 512Mi \
  --set-env-vars "SEED_DEMO=1,API_TOKEN=${API_TOKEN},GOOGLE_GENAI_USE_VERTEXAI=true,GOOGLE_CLOUD_PROJECT=${PROJECT},GOOGLE_CLOUD_LOCATION=${GEMINI_LOCATION}"

URL=$(gcloud run services describe "$SERVICE" --region "$REGION" --format='value(status.url)')
echo
echo "URL:       $URL"
echo "API_TOKEN: $API_TOKEN"
echo "確認:      curl -H \"Authorization: Bearer $API_TOKEN\" $URL/status"
