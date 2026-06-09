import { describe, expect, it } from "vitest";
import { computeLetterbox, TARGET_SIZE } from "./resize";

describe("computeLetterbox", () => {
  it("leaves a square source untouched (fills the target exactly)", () => {
    expect(computeLetterbox(480, 480)).toEqual({
      drawW: 240,
      drawH: 240,
      offsetX: 0,
      offsetY: 0,
    });
  });

  it("letterboxes a landscape source with vertical padding, preserving aspect", () => {
    // 1280x720 (16:9) -> width-limited: scale 240/1280, height 135, padded top/bottom.
    const rect = computeLetterbox(1280, 720);
    expect(rect.drawW).toBe(240);
    expect(rect.drawH).toBe(135);
    expect(rect.offsetX).toBe(0);
    expect(rect.offsetY).toBeCloseTo((240 - 135) / 2);
  });

  it("pillarboxes a portrait source with horizontal padding", () => {
    // 480x640 (3:4) -> height-limited: scale 240/640, width 180, padded left/right.
    const rect = computeLetterbox(480, 640);
    expect(rect.drawH).toBe(240);
    expect(rect.drawW).toBe(180);
    expect(rect.offsetY).toBe(0);
    expect(rect.offsetX).toBeCloseTo((240 - 180) / 2);
  });

  it("never exceeds the target square on either axis", () => {
    for (const [w, h] of [
      [1920, 1080],
      [640, 480],
      [240, 800],
      [1000, 1000],
    ]) {
      const { drawW, drawH } = computeLetterbox(w, h);
      expect(drawW).toBeLessThanOrEqual(TARGET_SIZE);
      expect(drawH).toBeLessThanOrEqual(TARGET_SIZE);
    }
  });

  it("respects a custom target size", () => {
    expect(computeLetterbox(100, 50, 100)).toEqual({
      drawW: 100,
      drawH: 50,
      offsetX: 0,
      offsetY: 25,
    });
  });
});
