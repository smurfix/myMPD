"use strict";
// SPDX-License-Identifier: GPL-3.0-or-later
// myMPD (c) 2018-2023 Juergen Mang <mail@jcgames.de>
// https://github.com/jcorporation/mympd

/** @module forms_js */

/**
 * Populates the settings json from input elements
 * @param {string} prefix for element ids
 * @param {object} settingsParams settings object to populate
 * @param {object} defaultFields settingsFields object
 * @returns {boolean} true on success, else false
 */
function formToJson(prefix, settingsParams, defaultFields) {
    for (const key in defaultFields) {
        if (defaultFields[key].inputType === 'none') {
            continue;
        }
        const id = prefix + ucFirst(key) + 'Input';
        const el = elGetById(id);
        if (el) {
            const value = defaultFields[key].inputType === 'select'
                ? getSelectValue(el)
                : defaultFields[key].inputType === 'mympd-select-search'
                    ? getData(el, 'value')
                    : defaultFields[key].inputType === 'checkbox'
                        ? getBtnChkValue(el)
                        : el.value;
            if (defaultFields[key].validate !== undefined) {
                const func = getFunctionByName(defaultFields[key].validate.cmd);
                if (func(el, ... defaultFields[key].validate.options) === false) {
                    logError('Validation failed for ' + key);
                    return false;
                }
            }
            settingsParams[key] = defaultFields[key].contentType === 'number'
                ? Number(value)
                : value;
        }
        else {
            logError('Element not found: ' + id);
            return false;
        }
    }
    return true;
}

/**
 * Creates form fields
 * @param {object} defaultFields object with elements to create and the default values
 * @param {string} prefix prefix for element ids
 * @param {object} forms cache for the form field containers
 * @returns {void}
 */
