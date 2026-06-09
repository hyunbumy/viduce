// Downscale a camera track to exactly 240x240 using Insertable Streams.
//
// Each VideoFrame is letterboxed (scaled to fit, padded with black) onto a
// 240x240 OffscreenCanvas, then re-wrapped as a new VideoFrame preserving the
// original timestamp. The output is a MediaStreamTrack (via a generator) that
// can feed both the local preview and the MoQ encoder.

/** Target square resolution. 240 is a multiple of 16, so the encoder keeps it exactly. */
export const TARGET_SIZE = 240;

export interface ResizePipeline {
  /** The 240x240 output track. (A MediaStreamTrackGenerator is a MediaStreamTrack.) */
  readonly track: MediaStreamTrack;
  /** Tear down the pipeline and end the output track. */
  stop(): void;
}

/** The placement of a scaled source rectangle, centered inside the target square. */
export interface LetterboxRect {
  drawW: number;
  drawH: number;
  offsetX: number;
  offsetY: number;
}

/**
 * Compute the aspect-preserving, centered placement of a `srcW`x`srcH` frame
 * inside a `target`x`target` square (letterbox: scale to fit, pad the rest).
 * Pure arithmetic, unit-tested in resize.test.ts.
 */
export function computeLetterbox(srcW: number, srcH: number, target = TARGET_SIZE): LetterboxRect {
  const scale = Math.min(target / srcW, target / srcH);
  const drawW = Math.round(srcW * scale);
  const drawH = Math.round(srcH * scale);
  return {
    drawW,
    drawH,
    offsetX: (target - drawW) / 2,
    offsetY: (target - drawH) / 2,
  };
}

/** Build a 240x240 letterbox pipeline fed by `source`. Throws if the browser lacks the APIs. */
export function createLetterboxPipeline(source: MediaStreamTrack): ResizePipeline {
  if (
    typeof MediaStreamTrackProcessor === "undefined" ||
    typeof MediaStreamTrackGenerator === "undefined"
  ) {
    throw new Error(
      "This browser lacks Insertable Streams (MediaStreamTrackProcessor/Generator). " +
        "Use a recent Chromium-based browser.",
    );
  }

  const processor = new MediaStreamTrackProcessor({ track: source });
  const generator = new MediaStreamTrackGenerator({ kind: "video" });

  const canvas = new OffscreenCanvas(TARGET_SIZE, TARGET_SIZE);
  const ctx = canvas.getContext("2d", { alpha: false });
  if (!ctx) throw new Error("Could not create a 2D canvas context for resizing.");

  const transform = new TransformStream<VideoFrame, VideoFrame>({
    transform(frame, controller) {
      controller.enqueue(letterbox(frame, canvas, ctx));
    },
  });

  const abort = new AbortController();
  processor.readable
    .pipeThrough(transform, { signal: abort.signal })
    .pipeTo(generator.writable, { signal: abort.signal })
    .catch((err) => {
      // AbortError on stop() is expected; surface anything else.
      if (!abort.signal.aborted) {
        console.error("resize pipeline error:", err);
      }
    });

  return {
    track: generator,
    stop: () => abort.abort(),
  };
}

/** Draw one frame letterboxed into the 240x240 canvas and return a new VideoFrame. */
function letterbox(
  frame: VideoFrame,
  canvas: OffscreenCanvas,
  ctx: OffscreenCanvasRenderingContext2D,
): VideoFrame {
  const { drawW, drawH, offsetX, offsetY } = computeLetterbox(
    frame.displayWidth,
    frame.displayHeight,
  );

  ctx.fillStyle = "black";
  ctx.fillRect(0, 0, TARGET_SIZE, TARGET_SIZE);
  ctx.drawImage(frame, offsetX, offsetY, drawW, drawH);

  const out = new VideoFrame(canvas, {
    timestamp: frame.timestamp,
    duration: frame.duration ?? undefined,
  });

  // Critical: release the source frame or the decoder pool starves and video stalls.
  frame.close();
  return out;
}
