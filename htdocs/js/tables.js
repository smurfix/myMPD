"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

function dragAndDropTable(table) {
    const tableBody = document.getElementById(table).getElementsByTagName('tbody')[0];
    tableBody.addEventListener('dragstart', function(event) {
        if (event.target.nodeName === 'TR') {
            hidePopover();
            event.target.classList.add('opacity05');
            event.dataTransfer.setDragImage(event.target, 0, 0);
            event.dataTransfer.effectAllowed = 'move';
            event.dataTransfer.setData('Text', event.target.getAttribute('id'));
            dragEl = event.target.cloneNode(true);
        }
    }, false);
    tableBody.addEventListener('dragleave', function(event) {
        event.preventDefault();
        if (dragEl === undefined || dragEl.nodeName !== 'TR') {
            return;
        }
        let target = event.target;
        if (event.target.nodeName === 'TD') {
            target = event.target.parentNode;
        }
        if (target.nodeName === 'TR') {
            target.classList.remove('dragover');
        }
    }, false);
    tableBody.addEventListener('dragover', function(event) {
        event.preventDefault();
        if (dragEl === undefined || dragEl.nodeName !== 'TR') {
            return;
        }
        const tr = tableBody.getElementsByClassName('dragover');
        for (let i = 0, j = tr.length; i < j; i++) {
            tr[i].classList.remove('dragover');
        }
        let target = event.target;
        if (event.target.nodeName === 'TD') {
            target = event.target.parentNode;
        }
        if (target.nodeName === 'TR') {
            target.classList.add('dragover');
        }
        event.dataTransfer.dropEffect = 'move';
    }, false);
    tableBody.addEventListener('dragend', function(event) {
        event.preventDefault();
        if (dragEl === undefined || dragEl.nodeName !== 'TR') {
            return;
        }
        const tr = tableBody.getElementsByClassName('dragover');
        for (let i = 0, j = tr.length; i < j; i++) {
            tr[i].classList.remove('dragover');
        }
        if (document.getElementById(event.dataTransfer.getData('Text'))) {
            document.getElementById(event.dataTransfer.getData('Text')).classList.remove('opacity05');
        }
        dragEl = undefined;
    }, false);
    tableBody.addEventListener('drop', function(event) {
        event.stopPropagation();
        event.preventDefault();
        if (dragEl === undefined || dragEl.nodeName !== 'TR') {
            return;
        }
        let target = event.target;
        if (event.target.nodeName === 'TD') {
            target = event.target.parentNode;
        }
        const oldSongpos = getDataId(event.dataTransfer.getData('Text'), 'songpos');
        const newSongpos = getData(target, 'songpos');
        document.getElementById(event.dataTransfer.getData('Text')).remove();
        dragEl.classList.remove('opacity05');
        tableBody.insertBefore(dragEl, target);
        const tr = tableBody.getElementsByClassName('dragover');
        for (let i = 0, j = tr.length; i < j; i++) {
            tr[i].classList.remove('dragover');
        }
        document.getElementById(table).classList.add('opacity05');
        if (app.current.card === 'Queue' && app.current.tab === 'Current') {
            sendAPI("MYMPD_API_QUEUE_MOVE_SONG", {"from": oldSongpos, "to": newSongpos});
        }
        else if (app.current.card === 'Browse' && app.current.tab === 'Playlists' && app.current.view === 'Detail') {
            playlistMoveSong(oldSongpos, newSongpos);
        }
    }, false);
}

