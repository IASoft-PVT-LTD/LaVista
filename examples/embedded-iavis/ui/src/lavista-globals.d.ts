export {};

declare global {
	interface Window {
		/** Injected by LaVista after the SPA loads in the desktop shell. */
		LaVista_Minimize?: () => void;
		LaVista_Maximize?: () => void;
		LaVista_Close?: () => void;
	}
}
