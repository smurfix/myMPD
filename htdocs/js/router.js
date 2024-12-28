"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2024 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

/** @module router_js */

/**
 * Shows the current view and highlights the navbar icon
 * @returns {void}
 */
function appPrepare() {
    if (app.current.card !== app.last.card ||
        app.current.tab !== app.last.tab ||
        app.current.view !== app.last.view)
    {
        //Hide all cards + nav
        for (let i = 0; i < domCache.navbarBtnsLen; i++) {
            domCache.navbarBtns[i].classList.remove('active');
        }
        const cards = ['cardHome', 'cardPlayback', 'cardSearch',
            'cardQueue', 'tabQueueCurrent', 'tabQueueLastPlayed',
            'tabQueueJukebox', 'viewQueueJukeboxSong', 'viewQueueJukeboxAlbum',
            'cardBrowse', 'tabBrowseFilesystem',
            'tabBrowseRadio', 'viewBrowseRadioFavorites', 'viewBrowseRadioWebradiodb',
            'tabBrowsePlaylist', 'viewBrowsePlaylistDetail', 'viewBrowsePlaylistList',
            'tabBrowseDatabase', 'viewBrowseDatabaseTagList', 'viewBrowseDatabaseAlbumDetail', 'viewBrowseDatabaseAlbumList'];
        for (const card of cards) {
            elHideId(card);
        }
        //show active card
        elShowId('card' + app.current.card);
        //show active tab
        if (app.current.tab !== undefined &&
            app.current.tab !== '')
        {
            elShowId('tab' + app.current.card + app.current.tab);
        }
        //show active view
        if (app.current.view !== undefined &&
            app.current.view !== '')
        {
            elShowId('view' + app.current.card + app.current.tab + app.current.view);
        }
        //highlight active navbar icon
        let nav = elGetById('nav' + app.current.card + app.current.tab);
        if (nav) {
            nav.classList.add('active');
        }
        else {
            nav = elGetById('nav' + app.current.card);
            if (nav) {
                elGetById('nav' + app.current.card).classList.add('active');
            }
        }
    }
    const list = elGetById(app.id + 'List');
    if (list) {
        setUpdateView(list);
    }
}

/**
 * Calculates the location hash and calls appRoute
 * @param {string} card card element name
 * @param {string} [tab] tab element name
 * @param {string} [view] view element name
 * @param {number} [offset] list offset
 * @param {number} [limit] list limit
 * @param {string | object} [filter] filter
 * @param {object} [sort] sort object
 * @param {string} [tag] tag name
 * @param {string | object} [search] search object or string
 * @param {number} [newScrollPos] new scrolling position
 * @returns {void}
 */
function appGoto(card, tab, view, offset, limit, filter, sort, tag, search, newScrollPos) {
    //old app
    const oldptr = app.cards[app.current.card].offset !== undefined
        ? app.cards[app.current.card]
        : app.cards[app.current.card].tabs[app.current.tab].offset !== undefined
            ? app.cards[app.current.card].tabs[app.current.tab]
            : app.cards[app.current.card].tabs[app.current.tab].views[app.current.view];

    //get default active tab or view from state
    if (app.cards[card].tabs) {
        if (tab === undefined) {
            tab = app.cards[card].active;
        }
        if (app.cards[card].tabs[tab].views) {
            if (view === undefined) {
                view = app.cards[card].tabs[tab].active;
            }
        }
    }

    //overwrite view for jukebox queue view
    if (card === 'Queue' &&
        tab === 'Jukebox')
    {
        view = settings.partition.jukeboxMode === 'album'
            ? 'Album'
            : 'Song';
    }

    //get ptr to new app
    const ptr = app.cards[card].offset !== undefined
        ? app.cards[card]
        : app.cards[card].tabs[tab].offset !== undefined
            ? app.cards[card].tabs[tab]
            : app.cards[card].tabs[tab].views[view];

    //save scrollPos of old app
    if (oldptr !== ptr) {
        oldptr.scrollPos = getScrollPosY();
    }

    //set options to default, if not defined
    if (offset === null || offset === undefined) { offset = ptr.offset; }
    if (limit === null || limit === undefined)   { limit = ptr.limit; }
    if (filter === null || filter === undefined) { filter = ptr.filter; }
    if (sort === null || sort === undefined)     { sort = ptr.sort; }
    if (tag === null || tag === undefined)       { tag = ptr.tag; }
    if (search === null || search === undefined) { search = ptr.search; }
    //enforce number type
    offset = Number(offset);
    limit = Number(limit);
    //set new scrollpos
    if (newScrollPos !== undefined) {
        ptr.scrollPos = newScrollPos;
    }
    //build hash
    app.goto = true;
    const newHash = myEncodeURIComponent(
        JSON.stringify({
            "card": card,
            "tab": tab,
            "view": view,
            "offset": offset,
            "limit": limit,
            "filter": filter,
            "sort": sort,
            "tag": tag,
            "search": search
        })
    );
    if (location.hash !== '#' + newHash) {
        location.hash = newHash;
    }
    appRoute(card, tab, view, offset, limit, filter, sort, tag, search);
}

/**
 * Checks if obj is string or object
 * @param {string | object} obj object to check
 * @returns {boolean} true if obj is object or string, else false
 */
function isArrayOrString(obj) {
    if (typeof obj === 'string') {
        return true;
    }
    return Array.isArray(obj);
}