function dragAndDropTableHeader(table) {
    let tableHeader;
    if (document.getElementById(table + 'List')) {
        tableHeader = document.getElementById(table + 'List').getElementsByTagName('tr')[0];
    }
    else {
        tableHeader = table.getElementsByTagName('tr')[0];
        table = 'BrowseDatabase';
    }

    tableHeader.addEventListener('dragstart', function(event) {
        if (event.target.nodeName === 'TH') {
            event.target.classList.add('opacity05');
            event.dataTransfer.setDragImage(event.target, 0, 0);
            event.dataTransfer.effectAllowed = 'move';
            event.dataTransfer.setData('Text', event.target.getAttribute('data-col'));
            dragEl = event.target.cloneNode(true);
        }
    }, false);
    tableHeader.addEventListener('dragleave', function(event) {
        event.preventDefault();
        if (dragEl === undefined || dragEl.nodeName !== 'TH') {
            return;
        }
        if (event.target.nodeName === 'TH') {
            event.target.classList.remove('dragover-th');
        }
    }, false);
    tableHeader.addEventListener('dragover', function(event) {
        event.preventDefault();
        if (dragEl === undefined || dragEl.nodeName !== 'TH') {
            return;
        }
        const th = tableHeader.getElementsByClassName('dragover-th');
        for (let i = 0, j = th.length; i < j; i++) {
            th[i].classList.remove('dragover-th');
        }
        if (event.target.nodeName === 'TH') {
            event.target.classList.add('dragover-th');
        }
        event.dataTransfer.dropEffect = 'move';
    }, false);
    tableHeader.addEventListener('dragend', function(event) {
        event.preventDefault();
        if (dragEl === undefined || dragEl.nodeName !== 'TH') {
            return;
        }
        const th = tableHeader.getElementsByClassName('dragover-th');
        for (let i = 0, j = th.length; i < j; i++) {
            th[i].classList.remove('dragover-th');
        }
        if (this.querySelector('[data-col=' + event.dataTransfer.getData('Text') + ']')) {
            this.querySelector('[data-col=' + event.dataTransfer.getData('Text') + ']').classList.remove('opacity05');
        }
        dragEl = undefined;
    }, false);
    tableHeader.addEventListener('drop', function(event) {
        event.stopPropagation();
        event.preventDefault();
        if (dragEl === undefined || dragEl.nodeName !== 'TH') {
            return;
        }
        this.querySelector('[data-col=' + event.dataTransfer.getData('Text') + ']').remove();
        dragEl.classList.remove('opacity05');
        tableHeader.insertBefore(dragEl, event.target);
        const th = tableHeader.getElementsByClassName('dragover-th');
        for (let i = 0, j = th.length; i < j; i++) {
            th[i].classList.remove('dragover-th');
        }
        if (document.getElementById(table + 'List')) {
            document.getElementById(table + 'List').classList.add('opacity05');
            saveCols(table);
        }
        else {
            saveCols(table, this.parentNode.parentNode);
        }
    }, false);
}

function setColTags(table) {
    const tags = settings.tagList.slice();
    if (features.featTags === false) {
        tags.push('Title');
    }
    tags.push('Duration');
    tags.push('LastModified');

    switch(table) {
        case 'QueueCurrent':
            tags.push('AudioFormat');
            tags.push('Priority');
            //fall through
        case 'BrowsePlaylistsDetail':
        case 'QueueJukebox':
            tags.push('Pos');
            break;
        case 'BrowseFilesystem':
            tags.push('Type');
            tags.push('Filename');
            break;
        case 'Playback':
            tags.push('AudioFormat');
            tags.push('Filetype');
            if (features.featLyrics === true) {
                tags.push('Lyrics');
            }
            break;
        case 'QueueLastPlayed':
            tags.push('Pos');
            tags.push('LastPlayed');
            break;
    }
    //sort tags and append stickers
    tags.sort();
    tags.push('dropdownTitleSticker');
    if (features.featStickers === true) {
        for (const sticker of stickerList) {
            tags.push(sticker);
        }
    }
    return tags;
}

function setColsChecklist(table, menu) {
    const tags = setColTags(table);
    for (let i = 0, j = tags.length; i < j; i++) {
        if (table === 'Playback' && tags[i] === 'Title') {
            continue;
        }
        if (tags[i] === 'dropdownTitleSticker') {
            menu.appendChild(elCreateText('h6', {"class": ["dropdown-header"]}, tn('Sticker')));
        }
        else {
            const btn = elCreateText('button', {"class": ["btn", "btn-secondary", "btn-xs", "clickable", "mi", "mi-small", "me-2"], "name": tags[i]}, 'radio_button_unchecked');
            if (settings['cols' + table].includes(tags[i])) {
                btn.classList.add('active');
                btn.textContent = 'check'
            }
            const div = elCreateNodes('div', {"class": ["form-check"]}, [
                btn,
                elCreateText('lable', {"class": ["form-check-label"], "for": tags[i]}, tn(tags[i]))
            ]);
            menu.appendChild(div);
        }
    }
}

