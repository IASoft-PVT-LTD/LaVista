// LaVista Client Library

export type BinaryDataCallback = (buffer: ArrayBuffer) => void;

const binaryDataListeners: Set<BinaryDataCallback> = new Set();

/**
 * Registers a callback to be invoked when binary data is received from the host.
 * @param callback The function to call with the ArrayBuffer
 * @returns A function to unregister the callback
 */
export function onBinaryData(callback: BinaryDataCallback): () => void {
  binaryDataListeners.add(callback);
  return () => {
    binaryDataListeners.delete(callback);
  };
}

declare global {
  interface Window {
    LaVista_Handshake?: () => Promise<void>;
    chrome?: {
      webview?: {
        addEventListener: (event: string, callback: (e: any) => void) => void;
        releaseBuffer: (buffer: ArrayBuffer) => void;
      };
    };
    LaVista_Menu?: () => void;
    LaVista_Minimize?: () => void;
    LaVista_Maximize?: () => void;
    LaVista_QueryMaximized?: () => Promise<boolean> | boolean;
    LaVista_Close?: () => void;
  }
}

// Internal initialization
let initialized = false;

async function init() {
  if (initialized) return;
  initialized = true;

  // Handshake with the LaVista C++ host
  if (typeof window.LaVista_Handshake === 'function') {
    try {
      await window.LaVista_Handshake();
      console.log('LaVista: Handshake successful');
    } catch (err) {
      console.error('LaVista: Handshake failed', err);
    }
  } else {
    console.warn('LaVista: Handshake function not found. Are you running inside LaVista host?');
  }

  // Windows: WebView2 shared buffer
  if (typeof window.chrome !== 'undefined' && window.chrome.webview) {
    window.chrome.webview.addEventListener('sharedbufferreceived', (e: any) => {
      const buffer = e.getBuffer();
      for (const listener of binaryDataListeners) {
        try {
          listener(buffer);
        } catch (err) {
          console.error('LaVista: Error in binary data listener', err);
        }
      }
      window.chrome?.webview?.releaseBuffer(buffer);
    });
  }

  // Linux: WebKitGTK custom URI scheme
  window.addEventListener('lavista-bin-ready', async (e: Event | CustomEvent) => {
    const id = (e as CustomEvent).detail;
    if (id) {
      try {
        const response = await fetch(`lavista-bin://${id}`);
        if (response.ok) {
          const buffer = await response.arrayBuffer();
          for (const listener of binaryDataListeners) {
            listener(buffer);
          }
        } else {
          console.error(`LaVista: Failed to fetch binary data for id ${id}: ${response.statusText}`);
        }
      } catch (err) {
        console.error(`LaVista: Error fetching binary data for id ${id}`, err);
      }
    }
  });
}

// Auto-initialize when the module is loaded
if (typeof window !== 'undefined') {
  init();
}
