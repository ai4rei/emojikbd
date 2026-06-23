
""" Re-generates the emoji sqlite3 database based on current Unicode data files.
"""
import argparse
import os
import re
import sqlite3
import urllib.request
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from typing import Union

# from tqdm import tqdm

URL_EMOJI_LIST = r'https://unicode.org/Public/emoji/latest/emoji-test.txt'
URL_EMOJI_LANN_FMT = r'https://github.com/unicode-org/cldr/raw/refs/heads/main/common/annotations/{}.xml'
URL_EMOJI_LAND_FMT = r'https://github.com/unicode-org/cldr/raw/refs/heads/main/common/annotationsDerived/{}.xml'
LOCAL_EMOJI_LIST = r'emoji-test.txt'
LOCAL_EMOJI_LANN_FMT = r'emoji-lann-{}.xml'
LOCAL_EMOJI_LAND_FMT = r'emoji-land-{}.xml'
LOCAL_EMOJI_BASE = r'emojitab.db'
DEFAULT_LOCALE = 'en'

""" Database Structure:
    ╔═══════════════╗    ╔══════════╗
    ║ emoji_keyword ║    ║ emoji    ║
    ╟───────────────╢    ╟──────────╢  ╔══════════╗
    ║ emoji         ║───►║ id [pk]  ║  ║ subgroup ║  ╔═════════╗
    ║ keyword       ║─┐  ║ cp       ║  ╟──────────╢  ║ group   ║
    ╚═══════════════╝ │  ║ subgroup ║─►║ id [pk]  ║  ╟─────────╢
                      │  ║ order    ║  ║ group    ║─►║ id [pk] ║
                      │  ║ tts      ║  ║ name     ║  ║ name    ║
                      │  ║ version  ║  ╚══════════╝  ╚═════════╝
                      │  ║ label    ║
                      │  ╚══════════╝
                      │  ╔═════════╗
                      │  ║ keyword ║
                      │  ╟─────────╢
                      └─►║ id [pk] ║
                         ║ name    ║
                         ╚═════════╝
"""

DB_CREATE_TABLES = [
    """
    CREATE TABLE IF NOT EXISTS `group` (
        `id` INTEGER NOT NULL,
        `name` TEXT NOT NULL,
        PRIMARY KEY(`id`),
        UNIQUE(`name`)
    ) STRICT
    """,
    """
    CREATE TABLE IF NOT EXISTS `subgroup` (
        `id` INTEGER NOT NULL,
        `group` INTEGER NOT NULL,
        `name` TEXT NOT NULL,
        PRIMARY KEY(`id`),
        UNIQUE(`group`,`name`),
        FOREIGN KEY (`group`) REFERENCES `group` (`id`) ON DELETE CASCADE
    ) STRICT
    """,
    """
    CREATE TABLE IF NOT EXISTS `emoji` (
        `id` INTEGER NOT NULL,
        `cp` TEXT NOT NULL,
        `subgroup` INTEGER NOT NULL,
        `order` INTEGER NOT NULL,
        `tts` TEXT NOT NULL,
        `version` REAL NOT NULL,
        `label` TEXT NOT NULL,
        PRIMARY KEY(`id`),
        UNIQUE(`cp`),
        UNIQUE(`order`),
        FOREIGN KEY (`subgroup`) REFERENCES `subgroup` (`id`) ON DELETE CASCADE
    ) STRICT
    """,
    """
    CREATE TABLE IF NOT EXISTS `keyword` (
        `id` INTEGER NOT NULL,
        `name` TEXT NOT NULL,
        PRIMARY KEY(`id`),
        UNIQUE(`name`)
    ) STRICT
    """,
    """
    CREATE TABLE IF NOT EXISTS `emoji_keyword` (
        `emoji` INTEGER NOT NULL,
        `keyword` INTEGER NOT NULL,
        UNIQUE(`emoji`,`keyword`),
        FOREIGN KEY (`emoji`) REFERENCES `emoji` (`id`) ON DELETE CASCADE,
        FOREIGN KEY (`keyword`) REFERENCES `keyword` (`id`) ON DELETE CASCADE
    ) STRICT
    """
]
DB_DELETE_TABLES = [
    "DROP TABLE IF EXISTS `emoji_keyword`",
    "DROP TABLE IF EXISTS `keyword`",
    "DROP TABLE IF EXISTS `emoji`",
    "DROP TABLE IF EXISTS `subgroup`",
    "DROP TABLE IF EXISTS `group`",
]
DB_INSERT_GROUP = "INSERT INTO `group` (`name`) VALUES (?)"
DB_INSERT_SUBGROUP = "INSERT INTO `subgroup` (`group`, `name`) VALUES (?, ?)"
DB_INSERT_EMOJI = "INSERT INTO `emoji` (`cp`, `subgroup`, `order`, `tts`, `version`, `label`) "\
                  "VALUES (?, ?, ?, ?, ?, ?)"
