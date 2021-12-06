"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

const startTime = Date.now();
let socket = null;
let websocketConnected = false;
let websocketTimer = null;
let websocketKeepAliveTimer = null;
let currentSong = '';
let currentSongObj = {};
let currentState = {};
let settings = {"loglevel": 2};
let settingsParsed = 'no';
let progressTimer = null;
let deferredA2HSprompt;
let dragSrc;
let dragEl;
let showSyncedLyrics = false;
let scrollSyncedLyrics = true;
let appInited = false;
let scriptsInited = false;
let subdir = '';
let uiEnabled = true;
let locale = navigator.language || navigator.userLanguage;
let scale = '1.0';
const isMobile = /iPhone|iPad|iPod|Android/i.test(navigator.userAgent);
const hasIO = 'IntersectionObserver' in window ? true : false;
const ligatureMore = 'menu';
const progressBarTransition = 'width 1s linear';
const smallSpace = '\u2009';
const nDash = '\u2013';
let tagAlbumArtist = 'AlbumArtist';
const albumFilters = ["Composer", "Performer", "Conductor", "Ensemble"];
const session = {"token": "", "timeout": 0};
const sessionLifetime = 1780;
const sessionRenewInterval = sessionLifetime * 500;
let sessionTimer = null;
const messages = [];

//minimum mpd version to support all myMPD features
const mpdVersion = {
    "major": 0,
    "minor": 23,
    "patch": 5
};

//remember offset for filesystem browsing uris
const browseFilesystemHistory = {};

//list of stickers
const stickerList = ['stickerPlayCount', 'stickerSkipCount', 'stickerLastPlayed',
    'stickerLastSkipped', 'stickerLike'];

//application state
const app = {};
app.cards = {
    "Home": {
        "offset": 0,
        "limit": 100,
        "filter": "-",
        "sort": "-",
        "tag": "-",
        "search": "",
        "scrollPos": 0
    },
    "Playback": {
        "offset": 0,
        "limit": 100,
        "filter": "-",
        "sort": "-",
        "tag": "-",
        "search": "",
        "scrollPos": 0
    },
    "Queue": {
        "active": "Current",
        "tabs": {
            "Current": {
                "offset": 0,
                "limit": 100,
                "filter": "any",
                "sort": "-",
                "tag": "-",
                "search": "",
                "scrollPos": 0
            },
            "LastPlayed": {
                "offset": 0,
                "limit": 100,
                "filter": "any",
                "sort": "-",
                "tag": "-",
                "search": "",
                "scrollPos": 0
            },
            "Jukebox": {
                "offset": 0,
                "limit": 100,
                "filter": "any",
                "sort": "-",
                "tag": "-",
                "search": "",
                "scrollPos": 0
            }
        }
    },
    "Browse": {
        "active": "Database",
        "tabs": {
            "Filesystem": {
                "offset": 0,
                "limit": 100,
                "filter": "-",
                "sort": "-",
                "tag": "-",
                "search": "",
                "scrollPos": 0
            },
            "Playlists": {
                "active": "List",
                "views": {
                    "List": {
                        "offset": 0,
                        "limit": 100,
                        "filter": "-",
                        "sort": "-",
                        "tag": "-",
                        "search": "",
                        "scrollPos": 0
                    },
                    "Detail": {
                        "offset": 0,
                        "limit": 100,
                        "filter": "-",
                        "sort": "-",
                        "tag": "-",
                        "search": "",
                        "scrollPos": 0
                    }
                }
            },
            "Database": {
                "active": "List",
                "views": {
                    "List": {
                        "offset": 0,
                        "limit": 100,
                        "filter": "any",
                        "sort": "AlbumArtist",
                        "tag": "Album",
                        "search": "",
                        "scrollPos": 0
                    },
                    "Detail": {
                        "offset": 0,
                        "limit": 100,
                        "filter": "-",
                        "sort": "-",
                        "tag": "-",
                        "search": "",
                        "scrollPos": 0
                    }
                }
            }
        }
    },
    "Search": {
        "offset": 0,
        "limit": 100,
        "filter": "any",
        "sort": "-",
        "tag": "-",
        "search": "",
        "scrollPos": 0
    }
};

app.id = "Home";
app.current = {"card": "Home", "tab": undefined, "view": undefined, "offset": 0, "limit": 100, "filter": "", "search": "", "sort": "", "tag": "", "scrollPos": 0};
app.last = {"card": undefined, "tab": undefined, "view": undefined, "offset": 0, "limit": 100, "filter": "", "search": "", "sort": "", "tag": "", "scrollPos": 0};
app.goto = false;

