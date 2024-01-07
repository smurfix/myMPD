"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

/** @module viewBrowseRadioWebradiodb_js */

/**
 * WebradioDB Browse handler
 * @returns {void}
 */
function handleBrowseRadioWebradiodb() {
    setFocusId('BrowseRadioWebradiodbSearchStr');
    if (webradioDb === null) {
        //fetch webradiodb database
        getWebradiodb();
        return;
    }
    setDataId('BrowseRadioWebradiodbGenreFilter', 'value', app.current.filter['genre']);
    elGetById('BrowseRadioWebradiodbGenreFilter').value = app.current.filter['genre'];
    setDataId('BrowseRadioWebradiodbCountryFilter', 'value', app.current.filter['country']);
    elGetById('BrowseRadioWebradiodbCountryFilter').value = app.current.filter['country'];
    setDataId('BrowseRadioWebradiodbLanguageFilter', 'value', app.current.filter['language']);
    elGetById('BrowseRadioWebradiodbLanguageFilter').value = app.current.filter['language'];
    setDataId('BrowseRadioWebradiodbCodecFilter', 'value', app.current.filter['codec']);
    elGetById('BrowseRadioWebradiodbCodecFilter').value = app.current.filter['codec'];
    setDataId('BrowseRadioWebradiodbBitrateFilter', 'value', app.current.filter['bitrate']);
    elGetById('BrowseRadioWebradiodbBitrateFilter').value = app.current.filter['bitrate'];

    const result = searchWebradiodb(app.current.search, app.current.filter['genre'],
        app.current.filter['country'], app.current.filter['language'], app.current.filter['codec'],
        app.current.filter['bitrate'], app.current.sort, app.current.offset, app.current.limit);
    parseSearchWebradiodb(result);
}

/**
 * Initialization function for webradioDB elements
 * @returns {void}
 */
function initViewBrowseRadioWebradiodb() {
    elGetById('BrowseRadioWebradiodbSearchStr').addEventListener('keyup', function(event) {
        if (ignoreKeys(event) === true) {
            return;
        }
        clearSearchTimer();
        searchTimer = setTimeout(function() {
            doSearchWebradiodb();
        }, searchTimerTimeout);
    }, false);

    elGetById('BrowseRadioWebradiodbFilter').addEventListener('shown.bs.collapse', function() {
        elGetById('BrowseRadioWebradiodbFilterBtn').classList.add('active');
        setScrollViewHeight(elGetById('BrowseRadioWebradiodbList'));
    }, false);

    elGetById('BrowseRadioWebradiodbFilter').addEventListener('hidden.bs.collapse', function() {
        elGetById('BrowseRadioWebradiodbFilterBtn').classList.remove('active');
        setScrollViewHeight(elGetById('BrowseRadioWebradiodbList'));
    }, false);

    initWebradiodbFilter('BrowseRadioWebradiodbGenreFilter', 'webradioGenres', 'Genre');
    initWebradiodbFilter('BrowseRadioWebradiodbCountryFilter', 'webradioCountries', 'Country');
    initWebradiodbFilter('BrowseRadioWebradiodbLanguageFilter', 'webradioLanguages', 'Language');
    initWebradiodbFilter('BrowseRadioWebradiodbCodecFilter', 'webradioCodecs', 'Codec');
    initWebradiodbFilter('BrowseRadioWebradiodbBitrateFilter', 'webradioBitrates', 'Bitrate');

    document.querySelector('#BrowseRadioWebradiodbList > thead > tr').addEventListener('click', function(event) {
        const colName = event.target.getAttribute('data-col');
        toggleSort(event.target, colName);
        appGoto(app.current.card, app.current.tab, app.current.view,
            app.current.offset, app.current.limit, app.current.filter, app.current.sort, '-', app.current.search);
    }, false);

    elGetById('BrowseRadioWebradiodbList').addEventListener('click', function(event) {
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
                clickWebradiodb(uri, event);
            }
        }
    }, false);
}

/**
 * Initializes the webradioDB filter elements
 * @param {string} id input id to initialize
 * @param {string} dbField database fields
 * @param {string} name name of the field
 * @returns {void}
 */
