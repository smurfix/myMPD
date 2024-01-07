"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

/** @module modalScripts_js */

/**
 * Initialization functions for the script elements
 * @returns {void}
 */
function initModalScripts() {
    elGetById('modalScriptsAddArgumentInput').addEventListener('keyup', function(event) {
        if (event.key === 'Enter') {
            event.preventDefault();
            event.stopPropagation();
            addScriptArgument();
        }
    }, false);

    elGetById('modalScriptsArgumentsInput').addEventListener('click', function(event) {
        if (event.target.nodeName === 'OPTION') {
            removeScriptArgument(event);
            event.stopPropagation();
        }
    }, false);

    elGetById('modalScriptsList').addEventListener('click', function(event) {
        event.stopPropagation();
        event.preventDefault();
        const target = event.target.closest('TR');
        if (event.target.nodeName === 'A') {
            const action = getData(event.target, 'action');
            const script = getData(target, 'script');
            switch(action) {
                case 'delete':
                    deleteScript(event.target, script);
                    break;
                case 'execute':
                    execScript(getData(target, 'href'));
                    break;
                case 'add2home':
                    addScriptToHome(script, getData(target, 'href'));
                    break;
                default:
                    logError('Invalid action: ' + action);
            }
            return;
        }
        if (checkTargetClick(target) === true) {
            showEditScript(getData(target, 'script'));
        }
    }, false);

    elGetById('modalScriptsAddAPIcallBtn').parentNode.addEventListener('show.bs.dropdown', function() {
        const dw = elGetById('modalScriptsContentInput').offsetWidth - elGetById('modalScriptsAddAPIcallBtn').parentNode.offsetLeft;
        elGetById('modalScriptsAddAPIcallDropdown').style.width = dw + 'px';
    }, false);

    elGetById('modalScriptsAddFunctionBtn').parentNode.addEventListener('show.bs.dropdown', function() {
        const dw = elGetById('modalScriptsContentInput').offsetWidth - elGetById('modalScriptsAddFunctionBtn').parentNode.offsetLeft;
        elGetById('modalScriptsAddFunctionDropdown').style.width = dw + 'px';
    }, false);

    elGetById('modalScriptsImportBtn').parentNode.addEventListener('show.bs.dropdown', function() {
        const dw = elGetById('modalScriptsContentInput').offsetWidth - elGetById('modalScriptsImportBtn').parentNode.offsetLeft;
        elGetById('modalScriptsImportDropdown').style.width = dw + 'px';
        getImportScriptList();
    }, false);

    const modalScriptsAPIcallSelectEl = elGetById('modalScriptsAPIcallSelect');
    elClear(modalScriptsAPIcallSelectEl);
    modalScriptsAPIcallSelectEl.appendChild(
        elCreateTextTn('option', {"value": ""}, 'Select method')
    );
    for (const m of Object.keys(APImethods).sort()) {
        modalScriptsAPIcallSelectEl.appendChild(
            elCreateText('option', {"value": m}, m)
        );
    }

    modalScriptsAPIcallSelectEl.addEventListener('change', function(event) {
        const value = getSelectValue(event.target);
        elGetById('modalScriptsAPIdesc').textContent = value !== '' ? APImethods[value].desc : '';
    }, false);
  
    const modalScriptsFunctionSelectEl = elGetById('modalScriptsFunctionSelect');
    elClear(modalScriptsFunctionSelectEl);
    modalScriptsFunctionSelectEl.appendChild(
        elCreateTextTn('option', {"value": ""}, 'Select function')
    );
    for (const m in LUAfunctions) {
        modalScriptsFunctionSelectEl.appendChild(
            elCreateText('option', {"value": m}, m)
        );
    }

    modalScriptsFunctionSelectEl.addEventListener('change', function(event) {
        const value = getSelectValue(event.target);
        elGetById('modalScriptsFunctionDesc').textContent = value !== '' ? LUAfunctions[value].desc : '';
    }, false);
}