function setCols(table) {
    let sort = app.current.sort;
    if (table === 'Search' && app.cards.Search.sort === 'Title') {
        if (settings.tagList.includes('Title')) {
            sort = 'Title';
        }
        else if (features.featTags === false) {
            sort = 'Filename';
        }
        else {
            sort = '-';
        }
    }

    const thead = document.getElementById(table + 'List').getElementsByTagName('tr')[0];
    elClear(thead);

    for (let i = 0, j = settings['cols' + table].length; i < j; i++) {
        const hname = settings['cols' + table][i];
        const th = elCreateText('th', {"draggable": "true", "data-col": settings['cols' + table][i]}, tn(hname));
        if (hname === 'Track' || hname === 'Pos') {
            th.textContent = '#';
        }

        if (table === 'Search' && (hname === sort || ('-' + hname) === sort) ) {
            let sortdesc = false;
            if (app.current.sort.indexOf('-') === 0) {
                sortdesc = true;
            }
            th.appendChild(
                elCreateText('span', {"class": ["sort-dir", "mi", "float-end"]}, (sortdesc === true ? 'arrow_drop_up' : 'arrow_drop_down'))
            );
        }
        thead.appendChild(th);
    }
    //append action column
    const th = elCreateEmpty('th', {"data-col": "Action"});
    if (features.featTags === true) {
        th.appendChild(
            elCreateText('a', {"href": "#", "data-popover": "columns", "class": ["align-middle", "mi", "mi-small", "clickable"], "data-title-phrase": "Columns", "title": tn('Columns')}, 'settings')
        );
    }
    thead.appendChild(th);
}

function saveCols(table, tableEl) {
    const colsDropdown = document.getElementById(table + 'ColsDropdown');
    let header;
    if (tableEl === undefined) {
        header = document.getElementById(table + 'List').getElementsByTagName('tr')[0];
    }
    else if (typeof(tableEl) === 'string') {
        header = document.querySelector(tableEl).getElementsByTagName('tr')[0];
    }
    else {
        header = tableEl.getElementsByTagName('tr')[0];
    }
    if (colsDropdown) {
        const colInputs = colsDropdown.firstChild.getElementsByTagName('button');
        for (let i = 0, j = colInputs.length; i < j; i++) {
            if (colInputs[i].getAttribute('name') === null) {
                continue;
            }
            let th = header.querySelector('[data-col=' + colInputs[i].name + ']');
            if (colInputs[i].classList.contains('active') === false) {
                if (th) {
                    th.remove();
                }
            }
            else if (!th) {
                th = elCreateText('th', {"data-col": colInputs[i].name}, colInputs[i].name);
                header.insertBefore(th, header.lastChild);
            }
        }
    }

    const params = {"table": "cols" + table, "cols": []};
    const ths = header.getElementsByTagName('th');
    for (let i = 0, j = ths.length; i < j; i++) {
        const name = ths[i].getAttribute('data-col');
        if (name !== 'Action' && name !== null) {
            params.cols.push(name);
        }
    }
    sendAPI("MYMPD_API_COLS_SAVE", params, getSettings);
}

