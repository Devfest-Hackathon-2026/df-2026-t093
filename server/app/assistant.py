"""要約タブのアシスタントの返答を作る。

今はアプリ (ProductUi.kt / ProductViews.swift の SCRIPTS) と同じ固定の台本。
ponytail: Gemini などにつなぐときは reply() の中身だけ差し替える。
その場合は Settings.cloud_enabled が true のときだけ外部に送ること。
"""

from dataclasses import dataclass

GREETING = "今日の記録から、先生に伝えるとよさそうな点を見つけました。下のボタンか、自由な文章で相談してください。"


@dataclass(frozen=True)
class Script:
    chip: str
    answer: str
    memo: str


SCRIPTS = [
    Script(
        "何を伝えるべき？",
        "膝の痛みが今週5日あります。痛む時間帯（朝・午後）と、湿布で和らいだかを伝えると診察が早く進みます。物忘れ（鍵・曜日）も週2回ありました。",
        "膝の痛み: 週5日・朝と午後・湿布で対応",
    ),
    Script(
        "膝の痛みを詳しく",
        "朝食前後と14時ごろに訴えがあり、「ずきずき」という表現でした。痛みの強さを10段階で本人に聞いておくと、先生に伝えやすくなります。",
        "痛みの表現: 「ずきずき」（強さは未確認）",
    ),
    Script(
        "服薬は大丈夫？",
        "19:31に「飲んだよ」と返答があり、今週は6/7日で確認できました。確認できなかった1日は、先生にそのまま伝えましょう。",
        "服薬: 6/7日確認（1日不明）",
    ),
]


def reply(question: str) -> tuple[str, str]:
    """(返答, メモに追加する文) を返す。"""
    for s in SCRIPTS:
        if question.strip() == s.chip:
            return s.answer, s.memo
    q = question.strip()
    return f"「{q}」を診察で伝えるメモに追加しました。（ハリボテ: AIの応答は未接続）", q
