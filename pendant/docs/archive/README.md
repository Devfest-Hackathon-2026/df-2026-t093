# archive

やめた案や、置き換えられた古い調査ノートを、当時のまま置いている。**今の仕様ではない。** 今の仕様は [docs/spec/](../spec/)、決定の経緯は [docs/decisions.md](../decisions.md) を見る。

| ノート | 置いた理由 |
|---|---|
| 45_初期設定フロー雛形_キット版 | 「PC で鍵を作って SWD で書き、QR でスマホへ渡す」フロー。「出荷前に MCUboot を書き、以降は BLE DFU」に置き換えた（security.md §6） |
| 18_ストレージ再検討_1.8V面積電力コスト | 20（現 storage.md）で置き換えた古いストレージ調査。価格は古く、1 GB / 48 h の見積は 4 Gbit とバックログ連動ビットレートに置き換えた |
| 32_NOR_W25Q | Standard の NOR（W25Q256JW）。NOR は価格でも書込みエネルギーでも NAND に負けるので不採用（storage.md §2.2）。`fw/lib/nor` は 2026-10-15 に削除した（git の履歴にある） |
| 24_XIAO低背着脱キャリア | XIAO を同梱するキットのキャリア。XIAO は開発専用にしたので不採用（decisions.md X1）。まだ使える部分は mechanical.md §6 に移した |
| 27_AirTag互換外形_調査ログ | AirTag 互換の外形の調査ログ。結論は mechanical.md §8 |
| 42_JLCPCB部品在庫と単価_Phase1 | JLCPCB の PCBA（実装込み）の見積。自宅でリフロー実装することにしたので不要（prototype.md §6） |
| 25_XIAO無改造とレベル変換コスト | 未改造の XIAO（3.3 V）と 1.8 V の周辺をつなぐレベル変換の検討。XIAO は開発専用になり、製品の基板は 1.8 V で直接動く。結論は board.md §8 |
| 37_レベル変換IC比較 | TXU0204 / TXU0304 などの比較。XIAO の開発環境だけの話になった（board.md §8） |
| 47_ハードウェア構成変更の検討_外部AI指摘の検証 | 外部 AI の指摘の検証記録。採用した案（privacy latch、電池電圧の測定経路、逆流防止の footprint、テストポイント）は board.md §6 に移した |
| 49_XIAOキット案が厳しい理由 | XIAO を同梱するキット案が厳しい理由と、生かせる条件（2026-10-15）。結論は XIAO を開発専用にする（decisions.md X1）。要点は compliance.md に移した |
