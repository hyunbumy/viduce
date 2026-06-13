// Publish the 240x240 track to a MoQ relay via @moq/publish.
//
// For a LOCAL relay with a self-signed cert (viduce-relay), connect with an
// `http://` URL: @moq/net then fetches /certificate.sha256 and pins it via
// WebTransport `serverCertificateHashes`, upgrading to https:// internally.
// Passing `https://` directly skips that, and the browser rejects the cert.
import * as Moq from "@moq/net";
import * as Publish from "@moq/publish";
import { TARGET_SIZE } from "./resize";

export interface PublishOptions {
  /** Relay URL. For the local self-signed relay use `http://localhost:4443`. */
  relayUrl: string;
  /** Broadcast path on the relay, e.g. "viduce/cam". */
  name: string;
  /** The 240x240 video track to publish. */
  track: MediaStreamTrack;
}

export interface Publisher {
  /** Resolves when the relay connection closes (cleanly or on drop). */
  readonly closed: Promise<void>;
  /** Close the broadcast and the relay connection. */
  stop(): void;
}

/** Connect to the relay and publish `track` as a single 240x240 (`sd`) rendition. */
export async function startPublishing(options: PublishOptions): Promise<Publisher> {
  const connection = await Moq.Connection.connect(new URL(options.relayUrl));

  const broadcast = new Publish.Broadcast({
    connection,
    enabled: true,
    name: Moq.Path.from(options.name),
    audio: { enabled: false },
    video: {
      // A MediaStreamTrackGenerator is a MediaStreamTrack; the publish lib types
      // the source as StreamTrack (narrower getSettings), hence the cast.
      source: options.track as Publish.Video.Source,
      // Publish a single rendition at the source resolution (240x240).
      hd: { enabled: false },
      sd: { enabled: true, config: { maxPixels: TARGET_SIZE * TARGET_SIZE } },
    },
  });

  return {
    closed: connection.closed,
    stop() {
      broadcast.close();
      connection.close();
    },
  };
}