/**
 * Executes the actions after the view is shown
 * @param {string} [card] card element name
 * @param {string} [tab] tab element name
 * @param {string} [view] view element name
 * @param {number} [offset] list offset
 * @param {number} [limit] list limit
 * @param {string | object} [filter] filter
 * @param {object} [sort] sort object
 * @param {string} [tag] tag name
 * @param {string | object} [search] search object or string
 * @returns {void}
 */
function appRoute(card, tab, view, offset, limit, filter, sort, tag, search) {
    if (settingsParsed === 'false') {
        appInitStart();
        return;
    }
    if (card === undefined || card === null) {
        const hash = location.hash.match(/^#(.*)$/);
        let jsonHash = null;
        if (hash !== null) {
            try {
                jsonHash = JSON.parse(decodeURIComponent(hash[1]));
                app.current.card = isArrayOrString(jsonHash.card) ? jsonHash.card : undefined;
                app.current.tab = typeof jsonHash.tab === 'string' ? jsonHash.tab : undefined;
                app.current.view = typeof jsonHash.view === 'string' ? jsonHash.view : undefined;
                app.current.offset = typeof jsonHash.offset === 'number' ? jsonHash.offset : '';
                app.current.limit = typeof jsonHash.limit === 'number' ? jsonHash.limit : '';
                app.current.filter = jsonHash.filter;
                app.current.sort = jsonHash.sort;
                app.current.tag = isArrayOrString(jsonHash.tag) ? jsonHash.tag : '';
                app.current.search = isArrayOrString(jsonHash.search) ? jsonHash.search : '';
            }
            catch(error) {
                //do nothing
                logDebug(error);
            }
        }
        if (jsonHash === null) {
            appPrepare();
            const initialStartupView = settings.webuiSettings.startupView === undefined || settings.webuiSettings.startupView === null
                ? features.featHome === true
                    ? 'Home'
                    : 'Playback'
                : features.featHome === false && settings.webuiSettings.startupView === 'Home'
                    ? 'Playback'
                    : settings.webuiSettings.startupView;
            settings.webuiSettings.startupView = initialStartupView;
            const path = initialStartupView.split('/');
            // @ts-ignore
            appGoto(...path);
            return;
        }
    }
    else {
        app.current.card = card;
        app.current.tab = tab;
        app.current.view = view;
        app.current.offset = offset;
        app.current.limit = limit;
        app.current.filter = filter;
        app.current.sort = sort;
        app.current.tag = tag;
        app.current.search = search;
    }
    app.id = app.current.card +
        (app.current.tab === undefined ? '' : app.current.tab) +
        (app.current.view === undefined ? '' : app.current.view);

    //get ptr to app options and set active tab/view
    let ptr;
    if (app.cards[app.current.card].offset !== undefined) {
        ptr = app.cards[app.current.card];
    }
    else if (app.cards[app.current.card].tabs[app.current.tab].offset !== undefined) {
        ptr = app.cards[app.current.card].tabs[app.current.tab];
        app.cards[app.current.card].active = app.current.tab;
    }
    else if (app.cards[app.current.card].tabs[app.current.tab].views[app.current.view].offset !== undefined) {
        ptr = app.cards[app.current.card].tabs[app.current.tab].views[app.current.view];
        app.cards[app.current.card].active = app.current.tab;
        app.cards[app.current.card].tabs[app.current.tab].active = app.current.view;
    }
    //set app options
    ptr.offset = app.current.offset;
    ptr.limit = app.current.limit;
    ptr.filter = app.current.filter;
    ptr.sort = app.current.sort;
    ptr.tag = app.current.tag;
    ptr.search = app.current.search;
    app.current.scrollPos = ptr.scrollPos;
    appPrepare();

    switch(app.id) {
        case 'Home':                      handleHome(); break;
        case 'Playback':                  handlePlayback(); break;
        case 'QueueCurrent':              handleQueueCurrent(); break;
        case 'QueueLastPlayed':           handleQueueLastPlayed(); break;
        case 'QueueJukeboxSong':          handleQueueJukeboxSong(); break;
        case 'QueueJukeboxAlbum':         handleQueueJukeboxAlbum(); break;
        case 'BrowsePlaylistList':        handleBrowsePlaylistList(); break;
        case 'BrowsePlaylistDetail':      handleBrowsePlaylistDetail(); break;
        case 'BrowseFilesystem':          handleBrowseFilesystem(); break;
        case 'BrowseDatabaseTagList':     handleBrowseDatabaseTagList(); break;
        case 'BrowseDatabaseAlbumList':   handleBrowseDatabaseAlbumList(); break;
        case 'BrowseDatabaseAlbumDetail': handleBrowseDatabaseAlbumDetail(); break;
        case 'BrowseRadioFavorites':      handleBrowseRadioFavorites(); break;
        case 'BrowseRadioWebradiodb':     handleBrowseRadioWebradiodb(); break;
        case 'Search':                    handleSearch(); break;
        default: {
            let initialStartupView = settings.webuiSettings.startupView;
            if (initialStartupView === undefined ||
                initialStartupView === null)
            {
                initialStartupView = features.featHome === true ? 'Home' : 'Playback';
            }
            const path = initialStartupView.split('/');
            // @ts-ignore
            appGoto(...path);
        }
    }

    app.last.card = app.current.card;
    app.last.tab = app.current.tab;
    app.last.view = app.current.view;
}

/**
 * Emulates the browser back button
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function historyBack() {
    history.back();
}