function createForm(defaultFields, prefix, forms) {
    // iterate through sorted keys
    const settingsKeys = Object.keys(defaultFields);
    settingsKeys.sort(function(a, b) {
        return defaultFields[a].sort - defaultFields[b].sort;
    });
    for (let i = 0, j = settingsKeys.length; i < j; i++) {
        const key = settingsKeys[i];
        // check if we should add a field
        if (defaultFields[key].form === undefined ||
            defaultFields[key].inputType === 'none')
        {
            continue;
        }
        // calculate a camelCase id
        const id = prefix + ucFirst(key) + 'Input';
        // get the container
        const form = defaultFields[key].form;
        if (forms[form] === undefined) {
            forms[form] = elGetById(form);
            // clear the container if it was not cached
            elClear(forms[form]);
        }
        // create the form field
        const col = elCreateNode('div', {"class": ["col-sm-8", "position-relative"]},
            elCreateNode('div', {"class": ["input-group"]},
                elCreateEmpty('div', {"class": ["flex-grow-1", "position-relative"]})
            )
        );
        if (defaultFields[key].inputType === 'select') {
            // simple select
            const cssClass = concatArrays(["form-select"], defaultFields[key].class);
            const select = elCreateEmpty('select', {"class": cssClass, "id": id});
            for (const value in defaultFields[key].validValues) {
                select.appendChild(
                    elCreateTextTn('option', {"value": value}, defaultFields[key].validValues[value])
                );
            }
            col.firstChild.firstChild.appendChild(select);
        }
        else if (defaultFields[key].inputType === 'mympd-select-search') {
            // searchable select
            const cssClass = concatArrays(["form-select"], defaultFields[key].class);
            const input = elCreateEmpty('input', {"class": cssClass, "id": id});
            setData(input, 'cb-filter', defaultFields[key].cbCallback);
            setData(input, 'cb-filter-options', defaultFields[key].cbCallbackOptions);
            input.setAttribute('data-is', 'mympd-select-search');
            if (defaultFields[key].readOnly === true) {
                input.setAttribute('readonly', 'readonly');
            }
            col.firstChild.firstChild.appendChild(
                elCreateNode('div', {"class": ["btn-group", "d-flex"]}, input)
            );
        }
        else if (defaultFields[key].inputType === 'checkbox') {
            // checkbox
            const cssClass = concatArrays(["btn", "btn-sm", "btn-secondary", "mi", "chkBtn"], defaultFields[key].class);
            const btn = elCreateEmpty('button', {"type": "button", "id": id, "class": cssClass});
            if (defaultFields[key].onClick !== undefined) {
                // custom click handler
                btn.addEventListener('click', function(event) {
                    // @ts-ignore
                    window[defaultFields[key].onClick](event);
                }, false);
            }
            else {
                // default click handler
                btn.addEventListener('click', function(event) {
                    toggleBtnChk(event.target, undefined);
                }, false);
            }
            col.firstChild.firstChild.appendChild(btn);
        }
        else if (defaultFields[key].inputType === 'password') {
            // password field
            const cssClass = concatArrays(["form-control"], defaultFields[key].class);
            col.firstChild.firstChild.appendChild(
                elCreateEmpty('input', {"is": "mympd-input-password", "id": id,
                    "value": settingsFields[key], "class": cssClass, "type": "password"})
            );
        }
        else if (defaultFields[key].inputType === 'color') {
            // color input with reset to default button
            const cssClass = concatArrays(["form-control"], defaultFields[key].class);
            col.firstChild.firstChild.appendChild(
                elCreateEmpty('input', {"is": "mympd-input-reset", "id": id, "data-default": defaultFields[key].defaultValue,
                    "value": defaultFields[key].defaultValue, "class": cssClass, "type": defaultFields[key].inputType})
            );
        }
        else {
            // text input with reset to default button
            const cssClass = concatArrays(["form-control"], defaultFields[key].class);
            const placeholder = defaultFields[key].placeholder !== undefined
                ? defaultFields[key].placeholder
                : defaultFields[key].defaultValue;
            col.firstChild.firstChild.appendChild(
                elCreateEmpty('input', {"is": "mympd-input-reset", "id": id, "data-default": defaultFields[key].defaultValue,
                    "placeholder": placeholder, "value": "", "class": cssClass, "type": defaultFields[key].inputType})
            );
        }
        // unit
        if (defaultFields[key].unit !== undefined) {
            col.firstChild.appendChild(
                elCreateTextTn('span', {"class": ["input-group-text-nobg"]}, defaultFields[key].unit)
            );
        }
        // invalid feedback element for local validation
        if (defaultFields[key].invalid !== undefined) {
            col.firstChild.appendChild(
                elCreateTextTn('div', {"class": ["invalid-feedback"]}, defaultFields[key].invalid)
            );
        }
        // warning element
        if (defaultFields[key].warn !== undefined) {
            col.appendChild(
                elCreateTextTn('div', {"id": id + 'Warn', "class": ["mt-2", "mb-1", "alert", "alert-warning", "d-none"]}, defaultFields[key].warn)
            );
        }
        // help text
        if (defaultFields[key].help !== undefined) {
            col.appendChild(
                elCreateTextTn('small', {"class": ["help"]}, defaultFields[key].help)
            );
        }
        // create the label
        const label = defaultFields[key].hintIcon === undefined
            ? elCreateTextTn('label', {"class": ["col-sm-4", "col-form-label"], "for": id}, defaultFields[key].title)
            : elCreateNodes('label', {"class": ["col-sm-4", "col-form-label"], "for": id}, [
                    elCreateTextTn('span', {}, defaultFields[key].title),
                    elCreateText('small', {"class": ["mi", "mi-sm", "ms-1"], "title": tn(defaultFields[key].hintText),
                        "data-title-phrase": defaultFields[key].hintText}, defaultFields[key].hintIcon)
              ]);
        // create the row and append it to the form field container
        let rowClasses = ["mb-3", "row"];
        if (defaultFields[key].cssClass !== undefined) {
            rowClasses = rowClasses.concat(defaultFields[key].cssClass);
        }
        forms[form].appendChild(
            elCreateNodes('div', {"class": rowClasses}, [
                label,
                col
            ])
        );
    }

    // add event handler
    for (const key in defaultFields) {
        if (defaultFields[key].onChange !== undefined) {
            const id = prefix + ucFirst(key) + 'Input';
            elGetById(id).addEventListener('change', function(event) {
                // @ts-ignore
                window[defaultFields[key].onChange](event);
            }, false);
        }
    }
}

/**
 * Populates the form with values
 * @param {object} settingsFields object with the values for the elements
 * @param {object} defaultFields object with elements to populate
 * @param {string} prefix prefix for element ids
 * @returns {void}
 */
function jsonToForm(settingsFields, defaultFields, prefix) {
    // iterate through keys
    const settingsKeys = Object.keys(defaultFields);
    for (let i = 0, j = settingsKeys.length; i < j; i++) {
        const key = settingsKeys[i];
        // check if we should add a field
        if (defaultFields[key].form === undefined ||
            defaultFields[key].inputType === 'none')
        {
            continue;
        }
        // calculate a camelCase id
        const id = prefix + ucFirst(key) + 'Input';
        const field = elGetById(id);
        if (field) {
            switch(defaultFields[key].inputType) {
                case 'checkbox':
                    toggleBtnChk(field, settingsFields[key]);
                    break;
                case 'color':
                case 'password':
                case 'select':
                case 'text':
                    field.value = settingsFields[key];
                    break;
                case 'mympd-select-search':
                    setData(field, 'value', settingsFields[key]);
                    field.value = tn(settingsFields[key]);
                    field.filterInput.value = '';
                    break;
                default:
                    logError('Unhandled field type ' + defaultFields[key].inputType + ' for id ' + id + ' not found.');
            }
        }
        else {
            logError('Form field with id ' + id + 'not found.');
        }
    }
}
