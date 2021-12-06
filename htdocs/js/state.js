"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

function clearMPDerror() {
    sendAPI('MYMPD_API_PLAYER_CLEARERROR',{}, function() {
        sendAPI('MYMPD_API_PLAYER_STATE', {}, function(obj) {
            parseState(obj);
        }, false);
    }, false);
}

function parseStats(obj) {
    document.getElementById('mpdstatsArtists').textContent = obj.result.artists;
    document.getElementById('mpdstatsAlbums').textContent = obj.result.albums;
    document.getElementById('mpdstatsSongs').textContent = obj.result.songs;
    document.getElementById('mpdstatsDbPlaytime').textContent = beautifyDuration(obj.result.dbPlaytime);
    document.getElementById('mpdstatsPlaytime').textContent = beautifyDuration(obj.result.playtime);
    document.getElementById('mpdstatsUptime').textContent = beautifyDuration(obj.result.uptime);
    document.getElementById('mpdstatsMympd_uptime').textContent = beautifyDuration(obj.result.myMPDuptime);
    document.getElementById('mpdstatsDbUpdated').textContent = localeDate(obj.result.dbUpdated);
    document.getElementById('mympdVersion').textContent = obj.result.mympdVersion;

    const mpdInfoVersionEl = document.getElementById('mpdInfoVersion');
    elClear(mpdInfoVersionEl);
    mpdInfoVersionEl.appendChild(document.createTextNode(obj.result.mpdProtocolVersion));

    const mpdProtocolVersion = obj.result.mpdProtocolVersion.match(/(\d+)\.(\d+)\.(\d+)/);
    if (mpdProtocolVersion[1] >= mpdVersion.major &&
        mpdProtocolVersion[2] >= mpdVersion.minor &&
        mpdProtocolVersion[3] >= mpdVersion.patch)
    {
        //up2date mpd version
    }
    else {
        mpdInfoVersionEl.appendChild(
            elCreateText('div', {"class": ["alert", "alert-warning", "mt-2", "mb-1"], "data-phrase": 'MPD version is outdated'}, tn('MPD version is outdated'))
        );
    }
}

function getServerinfo() {
    const ajaxRequest = new XMLHttpRequest();
    ajaxRequest.open('GET', subdir + '/api/serverinfo', true);
    ajaxRequest.onreadystatechange = function() {
        if (ajaxRequest.readyState === 4) {
            let obj;
            try {
                obj = JSON.parse(ajaxRequest.responseText);
            }
            catch(error) {
                showNotification(tn('Can not parse response to json object'), '', 'general', 'error');
                logError('Can not parse response to json object:' + ajaxRequest.responseText);
            }
            document.getElementById('wsIP').textContent = obj.result.ip;
        }
    };
    ajaxRequest.send();
}

function setCounter() {
    const progressPx = currentState.totalTime > 0 ? Math.ceil(domCache.progress.offsetWidth * currentState.elapsedTime / currentState.totalTime) : 0;
    if (progressPx < domCache.progressBar.offsetWidth) {
        //prevent transition
        domCache.progressBar.style.transition = 'none';
        elReflow(domCache.progressBar);
        domCache.progressBar.style.width = progressPx + 'px';
        elReflow(domCache.progressBar);
        domCache.progressBar.style.transition = progressBarTransition;
        elReflow(domCache.progressBar);
    }
    else {
        domCache.progressBar.style.width = progressPx + 'px';
    }
    domCache.progress.style.cursor = currentState.totalTime <= 0 ? 'default' : 'pointer';

    //Set playing track in queue view
    queueSetCurrentSong();

    //Set counter in footer
    domCache.counter.textContent = beautifySongDuration(currentState.elapsedTime) + smallSpace +
        '/' + smallSpace + beautifySongDuration(currentState.totalTime);

    //synced lyrics
    if (showSyncedLyrics === true &&
        settings.colsPlayback.includes('Lyrics'))
    {
        const sl = document.getElementById('currentLyrics');
        const toHighlight = sl.querySelector('[data-sec="' + currentState.elapsedTime + '"]');
        const highlighted = sl.getElementsByClassName('highlight')[0];
        if (highlighted !== toHighlight) {
            if (toHighlight !== null) {
                toHighlight.classList.add('highlight');
                if (scrollSyncedLyrics === true) {
                    toHighlight.scrollIntoView({behavior: "smooth"});
                }
                if (highlighted !== undefined) {
                    highlighted.classList.remove('highlight');
                }
            }
        }
    }

    if (progressTimer) {
        clearTimeout(progressTimer);
    }
    if (currentState.state === 'play') {
        progressTimer = setTimeout(function() {
            currentState.elapsedTime += 1;
            requestAnimationFrame(function() {
                setCounter();
            });
        }, 1000);
    }
}