/**
 * Adds a function to the script content element
 * @param {Event} event triggering event
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function addScriptFunction(event) {
    event.preventDefault();
    event.stopPropagation();
    const value = getSelectValueId('modalScriptsFunctionSelect');
    if (value === '') {
        return;
    }
    const el = elGetById('modalScriptsContentInput');
    const [start, end] = [el.selectionStart, el.selectionEnd];
    el.setRangeText(LUAfunctions[value].func, start, end, 'end');
    BSN.Dropdown.getInstance(elGetById('modalScriptsAddFunctionBtn')).hide();
    setFocus(el);
}

/**
 * Adds an API call to the script content element
 * @param {Event} event triggering event
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function addScriptAPIcall(event) {
    event.preventDefault();
    event.stopPropagation();
    const method = getSelectValueId('modalScriptsAPIcallSelect');
    if (method === '') {
        return;
    }
    const el = elGetById('modalScriptsContentInput');
    const [start, end] = [el.selectionStart, el.selectionEnd];
    const newText =
        'options = {}\n' +
        apiParamsToArgs(APImethods[method].params) +
        'rc, result = mympd.api("' + method + '", options)\n' +
        'if rc == 0 then\n' +
        '\n' +
        'end\n';
    el.setRangeText(newText, start, end, 'preserve');
    BSN.Dropdown.getInstance(elGetById('modalScriptsAddAPIcallBtn')).hide();
    setFocus(el);
}

/**
 * Imports a script
 * @param {Event} event triggering event
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function importScript(event) {
    event.preventDefault();
    event.stopPropagation();
    const script = getSelectValueId('modalScriptsImportSelect');
    if (script === '') {
        return;
    }
    //@ts-ignore
    btnWaiting(event.target, true);
    getImportScript(script);
}

/**
 * Fetches the list of available scripts to import
 * @returns {void}
 */
function getImportScriptList() {
    const sel = elGetById('modalScriptsImportSelect');
    sel.setAttribute('disabled', 'disabled');
    httpGet(subdir + '/proxy?uri=' + myEncodeURI('https://jcorporation.github.io/myMPD/scripting/scripts/index.json'), function(obj) {
        sel.options.length = 0;
        for (const script of obj.scripts) {
            sel.appendChild(
                elCreateText('option', {"value": script}, script)
            );
        }
        sel.removeAttribute('disabled');
    }, true);
}

/**
 * Imports a script
 * @param {string} script script to import
 * @returns {void}
 */
function getImportScript(script) {
    elGetById('modalScriptsContentInput').setAttribute('disabled', 'disabled');
    httpGet(subdir + '/proxy?uri=' + myEncodeURI('https://jcorporation.github.io/myMPD/scripting/scripts/' + script), function(text) {
        const lines = text.split('\n');
        const firstLine = lines.shift();
        let obj;
        try {
            obj = JSON.parse(firstLine.substring(firstLine.indexOf('{')));
            const scriptArgEl = elGetById('modalScriptsArgumentsInput');
            scriptArgEl.options.length = 0;
            for (let i = 0, j = obj.arguments.length; i < j; i++) {
                scriptArgEl.appendChild(
                    elCreateText('option', {}, obj.arguments[i])
                );
            }
            elGetById('modalScriptsContentInput').value = lines.join('\n');
        }
        catch(error) {
            showModalAlert({
                "error": {
                    "message": "Can not parse script arguments"
                }
            });
            logError('Can not parse script arguments:' + firstLine);
        }

        elGetById('modalScriptsContentInput').removeAttribute('disabled');
        BSN.Dropdown.getInstance(elGetById('modalScriptsImportBtn')).hide();
        btnWaitingId('modalScriptsImportScriptBtn', false);
        setFocusId('modalScriptsContentInput');
    }, false);
}

/**
 * Adds the documented api params to the options lua table for the add api call function
 * @param {object} p parameters object
 * @returns {string} lua code
 */
function apiParamsToArgs(p) {
    let args = '';
    for (const param in p) {
        args += 'options["' + param + '"] = ';
        switch(p[param].type) {
            case APItypes.string:
                args += '"' + p[param].example + '"';
                break;
            case APItypes.array:
                args += '{' + p[param].example.slice(1, -1) + '}';
                break;
            case APItypes.object: {
                args += '{}';
                break;
            }
            default:
                args += p[param].example;
        }
        args += '\n';
    }
    return args;
}

/**
 * Saves a script
 * @param {Element} target triggering element
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function saveScript(target) {
    cleanupModalId('modalScripts');
    btnWaiting(target, true);
    const args = [];
    const argSel = elGetById('modalScriptsArgumentsInput');
    for (let i = 0, j = argSel.options.length; i < j; i++) {
        args.push(argSel.options[i].text);
    }
    sendAPI("MYMPD_API_SCRIPT_SAVE", {
        "oldscript": getDataId('modalScriptsEditTab', 'id'),
        "script": elGetById('modalScriptsScriptInput').value,
        "order": Number(elGetById('modalScriptsOrderInput').value),
        "content": elGetById('modalScriptsContentInput').value,
        "arguments": args
    }, saveScriptCheckError, true);
}

/**
 * Handler for the MYMPD_API_SCRIPT_SAVE jsonrpc response
 * @param {object} obj jsonrpc response
 * @returns {void}
 */
