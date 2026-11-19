"""Writes the MCM translation files (UTF-16 LE with BOM) of SN Sexual Memories.

usage: python -I make_strings.py <en|ja> <Interface/Translations folder>

The game reads only the file of its own language, so the chosen language is written under every
game language's name: the English package shows English whatever the game's language, and the
Japanese one Japanese.
"""
import json
import os
import sys

LANGUAGES = ['CHINESE', 'CZECH', 'ENGLISH', 'FRENCH', 'GERMAN', 'ITALIAN', 'JAPANESE', 'POLISH', 'RUSSIAN', 'SPANISH']

# key, English, Japanese
STRINGS = [
    ('$SNSH_ModName', 'SN Sexual Memories', 'SN Sexual Memories'),

    ('$SNSH_HeaderMemories', 'Memories', '記憶'),
    ('$SNSH_Witness', 'A witness remembers too', '目撃者の記憶も作る'),
    ('$SNSH_WitnessHelp',
     'When a scene ends, one bystander who saw it (picked at random among the NPCs nearby with the scene in sight, never a child) also gets a memory of what they saw. When nobody saw it, nobody does.',
     'シーンが終わったとき、それを見ていた近くのNPC（シーンが視界に入っていた人から無作為に1人。子どもは除く）にも、見た内容の記憶が作られます。見ていた人がいなければ作られません。'),

    ('$SNSH_HeaderComments', 'Lines during scenes', 'シーン中の発言'),
    ('$SNSH_Comments', 'NPCs speak during scenes', 'シーン中のNPCが発言する'),
    ('$SNSH_CommentsHelp',
     'Every so often an NPC who is in a running OStim or SexLab scene says a line about what is happening. SkyrimNet writes and voices the line.',
     '再生中のOStimまたはSexLabのシーンに参加しているNPCが、ときどきその場の状況について発言します。台詞は SkyrimNet が作って声に出します。'),
    ('$SNSH_Interval', 'Time between checks (seconds)', '確認の間隔（秒）'),
    ('$SNSH_IntervalHelp',
     'How often running scenes are looked for. Each check lets one NPC speak; with several scenes they take turns. Nothing is said while SkyrimNet is still speaking.',
     '再生中のシーンを探す間隔です。1回の確認で発言するNPCは1人で、シーンが複数あるときは順番に回ります。SkyrimNet が話している間は発言しません。'),
    ('$SNSH_PlayerScene', 'Also in your own scenes', 'プレイヤーのシーンも対象にする'),
    ('$SNSH_PlayerSceneHelp',
     'On: your partners in a scene you take part in speak too. Off: only scenes between NPCs.',
     'オン：あなたが参加しているシーンでも、相手のNPCが発言します。オフ：NPC同士のシーンだけが対象です。'),
    ('$SNSH_HearRadius', 'Hearing distance', '聞こえる距離'),
    ('$SNSH_HearRadiusHelp',
     'Only NPCs within this distance of you speak.',
     'あなたからこの距離以内にいるNPCだけが発言します。'),

    ('$SNSH_HeaderComplaints', 'Complaints about a partner', '夫・妻・恋人への不満'),
    ('$SNSH_Complaints', 'NPCs complain about their partner', '夫・妻・恋人への不満を言う'),
    ('$SNSH_ComplaintsHelp',
     'Now and then an NPC near you who has a husband, wife or lover complains to you about them, written and voiced by SkyrimNet. They only do it while that partner is too far away to overhear.',
     'ときどき、近くにいる夫・妻・恋人のいるNPCが、あなたに相手への不満をこぼします。台詞は SkyrimNet が作って声に出します。相手に聞こえる距離にいるときは言いません。'),
    ('$SNSH_ComplaintInterval', 'Time between chances (seconds)', 'きっかけの間隔（秒）'),
    ('$SNSH_ComplaintIntervalHelp',
     'How often a chance comes. Each chance, one NPC near you who has a partner out of earshot may complain; when nobody fits, nothing is said.',
     'きっかけが来る間隔です。そのたびに、相手が聞こえない所にいるNPCが近くにいれば、1人が不満を言います。当てはまる人がいなければ何も言いません。'),
    ('$SNSH_ComplaintSex', 'Who complains', '不満を言う人'),
    ('$SNSH_ComplaintSexHelp', 'Which NPCs may complain: anyone, only women, or only men.', '不満を言うNPCの性別です。両方、女性だけ、男性だけから選べます。'),
    ('$SNSH_SexAnyone', 'Women and men', '女性と男性'),
    ('$SNSH_SexWomen', 'Women only', '女性だけ'),
    ('$SNSH_SexMen', 'Men only', '男性だけ'),

    ('$SNSH_HeaderPlayerTalk', 'Your character speaks up', 'プレイヤーの話しかけ'),
    ('$SNSH_PlayerTalk', 'Speak to people nearby', '近くの人に話しかける'),
    ('$SNSH_PlayerTalkHelp',
     'Now and then your character says something to an NPC near you, drawing on what just happened, on what your character remembers (about that person above all) and on how the two of you stand. SkyrimNet writes the line, voices it if it voices your character, and the NPC answers.',
     'ときどき、あなたのキャラクターが近くのNPCに話しかけます。直前の出来事、あなたのキャラクターの記憶（特にその相手についての記憶）、相手との関係をもとに、SkyrimNet が台詞を作ります。プレイヤーの声が設定されていれば読み上げ、相手のNPCが返事をします。'),
    ('$SNSH_PlayerTalkInterval', 'Time between chances (seconds)', 'きっかけの間隔（秒）'),
    ('$SNSH_PlayerTalkIntervalHelp',
     'How often a chance comes. Nothing is said in combat, in a menu, during a scene or while SkyrimNet is speaking.',
     'きっかけが来る間隔です。戦闘中、メニューを開いているとき、シーン中、SkyrimNet が話している間は話しかけません。'),

    ('$SNSH_HeaderMap', 'Relationship map', '相関図'),
    ('$SNSH_MapKey', 'Open the relationship map', '相関図を開く'),
    ('$SNSH_MapKeyHelp',
     'Opens the relationship map: choose yourself or an NPC nearby (the one under your crosshair is chosen first), and SkyrimNet\'s AI maps what lies between them and up to ten people around you (you too, when an NPC is chosen), from their memories, diaries and events. Needs Magelight UI or Prisma UI; with Phantasm, faces are shown. Its AI model is set in SkyrimNet\'s plugin settings (SNSexualMemories).',
     '相関図を開きます。自分か近くのNPCを選ぶと（照準の先の人が最初から選ばれます）、その人と周りにいる最大10人（NPCを選んだときは自分も含む）との関係を、記憶・日記・イベントから SkyrimNet のAIがまとめます。Magelight UI か Prisma UI が必要です。Phantasm があれば顔写真も出ます。Magelight UI か Prisma UI が必要です。使うAIモデルは SkyrimNet のプラグイン設定（SNSexualMemories）で選べます。'),
    ('$SNSH_MapMemoryScan', 'Memories read per person', '1人あたり読む記憶の数'),
    ('$SNSH_MapMemoryScanHelp',
     'How many of each person\'s memories are read, most important first, to find the ones that name the others. More finds older ties; it takes a little longer.',
     '相手の名前が出てくる記憶を探すために、1人あたり何件の記憶を読むかです（重要度の高い順）。多くすると昔の関わりも拾えますが、少し時間がかかります。'),
    ('$SNSH_MapMemoriesUsed', 'Memories given per pair', '組ごとに渡す記憶の数'),
    ('$SNSH_MapMemoriesUsedHelp',
     'How many memories naming the other are given to the AI for each pair, in each direction. More gives a fuller description and costs more.',
     'ふたりの組ごとに、相手の名前が出てくる記憶を片側あたり何件AIに渡すかです。多いほど詳しくなりますが、AIの利用料金が増えます。'),
    ('$SNSH_MapEventScan', 'Latest events read', '読む直近のイベントの数'),
    ('$SNSH_MapEventScanHelp',
     'How many of the centre\'s latest SkyrimNet events are read to find the ones that name each person.',
     '中心人物の直近のイベントを何件読んで、各人の名前が出てくるものを探すかです。'),
    ('$SNSH_MapDiaryScan', 'Diary entries read per person', '1人あたり読む日記の数'),
    ('$SNSH_MapDiaryScanHelp',
     'How many of each person\'s latest diary entries are read; the passages that name the other are given to the AI. 0 leaves diaries out.',
     '1人あたり新しい日記を何件読むかです。相手の名前が出てくる部分をAIに渡します。0 にすると日記を使いません。'),
    ('$SNSH_MapTextChars', 'Length kept of each record (characters)', '1件あたりに使う長さ（文字）'),
    ('$SNSH_MapTextCharsHelp',
     'How many characters of each memory, event or diary passage the AI sees. Longer keeps the feelings and outcomes at the end of a memory, and costs more.',
     '記憶・イベント・日記の1件あたり、何文字までAIに渡すかです。長いほど記憶の後半の気持ちや結末まで伝わりますが、AIの利用料金が増えます。'),
    ('$SNSH_MapNoUI', 'The relationship map needs Magelight UI or Prisma UI.', '相関図には Magelight UI か Prisma UI が必要です。'),
]

