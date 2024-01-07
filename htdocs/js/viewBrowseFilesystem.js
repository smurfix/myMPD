"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

/** @module viewBrowseFilesystem_js */

/**
 * Handles BrowseFilesystem
 * @returns {void}
 */
function handleBrowseFilesystem() {
    handleSearchSimple('BrowseFilesystem');
    if (app.current.tag === '') {
        //default type is dir
        app.current.tag = 'dir';
    }
    sendAPI("MYMPD_API_DATABASE_FILESYSTEM_LIST", {
        "offset": app.current.offset,
        "limit": app.current.limit,
        "path": app.current.filter,
        "searchstr": app.current.search,
        "type": app.current.tag,
        "cols": settings.colsBrowseFilesystemFetch
    }, parseFilesystem, true);

    //Create breadcrumb
    const crumbEl = elGetById('BrowseBreadcrumb');
    elClear(crumbEl);
    const home = elCreateText('a', {"class": ["mi"], "href": "#"}, 'home');
    setData(home, 'uri', '/');
    crumbEl.appendChild(
        elCreateNode('li', {"class": ["breadcrumb-item"]}, home)
    );
    if (app.current.filter !== '/') {
        const pathArray = app.current.filter.split('/');
        const pathArrayLen = pathArray.length;
        let fullPath = '';
        for (let i = 0; i < pathArrayLen; i++) {
            if (pathArrayLen - 1 === i) {
                crumbEl.appendChild(
                    elCreateText('li', {"class": ["breadcrumb-item", "active"]}, pathArray[i])
                );
                break;
            }
            fullPath += pathArray[i];
            const a = elCreateText('a', {"href": "#"}, pathArray[i]);
            setData(a, 'uri', fullPath);
            crumbEl.appendChild(
                elCreateNode('li', {"class": ["breadcrumb-item"]}, a)
            );
            fullPath += '/';
        }
    }
}

/**
 * Initialization function for the browse filesystem view
 * @returns {void}
 */
function initViewBrowseFilesystem() {
    initSearchSimple('BrowseFilesystem');

    elGetById('BrowseFilesystemList').addEventListener('click', function(event) {
        const target = tableClickHandler(event);
        if (target !== null) {
            const uri = getData(target, 'uri');
            const dataType = getData(target, 'type');
            switch(dataType) {
                case 'parentDir': {
                    const offset = browseFilesystemHistory[uri] !== undefined
                        ? browseFilesystemHistory[uri].offset
                        : 0;
                    const scrollPos = browseFilesystemHistory[uri] !== undefined
                        ? browseFilesystemHistory[uri].scrollPos
                        : 0;
                    app.current.filter = '-';
                    appGoto('Browse', 'Filesystem', undefined, offset, app.current.limit, uri, app.current.sort, 'dir', '', scrollPos);
                    break;
                }
                case 'dir':
                    clickFolder(uri);
                    break;
                case 'song':
                    clickSong(uri, event);
                    break;
                case 'plist':
                    clickFilesystemPlaylist(uri, event);
                    break;
                default:
                    logError('Invalid type: ' + dataType);
            }
        }
    }, false);

    elGetById('BrowseBreadcrumb').addEventListener('click', function(event) {
        if (event.target.nodeName === 'A') {
            event.preventDefault();
            const uri = getData(event.target, 'uri');
            const offset = browseFilesystemHistory[uri] !== undefined
                ? browseFilesystemHistory[uri].offset
                : 0;
            const scrollPos = browseFilesystemHistory[uri] !== undefined
                ? browseFilesystemHistory[uri].scrollPos
                : 0;
            appGoto('Browse', 'Filesystem', undefined, offset, app.current.limit, uri, app.current.sort, 'dir', '', scrollPos);
        }
    }, false);
}

