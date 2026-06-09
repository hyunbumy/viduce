// Webcam capture. Kept deliberately thin: it opens a video-only MediaStream and
// reports failures as readable messages. Downscaling to 240x240 happens later in
// the resize pipeline (it consumes the track this returns).

/** Open the webcam as a video-only stream. Throws a human-readable Error on failure. */
export async function startCamera(): Promise<MediaStream> {
  if (!navigator.mediaDevices?.getUserMedia) {
    throw new Error("This browser does not support camera capture (getUserMedia).");
  }

  try {
    return await navigator.mediaDevices.getUserMedia({ video: true, audio: false });
  } catch (err) {
    throw new Error(describeCameraError(err));
  }
}

/** Stop every track so the OS releases the camera (clearing srcObject alone does not). */
export function stopStream(stream: MediaStream): void {
  for (const track of stream.getTracks()) {
    track.stop();
  }
}

/** Map the DOMException names getUserMedia can throw to actionable messages. */
function describeCameraError(err: unknown): string {
  if (err instanceof DOMException) {
    switch (err.name) {
      case "NotAllowedError":
        return "Camera permission was denied. Allow camera access and try again.";
      case "NotFoundError":
        return "No camera was found on this device.";
      case "NotReadableError":
        return "The camera is already in use by another application.";
    }
  }
  return `Could not start the camera: ${err instanceof Error ? err.message : String(err)}`;
}