# The relationship map page's texts (SKSE/Plugins/SN_SexualHistory/strings.json): key, English, Japanese.
UI_STRINGS = [
    ('title', 'Relationship map', '相関図'),
    ('you', 'you', 'あなた'),
    ('refresh', 'Refresh', '更新'),
    # Remakes the map in English. Empty in English: the button is hidden there (Refresh does it).
    ('english', '', '英語で出力'),
    ('close', 'Close', '閉じる'),
    ('choose', 'Choose a person on the left.', '左の一覧から人物を選んでください。'),
    ('loading', 'The AI is drawing the relationship map of {name}. This can take a minute.', '{name} の相関図をAIが作成しています。1分ほどかかることがあります。'),
    ('generated', 'Made {date}', '作成: {date}'),
    ('makeCenter', 'Put {name} in the centre', '{name} を中心にする'),
    ('circle', 'Their circle', '人間関係のまとめ'),
    ('feelings', 'Feelings', 'お互いの気持ち'),
    ('history', 'What they went through', '主な出来事'),
    ('follower', 'follower', '仲間'),
    ('links', '{name} and the others in the circle', '{name}と周りの人たち'),
    ('noLinks', 'No ties drawn.', '関係は描かれていません。'),
    ('noRecord', 'Little about the two of them was found in the records read. That does not mean there is nothing between them.', '読んだ記録の中には、ふたりのやり取りがあまり見つかりませんでした。関わりがなかったという意味ではありません。'),
    ('error_ai', 'The AI could not draw the map. Try Refresh, or check the model in SkyrimNet\'s plugin settings.', 'AIが相関図を作れませんでした。「更新」を押すか、SkyrimNet のプラグイン設定でモデルを確認してください。'),
    ('error_skyrimnet', 'SkyrimNet is not ready yet. Load a save first.', 'SkyrimNet の準備ができていません。先にセーブを読み込んでください。'),
    ('error_nobody', 'There is nobody near you to put on the map.', '近くに、相関図に載せられる人がいません。'),
    ('error_busy', 'Another map is still being drawn. Wait a moment.', '別の相関図を作成中です。少し待ってください。'),
    ('tone_positive', 'warm', '好意'),
    ('tone_negative', 'hostile', '敵意'),
    ('tone_mixed', 'mixed', '複雑'),
    ('tone_neutral', 'neutral', '普通'),
]

