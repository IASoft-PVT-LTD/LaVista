// LaVista Client Library
const binaryDataListeners = new Set();
/**
 * Registers a callback to be invoked when binary data is received from the host.
 * @param callback The function to call with the ArrayBuffer
 * @returns A function to unregister the callback
 */
export function onBinaryData(callback) {
    binaryDataListeners.add(callback);
    return () => {
        binaryDataListeners.delete(callback);
    };
}
// Internal initialization
let initialized = false;
function init() {
    if (initialized)
        return;
    initialized = true;
    // Windows: WebView2 shared buffer
    if (typeof window.chrome !== 'undefined' && window.chrome.webview) {
        window.chrome.webview.addEventListener('sharedbufferreceived', (e) => {
            const buffer = e.getBuffer();
            for (const listener of binaryDataListeners) {
                listener(buffer);
            }
        });
    }
    // Linux: WebKitGTK custom URI scheme
    window.addEventListener('lavista-bin-ready', async (e) => {
        const id = e.detail;
        if (id) {
            try {
                const response = await fetch(`lavista-bin://${id}`);
                if (response.ok) {
                    const buffer = await response.arrayBuffer();
                    for (const listener of binaryDataListeners) {
                        listener(buffer);
                    }
                }
                else {
                    console.error(`LaVista: Failed to fetch binary data for id ${id}: ${response.statusText}`);
                }
            }
            catch (err) {
                console.error(`LaVista: Error fetching binary data for id ${id}`, err);
            }
        }
    });
}
// Auto-initialize when the module is loaded
if (typeof window !== 'undefined') {
    init();
}
