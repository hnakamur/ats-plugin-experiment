local NORMALIZE_AE = 0
function __init__(argtb)
    if (#argtb) < 1 then
        ts.fatal(argtb[0] .. ' normalize_ae parameter required!!')
        return -1
    end
    NORMALIZE_AE = tonumber(argtb[1])
    ts.note(string.format('accept-encoding, NORMALIZE_AE=%s', NORMALIZE_AE))
end

function do_remap()
    local before = ts.client_request.header['Accept-Encoding']
    ts.client_request.normalize_accept_encoding(NORMALIZE_AE)
    local after = ts.client_request.header['Accept-Encoding']
    ts.debug(string.format('accept-encoding, NORMALIZE_AE=%s, before=%s, after=%s', NORMALIZE_AE, before, after))
    return 0
end
