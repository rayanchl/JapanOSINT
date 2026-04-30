// Scheduler-owned runner that refreshes gtfs_rt_feeds from the gtfs-data.jp
// catalogue, then (re)starts the GTFS-RT poller. Shape mirrors
// runCameraDiscovery / runTransportDiscovery so it plugs straight into
// withCollectorRun in scheduler.js.

import { seedGtfsRtFeedsFromGtfsDataJp } from './gtfsStore.js';
import { startRtPoller } from './gtfsRtPoller.js';

export async function runGtfsRtCatalogueRefresh() {
  const res = await seedGtfsRtFeedsFromGtfsDataJp();
  console.log('[gtfsRtCatalogue] seed →', res);
  startRtPoller();
  return res;
}