DB_INSERT_KEYWORD = "INSERT INTO `keyword` (`name`) VALUES (?)"
DB_INSERT_EMOJI_KEYWORD = "INSERT INTO `emoji_keyword` (`emoji`, `keyword`) VALUES (?, ?)"

@dataclass
class EmojiInfo:
    codepoints: list[str]
    order: int
    version: float
    label: str

EmojiList = dict[str, dict[str, dict[str, EmojiInfo]]]
EmojiMeta = dict[str, dict[str, Union[str, list[str]]]]

def _get_local_path() -> str:
    path, _ = os.path.split(__file__)
    if not path:
        raise NameError('cannot figure out current script path', __file__)
    return path

def _get_local_emoji_list_name() -> str:
    return LOCAL_EMOJI_LIST

def _get_local_emoji_lann_name(locale: str) -> str:
    return LOCAL_EMOJI_LANN_FMT.format(locale)

def _get_local_emoji_land_name(locale: str) -> str:
    return LOCAL_EMOJI_LAND_FMT.format(locale)

def _get_remote_emoji_list_name() -> str:
    return URL_EMOJI_LIST

def _get_remote_emoji_lann_name(locale: str) -> str:
    return URL_EMOJI_LANN_FMT.format(locale)

def _get_remote_emoji_land_name(locale: str) -> str:
    return URL_EMOJI_LAND_FMT.format(locale)

def _get_data_location(basename: str) -> str:
    return os.path.join(_get_local_path(), basename)

def _download_data(remote_name: str, local_name: str) -> bool:
    print(f'Downloading {remote_name}')
    with open(_get_data_location(local_name), 'xb') as local_file:
        with urllib.request.urlopen(remote_name) as response:
            local_file.write(response.read())

def _have_data(basename: str) -> bool:
    return os.path.isfile(_get_data_location(basename))

def _load_unicode_data(locale: str):
    if not _have_data(_get_local_emoji_list_name()):
        _download_data(_get_remote_emoji_list_name(), _get_local_emoji_list_name())
    if not _have_data(_get_local_emoji_lann_name(locale)):
        _download_data(_get_remote_emoji_lann_name(locale), _get_local_emoji_lann_name(locale))
    if not _have_data(_get_local_emoji_land_name(locale)):
        _download_data(_get_remote_emoji_land_name(locale), _get_local_emoji_land_name(locale))

def _verify_codepoints(emoji: str, codepoints: list[str]) -> bool:
    ref_emoji = ''.join([chr(int(cp, 16)) for cp in codepoints])
    return ref_emoji == emoji