function parseState(obj) {
    //Get current song if songid has changed
    //Get current song if queueVersion has changed - updates stream titles
    if (currentState.currentSongId !== obj.result.currentSongId ||
        currentState.queueVersion !== obj.result.queueVersion)
    {
        sendAPI("MYMPD_API_PLAYER_CURRENT_SONG", {}, songChange);
    }
    //save state
    currentState = obj.result;
    //Set play and queue state
    parseUpdateQueue(obj);
    //Set volume
    parseVolume(obj);
    //Set play counters
    setCounter();
    //clear playback card if no current song
    if (obj.result.songPos === -1) {
        document.getElementById('currentTitle').textContent = tn('Not playing');
        document.title = 'myMPD';
        elClear(document.getElementById('footerTitle'));
        document.getElementById('footerTitle').removeAttribute('title');
        document.getElementById('footerTitle').classList.remove('clickable');
        document.getElementById('footerCover').classList.remove('clickable');
        document.getElementById('currentTitle').classList.remove('clickable');
        clearCurrentCover();
        if (settings.webuiSettings.uiBgCover === true) {
            clearBackgroundImage();
        }
        const pb = document.getElementById('cardPlaybackTags').getElementsByTagName('p');
        for (let i = 0, j = pb.length; i < j; i++) {
            elClear(pb[i]);
        }
    }
    else {
        const cff = document.getElementById('currentAudioFormat');
        if (cff) {
            elClear(cff.getElementsByTagName('p')[0]);
            cff.getElementsByTagName('p')[0].appendChild(printValue('AudioFormat', obj.result.AudioFormat));
        }
    }

    //handle error from mpd status response
    if (obj.result.lastError === '') {
        toggleAlert('alertMpdStatusError', false, '');
    }
    else {
        toggleAlert('alertMpdStatusError', true, obj.result.lastError);
    }
    toggleTopAlert();

    //refresh settings if mpd is not connected or ui is disabled
    //ui is disabled at startup
    if (settings.mpdConnected === false ||
        uiEnabled === false)
    {
        logDebug((settings.mpdConnected === false ? 'MPD disconnected' : 'UI disabled') + ' - refreshing settings');
        getSettings(true);
    }
}

function setBackgroundImage(url) {
    if (url === undefined) {
        clearBackgroundImage();
        return;
    }
    const bgImageUrl = 'url("' + subdir + '/albumart/' + myEncodeURI(url) + '")';
    const old = document.querySelectorAll('.albumartbg');
    if (old[0] && old[0].style.backgroundImage === bgImageUrl) {
        logDebug('Background image already set');
        return;
    }
    for (let i = 0, j = old.length; i < j; i++) {
        if (old[i].style.zIndex === '-10') {
            old[i].remove();
        }
        else {
            old[i].style.zIndex = '-10';
            old[i].style.opacity = '0';
        }
    }
    const div = elCreateEmpty('div', {"class": ["albumartbg"]});
    div.style.filter = settings.webuiSettings.uiBgCssFilter;
    div.style.backgroundImage = bgImageUrl;
    div.style.opacity = 0;
    domCache.body.insertBefore(div, domCache.body.firstChild);

    const img = new Image();
    img.onload = function() {
        document.querySelector('.albumartbg').style.opacity = 1;
    };
    img.src = subdir + '/albumart/' + myEncodeURI(url);
}

function clearBackgroundImage() {
    const old = document.querySelectorAll('.albumartbg');
    for (let i = 0, j = old.length; i < j; i++) {
        if (old[i].style.zIndex === '-10') {
            old[i].remove();
        }
        else {
            old[i].style.zIndex = '-10';
            old[i].style.opacity = '0';
        }
    }
}

function setCurrentCover(url) {
    _setCurrentCover(url, document.getElementById('currentCover'));
    _setCurrentCover(url, document.getElementById('footerCover'));
}

