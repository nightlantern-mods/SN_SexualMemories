"""Writes the MCM translation files (UTF-16 LE with BOM) of SN Sexual Memories.

usage: python -I make_strings.py <en|ja> <Interface/Translations folder>

The game reads only the file of its own language, so the chosen language is written under every
game language's name: the English package shows English whatever the game's language, and the
Japanese one Japanese.
"""
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
]

COLUMNS = {'en': 1, 'ja': 2}


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in COLUMNS:
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


if __name__ == '__main__':
    main()