COLUMNS = {'en': 1, 'ja': 2}


def main():
    # usage: make_strings.py <en|ja> <Interface/Translations folder> [<strings.json path>]
    if len(sys.argv) not in (3, 4) or sys.argv[1] not in COLUMNS:
        sys.exit(__doc__)
    column = COLUMNS[sys.argv[1]]
    out = sys.argv[2]
    keys = [row[0] for row in STRINGS]
    assert len(keys) == len(set(keys)), 'duplicate key'
    os.makedirs(out, exist_ok=True)
    lines = [row[0] + '\t' + row[column] for row in STRINGS]
    for language in LANGUAGES:
        path = os.path.join(out, 'SN_SexualHistory_' + language + '.txt')
        with open(path, 'w', encoding='utf-16', newline='') as handle:
            handle.write('\r\n'.join(lines) + '\r\n')
    print(out, sys.argv[1], len(lines), 'strings x', len(LANGUAGES), 'files')
    if len(sys.argv) == 4:
        ui = {row[0]: row[column] for row in UI_STRINGS}
        assert len(ui) == len(UI_STRINGS), 'duplicate UI key'
        os.makedirs(os.path.dirname(sys.argv[3]), exist_ok=True)
        with open(sys.argv[3], 'w', encoding='utf-8', newline='\n') as handle:
            json.dump(ui, handle, ensure_ascii=False, indent=1)
            handle.write('\n')
        print(sys.argv[3], len(ui), 'UI strings')


if __name__ == '__main__':
    main()