//eslint-disable-next-line no-unused-vars
function saveColsPlayback(table) {
    const colInputs = document.getElementById(table + 'ColsDropdown').firstChild.getElementsByTagName('button');
    const header = document.getElementById('cardPlaybackTags');

    for (let i = 0, j = colInputs.length - 1; i < j; i++) {
        let th = document.getElementById('current' + colInputs[i].name);
        if (colInputs[i].classList.contains('active') === false) {
            //remove disabled tags
            if (th) {
                th.remove();
            }
        }
        else if (!th) {
            //add enabled tags if not already shown
            th = elCreateNodes('div', {"id": "current" + colInputs[i].name}, [
                elCreateText('small', {}, tn(colInputs[i].name)),
                elCreateEmpty('p', {})
            ]);
            setData(th, 'tag', colInputs[i].name);
            header.appendChild(th);
        }
    }

    //construct columns to save from actual playback card
    const params = {"table": "cols" + table, "cols": []};
    const ths = header.getElementsByTagName('div');
    for (let i = 0, j = ths.length; i < j; i++) {
        const name = getData(ths[i], 'tag');
        if (name) {
            params.cols.push(name);
        }
    }
    sendAPI("MYMPD_API_COLS_SAVE", params, getSettings);
}

function replaceTblRow(row, el) {
    const menuEl = row.querySelector('[data-popover]');
    if (menuEl) {
        hidePopover();
    }
    row.replaceWith(el);
}

function addDiscRow(disc, album, albumartist, colspan) {
    const row = elCreateNodes('tr', {"class": ["not-clickable"]}, [
        elCreateNode('td', {},
            elCreateText('span', {"class": ["mi"]}, 'album')
        ),
        elCreateText('td', {"colspan": (colspan - 1)}, tn('Disc') + ' ' + disc),
        elCreateNode('td', {},
            elCreateText('a', {"data-popover": "disc", "href": "#", "class": ["mi", "color-darkgrey"], "title": tn('Actions')}, ligatureMore)
        )
    ]);
    setData(row, 'Disc', disc);
    setData(row, 'Album', album);
    setData(row, 'AlbumArtist', albumartist);
    return row;
}

function updateTable(obj, list, perRowCallback, createRowCellsCallback) {
    const table = document.getElementById(list + 'List');
    setScrollViewHeight(table);
    const tbody = table.getElementsByTagName('tbody')[0];
    const colspan = settings['cols' + list] !== undefined ? settings['cols' + list].length : 0;

    const nrItems = obj.result.returnedEntities;
    const tr = tbody.getElementsByTagName('tr');
    const smallWidth = window.innerWidth < 576 ? true : false;

    //disc handling for album view
    let z = 0;
    let lastDisc = obj.result.data.length > 0 && obj.result.data[0].Disc !== undefined ? Number(obj.result.data[0].Disc) : 0;
    if (obj.result.Discs !== undefined && obj.result.Discs > 1) {
        const row = addDiscRow(1, obj.result.data[0].Album, obj.result.data[0][tagAlbumArtist], colspan);
        if (z < tr.length) {
            replaceTblRow(tr[z], row);
        }
        else {
            tbody.append(row);
        }
        z++;
    }
    for (let i = 0; i < nrItems; i++) {
        //disc handling for album view
        if (obj.result.data[0].Disc !== undefined && lastDisc < Number(obj.result.data[i].Disc)) {
            const row = addDiscRow(obj.result.data[i].Disc, obj.result.data[i].Album, obj.result.data[i][tagAlbumArtist], colspan);
            if (i + z < tr.length) {
                replaceTblRow(tr[i + z], row);
            }
            else {
                tbody.append(row);
            }
            z++;
            lastDisc = obj.result.data[i].Disc;
        }
        const row = elCreateEmpty('tr', {});
        if (perRowCallback !== undefined && typeof(perRowCallback) === 'function') {
            perRowCallback(row, obj.result.data[i]);
        }
        //data row
        row.setAttribute('tabindex', 0);
        //set artist and album data
        if (obj.result.data[i].Album !== undefined) {
            setData(row, 'Album', obj.result.data[i].Album);
        }
        if (obj.result.data[i][tagAlbumArtist] !== undefined) {
            setData(row, 'AlbumArtist', obj.result.data[i][tagAlbumArtist]);
        }
        //and other browse tags
        for (const tag of settings.tagListBrowse) {
            if (albumFilters.includes(tag) &&
                obj.result.data[i][tag] !== undefined &&
                checkTagValue(obj.result.data[i][tag], '-') === false)
            {
                setData(row, tag, obj.result.data[i][tag]);
            }
        }
        //set Title to name if not defined - for folders and playlists
        if (obj.result.data[i].Title === undefined) {
            obj.result.data[i].Title = obj.result.data[i].name;
        }

        if (createRowCellsCallback !== undefined && typeof(createRowCellsCallback) === 'function') {
            //custom row content
            createRowCellsCallback(row, obj.result.data[i]);
        }
        else {
            //default row content
            tableRow(row, obj.result.data[i], list, colspan, smallWidth);
        }
        if (i + z < tr.length) {
            replaceTblRow(tr[i + z], row);
        }
        else {
            tbody.append(row);
        }
    }

    const trLen = tr.length - 1;
    for (let i = trLen; i >= nrItems + z; i --) {
        tr[i].remove();
    }

    if (nrItems === 1000) {
        tbody.appendChild(warningRow('Too many results, list is cropped', colspan + 1));
    }

    setPagination(obj.result.totalEntities, obj.result.returnedEntities);

    if (nrItems === 0) {
        tbody.appendChild(emptyRow(colspan + 1));
    }
    table.classList.remove('opacity05');
}

