--
-- mympd.lua
--
-- SPDX-License-Identifier: GPL-3.0-or-later
-- myMPD (c) 2018-2021 Juergen Mang <mail@jcgames.de>
-- https://github.com/jcorporation/mympd
--

mympd = { _version = "0.2.1" }

--
-- Function to retrieve mympd_state
--
function mympd.init()
  return mympd_api("INTERNAL_API_SCRIPT_INIT")
end

--
-- Function to retrieve console output
--
function mympd.os_capture(cmd)
  if io then
    local handle = assert(io.popen(cmd, 'r'))
    local output = assert(handle:read('*a'))
    handle:close()

    output = string.gsub(
      string.gsub(
        string.gsub(output, '^%s+', ''),
          '%s+$',
          ''
      ),
      '[\n\r]+',
      ' '
    )
    return output
  else
    return "io library must be loaded"
  end
end

return mympd