/**
 * Parses the MYMPD_API_DATABASE_FILESYSTEM_LIST response
 * @param {object} obj jsonrpc response object
 * @returns {void}
 */
 function parseFilesystem(obj) {
    //show images in folder
    const imageList = elGetById('BrowseFilesystemImages');
    elClear(imageList);

    const table = elGetById('BrowseFilesystemList');
    const tfoot = table.querySelector('tfoot');
    elClear(tfoot);

    if (checkResultId(obj, 'BrowseFilesystemList') === false) {
        elHide(imageList);
        return;
    }

    if (obj.result.images !== undefined) {
        if (obj.result.images.length === 0 &&
            obj.result.bookletPath === '')
        {
            elHide(imageList);
        }
        else {
            elShow(imageList);
        }
        if (obj.result.bookletPath !== '') {
            const img = elCreateEmpty('div', {"class": ["booklet"], "title": tn('Booklet')});
            img.style.backgroundImage = 'url("' + subdir + '/assets/coverimage-booklet")';
            setData(img, 'href', subdir + myEncodeURI(obj.result.bookletPath));
            imageList.appendChild(img);
        }
        for (let i = 0, j = obj.result.images.length; i < j; i++) {
            if (isThumbnailfile(obj.result.images[i]) === true) {
                continue;
            }
            const img = elCreateEmpty('div', {});
            img.style.backgroundImage = getCssImageUri(myEncodeURI(obj.result.images[i]));
            imageList.appendChild(img);
        }
    }
    else {
        //playlist response
        elHide(imageList);
        obj.result.totalEntities++;
        obj.result.returnedEntities++;
        const parentUri = dirname(obj.result.plist);
        obj.result.data.unshift({"Type": "parentDir", "name": "parentDir", "uri": parentUri});
    }

    const rowTitleSong = settingsWebuiFields.clickSong.validValues[settings.webuiSettings.clickSong];
    const rowTitleFolder = 'Open directory';
    const rowTitlePlaylist = settingsWebuiFields.clickFilesystemPlaylist.validValues[settings.webuiSettings.clickFilesystemPlaylist];

    updateTable(obj, 'BrowseFilesystem', function(row, data) {
        setData(row, 'type', data.Type);
        setData(row, 'uri', data.uri);
        //set Title to name if not defined - for folders and playlists
        setData(row, 'name', data.Title === undefined ? data.name : data.Title);
        row.setAttribute('title', tn(data.Type === 'song' ? rowTitleSong :
            data.Type === 'dir' ? rowTitleFolder : rowTitlePlaylist));
    });

    const colspan = settings.colsBrowseFilesystem.length + 1;
    tfoot.appendChild(
        elCreateNode('tr', {"class": ["not-clickable"]},
            elCreateTextTnNr('td', {"colspan": colspan}, 'Num entries', obj.result.totalEntities)
        )
    );
}

/**
 * Appends the current filesystem view to the queue
 * @param {string} mode one of: append, appendPlay, insertAfterCurrent, insertPlayAfterCurrent, replace, replacePlay
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function addAllFromFilesystem(mode) {
    switch(mode) {
        case 'append':
            appendQueue('searchdir', [app.current.filter, app.current.search, app.current.sort.tag, app.current.sort.desc]);
            break;
        case 'appendPlay':
            appendPlayQueue('searchdir', [app.current.filter, app.current.search, app.current.sort.tag, app.current.sort.desc]);
            break;
        case 'insertAfterCurrent':
            insertAfterCurrentQueue('searchdir', [app.current.filter, app.current.search, app.current.sort.tag, app.current.sort.desc]);
            break;
        case 'insertPlayAfterCurrent':
            insertPlayAfterCurrentQueue('searchdir', [app.current.filter, app.current.search, app.current.sort.tag, app.current.sort.desc]);
            break;
        case 'replace':
            replaceQueue('searchdir', [app.current.filter, app.current.search, app.current.sort.tag, app.current.sort.desc]);
            break;
        case 'replacePlay':
            replacePlayQueue('searchdir', [app.current.filter, app.current.search, app.current.sort.tag, app.current.sort.desc]);
            break;
        default:
            logError('Invalid mode: ' + mode);
    }
}

/**
 * Adds the current directory to a playlist
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function showAddToPlaylistFromFilesystem() {
    showAddToPlaylist('searchdir', [app.current.filter, app.current.search, app.current.sort.tag, app.current.sort.desc]);
}
