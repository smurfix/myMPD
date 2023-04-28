"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

/** @module outputs_js */

/**
 * Initializes the outputs html elements
 * @returns {void}
 */
function initOutputs() {
    domCache.volumeBar.addEventListener('change', function() {
        setVolume();
    }, false);

    document.getElementById('volumeMenu').parentNode.addEventListener('show.bs.dropdown', function() {
        sendAPI("MYMPD_API_PLAYER_OUTPUT_LIST", {}, parseOutputs, true);
    });

    document.getElementById('outputs').addEventListener('click', function(event) {
        if (event.target.nodeName === 'A') {
            event.preventDefault();
            BSN.Dropdown.getInstance(document.getElementById('volumeMenu')).toggle();
            showListOutputAttributes(getData(event.target.parentNode, 'output-name'));
        }
        else {
            const target = event.target.nodeName === 'BUTTON' ? event.target : event.target.parentNode;
            event.preventDefault();
            sendAPI("MYMPD_API_PLAYER_OUTPUT_TOGGLE", {
                "outputId": Number(getData(target, 'output-id')),
                "state": (target.classList.contains('active') ? 0 : 1)
            }, null, false);
            toggleBtn(target, undefined);
        }
    }, false);
}

/**
 * Parses the response of MYMPD_API_PLAYER_OUTPUT_LIST
 * @param {object} obj jsonrpc response
 * @returns {void}
 */
function parseOutputs(obj) {
    const outputList = document.getElementById('outputs');
    elClear(outputList);
    if (obj.error) {
        outputList.appendChild(
            elCreateTextTn('div', {"class": ["list-group-item", "alert", "alert-danger"]}, obj.error.message, obj.error.data)
        );
        return;
    }
    if (obj.result.numOutputs === 0) {
        outputList.appendChild(
            elCreateTextTn('div', {"class": ["list-group-item", "alert", "alert-secondary"]}, 'No outputs found')
        );
        return;
    }

    for (let i = 0; i < obj.result.numOutputs; i++) {
        if (obj.result.data[i].plugin === 'dummy') {
            continue;
        }
        const titlePhrase = Object.keys(obj.result.data[i].attributes).length > 0 ? 'Edit attributes' : 'Show attributes';
        const icon = settings.webuiSettings.outputLigatures[obj.result.data[i].plugin] !== undefined 
            ? settings.webuiSettings.outputLigatures[obj.result.data[i].plugin]
            : settings.webuiSettings.outputLigatures.default;
        const buttonTitle = tn('Plugin') + ': ' + tn(obj.result.data[i].plugin);
        const btn = elCreateNodes('button', {"class": ["btn", "btn-secondary", "d-flex", "justify-content-between"], "title": buttonTitle, "id": "btnOutput" + obj.result.data[i].id}, [
            elCreateText('span', {"class": ["mi", "align-self-center"]}, icon),
            elCreateText('span', {"class": ["mx-2", "align-self-center"]}, obj.result.data[i].name),
            elCreateText('a', {"class": ["mi", "align-self-center"], "data-title-phrase": titlePhrase}, 'settings')
        ]);
        setData(btn, 'output-name', obj.result.data[i].name);
        setData(btn, 'output-id', obj.result.data[i].id);
        if (obj.result.data[i].state === 1) {
            btn.classList.add('active');
        }
        outputList.appendChild(btn);
    }
    //prevent overflow of dropup
    const outputsEl = document.getElementById('outputs');
    const posY = getYpos(document.getElementById('outputsDropdown'));
    if (posY < 0) {
        outputsEl.style.maxHeight = (outputsEl.offsetHeight + posY) + 'px';
    }
    else {
        outputsEl.style.maxHeight = 'none';
    }
}

/**
 * Shows the output attributes modal 
 * @param {string} outputName the output name
 * @returns {void}
 */
