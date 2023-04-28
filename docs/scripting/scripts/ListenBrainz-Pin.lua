-- {"order":1,"arguments":["uri","blurb_content","pinned_until"]}
mympd.init()
pin_uri = "https://api.listenbrainz.org/1/pin"
unpin_uri = "https://api.listenbrainz.org/1/pin/unpin"
headers = "Content-type: application/json\r\n"..
  "Authorization: Token "..mympd_state["listenbrainz_token"].."\r\n"
payload = ""
uri = ""

if arguments["uri"] ~= "" then
  rc, song = mympd.api("MYMPD_API_SONG_DETAILS", {uri = arguments["uri"]})
  if rc == 0 then
    mbid = song["MUSICBRAINZ_TRACKID"]
    if mbid ~= nil then
      payload = json.encode({
        recording_mbid = mbid,
        blurb_content = arguments["blurb_content"],
        pinned_until = arguments["pinned_until"]
      });
      uri = pin_uri
    end
  end
else
  uri = unpin_uri
end

if uri ~= "" then
  rc, code, header, body = mympd.http_client("POST", uri, headers, payload)
  if rc > 0 then
    return body
  end
end
