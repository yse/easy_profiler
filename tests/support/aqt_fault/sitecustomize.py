"""Fault injector for the resolver tests. Put this directory on PYTHONPATH and
set QT_FAULT=SSLError|ReadTimeout and QT_FAULT_URL=<substring>: every HTTP
request whose URL contains the substring fails with that requests exception,
inside the real aqtinstall, on the real code path. Everything else goes out
to the network untouched."""
import os

try:
    import requests.adapters
    import requests.exceptions
except ImportError:
    requests = None

if requests is not None and os.environ.get("QT_FAULT"):
    _fault = getattr(requests.exceptions, os.environ["QT_FAULT"])
    _needle = os.environ.get("QT_FAULT_URL", "")
    _send = requests.adapters.HTTPAdapter.send

    def _faulty_send(self, request, *args, **kwargs):
        if _needle and _needle in request.url:
            raise _fault(f"injected {os.environ['QT_FAULT']} for {request.url}",
                         request=request)
        return _send(self, request, *args, **kwargs)

    requests.adapters.HTTPAdapter.send = _faulty_send
