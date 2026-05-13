export type BinaryDataCallback = (buffer: ArrayBuffer) => void;
/**
 * Registers a callback to be invoked when binary data is received from the host.
 * @param callback The function to call with the ArrayBuffer
 * @returns A function to unregister the callback
 */
export declare function onBinaryData(callback: BinaryDataCallback): () => void;