def _parse_emoji_list() -> EmojiList:
    emoji_list = {}
    order_id = 0

    with open(_get_data_location(_get_local_emoji_list_name()), 'r', encoding='UTF-8') as txt_file:
        current_group = ''
        current_subgroup = ''
        matcher =re.compile(r'^([^;]+); (component|(?:un|(?:fully|minimally)-)qualified) +# ([^ ]+) E(\d+\.\d+) (.+)$')
        for line in txt_file:
            line = line.rstrip('\r\n')
            if not line:
                continue
            if line[0] == '#':
                if line.startswith('# group: '):
                    current_group = line[9:]
                    emoji_list[current_group] = {}
                elif line.startswith('# subgroup: '):
                    current_subgroup = line[12:]
                    emoji_list[current_group][current_subgroup] = {}
                elif line == '#EOF':
                    break
                continue
            matches = matcher.match(line)
            if matches is None:
                raise ValueError('cannot parse emoji list line', line)
            if not current_group or not current_subgroup:
                raise ValueError('emoji without group or subgroup', line)
            status = matches[2]
            if status != 'fully-qualified':
                continue
            emoji = matches[3]
            codepoints = matches[1].strip(' ').split(' ')
            if not _verify_codepoints(emoji, codepoints):
                raise ValueError('emoji does not correspond to codepoint literals')
            emoji_list[current_group][current_subgroup][emoji] = EmojiInfo(
                codepoints,
                order_id,
                float(matches[4]),
                matches[5],
            )
            order_id += 1

    return emoji_list

def _parse_emoji_localized_annotation(local_name: str) -> EmojiMeta:
    emoji_meta = {}

    with open(_get_data_location(local_name), 'rb') as xml_file:
        tree = ET.parse(xml_file)
        for annotation in tree.getroot().findall('annotations/annotation'):
            codepoint = annotation.get('cp')
            if codepoint not in emoji_meta:
                emoji_meta[codepoint] = {}
            atype = annotation.get('type', '')
            if atype == 'tts':
                emoji_meta[codepoint]['tts'] = annotation.text
            elif atype =='':
                emoji_meta[codepoint]['kw'] = annotation.text.split(' | ')

    return emoji_meta

def _parse_emoji_localized_annotations(locale: str) -> EmojiMeta:
    emoji_meta = {}
    emoji_meta |= _parse_emoji_localized_annotation(_get_local_emoji_lann_name(locale))
    emoji_meta |= _parse_emoji_localized_annotation(_get_local_emoji_land_name(locale))
    return emoji_meta

def _save_emoji_database(emoji_list: EmojiList, emoji_meta: EmojiMeta):
    keyword_ids = {}
    with sqlite3.connect(_get_data_location(LOCAL_EMOJI_BASE)) as backend:
        cur = backend.cursor()
        for stmt in DB_DELETE_TABLES:
            cur.execute(stmt)
        for stmt in DB_CREATE_TABLES:
            cur.execute(stmt)
        for group, subgroups in emoji_list.items():
            cur.execute(DB_INSERT_GROUP, (group,))
            group_id = cur.lastrowid
            for subgroup, emojis in subgroups.items():
                cur.execute(DB_INSERT_SUBGROUP, (group_id, subgroup))
                subgroup_id = cur.lastrowid
                for emoji, info in emojis.items():
                    meta = emoji_meta.get(emoji, {})
                    tts = meta.get('tts', '')
                    if not tts:
                        tts = info.label
                    keywords = meta.get('kw', [])
                    cur.execute(DB_INSERT_EMOJI, (emoji, subgroup_id, info.order, tts, info.version, info.label))
                    emoji_id = cur.lastrowid
                    for keyword in keywords:
                        if keyword not in keyword_ids:
                            cur.execute(DB_INSERT_KEYWORD, (keyword,))
                            keyword_id = cur.lastrowid
                            keyword_ids[keyword] = keyword_id
                        else:
                            keyword_id = keyword_ids[keyword]
                        cur.execute(DB_INSERT_EMOJI_KEYWORD, (emoji_id, keyword_id))
        cur.close()
        backend.commit()

def _main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--locale', default=DEFAULT_LOCALE, help='specifies the locale for emoji annotations')
    argp = parser.parse_args()

    print('Loading data files...')
    _load_unicode_data(argp.locale)

    print('Parsing emoji list...')
    emoji_list = _parse_emoji_list()

    print('Parsing emoji localization data...')
    emoji_meta = _parse_emoji_localized_annotations(argp.locale)

    print('Saving database...')
    _save_emoji_database(emoji_list, emoji_meta)

    print('Done.')

if __name__ == '__main__':
    _main()