function initWebradiodbFilter(id, dbField, name) {
    elGetById(id).addEventListener('change', function() {
        doSearchWebradiodb();
    }, false);
    setDataId(id, 'cb-filter', 'filterWebradiodbFilter');
    setDataId(id, 'cb-filter-options', [id, dbField, name]);
}

/**
 * Fetches the webradioDB
 * @returns {void}
 */
function getWebradiodb() {
    const list = document.querySelector('#BrowseRadioWebradiodbList > tbody');
    elReplaceChild(list, 
        loadingRow(settings.colsBrowseRadioWebradiodb.length + 1)
    );
    sendAPI("MYMPD_API_CLOUD_WEBRADIODB_COMBINED_GET", {}, function(obj) {
        webradioDb = obj.result.data;
        filterWebradiodbFilter('BrowseRadioWebradiodbGenreFilter', 'webradioGenres', 'Genre', '');
        filterWebradiodbFilter('BrowseRadioWebradiodbCountryFilter', 'webradioCountries', 'Country', '');
        filterWebradiodbFilter('BrowseRadioWebradiodbLanguageFilter', 'webradioLanguages', 'Language', '');
        filterWebradiodbFilter('BrowseRadioWebradiodbCodecFilter', 'webradioCodecs', 'Codec', '');
        filterWebradiodbFilter('BrowseRadioWebradiodbBitrateFilter', 'webradioBitrates', 'Bitrate', '');
        const result = searchWebradiodb(app.current.search, app.current.filter['genre'],
            app.current.filter['country'], app.current.filter['language'], app.current.filter['codec'],
            app.current.filter['bitrate'], app.current.sort, app.current.offset, app.current.limit);
        parseSearchWebradiodb(result);
    }, false);
}

/**
 * Callback function for the custom select filter for webradioDB
 * @param {string} id element id
 * @param {string} dbField name of the array to filter
 * @param {string} placeholder placeholder value
 * @param {string} searchStr search string
 * @returns {void}
 */
function filterWebradiodbFilter(id, dbField, placeholder, searchStr) {
    searchStr = searchStr.toLowerCase();
    const el = elGetById(id);
    elClear(el.filterResult);
    el.addFilterResult(tn(placeholder), '');
    let i = 0;
    for (const value of webradioDb[dbField]) {
        if (searchStr === '' ||
            value.toLowerCase().indexOf(searchStr) > -1)
        {
            el.addFilterResult(value, value);
            i++;
        }
        if (i === 50) {
            break;
        }
    }
}

/**
 * Starts the webradioDB search
 * @returns {void}
 */
function doSearchWebradiodb() {
    const searchstr = elGetById('BrowseRadioWebradiodbSearchStr').value;
    const genre = getDataId('BrowseRadioWebradiodbGenreFilter', 'value');
    const country = getDataId('BrowseRadioWebradiodbCountryFilter', 'value');
    const language = getDataId('BrowseRadioWebradiodbLanguageFilter', 'value');
    const codec = getDataId('BrowseRadioWebradiodbCodecFilter', 'value');
    const bitrate = getDataId('BrowseRadioWebradiodbBitrateFilter', 'value');
    appGoto('Browse', 'Radio', 'Webradiodb',
        0, app.current.limit, {"genre": genre, "country": country, "language": language, "codec": codec, "bitrate": bitrate},
        app.current.sort, undefined, searchstr, 0);
}

/**
 * Searches the webradioDB
 * @param {string} name webradio name
 * @param {string} genre webradio genre
 * @param {string} country webradio country
 * @param {string} language webradio language
 * @param {string} codec webradio codec
 * @param {number} bitrate webradio bitrate
 * @param {object} sort webradio sort
 * @param {number} offset start offset
 * @param {number} limit maximum number of results
 * @returns {object} the search result
 */
