// App entry. Two independent lifecycles:
//   - Preview: starts automatically on load and stays on (local only).
//   - Broadcast: the Start button publishes the 240x240 feed to a MoQ relay;
//     Stop ends publishing but keeps the preview running.
// A badge + red border make "broadcasting" vs "preview only" unmistakable.
import { startCamera, stopStream } from "./camera";
import { createLetterboxPipeline, type ResizePipeline, TARGET_SIZE } from "./resize";
import { startPublishing, type Publisher } from "./publisher";

const preview = requireEl<HTMLVideoElement>("#preview");
const badge = requireEl<HTMLSpanElement>("#live-badge");
const startBtn = requireEl<HTMLButtonElement>("#start");
const stopBtn = requireEl<HTMLButtonElement>("#stop");
const statusEl = requireEl<HTMLDivElement>("#status");
const relayUrlInput = requireEl<HTMLInputElement>("#relay-url");
const nameInput = requireEl<HTMLInputElement>("#broadcast-name");

let stream: MediaStream | null = null;
let pipeline: ResizePipeline | null = null;
let publisher: Publisher | null = null;

startBtn.addEventListener("click", onStart);
stopBtn.addEventListener("click", () => stopBroadcast());
// Release the camera if the page goes away.
window.addEventListener("pagehide", teardownPreview);

// Show the preview feed as soon as the page loads.
void enablePreview();

/** Start button: begin broadcasting if previewing, or (re)start the preview after a failure. */
function onStart(): void {
  if (stream) void startBroadcast();
  else void enablePreview();
}

/** Open the camera and show the 240x240 preview. Stays running until the page unloads. */
async function enablePreview(): Promise<void> {
  startBtn.disabled = true;
  setStatus("Starting camera…");
  try {
    stream = await startCamera();
    const cameraTrack = stream.getVideoTracks()[0];
    if (!cameraTrack) throw new Error("Camera stream has no video track.");

    pipeline = createLetterboxPipeline(cameraTrack);
    preview.srcObject = new MediaStream([pipeline.track]);
    await preview.play();

    showPreviewing();
  } catch (err) {
    teardownPreview();
    setBadge(false);
    setStatus(err instanceof Error ? err.message : String(err), true);
    startBtn.textContent = "Enable camera";
    startBtn.disabled = false;
    stopBtn.disabled = true;
    setInputsDisabled(false);
  }
}

/** Publish the resized preview track to the relay. */
async function startBroadcast(): Promise<void> {
  if (!pipeline) return;
  startBtn.disabled = true;
  setInputsDisabled(true);
  const name = nameInput.value.trim();
  setStatus("Connecting to relay…");
  try {
    publisher = await startPublishing({
      relayUrl: relayUrlInput.value.trim(),
      name,
      track: pipeline.track,
    });
    showBroadcasting(name);

    // Reflect an unexpected connection drop in the UI.
    const current = publisher;
    const onClosed = () => {
      if (publisher === current) stopBroadcast("Relay connection closed.", true);
    };
    current.closed.then(onClosed, onClosed);
  } catch (err) {
    stopBroadcast(err instanceof Error ? err.message : String(err), true);
  }
}

/** Stop publishing and return to preview-only. The camera/preview keeps running. */
function stopBroadcast(message = "Previewing — not broadcasting", isError = false): void {
  publisher?.stop();
  publisher = null;
  showPreviewing(message, isError);
}

/** Apply the preview-only UI state (camera live, not broadcasting). */
function showPreviewing(message = "Previewing — not broadcasting", isError = false): void {
  setBadge(false);
  startBtn.textContent = "Start broadcast";
  startBtn.disabled = false;
  stopBtn.disabled = true;
  setInputsDisabled(false);
  setStatus(message, isError);
}

/** Apply the broadcasting UI state. */
function showBroadcasting(name: string): void {
  setBadge(true);
  startBtn.disabled = true;
  stopBtn.disabled = false;
  setInputsDisabled(true);
  setStatus(`Broadcasting · ${TARGET_SIZE}×${TARGET_SIZE} → ${name}`);
}

/** Stop publisher, pipeline, and camera, releasing the device. Idempotent. */
function teardownPreview(): void {
  publisher?.stop();
  publisher = null;
  pipeline?.stop();
  pipeline = null;
  if (stream) {
    stopStream(stream);
    stream = null;
  }
  preview.srcObject = null;
}

function setBadge(live: boolean): void {
  badge.textContent = live ? "● LIVE" : "Preview";
  badge.classList.toggle("live", live);
  preview.classList.toggle("live", live);
}

function setInputsDisabled(disabled: boolean): void {
  relayUrlInput.disabled = disabled;
  nameInput.disabled = disabled;
}

function setStatus(message: string, isError = false): void {
  statusEl.textContent = message;
  statusEl.classList.toggle("error", isError);
}

/** Query a required element, failing loudly if the markup and code drift apart. */
function requireEl<T extends Element>(selector: string): T {
  const el = document.querySelector<T>(selector);
  if (!el) throw new Error(`Missing required element: ${selector}`);
  return el;
}
