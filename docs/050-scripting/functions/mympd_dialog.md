---
title: Accessing the myMPD API
---

Returns an Jsonrpc response for a script dialog.

```lua
local title = "Script title"
local data = {
  {
    name = "testinput",
    type = "text",
    value = "testvalue"
  },
  {
    name = "testpassword",
    type = "password",
    value = ""
  },
  {
    name = "action",
    type = "hidden",
    value = "test"
  },
  {
    name = "testcheckbox",
    type = "checkbox",
    value = false
  },
  {
    name = "testradio",
    type = "radio",
    value = {
      "radio1",
      "radio2"
    },
    displayValue = {
      "Radio 1",
      "Radio 2"
    },
    defaultValue = "radio2"
  },
  {
    name = "testselect",
    type = "select",
    value = {
      "option1",
      "option2"
    },
    ,
    displayValue = {
      "Option1 1",
      "Option 2"
    },
    defaultValue = "option1"
  },
  {
    name = "testlist",
    type = "list",
    value = {
      "val1",
      "val2"
    },
    displayValue = {
      {
        title = "Title 1",
        text = "Text 1",
        small = "Hint 1"
      },
      {
        title = "Title 2",
        text = "Text 2",
        small = "Hint 2"
      }
    }
  }
}
local callback = "testscript"
return mympd.dialog(title, data, callback)
```

**Parameters:**

| PARAMETER | TYPE | DESCRIPTION |
| --------- | ---- | ----------- |
| title | string | Dialog title |
| data | table | The dialog definition. |
| callback | string | Script to call for the submit button |

**Returns:**

A Jsonrpc string with method `script_dialog`.

## Dialog definition

| KEY | DESCRIPTION |
| --- | ----------- |
| name | Name of the form element. |
| type | Type of the form element. |
| value | The value(s) of the form element. |
| displayValue | The display values of the form element, it is optional and only valid for `select`, `radio` and `list`. |
| defaultValue | The defaultValue of the form element. |

| TYPE | DESCRIPTION |
| ---- | ----------- |
| text | Text input field. |
| password | Password input field. |
| checkbox | Checkbox, value is true if checked, else false. |
| select | Selectbox with multiple options. |
| radio | Radios |
| list | List of elements to select from. Selected items are separated by `;;` |
| hidden | Hidden input field. |

These are the same types as for script arguments.
