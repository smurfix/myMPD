"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

/** @module viewBrowseRadioRadiobrowser_js */

/**
 * Browse Radiobrowser handler
 * @returns {void}
 */
function handleBrowseRadioRadiobrowser() {
    setFocusId('BrowseRadioRadiobrowserSearchStr');
    elGetById('BrowseRadioRadiobrowserTagsInput').value = app.current.filter['tags'];
    elGetById('BrowseRadioRadiobrowserCountryInput').value = app.current.filter['country'];
    elGetById('BrowseRadioRadiobrowserLanguageInput').value = app.current.filter['language'];
    if (app.current.search === '') {
        sendAPI("MYMPD_API_CLOUD_RADIOBROWSER_NEWEST", {
            "offset": app.current.offset,
            "limit": app.current.limit,
        }, parseRadiobrowserList, true);
    }
    else {
        sendAPI("MYMPD_API_CLOUD_RADIOBROWSER_SEARCH", {
            "offset": app.current.offset,
            "limit": app.current.limit,
            "tags": app.current.filter['tags'],
            "country": app.current.filter['country'],
            "language": app.current.filter['language'],
            "searchstr": app.current.search
        }, parseRadiobrowserList, true);
    }
}

/**
 * Initializes the radiobrowser elements
 * @returns {void}
 */
function initViewBrowseRadioRadiobrowser() {
    elGetById('BrowseRadioRadiobrowserSearchStr').addEventListener('keyup', function(event) {
        if (ignoreKeys(event) === true) {
            return;
        }
        clearSearchTimer();
        searchTimer = setTimeout(function() {
            searchRadiobrowser();
        }, searchTimerTimeout);
    }, false);

    elGetById('BrowseRadioRadiobrowserFilter').addEventListener('show.bs.collapse', function() {
        elGetById('BrowseRadioRadiobrowserFilterBtn').classList.add('active');
    }, false);

    elGetById('BrowseRadioRadiobrowserFilter').addEventListener('hide.bs.collapse', function() {
        elGetById('BrowseRadioRadiobrowserFilterBtn').classList.remove('active');
    }, false);

    elGetById('BrowseRadioRadiobrowserList').addEventListener('click', function(event) {
        const target = tableClickHandler(event);
        if (target !== null) {
            const uri = getData(target, 'uri');
            if (settings.webuiSettings.clickRadiobrowser === 'add') {
                showEditRadioFavorite({
                    "Name": getData(target, 'name'),
                    "Genre": getData(target, 'genre'),
                    "Image": getData(target, 'image'),
                    "StreamUri": uri
                });
            }
            else {
                clickRadiobrowser(uri, getData(target, 'RADIOBROWSERUUID'), event);
            }
        }
    }, false);
}

/**
 * Sends a click count message to the radiobrowser api
 * @param {string} uuid station uuid
 * @returns {void}
 */
function countClickRadiobrowser(uuid) {
    if (uuid !== '' && 
        settings.webuiSettings.radiobrowserStationclicks === true)
    {
        sendAPI("MYMPD_API_CLOUD_RADIOBROWSER_CLICK_COUNT", {
            "uuid": uuid
        }, null, false);
    }
}

/**
 * Searches the radiobrowser
 * @returns {void}
 */
function searchRadiobrowser() {
    app.current.filter['tags'] = elGetById('BrowseRadioRadiobrowserTagsInput').value;
    app.current.filter['country'] = elGetById('BrowseRadioRadiobrowserCountryInput').value;
    app.current.filter['language'] = elGetById('BrowseRadioRadiobrowserLanguageInput').value;
    appGoto(app.current.card, app.current.tab, app.current.view,
        0, app.current.limit, app.current.filter, '-', '-', elGetById('BrowseRadioRadiobrowserSearchStr').value);
}

/**
 * Parses the MYMPD_API_CLOUD_RADIOBROWSER_NEWEST and
 * MYMPD_API_CLOUD_RADIOBROWSER_SEARCH jsonrpc response
 * @param {object} obj jsonrpc response
 * @returns {void}
 */
function parseRadiobrowserList(obj) {
    if (app.current.filter['tags'] === '' &&
        app.current.filter['country'] === '' &&
        app.current.filter['language'] === '')
    {
        elGetById('BrowseRadioRadiobrowserFilterBtn').textContent = 'filter_list_off';
    }
    else {
        elGetById('BrowseRadioRadiobrowserFilterBtn').textContent = 'filter_list';
    }

    if (checkResultId(obj, 'BrowseRadioRadiobrowserList') === false) {
        return;
    }

    const rowTitle = tn(settingsWebuiFields.clickRadiobrowser.validValues[settings.webuiSettings.clickRadiobrowser]);
    //set result keys for pagination
    obj.result.returnedEntities = obj.result.data.length;
    obj.result.totalEntities = -1;

    updateTable(obj, 'BrowseRadioRadiobrowser', function(row, data) {
        setData(row, 'uri', data.url_resolved);
        setData(row, 'name', data.name);
        setData(row, 'genre', data.tags);
        setData(row, 'image', data.favicon);
        setData(row, 'homepage', data.homepage);
        setData(row, 'country', data.country);
        setData(row, 'language', data.language);
        setData(row, 'codec', data.codec);
        setData(row, 'bitrate', data.bitrate);
        setData(row, 'RADIOBROWSERUUID', data.stationuuid);
        setData(row, 'type', 'stream');
        row.setAttribute('title', rowTitle);
    });
}
