import lldb

CL_GRAY         = "\u00feC"
CL_CLEAN        = "\u00feE"
CL_WHITE_GRAY_I = "\u00feK"
CL_WHITE        = "\u00feV"
CL_YELLOW_S     = "\u00feS"
CL_NUMBER       = "\u00feN"



def Style(message, color=CL_WHITE):
    return f"{color}{message}{CL_CLEAN}"


def _raw(value):
    if value is None:
        return None
    try:
        return value.GetNonSyntheticValue()
    except Exception:
        return value


def _eval_unsigned(value, expression):
    try:
        result = value.EvaluateExpression(expression)
        if result.IsValid() and result.GetError().Success():
            return result.GetValueAsUnsigned()
    except Exception:
        pass
    return None


def _child_unsigned(value, index):
    try:
        child = value.GetChildAtIndex(index)
        if child.IsValid():
            return child.GetValueAsUnsigned()
    except Exception:
        pass
    return None


def _stream_is_char(value):
    try:
        type_name = value.GetTypeName() or ""
        return type_name.endswith("RecvStream<char>")
    except Exception:
        return False


def _escape_char(number):
    if number is None:
        return "?"
    if number == 0:
        return "\\0"
    if number == 9:
        return "\\t"
    if number == 10:
        return "\\n"
    if number == 13:
        return "\\r"
    if 32 <= number < 127 and chr(number) not in ('\\', '"'):
        return chr(number)
    if number == 92:
        return "\\\\"
    if number == 34:
        return '\\"'
    return "\\x%02x" % number


def RecvStreamSummary(value, _dict):
    streamLength = 32
    output = ""

    value = _raw(value)
    state = (value
             .GetChildMemberWithName("m_policy").Dereference()
             .GetChildMemberWithName("m_streamState")
             .GetChildMemberWithName("[ptr]"))

    if not state.IsValid():
        return "Invalid state"

    hasVal = value.EvaluateExpression("m_policy->m_streamState->status.has_value()")
    if hasVal.IsValid() and hasVal.GetValueAsUnsigned(1) == 0:
        errVal = value.EvaluateExpression("m_policy->m_streamState->status.error()").GetValue()
        output += f"[{Style(errVal, CL_GRAY)}] "

    idx = state.GetChildMemberWithName("index").GetValueAsUnsigned(0)
    size = state.GetChildMemberWithName("size").GetValueAsUnsigned(0)

    endIdx = min(idx + streamLength, size)
    bytesToRead = endIdx - idx

    if bytesToRead <= 0:
        output += "(empty)"
        return output

    buffer = state.GetChildMemberWithName("buffer")
    process = value.GetTarget().GetProcess()
    readErr = lldb.SBError()
    addr = buffer.GetLoadAddress() + idx
    rawBytes = process.ReadMemory(addr, bytesToRead, readErr)

    if readErr.Fail():
        output += f"(Memory read error: {readErr.GetCString()})"
        return output

    if _stream_is_char(value):
        text = "".join(_escape_char(b) for b in rawBytes)
        output += Style(text, CL_YELLOW_S)
    else:
        numbers = [f"0x{b:02x}" for b in rawBytes]
        output += f"{{ {', '.join(numbers)}"

    if idx + streamLength < size:
        output += "..."

    if not _stream_is_char(value):
        output += " }"
    else:
        output += f" ({idx} of {size})"

    return output


def _variant_index(ip):
    # Do not use the synthetic child named "index". In CLion that child is
    # supplied by the declarative formatter and can be unavailable to Python.
    # Ask the C++ object for the actual variant index instead.
    index = _eval_unsigned(ip, "m_data.index()")
    if index in (0, 1):
        return index

    # Fallback for LLDBs whose expression parser cannot call variant::index().
    data = ip.GetChildMemberWithName("m_data")
    child = data.GetChildMemberWithName("index") if data.IsValid() else None
    if child is not None and child.IsValid():
        index = child.GetValueAsUnsigned()
        if index in (0, 1):
            return index
    return None


def _variant_byte(ip, alternative, index):
    # std::get avoids relying on implementation-specific std::variant child
    # names such as [value], _M_u or __data.
    byte = _eval_unsigned(ip, f"std::get<{alternative}>(m_data)[{index}]")
    if byte is not None:
        return byte & 0xff

    # Fallback for the CLion synthetic tree shown in the debugger:
    data = ip.GetChildMemberWithName("m_data")
    active = data.GetChildMemberWithName("[value]") if data.IsValid() else None
    if active is None or not active.IsValid():
        active = data.GetChildMemberWithName("value") if data.IsValid() else None
    if active is not None and active.IsValid():
        byte = _child_unsigned(active, index)
        if byte is not None:
            return byte & 0xff
    return None


def IpAddressSummary(value, _dict):
    if value is None or not value.IsValid():
        return "Invalid IpAddress"
    value = value.GetChildMemberWithName("m_data")

    index = value.GetChildMemberWithName("index").GetValueAsUnsigned()
    codes = [el.GetValueAsUnsigned() for el in value.GetChildAtIndex(1).children[:-1]]

    if index == 0:
        return Style(".".join(map(str, codes)), CL_NUMBER)

    elif index == 1:
        parts = [(codes[i] << 8) | codes[i+1] for i in range(0, 16, 2)]

        best_start, best_len, curr_start, curr_len = -1, 0, -1, 0
        for i, p in enumerate(parts):
            if p == 0:
                if curr_len == 0: curr_start = i
                curr_len += 1
                if curr_len > best_len:
                    best_len = curr_len
                    best_start = curr_start
            else:
                curr_len = 0

        if best_len > 1:
            parts[best_start:best_start+best_len] = ['']
            if best_start == 0: parts.insert(0, '')
            if best_start + best_len == 8: parts.append('')

        return Style(f"[{':'.join(f'{p:x}' if p != '' else '' for p in parts)}]", CL_NUMBER)

    return "Unknown variant index"

def IpEndpointSummary(value, _dict):
    endpoint = _raw(value)
    ip = endpoint.GetChildMemberWithName("m_ip")
    port = endpoint.GetChildMemberWithName("m_port")
    if not ip.IsValid() or not port.IsValid():
        return "Invalid IpEndpoint"
    return f"{ip.GetSummary()}:{Style(port.GetValueAsUnsigned(), CL_NUMBER)}"

def __lldb_init_module(debugger, _dict):
    recv_stream_names = "^Hermes::(DefaultTransferPolicy|TlsTransferPolicy)<.*>::RecvStream<.*>$"

    debugger.HandleCommand('type category delete Hermes')

    debugger.HandleCommand(f'type summary add -w Hermes -F HermesLldb.RecvStreamSummary -x "{recv_stream_names}"')
    debugger.HandleCommand('type summary add -w Hermes -F HermesLldb.IpAddressSummary -x "^Hermes::IpAddress$"')
    debugger.HandleCommand('type summary add -w Hermes -F HermesLldb.IpEndpointSummary -x "^Hermes::IpEndpoint$"')

    debugger.HandleCommand('type category enable Hermes')