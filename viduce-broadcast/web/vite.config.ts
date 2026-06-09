import { defineConfig } from "vite";

// MoQ uses WebTransport + WebCodecs, which require a secure context.
// The host browser connects via http://localhost:5173 (localhost counts as
// secure); `host: true` binds 0.0.0.0 so the port is reachable from the
// container, and `strictPort` fails fast if 5173 is taken.
export default defineConfig({
  server: {
    host: true,
    port: 5173,
    strictPort: true,
  },
});