function showListOutputAttributes(outputName) {
    cleanupModalId('modalOutputAttributes');
    sendAPI("MYMPD_API_PLAYER_OUTPUT_LIST", {}, function(obj) {
        const tbody = document.getElementById('outputAttributesList');
        if (checkResult(obj, tbody) === false) {
            return;
        }
        //we get all outputs, filter by outputName
        for (const output of obj.result.data) {
            if (output.name === outputName) {
                parseOutputAttributes(output);
                break;
            }
        }
    }, false);
    uiElements.modalOutputAttributes.show();
}

/**
 * Creates the output attributes table content
 * @param {object} output output object
 * @returns {void}
 */
function parseOutputAttributes(output) {
    document.getElementById('modalOutputAttributesId').value = output.id;
    const tbody = document.getElementById('outputAttributesList');
    elClear(tbody);
    for (const n of ['name', 'state', 'plugin']) {
        if (n === 'state') {
            output[n] = output[n] === 1 ? tn('Enabled') : tn('Disabled');
        }
        tbody.appendChild(
            elCreateNodes('tr', {}, [
                elCreateTextTn('td', {}, n),
                elCreateText('td', {}, output[n])
            ])
        );
    }
    let i = 0;
    for (const key in output.attributes) {
        i++;
        tbody.appendChild(
            elCreateNodes('tr', {}, [
                elCreateText('td', {}, key),
                elCreateNode('td', {},
                    elCreateEmpty('input', {"name": key, "class": ["form-control"], "type": "text", "value": output.attributes[key]})
                )
            ])
        );
    }
    if (i > 0) {
        elEnableId('btnOutputAttributesSave');
    }
    else {
        elDisableId('btnOutputAttributesSave');
    }
}

/**
 * Saves the output attributes
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function saveOutputAttributes() {
    cleanupModalId('modalOutputAttributes');
    const params = {};
    params.outputId = Number(document.getElementById('modalOutputAttributesId').value);
    params.attributes = {};
    const els = document.querySelectorAll('#outputAttributesList input');
    for (let i = 0, j = els.length; i < j; i++) {
        params.attributes[els[i].name] = els[i].value;
    }
    sendAPI('MYMPD_API_PLAYER_OUTPUT_ATTRIBUTES_SET', params, saveOutputAttributesClose, true);
}

/**
 * Handler for MYMPD_API_PLAYER_OUTPUT_ATTRIBUTES_SET response
 * @param {object} obj jsonrpc response
 * @returns {void}
 */
function saveOutputAttributesClose(obj) {
    if (obj.error) {
        showModalAlert(obj);
    }
    else {
        uiElements.modalOutputAttributes.hide();
    }
}

/**
 * Parses the response of MYMPD_API_PLAYER_VOLUME_GET
 * @param {object} obj jsonrpc response
 * @returns {void}
 */
function parseVolume(obj) {
    if (obj.result.volume === -1) {
        document.getElementById('volumePrct').textContent = tn('Volumecontrol disabled');
        elHideId('volumeControl');
        elClear(
            document.getElementById('volumeMenu').lastElementChild
        );
    }
    else {
        elShowId('volumeControl');
        document.getElementById('volumePrct').textContent = obj.result.volume + ' %';
        const volumeMenu = document.getElementById('volumeMenu');
        volumeMenu.firstElementChild.textContent =
            obj.result.volume === 0 ? 'volume_off' :
                obj.result.volume < 50 ? 'volume_down' : 'volume_up';
        volumeMenu.lastElementChild.textContent = obj.result.volume + smallSpace + '%';
    }
    domCache.volumeBar.value = obj.result.volume;
}

/**
 * Changes the relative volume 
 * @param {string} dir direction: on of up, down
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function volumeStep(dir) {
    const step = dir === 'up' ? settings.volumeStep : 0 - settings.volumeStep;
    sendAPI("MYMPD_API_PLAYER_VOLUME_CHANGE", {
        "volume": step
    }, null, false);
}

/**
 * Sets the volume to an absolute value
 * @returns {void}
 */
function setVolume() {
    sendAPI("MYMPD_API_PLAYER_VOLUME_SET", {
        "volume": Number(domCache.volumeBar.value)
    }, null, false);
}