function _setCurrentCover(url, el) {
    if (url === undefined) {
        clearCurrentCover();
        return;
    }
    const old = el.querySelectorAll('.coverbg');
    for (let i = 0, j = old.length; i < j; i++) {
        if (old[i].style.zIndex === '2') {
            old[i].remove();
        }
        else {
            old[i].style.zIndex = '2';
        }
    }

    const div = elCreateEmpty('div', {"class": ["coverbg", "carousel", "rounded"]});
    div.style.backgroundImage = 'url("' + subdir + '/albumart/' + myEncodeURI(url) + '")';
    div.style.opacity = 0;
    setData(div, 'uri', url);
    el.insertBefore(div, el.firstChild);

    const img = new Image();
    img.onload = function() {
        el.querySelector('.coverbg').style.opacity = 1;
    };
    img.src = subdir + '/albumart/' + myEncodeURI(url);
}

function clearCurrentCover() {
    _clearCurrentCover(document.getElementById('currentCover'));
    _clearCurrentCover(document.getElementById('footerCover'));
}

function _clearCurrentCover(el) {
    const old = el.querySelectorAll('.coverbg');
    for (let i = 0, j = old.length; i < j; i++) {
        if (old[i].style.zIndex === '2') {
            old[i].remove();
        }
        else {
            old[i].style.zIndex = '2';
            old[i].style.opacity = '0';
        }
    }
}

function songChange(obj) {
    //check for song change
    //use title to detect stream changes
    const newSong = obj.result.uri + ':' + obj.result.Title + ':' + obj.result.currentSongId;
    if (currentSong === newSong) {
        return;
    }
    let textNotification = '';
    let pageTitle = '';

    mediaSessionSetMetadata(obj.result.Title, joinArray(obj.result.Artist), obj.result.Album, obj.result.uri);

    setCurrentCover(obj.result.uri);
    setBackgroundImage(obj.result.uri);

    for (const elName of ['footerArtist', 'footerAlbum', 'footerCover', 'currentTitle']) {
        document.getElementById(elName).classList.remove('clickable');
    }

    if (obj.result.Artist !== undefined &&
        obj.result.Artist[0] !== '-')
    {
        textNotification += joinArray(obj.result.Artist);
        pageTitle += obj.result.Artist.join(', ') + smallSpace + nDash + smallSpace;
        document.getElementById('footerArtist').textContent = obj.result.Artist;
        setDataId('footerArtist', 'name', obj.result.Artist);
        if (features.featAdvsearch === true) {
            document.getElementById('footerArtist').classList.add('clickable');
        }
    }
    else {
        document.getElementById('footerArtist').textContent = '';
        setDataId('footerArtist', 'name', ['']);
    }

    if (obj.result.Album !== undefined &&
        obj.result.Album !== '-')
    {
        textNotification += ' - ' + obj.result.Album;
        document.getElementById('footerAlbum').textContent = obj.result.Album;
        setDataId('footerAlbum', 'name', obj.result.Album);
        setDataId('footerAlbum', 'AlbumArtist', obj.result[tagAlbumArtist]);
        if (features.featAdvsearch === true) {
            document.getElementById('footerAlbum').classList.add('clickable');
        }
    }
    else {
        document.getElementById('footerAlbum').textContent = '';
        setDataId('footerAlbum', 'name', '');
        setDataId('footerAlbum', 'AlbumArtist', ['']);
    }

    if (obj.result.Title !== undefined &&
        obj.result.Title !== '-')
    {
        pageTitle += obj.result.Title;
        document.getElementById('currentTitle').textContent = obj.result.Title;
        setDataId('currentTitle', 'uri', obj.result.uri);
        document.getElementById('footerTitle').textContent = obj.result.Title;
        document.getElementById('footerCover').classList.add('clickable');
    }
    else {
        document.getElementById('currentTitle').textContent = '';
        setDataId('currentTitle', 'uri', '');
        document.getElementById('footerTitle').textContent = '';
        document.getElementById('currentTitle').classList.remove('clickable');
        document.getElementById('footerTitle').classList.remove('clickable');
        document.getElementById('footerCover').classList.remove('clickable');
    }
    document.title = 'myMPD: ' + pageTitle;
    document.getElementById('footerCover').title = pageTitle;

    if (isValidUri(obj.result.uri) === true &&
        isStreamUri(obj.result.uri) === false)
    {
        document.getElementById('footerTitle').classList.add('clickable');
        document.getElementById('currentTitle').classList.add('clickable');
    }
    else {
        document.getElementById('footerTitle').classList.remove('clickable');
        document.getElementById('currentTitle').classList.remove('clickable');
    }

    if (obj.result.uri !== undefined) {
        obj.result['Filetype'] = filetype(obj.result.uri);
        elEnableId('addCurrentSongToPlaylist');
    }
    else {
        obj.result['Filetype'] = '';
        elDisableId('addCurrentSongToPlaylist');
    }

    if (features.featStickers === true) {
        setVoteSongBtns(obj.result.stickerLike, obj.result.uri);
    }

    setPlaybackCardTags(obj.result);

    const bookletEl = document.getElementById('currentBooklet');
    elClear(bookletEl);
    if (obj.result.bookletPath !== '' &&
        obj.result.bookletPath !== undefined &&
        features.featLibrary === true)
    {
        bookletEl.appendChild(elCreateText('span', {"class": ["mi", "me-2"]}, 'description'));
        bookletEl.appendChild(elCreateText('a', {"target": "_blank", "href": subdir + '/browse/music/' +
            myEncodeURI(obj.result.bookletPath)}, tn('Download booklet')));
    }

    //update title in queue view for http streams
    const playingTr = document.getElementById('queueTrackId' + obj.result.currentSongId);
    if (playingTr !== null) {
        const titleCol = playingTr.querySelector('[data-col=Title');
        if (titleCol) {
            titleCol.textContent = obj.result.Title;
        }
    }

    if (currentState.state === 'play') {
        showNotification(obj.result.Title, textNotification, 'player', 'info');
    }

    //remember current song
    currentSong = newSong;
    currentSongObj = obj.result;
}

