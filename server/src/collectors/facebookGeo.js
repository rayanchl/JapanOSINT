/**
 * Facebook Geo-coded Posts Collector
 * Maps public Facebook check-ins and geotagged posts from Japan using the
 * Graph API. Returns an empty FeatureCollection when FACEBOOK_ACCESS_TOKEN
 * is absent or the API call fails — no OSM proxy or seed fallback.
 */

const FB_ACCESS_TOKEN = process.env.FACEBOOK_ACCESS_TOKEN || '';

export default async function collectFacebookGeo() {
  let features = [];
  let live = false;

  if (FB_ACCESS_TOKEN) {
    try {
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 8000);
      const res = await fetch(
        `https://graph.facebook.com/v18.0/search?type=place&center=35.6762,139.6503&distance=50000&fields=name,location,checkins&access_token=${FB_ACCESS_TOKEN}`,
        { signal: controller.signal }
      );
      clearTimeout(timeout);
      if (res.ok) {
        const data = await res.json();
        if (data.data && data.data.length > 0) {
          features = data.data
            .filter((p) => p?.location?.longitude != null && p?.location?.latitude != null)
            .map((place) => ({
              type: 'Feature',
              geometry: {
                type: 'Point',
                coordinates: [place.location.longitude, place.location.latitude],
              },
              properties: {
                id: place.id,
                platform: 'facebook',
                place_name: place.name,
                checkins: place.checkins || 0,
                source: 'facebook_api',
              },
            }));
          live = features.length > 0;
        }
      }
    } catch { /* no fallback — return empty */ }
  }

  return {
    type: 'FeatureCollection',
    features,
    _meta: {
      source: 'facebook_geo',
      fetchedAt: new Date().toISOString(),
      recordCount: features.length,
      live,
      live_source: live ? 'facebook_api' : null,
      description: 'Facebook public check-ins across Japan (Graph API only)',
    },
    metadata: {},
  };
}