function saveScriptCheckError(obj) {
    if (modalApply(obj) === true) {
        showListScripts();
    }
}

/**
 * Validates a script
 * @param {Element} target triggering element
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function validateScript(target) {
    cleanupModalId('modalScripts');
    btnWaiting(target, true);
    sendAPI("MYMPD_API_SCRIPT_VALIDATE", {
        "script": elGetById('modalScriptsScriptInput').value,
        "content": elGetById('modalScriptsContentInput').value,
    }, validateScriptCheckError, true);
}

/**
 * Handler for the MYMPD_API_SCRIPT_VALIDATE jsonrpc response
 * @param {object} obj jsonrpc response
 * @returns {void}
 */
function validateScriptCheckError(obj) {
    if (modalApply(obj) === true) {
        showModalInfo('Script syntax is valid');
    }
}

/**
 * Appends an argument to the list of script arguments
 * @returns {void}
 */
function addScriptArgument() {
    const el = elGetById('modalScriptsAddArgumentInput');
    if (validatePrintableEl(el)) {
        elGetById('modalScriptsArgumentsInput').appendChild(
            elCreateText('option', {}, el.value)
        );
        el.value = '';
    }
}

/**
 * Removes an argument from the list of script arguments
 * @param {Event} ev triggering element
 * @returns {void}
 */
function removeScriptArgument(ev) {
    const el = elGetById('modalScriptsAddArgumentInput');
    // @ts-ignore
    el.value = ev.target.text;
    ev.target.remove();
    setFocus(el);
}

/**
 * Opens the scripts modal and shows the edit tab
 * @param {string} script name to edit
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function showEditScriptModal(script) {
    uiElements.modalScripts.show();
    showEditScript(script);
}

/**
 * Opens the scripts modal and shows the list tab
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function showListScriptModal() {
    uiElements.modalScripts.show();
    showListScripts();
}

/**
 * Shows the edit script tab
 * @param {string} script script name
 * @returns {void}
 */
//eslint-disable-next-line no-unused-vars
function showEditScript(script) {
    cleanupModalId('modalScripts');
    elGetById('modalScriptsContentInput').removeAttribute('disabled');
    elGetById('modalScriptsListTab').classList.remove('active');
    elGetById('modalScriptsEditTab').classList.add('active');
    elHideId('modalScriptsListFooter');
    elShowId('modalScriptsEditFooter');

    if (script !== '') {
        sendAPI("MYMPD_API_SCRIPT_GET", {"script": script}, parseEditScript, false);
    }
    else {
        setDataId('modalScriptsEditTab', 'id', '');
        elGetById('modalScriptsScriptInput').value = '';
        elGetById('modalScriptsOrderInput').value = '1';
        elGetById('modalScriptsAddArgumentInput').value = '';
        elClearId('modalScriptsArgumentsInput');
        elGetById('modalScriptsContentInput').value = '';
    }
    setFocusId('modalScriptsScriptInput');
}

/**
 * Parses the MYMPD_API_SCRIPT_GET jsonrpc response
 * @param {object} obj jsonrpc response
 * @returns {void}
 */
function parseEditScript(obj) {
    setDataId('modalScriptsEditTab', 'id', obj.result.script);
    elGetById('modalScriptsScriptInput').value = obj.result.script;
    elGetById('modalScriptsOrderInput').value = obj.result.metadata.order;
    elGetById('modalScriptsAddArgumentInput').value = '';
    const selSA = elGetById('modalScriptsArgumentsInput');
    selSA.options.length = 0;
    for (let i = 0, j = obj.result.metadata.arguments.length; i < j; i++) {
        selSA.appendChild(
            elCreateText('option', {}, obj.result.metadata.arguments[i])
        );
    }
    elGetById('modalScriptsContentInput').value = obj.result.content;
}

/**
 * Shows the list scripts tab
 * @returns {void}
 */
function showListScripts() {
    cleanupModalId('modalScripts');
    elGetById('modalScriptsListTab').classList.add('active');
    elGetById('modalScriptsEditTab').classList.remove('active');
    elShowId('modalScriptsListFooter');
    elHideId('modalScriptsEditFooter');
    getScriptList(true);
}