function tableRow(row, data, list, colspan, smallWidth) {
    if (data.Type === 'parentDir') {
        row.appendChild(elCreateText('td', {"colspan": (colspan + 1), "title": tn('Open parent folder')}, '..'));
    }
    else {
        if (smallWidth === true) {
            const td = elCreateEmpty('td', {"colspan": colspan});
            for (let c = 0, d = settings['cols' + list].length; c < d; c++) {
                td.appendChild(
                    elCreateNodes('div', {"class": ["row"]}, [
                        elCreateText('small', {"class": ["col-3"]}, tn(settings['cols' + list][c])),
                        elCreateNode('span', {"data-col": settings['cols' + list][c], "class": ["col-9"]}, printValue(settings['cols' + list][c], data[settings['cols' + list][c]]))
                    ])
                );
            }
            row.appendChild(td);
        }
        else {
            for (let c = 0, d = settings['cols' + list].length; c < d; c++) {
                row.appendChild(
                    elCreateNode('td', {"data-col": settings['cols' + list][c]},
                        printValue(settings['cols' + list][c], data[settings['cols' + list][c]])
                    )
                );
            }
        }
        row.appendChild(
            elCreateNode('td', {},
                elCreateText('a', {"data-col": "Action", "href": "#", "class": ["mi", "color-darkgrey"], "title": tn('Actions')}, ligatureMore)
            )
        );
    }
}

function emptyRow(colspan) {
    return elCreateNode('tr', {"class": ["not-clickable"]},
        elCreateNode('td', {"colspan": colspan},
            elCreateText('div', {"class": ["alert", "alert-secondary"]}, tn('Empty list'))
        )
    );
}

function errorRow(obj, colspan) {
    return elCreateNode('tr', {"class": ["not-clickable"]},
        elCreateNode('td', {"colspan": colspan},
            elCreateText('div', {"class": ["alert", "alert-danger"]}, tn(obj.error.message, obj.error.data))
        )
    );
}

function warningRow(message, colspan) {
    return elCreateNode('tr', {"class": ["not-clickable"]},
        elCreateNode('td', {"colspan": colspan},
            elCreateText('div', {"class": ["alert", "alert-warning"]}, tn(message))
        )
    );
}

function checkResultId(obj, id) {
    return checkResult(obj, document.getElementById(id).getElementsByTagName('tbody')[0]);
}

function checkResult(obj, tbody) {
    const colspan = tbody.parentNode.getElementsByTagName('tr')[0].getElementsByTagName('th').length;
    if (obj.error) {
        elClear(tbody);
        tbody.appendChild(errorRow(obj, colspan));
        tbody.parentNode.classList.remove('opacity05');
        setPagination(0, 0);
        return false;
    }
    if (obj.result.returnedEntities === 0) {
        elClear(tbody);
        tbody.appendChild(emptyRow(colspan));
        tbody.parentNode.classList.remove('opacity05');
        setPagination(0, 0);
        return false;
    }
    return true;
}
