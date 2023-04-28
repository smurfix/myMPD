"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

/** @module state_js */

/**
 * Clears the mpd error
 * @returns {void}
 */
function clearMPDerror() {
    sendAPI('MYMPD_API_PLAYER_CLEARERROR',{}, function() {
        sendAPI('MYMPD_API_PLAYER_STATE', {}, function(obj) {
            parseState(obj);
        }, false);
    }, false);
}

/**
 * Parses the MYMPD_API_STATS jsonrpc response
 * @param {object} obj jsonrpc response
 * @returns {void}
 */
function parseStats(obj) {
    document.getElementById('mpdstatsArtists').textContent = obj.result.artists;
    document.getElementById('mpdstatsAlbums').textContent = obj.result.albums;
    document.getElementById('mpdstatsSongs').textContent = obj.result.songs;
    document.getElementById('mpdstatsDbPlaytime').textContent = fmtDuration(obj.result.dbPlaytime);
    document.getElementById('mpdstatsPlaytime').textContent = fmtDuration(obj.result.playtime);
    document.getElementById('mpdstatsUptime').textContent = fmtDuration(obj.result.uptime);
    document.getElementById('mpdstatsMympd_uptime').textContent = fmtDuration(obj.result.myMPDuptime);
    document.getElementById('mpdstatsDbUpdated').textContent = fmtDate(obj.result.dbUpdated);
    document.getElementById('mympdVersion').textContent = obj.result.mympdVersion;
    document.getElementById('myMPDuri').textContent = obj.result.myMPDuri;

    const mpdInfoVersionEl = document.getElementById('mpdInfoVersion');
    elClear(mpdInfoVersionEl);
    mpdInfoVersionEl.appendChild(document.createTextNode(obj.result.mpdProtocolVersion));

    const mpdProtocolVersion = obj.result.mpdProtocolVersion.match(/(\d+)\.(\d+)\.(\d+)/);
    if ((mpdProtocolVersion[1] < mpdVersion.major) ||
        (mpdProtocolVersion[1] <= mpdVersion.major && mpdProtocolVersion[2] < mpdVersion.minor) ||
        (mpdProtocolVersion[1] <= mpdVersion.major && mpdProtocolVersion[2] <= mpdVersion.minor && mpdProtocolVersion[3] < mpdVersion.patch)
       )
    {
        mpdInfoVersionEl.appendChild(
            elCreateTextTn('div', {"class": ["alert", "alert-warning", "mt-2", "mb-1"]}, 'MPD version is outdated')
        );
    }
}

/**
 * Gets the serverinfo (ip address)
 * @returns {void}
 */
function getServerinfo() {
    httpGet(subdir + '/serverinfo', function(obj) {
        document.getElementById('wsIP').textContent = obj.result.ip;
    }, true);
}

/**
 * Creates the elapsed / duration counter text
 * @returns {string} song counter text
 */
function getCounterText() {
    return fmtSongDuration(currentState.elapsedTime) + smallSpace +
        '/' + smallSpace + fmtSongDuration(currentState.totalTime);
}

/**
 * Sets the song counters
 * @returns {void}
 */
function setCounter() {
    //progressbar in footer
    //calc percent with two decimals after comma
    const prct = currentState.totalTime > 0 ?
        Math.ceil((100 / currentState.totalTime) * currentState.elapsedTime * 100) / 100 : 0;
    domCache.progressBar.style.width = `${prct}vw`;
    domCache.progress.style.cursor = currentState.totalTime <= 0 ? 'default' : 'pointer';

    //counter
    const counterText = getCounterText();
    //counter in footer
    domCache.counter.textContent = counterText;
    //update queue card
    const playingRow = document.getElementById('queueSongId' + currentState.currentSongId);
    if (playingRow !== null) {
        //progressbar and counter in queue card
        setQueueCounter(playingRow, counterText);
    }

    //synced lyrics
    if (showSyncedLyrics === true &&
        settings.colsPlayback.includes('Lyrics'))
    {
        const sl = document.getElementById('currentLyrics');
        const toHighlight = sl.querySelector('[data-sec="' + currentState.elapsedTime + '"]');
        const highlighted = sl.querySelector('.highlight');
        if (highlighted !== toHighlight &&
            toHighlight !== null)
        {
            toHighlight.classList.add('highlight');
            if (scrollSyncedLyrics === true) {
                toHighlight.scrollIntoView({behavior: "smooth"});
            }
            if (highlighted !== null) {
                highlighted.classList.remove('highlight');
            }
        }
    }

    if (progressTimer) {
        clearTimeout(progressTimer);
    }
    if (currentState.state === 'play') {
        progressTimer = setTimeout(function() {
            currentState.elapsedTime += 1;
            setCounter();
        }, 1000);
    }
}

