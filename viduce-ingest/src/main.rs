#[tokio::main]
async fn main() -> anyhow::Result<()> {
    moq_native::Log::new(tracing::Level::DEBUG).init()?;

	let origin_producer = moq_net::Origin::random().produce();
    let client = moq_native::ClientConfig::default().init()?;

	let url = url::Url::parse("http://relay:4443").unwrap();

	// Establish a WebTransport/QUIC connection and MoQ handshake.
    // "I'm going to consume with this origin's producer"
	let session = client.with_consume(origin_producer.clone()).connect(url).await?;

    // Wait for announcement
    // "Consume this origin's produced broadcast"
    let (path, broadcast) = origin_producer.consume().announced().await.ok_or_else(|| anyhow::anyhow!("origin closed"))?;
	let _ = broadcast.ok_or_else(|| anyhow::anyhow!("broadcast unannounced: {path}"))?;
	tracing::info!(%path, "broadcast announced");

	// // Read the catalog to discover available tracks.
	// let catalog_track = broadcast.subscribe_track()?;
	// let mut catalog: moq_mux::catalog::hang::Consumer = moq_mux::catalog::hang::Consumer::new(catalog_track);

	// let info = catalog.next().await?.ok_or_else(|| anyhow::anyhow!("no catalog"))?;

	// // Find the first video track.
	// let (name, config) = info
	// 	.video
	// 	.renditions
	// 	.iter()
	// 	.next()
	// 	.ok_or_else(|| anyhow::anyhow!("no video renditions"))?;

	// tracing::info!(
	// 	%name,
	// 	codec = %config.codec,
	// 	width = ?config.coded_width,
	// 	height = ?config.coded_height,
	// 	"subscribing to video track"
	// );

	// // Subscribe to the video track.
	// let track = moq_net::Track {
	// 	name: name.clone(),
	// 	priority: 1,
	// };

	// let track_consumer = broadcast.subscribe_track(&track)?;
	// let mut ordered = moq_mux::container::Consumer::new(track_consumer, moq_mux::catalog::hang::Container::Legacy)
	// 	.with_latency(Duration::from_millis(500));

	// // Read frames in latency-bounded presentation order.
	// while let Some(frame) = ordered.read().await? {
	// 	tracing::info!(
	// 		timestamp = ?frame.timestamp,
	// 		keyframe = frame.keyframe,
	// 		bytes = frame.payload.len(),
	// 		"received frame"
	// 	);
	// }

	// Wait until the session is closed.
	session.closed().await.map_err(Into::<anyhow::Error>::into)?;

    Ok(())
}