//normal settings
const settingFields = {
    "volumeMin": {
        "defaultValue": 0,
        "inputType": "input",
        "title": "Volume min.",
        "form": "volumeSettingsFrm",
        "reset": true
    },
    "volumeMax": {
        "defaultValue": 100,
        "inputType": "input",
        "title": "Volume max.",
        "form": "volumeSettingsFrm",
        "reset": true
    },
    "volumeStep": {
        "defaultValue": 5,
        "inputType": "input",
        "title": "Volume step",
        "form": "volumeSettingsFrm",
        "reset": true
    },
    "lyricsUsltExt": {
        "defaultValue": "txt",
        "inputType": "input",
        "title": "Unsynced lyrics extension",
        "form": "collapseEnableLyrics",
        "reset": true
    },
    "lyricsSyltExt": {
        "defaultValue": "lrc",
        "inputType": "input",
        "title": "Synced lyrics extension",
        "form": "collapseEnableLyrics",
        "reset": true
    },
    "lyricsVorbisUslt": {
        "defaultValue": "LYRICS",
        "inputType": "input",
        "title": "Unsynced lyrics vorbis comment",
        "form": "collapseEnableLyrics",
        "reset": true
    },
    "lyricsVorbisSylt": {
        "defaultValue": "SYNCEDLYRICS",
        "inputType": "input",
        "title": "Synced lyrics vorbis comment",
        "form": "collapseEnableLyrics",
        "reset": true
    },
    "lastPlayedCount": {
        "defaultValue": 200,
        "inputType": "input",
        "title": "Last played list count",
        "form": "statisticsFrm",
        "reset": true,
        "invalid": "Must be a number and greater than zero"
    }
};

