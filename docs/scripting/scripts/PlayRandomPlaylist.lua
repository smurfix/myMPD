-- {"order":1,"arguments":[]}
-- get the first 2000 playlists
rc, raw_result = mympd_api_raw("MYMPD_API_PLAYLIST_LIST", json.encode({offset = 0, limit = 2000, searchstr = "", type = 0}))
result = json.decode(raw_result)
-- random number
math.randomseed(os.time())
number = math.random(1, #result.result.data)
-- get playlist by random number
playlist = result.result.data[number].uri
-- play the playlist
rc, raw_result = mympd_api_raw("MYMPD_API_QUEUE_REPLACE_PLAYLIST", json.encode({plist = playlist, play = true}))
-- return the playlist name
return "Playing" .. playlist