/**
 * Parses the MYMPD_API_PLAYER_STATE jsonrpc response
 * @param {object} obj jsonrpc response
 * @returns {void}
 */
function parseState(obj) {
    if (obj.result === undefined) {
        logError('State is undefined');
        return;
    }
    //Get current song if songid or queueVersion has changed
    if (currentState.currentSongId !== obj.result.currentSongId ||
        currentState.queueVersion !== obj.result.queueVersion)
    {
        sendAPI("MYMPD_API_PLAYER_CURRENT_SONG", {}, parseCurrentSong, false);
    }
    //save state
    currentState = obj.result;
    //Set playback buttons
    if (obj.result.state === 'stop') {
        document.getElementById('btnPlay').textContent = 'play_arrow';
        domCache.progressBar.style.width = '0';
    }
    else if (obj.result.state === 'play') {
        document.getElementById('btnPlay').textContent =
            settings.webuiSettings.uiFooterPlaybackControls === 'stop' ? 'stop' : 'pause';
    }
    else {
        //pause
        document.getElementById('btnPlay').textContent = 'play_arrow';
    }
    if (app.id === 'QueueCurrent') {
        setPlayingRow();
    }

    if (obj.result.queueLength === 0) {
        elDisableId('btnPlay');
    }
    else {
        elEnableId('btnPlay');
    }

    if (obj.result.nextSongPos === -1 &&
        settings.partition.jukeboxMode === 'off')
    {
        elDisableId('btnNext');
    }
    else {
        elEnableId('btnNext');
    }

    if (obj.result.songPos < 0) {
        elDisableId('btnPrev');
    }
    else {
        elEnableId('btnPrev');
    }
    //media session
    mediaSessionSetState();
    mediaSessionSetPositionState(obj.result.totalTime, obj.result.elapsedTime);
    //local playback
    controlLocalPlayback(currentState.state);
    //queue badge in navbar
    const badgeQueueItemsEl = document.getElementById('badgeQueueItems');
    if (badgeQueueItemsEl) {
        badgeQueueItemsEl.textContent = obj.result.queueLength;
    }
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
        const pb = document.querySelectorAll('#cardPlaybackTags p');
        for (let i = 0, j = pb.length; i < j; i++) {
            elClear(pb[i]);
        }
    }
    else {
        const cff = document.getElementById('currentAudioFormat');
        if (cff) {
            elReplaceChild(
                cff.querySelector('p'),
                printValue('AudioFormat', obj.result.AudioFormat)
            );
        }
    }

    //handle error from mpd status response
    toggleAlert('alertMpdStatusError', (obj.result.lastError === '' ? false : true), obj.result.lastError);

    //handle mpd update status
    toggleAlert('alertUpdateDBState', (obj.result.updateState === 0 ? false : true), tn('Updating MPD database'));
    
    //handle myMPD cache update status
    toggleAlert('alertUpdateCacheState', obj.result.updateCacheState, tn('Updating caches'));

    //check if we need to refresh the settings
    if (localSettings.partition !== obj.result.partition || /* partition has changed */
        settings.partition.mpdConnected === false ||        /* mpd is not connected */
        uiEnabled === false)                                /* ui is disabled at startup */
    {
        if (document.getElementById('modalSettings').classList.contains('show') ||
            document.getElementById('modalConnection').classList.contains('show') ||
            document.getElementById('modalQueueSettings').classList.contains('show'))
        {
            //do not refresh settings, if a settings modal is open
            return;
        }
        logDebug('Refreshing settings');
        getSettings();
    }
}

/**
 * Sets the background image
 * @param {HTMLElement} el element for the background image
 * @param {string} url background image url
 * @returns {void}
 */