//webui settings default values
const webuiSettingsDefault = {
    "clickSong": {
        "defaultValue": "append",
        "validValues": {
            "append": "Append to queue",
            "appendPlay": "Append to queue and play",
            "insertAfterCurrent": "Insert after current playing song",
            "replace": "Replace queue",
            "replacePlay": "Replace queue and play",
            "view": "Song details"
        },
        "inputType": "select",
        "title": "Click song",
        "form": "clickSettingsFrm"
    },
    "clickQueueSong": {
        "defaultValue": "play",
        "validValues": {
            "play": "Play",
            "view": "Song details",
        },
        "inputType": "select",
        "title": "Click song in queue",
        "form": "clickSettingsFrm"
    },
    "clickPlaylist": {
        "defaultValue": "append",
        "validValues": {
            "append": "Append to queue",
            "appendPlay": "Append to queue and play",
            "insertAfterCurrent": "Insert after current playing song",
            "replace": "Replace queue",
            "replacePlay": "Replace queue and play",
            "view": "View playlist"
        },
        "inputType": "select",
        "title": "Click playlist",
        "form": "clickSettingsFrm"
    },
    "clickFolder": {
        "defaultValue": "view",
        "validValues": {
            "append": "Append to queue",
            "appendPlay": "Append to queue and play",
            "insertAfterCurrent": "Insert after current playing song",
            "replace": "Replace queue",
            "replacePlay": "Replace queue and play",
            "view": "Open folder"
        },
        "inputType": "select",
        "title": "Click folder",
        "form": "clickSettingsFrm"
    },
    "clickAlbumPlay": {
        "defaultValue": "replace",
        "validValues": {
            "append": "Append to queue",
            "appendPlay": "Append to queue and play",
            "insertAfterCurrent": "Insert after current playing song",
            "replace": "Replace queue",
            "replacePlay": "Replace queue and play"
        },
        "inputType": "select",
        "title": "Click album play button",
        "form": "clickSettingsFrm"
    },
    "notificationAAASection": {
        "inputType": "section",
        "subtitle": "Facilities",
        "form": "NotificationSettingsAdvFrm"
    },
    "notificationPlayer": {
        "defaultValue": false,
        "inputType": "checkbox",
        "title": "Playback",
        "form": "NotificationSettingsAdvFrm"
    },
    "notificationQueue": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "Queue",
        "form": "NotificationSettingsAdvFrm"
    },
    "notificationGeneral": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "General",
        "form": "NotificationSettingsAdvFrm"
    },
    "notificationDatabase": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "Database",
        "form": "NotificationSettingsAdvFrm"
    },
    "notificationPlaylist": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "Playlist",
        "form": "NotificationSettingsAdvFrm"
    },
    "notificationScript": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "Script",
        "form": "NotificationSettingsAdvFrm"
    },
    "notifyPage": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "On page notifications",
        "form": "NotificationSettingsFrm"
    },
    "notifyWeb": {
        "defaultValue": false,
        "inputType": "checkbox",
        "title": "Web notifications",
        "form": "NotificationSettingsFrm",
        "onClick": "toggleBtnNotifyWeb"
    },
    "mediaSession": {
        "defaultValue": false,
        "inputType": "checkbox",
        "title": "Media session",
        "form": "NotificationSettingsFrm"
    },
    "uiFooterQueueSettings": {
        "defaultValue": false,
        "inputType": "checkbox",
        "title": "Show playback settings in footer",
        "form": "footerFrm"
    },
    "uiFooterPlaybackControls": {
        "defaultValue": "pause",
        "validValues": {
            "pause": "pause only",
            "stop": "stop only",
            "both": "pause and stop"
        },
        "inputType": "select",
        "title": "Playback controls",
        "form": "footerFrm"
    },
    "uiMaxElementsPerPage": {
        "defaultValue": "100",
        "validValues": {
            "25": "25",
            "50": "50",
            "100": "100",
            "250": "250",
            "500": "500"
        },
        "inputType": "select",
        "contentType": "integer",
        "title": "Elements per page",
        "form": "appearanceSettingsFrm"
    },
    "enableHome": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "Homescreen",
        "form": "enableFeaturesFrm"
    },
    "enableScripting": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "Scripting",
        "form": "enableFeaturesFrm",
        "warn": "Lua is not compiled in"
    },
    "enableTrigger": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "Trigger",
        "form": "enableFeaturesFrm"
    },
    "enableTimer": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "Timer",
        "form": "enableFeaturesFrm"
    },
    "enableMounts": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "Mounts",
        "form": "enableFeaturesFrm",
        "warn": "MPD does not support mounts"
    },
    "enableLocalPlayback": {
        "defaultValue": false,
        "inputType": "checkbox",
        "title": "Local playback",
        "form": "enableFeaturesFrm"
    },
    "enablePartitions": {
        "defaultValue": false,
        "inputType": "checkbox",
        "title": "Partitions",
        "form": "enableFeaturesFrm",
        "warn": "MPD does not support partitions"
    },
    "uiTheme": {
        "defaultValue": "theme-dark",
        "validValues": {
            "theme-autodetect": "Autodetect",
            "theme-dark": "Dark",
            "theme-light": "Light"
        },
        "inputType": "select",
        "title": "Theme",
        "form": "themeFrm",
        "onChange": "eventChangeTheme"
    },
    "uiHighlightColor": {
        "defaultValue": "#28a745",
        "inputType": "color",
        "title": "Highlight color",
        "form": "themeFrm",
        "reset": true
    },
    "uiCoverimageSize": {
        "defaultValue": 250,
        "inputType": "input",
        "contentType": "integer",
        "title": "Size normal",
        "form": "coverimageFrm",
        "reset": true
    },
    "uiCoverimageSizeSmall": {
        "defaultValue": 175,
        "inputType": "input",
        "contentType": "integer",
        "title": "Size small",
        "form": "coverimageFrm",
        "reset": true
    },
    "uiBgColor": {
        "defaultValue": "#000000",
        "inputType": "color",
        "title": "Color",
        "form": "bgFrm",
        "reset": true
    },
    "uiBgImage": {
        "defaultValue": "",
        "inputType": "select",
        "title": "Image",
        "form": "bgFrm2"
    },
    "uiBgCover": {
        "defaultValue": true,
        "inputType": "checkbox",
        "title": "Albumart",
        "form": "bgFrm"
    },
    "uiBgCssFilter": {
        "defaultValue": "grayscale(100%) opacity(10%)",
        "inputType": "input",
        "title": "CSS filter",
        "form": "bgCssFilterFrm",
        "reset": true
    },
    "uiLocale": {
        "defaultValue": "default",
        "inputType": "select",
        "title": "Locale",
        "form": "localeFrm",
        "onChange": "eventChangeLocale"
    }
};

//features
const features = {
    "featAdvsearch": true,
    "featCacert": false,
    "featHome": true,
    "featLibrary": false,
    "featLocalPlayback": false,
    "featLyrics": false,
    "featMounts": true,
    "featNeighbors": true,
    "featPartitions": true,
    "featPlaylists": true,
    "featSingleOneShot": true,
    "featScripting": true,
    "featSmartpls": true,
    "featStickers": false,
    "featTags": true,
    "featTimer": true,
    "featTrigger": true,
    "featBinarylimit": true,
    "featFingerprint": false
};

