// App entry: wires the Start/Stop controls to webcam capture -> 240x240 resize
// -> local preview. The MoQ publish step (task 5) attaches to the same resized track.
import { startCamera, stopStream } from "./camera";
import { createLetterboxPipeline, type ResizePipeline, TARGET_SIZE } from "./resize";

const preview = requireEl<HTMLVideoElement>("#preview");
const startBtn = requireEl<HTMLButtonElement>("#start");
const stopBtn = requireEl<HTMLButtonElement>("#stop");
const statusEl = requireEl<HTMLDivElement>("#status");

let stream: MediaStream | null = null;
let pipeline: ResizePipeline | null = null;

startBtn.addEventListener("click", start);
stopBtn.addEventListener("click", stop);

async function start(): Promise<void> {
  startBtn.disabled = true;
  setStatus("Requesting camera…");
  try {
    stream = await startCamera();

    const cameraTrack = stream.getVideoTracks()[0];
    if (!cameraTrack) throw new Error("Camera stream has no video track.");

    pipeline = createLetterboxPipeline(cameraTrack);

    // Preview the *resized* track so we see exactly what will be published.
    preview.srcObject = new MediaStream([pipeline.track]);
    await preview.play();

    setStatus(`Camera live · downscaled to ${TARGET_SIZE}×${TARGET_SIZE}`);
    stopBtn.disabled = false;
  } catch (err) {
    cleanup();
    setStatus(err instanceof Error ? err.message : String(err), true);
    startBtn.disabled = false;
  }
}

function stop(): void {
  cleanup();
  setStatus("Stopped");
  startBtn.disabled = false;
  stopBtn.disabled = true;
}

/** Tear down the pipeline and camera, releasing the device. Safe to call repeatedly. */
function cleanup(): void {
  pipeline?.stop();
  pipeline = null;
  if (stream) {
    stopStream(stream);
    stream = null;
  }
  preview.srcObject = null;
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