function setBackgroundImage(el, url) {
    if (url === undefined) {
        clearBackgroundImage(el);
        return;
    }
    const bgImageUrl = subdir + '/albumart?offset=0&uri=' + myEncodeURIComponent(url);
    const old = el.parentNode.querySelectorAll(el.tagName + '> div.albumartbg');
    //do not update if url is the same
    if (old[0] &&
        getData(old[0], 'uri') === bgImageUrl)
    {
        logDebug('Background image already set for: ' + el.tagName);
        return;
    }
    //remove old covers that are already hidden and
    //update z-index of current displayed cover
    for (let i = 0, j = old.length; i < j; i++) {
        if (old[i].style.zIndex === '-10') {
            old[i].remove();
        }
        else {
            old[i].style.zIndex = '-10';
        }
    }
    //add new cover and let it fade in
    const div = elCreateEmpty('div', {"class": ["albumartbg"]});
    if (el.tagName === 'BODY') {
        div.style.filter = settings.webuiSettings.uiBgCssFilter;
    }
    div.style.backgroundImage = 'url("' + bgImageUrl + '")';
    div.style.opacity = 0;
    setData(div, 'uri', bgImageUrl);
    el.insertBefore(div, el.firstChild);
    //create dummy img element for preloading and fade-in
    const img = new Image();
    setData(img, 'div', div);
    img.onload = function(event) {
        //fade-in current cover
        getData(event.target, 'div').style.opacity = 1;
        //fade-out old cover with some overlap
        setTimeout(function() {
            const bgImages = el.parentNode.querySelectorAll(el.tagName + '> div.albumartbg');
            for (let i = 0, j = bgImages.length; i < j; i++) {
                if (bgImages[i].style.zIndex === '-10') {
                    bgImages[i].style.opacity = 0;
                }
            }
        }, 800);
    };
    img.src = bgImageUrl;
}

/**
 * Clears the background image
 * @param {HTMLElement} el element for the background image
 * @returns {void}
 */
function clearBackgroundImage(el) {
    const old = el.parentNode.querySelectorAll(el.tagName + '> div.albumartbg');
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

/**
 * Sets the current cover in playback view and footer
 * @param {string} url song uri
 * @returns {void}
 */
function setCurrentCover(url) {
    setBackgroundImage(document.getElementById('currentCover'), url);
    setBackgroundImage(document.getElementById('footerCover'), url);
    if (settings.webuiSettings.uiBgCover === true) {
        setBackgroundImage(domCache.body, url);
    }
}

/**
 * Clears the current cover in playback view and footer
 * @returns {void}
 */
function clearCurrentCover() {
    clearBackgroundImage(document.getElementById('currentCover'));
    clearBackgroundImage(document.getElementById('footerCover'));
    if (settings.webuiSettings.uiBgCover === true) {
        clearBackgroundImage(domCache.body);
    }
}

/**
 * Sets the elapsed time for the media session api
 * @param {number} duration song duration
 * @param {number} position elapsed time
 * @returns {void}
 */
function mediaSessionSetPositionState(duration, position) {
    if (features.featMediaSession === false ||
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

/**
 * Sets the state for the media session api
 * @returns {void}
 */
function mediaSessionSetState() {
    if (features.featMediaSession === false) {
        return;
    }
    const state = currentState.state === 'play' ? 'playing' : 'paused';
    logDebug('Set mediaSession.playbackState to ' + state);
    navigator.mediaSession.playbackState = state;
}

/**
 * Sets metadata for the media session api
 * @param {string} title song title
 * @param {object} artist array of artists
 * @param {string} album album name
 * @param {string} url song uri
 * @returns {void}
 */
function mediaSessionSetMetadata(title, artist, album, url) {
    if (features.featMediaSession === false) {
        return;
    }
    const artwork = window.location.protocol + '//' + window.location.hostname +
        (window.location.port !== '' ? ':' + window.location.port : '') + subdir + '/albumart-thumb?offset=0&uri=' + myEncodeURIComponent(url);
    navigator.mediaSession.metadata = new MediaMetadata({
        title: title,
        artist: joinArray(artist),
        album: album,
        artwork: [{src: artwork}]
    });
}