//keyboard shortcuts
const keymap = {
    "playback": {"order": 0, "desc": "Playback"},
        " ": {"order": 1, "cmd": "clickPlay", "options": [], "desc": "Toggle play / pause", "key": "space_bar"},
        "s": {"order": 2, "cmd": "clickStop", "options": [], "desc": "Stop playing"},
        "ArrowLeft": {"order": 3, "cmd": "clickPrev", "options": [], "desc": "Previous song", "key": "keyboard_arrow_left"},
        "ArrowRight": {"order": 4, "cmd": "clickNext", "options": [], "desc": "Next song", "key": "keyboard_arrow_right"},
        "-": {"order": 5, "cmd": "volumeStep", "options": ["down"], "desc": "Volume down"},
        "+": {"order": 6, "cmd": "volumeStep", "options": ["up"], "desc": "Volume up"},
    "update": {"order": 100, "desc": "Update"},
        "u": {"order": 101, "cmd": "updateDB", "options": ["", true, false], "desc": "Update database"},
        "r": {"order": 102, "cmd": "updateDB", "options": ["", true, true], "desc": "Rescan database"},
        "p": {"order": 103, "cmd": "updateSmartPlaylists", "options": [false], "desc": "Update smart playlists", "req": "featSmartpls"},
    "modals": {"order": 200, "desc": "Dialogs"},
        "a": {"order": 201, "cmd": "showAddToPlaylist", "options": ["STREAM"], "desc": "Add stream"},
        "c": {"order": 202, "cmd": "openModal", "options": ["modalConnection"], "desc": "Open MPD connection"},
        "q": {"order": 203, "cmd": "openModal", "options": ["modalQueueSettings"], "desc": "Open queue settings"},
        "t": {"order": 204, "cmd": "openModal", "options": ["modalSettings"], "desc": "Open settings"},
        "m": {"order": 205, "cmd": "openModal", "options": ["modalMaintenance"], "desc": "Open maintenance"},
        "?": {"order": 206, "cmd": "openModal", "options": ["modalAbout"], "desc": "Open about"},
    "navigation": {"order": 300, "desc": "Navigation"},
        "0": {"order": 301, "cmd": "appGoto", "options": ["Home"], "desc": "Show home"},
        "1": {"order": 302, "cmd": "appGoto", "options": ["Playback"], "desc": "Show playback"},
        "2": {"order": 303, "cmd": "appGoto", "options": ["Queue", "Current"], "desc": "Show queue"},
        "3": {"order": 304, "cmd": "appGoto", "options": ["Queue", "LastPlayed"], "desc": "Show last played"},
        "4": {"order": 305, "cmd": "appGoto", "options": ["Queue", "Jukebox"], "desc": "Show jukebox queue"},
        "5": {"order": 306, "cmd": "appGoto", "options": ["Browse", "Database"], "desc": "Show browse database", "req": "featTags"},
        "6": {"order": 307, "cmd": "appGoto", "options": ["Browse", "Playlists"], "desc": "Show browse playlists", "req": "featPlaylists"},
        "7": {"order": 308, "cmd": "appGoto", "options": ["Browse", "Filesystem"], "desc": "Show browse filesystem"},
        "8": {"order": 309, "cmd": "appGoto", "options": ["Search"], "desc": "Show search"},
        "/": {"order": 310, "cmd": "focusSearch", "options": [], "desc": "Focus search"}
};

//cache often accessed dom elements
const domCache = {};
domCache.body = document.getElementsByTagName('body')[0];
domCache.counter = document.getElementById('counter');
domCache.progress = document.getElementById('footerProgress');
domCache.progressBar = document.getElementById('footerProgressBar');
domCache.progressPos = document.getElementById('footerProgressPos');
domCache.notificationCount = document.getElementById('notificationCount');

//Get BSN object references for fast access
const uiElements = {};
//all modals
for (const m of document.getElementsByClassName('modal')) {
    uiElements[m.id] = document.getElementById(m.id).Modal;
}
//other directly accessed BSN objects
uiElements.dropdownHomeIconLigature = document.getElementById('btnHomeIconLigature').Dropdown;
uiElements.collapseJukeboxMode = document.getElementById('collapseJukeboxMode').Collapse;

const LUAfunctions = {
    "mympd_api_http_client": {
        "desc": "HTTP client",
        "func": "rc, response, header, body = mympd_api_http_client(method, uri, headers, payload)"
    },
    "mympd.init": {
        "desc": "Initializes the mympd_state lua table",
        "func": "mympd.init()"
    },
    "mympd.os_capture": {
        "desc":	"Executes a system command and capture its output.",
        "func": "output = mympd.os_capture(command)"
    }
};

const typeFriendly = {
    'plist': 'Playlist',
    'smartpls': 'Smart playlist',
    'dir': 'Directory',
    'song': 'Song',
    'search': 'Search',
    'album': 'Album',
    'stream': 'Stream',
    'view': 'View',
    'script': 'Script'
};
