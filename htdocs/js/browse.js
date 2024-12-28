"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

/** @module Browse_js */

/**
 * Initialization function for the browse view
 * @returns {void}
 */
function initBrowse() {
    for (const nav of ['BrowseDatabaseTagListTagDropdown', 'BrowseDatabaseAlbumListTagDropdown',
        'BrowsePlaylistListNavDropdown', 'BrowseFilesystemNavDropdown',
        'BrowseRadioWebradiodbNavDropdown', 'BrowseRadioFavoritesNavDropdown'])
    {
        elGetById(nav).addEventListener('click', function(event) {
            navBrowseHandler(event);
        }, false);
    }
}

/**
 * Event handler for the navigation dropdown in the browse views
 * @param {event} event triggering event
 * @returns {void}
 */
function navBrowseHandler(event) {
    if (event.target.nodeName === 'BUTTON') {
        const tag = getData(event.target, 'tag');
        if (tag === 'Playlist' ||
            tag === 'Filesystem' ||
            tag === 'Radio')
        {
            appGoto('Browse', tag, undefined);
            return;
        }

        if (app.current.card === 'Browse' &&
            app.current.tab !== 'Database')
        {
            appGoto('Browse', 'Database', app.cards.Browse.tabs.Database.active);
            return;
        }
        if (tag === 'Album') {
            elGetById('BrowseDatabaseAlbumListSearchMatch').value = 'contains';
            appGoto('Browse', 'Database', 'AlbumList',
                0, undefined, undefined, undefined, undefined, '');
            return;
        }
        // other tags
        appGoto('Browse', 'Database', 'TagList',
            0, undefined, tag, {'tag': tag, 'desc': false}, tag, '');
    }
}

/**
 * Event handler for links to browse views
 * @param {event} event triggering event
 * @returns {void}
 */
function gotoBrowse(event) {
    let target = event.target;
    let tag = getData(target, 'tag');
    if (tag === 'undefined') {
        // String undefined means do not go further down the dom
        return;
    }
    let name = getData(target, 'name');
    let i = 0;
    while (tag === undefined) {
        i++;
        target = target.parentNode;
        tag = getData(target, 'tag');
        name = getData(target, 'name');
        if (i > 2) {
            break;
        }
    }
    if (tag === '' ||
        name === '')
    {
        return;
    }
    if (settings.albumMode === 'simple' &&
        settings.tagListAlbum.includes(tag) === false)
    {
        // The tag is not available in simple album mode
        gotoSearch(tag, name);
    }
    else if (settings.tagListBrowse.includes(tag)) {
        if (tag === 'Album') {
            let albumId = getData(target, 'AlbumId');
            if (albumId === undefined) {
                albumId = getData(target.parentNode, 'AlbumId');
            }
            if (albumId !== null) {
                // Show album details
                gotoAlbum(albumId);
            }
            else {
                // Show filtered album list
                gotoAlbumList(tag, name);
            }
        }
        else {
            // Show filtered album list
            gotoAlbumList(tag, name);
        }
    }
    else {
        // The tag is not available for albums
        gotoSearch(tag, name);
    }
}

/**
 * Go's to the album detail view
 * @param {string} albumId the album id
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function gotoAlbum(albumId) {
    appGoto('Browse', 'Database', 'AlbumDetail', 0, undefined, albumId);
}

/**
 * Go's to a filtered album list
 * @param {string} tag tag to search
 * @param {Array} value array of values to match
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function gotoAlbumList(tag, value) {
    if (typeof value === 'string') {
        //convert string to array
        value = [value];
    }
    elGetById('BrowseDatabaseAlbumListSearchStr').value = '';
    let expression = '(';
    for (let i = 0, j = value.length; i < j; i++) {
        if (i > 0) {
            expression += ' AND ';
        }
        expression += '(' + tag + ' == \'' + escapeMPD(value[i]) + '\')';
    }
    expression += ')';
    appGoto('Browse', 'Database', 'AlbumList', 0, undefined, 'any', {'tag': settings.webuiSettings.browseDatabaseAlbumListSort, 'desc': false}, 'Album', expression);
}

/**
 * Go's to the filesystem view
 * @param {string} uri uri to list
 * @param {string} type "dir" or "plist"
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function gotoFilesystem(uri, type) {
    elGetById('BrowseFilesystemSearchStr').value = '';
    appGoto('Browse', 'Filesystem', undefined, 0, undefined, uri, {'tag':'', 'desc': false}, type, '');
}

/**
 * Go's to the search view
 * @param {string} tag tag to search
 * @param {string|Array} value value to search
 * @returns {void}
 */
function gotoSearch(tag, value) {
    const filters = [];
    if (typeof(value) === 'string') {
        filters.push(createSearchExpressionComponent(tag, '==', value));
    }
    else if (value !== undefined) {
        for (const v of value) {
            filters.push(createSearchExpressionComponent(tag, '==', v));
        }
    }
    const expression = '(' + filters.join(' AND ') + ')';
    appGoto('Search', undefined, undefined, 0, undefined, tag, {'tag':app.cards.Search.sort.tag, 'desc': false}, tag, expression);
}