/**
 * Deletes a script after confirmation
 * @param {EventTarget} el triggering element
 * @param {string} script script to delete
 * @returns {void}
 */
function deleteScript(el, script) {
    showConfirmInline(el.parentNode.previousSibling, tn('Do you really want to delete the script?', {"script": script}), tn('Yes, delete it'), function() {
        sendAPI("MYMPD_API_SCRIPT_RM", {
            "script": script
        }, deleteScriptCheckError, true);
    });
}

/**
 * Handler for the MYMPD_API_SCRIPT_RM jsonrpc response
 * @param {object} obj jsonrpc response
 * @returns {void}
 */
function deleteScriptCheckError(obj) {
    if (modalApply(obj) === true) {
        getScriptList(true);
    }
}

/**
 * Gets the list of scripts
 * @param {boolean} all true = get all scripts, false = get all scripts with pos > 0
 * @returns {void}
 */
function getScriptList(all) {
    sendAPI("MYMPD_API_SCRIPT_LIST", {
        "all": all
    }, parseScriptList, true);
}

/**
 * Parses the MYMPD_API_SCRIPT_LIST jsonrpc response
 * @param {object} obj jsonrpc response
 * @returns {void}
 */
function parseScriptList(obj) {
    const tbodyScripts = elGetById('modalScriptsList');
    elClear(tbodyScripts);
    const mainmenuScripts = elGetById('scripts');
    elClear(mainmenuScripts);
    const triggerScripts = elGetById('modalTriggerScriptInput');
    elClear(triggerScripts);

    if (checkResult(obj, tbodyScripts) === false) {
        return;
    }

    const timerActions = elCreateEmpty('optgroup', {"id": "timerActionsScriptsOptGroup", "label": tn('Script')});
    setData(timerActions, 'value', 'script');
    const scriptListLen = obj.result.data.length;
    if (scriptListLen > 0) {
        obj.result.data.sort(function(a, b) {
            return a.metadata.order - b.metadata.order;
        });
        for (let i = 0; i < scriptListLen; i++) {
            //script list in main menu
            if (obj.result.data[i].metadata.order > 0) {
                const a = elCreateNodes('a', {"class": ["dropdown-item", "alwaysEnabled", "py-2"], "href": "#"}, [
                    elCreateText('span', {"class": ["mi", "me-2"]}, "code"),
                    elCreateText('span', {}, obj.result.data[i].name)
                ]);
                setData(a, 'href', {"script": obj.result.data[i].name, "arguments": obj.result.data[i].metadata.arguments});
                mainmenuScripts.appendChild(a);
            }
            //script list in scripts modal
            const tr = elCreateNodes('tr', {"title": tn('Edit')}, [
                elCreateText('td', {}, obj.result.data[i].name),
                elCreateNodes('td', {"data-col": "Action"}, [
                    elCreateText('a', {"href": "#", "data-title-phrase": "Delete", "data-action": "delete", "class": ["me-2", "mi", "color-darkgrey"]}, 'delete'),
                    elCreateText('a', {"href": "#", "data-title-phrase": "Execute", "data-action": "execute", "class": ["me-2", "mi", "color-darkgrey"]}, 'play_arrow'),
                    elCreateText('a', {"href": "#", "data-title-phrase": "Add to homescreen", "data-action": "add2home", "class": ["me-2", "mi", "color-darkgrey"]}, 'add_to_home_screen')
                ])
            ]);
            setData(tr, 'script', obj.result.data[i].name);
            setData(tr, 'href', {"script": obj.result.data[i].name, "arguments": obj.result.data[i].metadata.arguments});
            tbodyScripts.appendChild(tr);

            //script list select for timers
            const option = elCreateText('option', {"value": obj.result.data[i].name}, obj.result.data[i].name);
            setData(option, 'arguments', {"arguments": obj.result.data[i].metadata.arguments});
            timerActions.appendChild(option);
            //script list select for trigger
            const option2 = option.cloneNode(true);
            setData(option2, 'arguments', {"arguments": obj.result.data[i].metadata.arguments});
            triggerScripts.appendChild(option2);
        }
    }

    if (scriptListLen === 0) {
        elHide(mainmenuScripts.previousElementSibling);
    }
    else {
        elShow(mainmenuScripts.previousElementSibling);
    }
    //update timer actions select
    const old = elGetById('timerActionsScriptsOptGroup');
    if (old) {
        old.replaceWith(timerActions);
    }
    else {
        elGetById('modalTimerActionInput').appendChild(timerActions);
    }
}