function searchWebradiodb(name, genre, country, language, codec, bitrate, sort, offset, limit) {
    name = name.toLowerCase();
    const obj = {
        "result": {
            "totalEntities": 0,
            "returnedEntities": 0,
            "data": []
        }
    };
    if (webradioDb === null) {
        logDebug('WebradioDb is empty');
        return obj;
    }

    for (const key in webradioDb.webradios) {
        if (webradioDb.webradios[key].Name.toLowerCase().indexOf(name) > -1 &&
            (genre === '' || webradioDb.webradios[key].Genre.includes(genre)) &&
            (country === '' || country === webradioDb.webradios[key].Country) &&
            (language === '' || webradioDb.webradios[key].Languages.includes(language)) &&
            (codec === '' || webradioDb.webradios[key].allCodecs.includes(codec)) &&
            (bitrate === 0 || bitrate <= webradioDb.webradios[key].highestBitrate)
        ) {
            obj.result.data.push(webradioDb.webradios[key]);
            obj.result.totalEntities++;
        }
    }
    obj.result.data.sort(function(a, b) {
        //case insensitive sorting
        let lca;
        let lcb;
        if (typeof a === 'string') {
            lca = a[sort.tag].toLowerCase();
            lcb = b[sort.tag].toLowerCase();
        }
        else {
            lca = a[sort.tag];
            lcb = b[sort.tag];
        }
        //primary sort by defined tag
        if (lca < lcb) {
            return sort.desc === false ? -1 : 1;
        }
        if (lca > lcb) {
            return sort.desc === false ? 1 : -1;
        }
        //secondary sort by Name
        if (sort.tag !== 'Name') {
            lca = a.Name.toLowerCase();
            lcb = b.Name.toLowerCase();
            if (lca < lcb) {
                return sort.desc === false ? -1 : 1;
            }
            if (lca > lcb) {
                return sort.desc === false ? 1 : -1;
            }
        }
        //equal
        return 0;
    });
    if (offset > 0) {
        obj.result.data.splice(0, offset - 1);
    }
    const last = obj.result.data.length - limit;
    if (last > 0) {
        obj.result.data.splice(limit, last);
    }
    obj.result.returnedEntities = obj.result.data.length;
    return obj;
}

/**
 * Parses the webradioDB search result
 * @param {object} obj the search result
 * @returns {void}
 */
function parseSearchWebradiodb(obj) {
    const tfoot = document.querySelector('#BrowseRadioWebradiodbList > tfoot');
    elClear(tfoot);

    if (app.current.filter['genre'] === '' &&
        app.current.filter['country'] === '' &&
        app.current.filter['language'] === '' &&
        app.current.filter['codec'] === '' &&
        app.current.filter['bitrate'] === '')
    {
        elGetById('BrowseRadioWebradiodbFilterBtn').textContent = 'filter_list_off';
    }
    else {
        elGetById('BrowseRadioWebradiodbFilterBtn').textContent = 'filter_list';
    }

    if (checkResultId(obj, 'BrowseRadioWebradiodbList') === false) {
        return;
    }

    const rowTitle = tn(settingsWebuiFields.clickRadiobrowser.validValues[settings.webuiSettings.clickRadiobrowser]);

    updateTable(obj, 'BrowseRadioWebradiodb', function(row, data) {
        setData(row, 'uri', data.StreamUri);
        setData(row, 'name', data.Name);
        setData(row, 'genre', data.Genre);
        setData(row, 'image', webradioDbPicsUri + data.Image);
        setData(row, 'homepage', data.Homepage);
        setData(row, 'country', data.Country);
        setData(row, 'language', data.Languages);
        setData(row, 'description', data.Description);
        setData(row, 'codec', data.Codec);
        setData(row, 'bitrate', data.Bitrate);
        setData(row, 'type', 'stream');
        row.setAttribute('title', rowTitle);
    });

    if (obj.result.totalEntities > 0) {
        const colspan = settings.colsBrowseRadioWebradiodb.length + 1;
        tfoot.appendChild(
            elCreateNode('tr', {"class": ["not-clickable"]},
                elCreateTextTnNr('td', {"colspan": colspan}, 'Num entries', obj.result.totalEntities)
            )
        );
    }
}

/**
 * Converts a stream uri to the webradioDB and webradio favorites filename
 * @param {string} uri uri to convert
 * @returns {string} converted string
 */
function streamUriToName(uri) {
    return uri.replace(/[<>/.:?&$!#|;=]/g, '_');
}