function setPlaybackCardTags(songObj) {
    for (const col of settings.colsPlayback) {
        const c = document.getElementById('current' + col);
        if (c === null) {
            continue;
        }
        switch(col) {
            case 'Lyrics':
                getLyrics(songObj.uri, c.getElementsByTagName('p')[0]);
                break;
            case 'AudioFormat':
                //songObj has no audioformat definition - use current state
                elReplaceChild(c.getElementsByTagName('p')[0], printValue('AudioFormat', currentState.AudioFormat));
                break;
            default: {
                let value = songObj[col];
                if (value === undefined) {
                    value = '-';
                }
                elReplaceChild(c.getElementsByTagName('p')[0], printValue(col, value));
                if ((typeof value === 'string' && value === '-') ||
                    (typeof value === 'object' && value[0] === '-') ||
                    settings.tagListBrowse.includes(col) === false)
                {
                    c.getElementsByTagName('p')[0].classList.remove('clickable');
                }
                else {
                    c.getElementsByTagName('p')[0].classList.add('clickable');
                }
                setData(c, 'name', value);
                if (col === 'Album' &&
                    songObj[tagAlbumArtist] !== undefined)
                {
                    setData(c, 'AlbumArtist', songObj[tagAlbumArtist]);
                }
            }
        }
    }
}

//eslint-disable-next-line no-unused-vars
function gotoTagList() {
    appGoto(app.current.card, app.current.tab, app.current.view, 0, undefined, '-', '-', '-', '');
}

//eslint-disable-next-line no-unused-vars
function clickTitle() {
    const uri = getDataId('currentTitle', 'uri');
    if (isValidUri(uri) === true &&
        isStreamUri(uri) === false)
    {
        songDetails(uri);
    }
}

function mediaSessionSetPositionState(duration, position) {
    if (checkMediaSessionSupport === false ||
        navigator.mediaSession.setPositionState === undefined)
    {
        return;
    }
    if (position < duration) {
        //streams have position > duration
        navigator.mediaSession.setPositionState({
            duration: duration,
            position: position
        });
    }
}

function mediaSessionSetState() {
    if (checkMediaSessionSupport === false) {
        return;
    }
    navigator.mediaSession.playbackState = currentState.state === 'play' ? 'playing' : 'paused';
}

function mediaSessionSetMetadata(title, artist, album, url) {
    if (checkMediaSessionSupport === false) {
        return;
    }
    const artwork = window.location.protocol + '//' + window.location.hostname +
        (window.location.port !== '' ? ':' + window.location.port : '') + subdir + '/albumart/' + myEncodeURI(url);
    navigator.mediaSession.metadata = new MediaMetadata({
        title: title,
        artist: artist,
        album: album,
        artwork: [{src: artwork}]
    });
}
