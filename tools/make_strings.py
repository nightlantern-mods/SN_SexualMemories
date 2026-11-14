"""Writes the MCM translation files (UTF-16 LE with BOM) of SN Sexual History.

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
    ('$SNSH_ModName', 'SN Sexual History', 'SN Sexual History'),

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
