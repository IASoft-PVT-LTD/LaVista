# liblavista (JavaScript)

Your SPA must include the **liblavista** package to handshake with the LaVista C++ host and receive binary data from `post_binary_data`.

## Installation

From your frontend project:

```bash
npm install path/to/LaVista/JavaScript/liblavista
```

The hello-lavista example in `spa-template/` depends on the local package at `JavaScript/liblavista`.

## Usage

Import the library once at application startup:

```javascript
import { onBinaryData } from "liblavista";

// Handshake runs automatically when the module loads.

onBinaryData((buffer) => {
  // buffer is an ArrayBuffer from the C++ host
});
```

`onBinaryData` registers a callback and returns an unsubscribe function:

```javascript
const unsubscribe = onBinaryData((buffer) => { /* ... */ });
unsubscribe();
```

## Host handshake

On load, liblavista calls `window.LaVista_Handshake()` if the C++ host injected it. A successful handshake confirms the SPA is running inside LaVista. If the function is missing, a console warning is emitted (typical when developing in a normal browser).

## Binary data delivery

### Windows (WebView2)

The host posts buffers via WebView2 shared memory. liblavista listens for `sharedbufferreceived` on `window.chrome.webview`, invokes all registered listeners, then calls `releaseBuffer`.

Call from C++: [`post_binary_data`](lavista/namespace_la_vista.md#function-post_binary_data).

### Linux (WebKitGTK)

The host stores buffers and dispatches a `lavista-bin-ready` custom event with a buffer id. liblavista fetches the data from `lavista-bin://<id>` and passes the resulting `ArrayBuffer` to listeners.

## Host-injected globals

The C++ host may expose these on `window` for title bar and window chrome:

| Global | Purpose |
|--------|---------|
| `LaVista_Handshake` | Async handshake invoked on SPA load |
| `LaVista_Menu` | Default title bar menu button |
| `LaVista_Minimize` | Minimize window |
| `LaVista_Maximize` | Toggle maximize |
| `LaVista_QueryMaximized` | Query maximized state |
| `LaVista_Close` | Close window |

These are wired by LaVista when using the default or custom title bar; your SPA normally does not call them directly unless building custom chrome.

## Related C++ API

- [`bind_window_function`](lavista/namespace_la_vista.md) — expose C++ handlers as `window.<name>(...)` returning JSON
- [`bind_window_event`](lavista/namespace_la_vista.md) — string-keyed event callbacks from the SPA
- [`dispatch_window_event`](lavista/namespace_la_vista.md) — push `CustomEvent` instances into the SPA

See the [C++ API Reference](lavista/links.md) for the full host API.
