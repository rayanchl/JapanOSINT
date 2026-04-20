// Rasterize a react-icons component to ImageData so MapLibre can consume
// it via map.addImage. Icons are React components that render as a single
// <svg>; we stringify them, inline the color, then paint to a canvas.

import React from 'react';
import { renderToStaticMarkup } from 'react-dom/server';

export function iconToSvgString(IconComponent, color = '#ffffff', size = 64) {
  const element = React.createElement(IconComponent, {
    color,
    size,
    'aria-hidden': 'true',
  });
  return renderToStaticMarkup(element);
}

export function iconToDataUrl(IconComponent, color = '#ffffff', size = 64) {
  const svg = iconToSvgString(IconComponent, color, size);
  return `data:image/svg+xml;utf8,${encodeURIComponent(svg)}`;
}

// Scan a rasterized icon for the bounding box of visible (non-zero-alpha)
// pixels and return offsets that would horizontally + vertically center
// that bbox within the sprite. Needed because react-icons glyphs often
// aren't centered in their viewBox (e.g. MdFlight is a tilted plane
// whose ink bbox sits off-center), which makes map droplines under the
// sprite center look misaligned with the visible glyph.
function findInkBoundsAndOffset(imageData, size) {
  const { data } = imageData;
  let minX = size;
  let minY = size;
  let maxX = -1;
  let maxY = -1;
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      if (data[(y * size + x) * 4 + 3] !== 0) {
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
    }
  }
  if (maxX < 0) return { dx: 0, dy: 0 }; // empty canvas, nothing to center
  const inkCenterX = (minX + maxX) / 2;
  const inkCenterY = (minY + maxY) / 2;
  const spriteCenter = size / 2;
  return {
    dx: Math.round(spriteCenter - inkCenterX),
    dy: Math.round(spriteCenter - inkCenterY),
  };
}

export function rasterizeIcon(IconComponent, color, size = 64) {
  if (typeof document === 'undefined') return Promise.resolve(null);
  const url = iconToDataUrl(IconComponent, color, size);

  return new Promise((resolve) => {
    const img = new Image();
    img.onload = () => {
      const canvas = document.createElement('canvas');
      canvas.width = size;
      canvas.height = size;
      const ctx = canvas.getContext('2d');
      if (!ctx) return resolve(null);

      // First pass: draw at (0,0) to measure the visible ink bbox.
      ctx.drawImage(img, 0, 0, size, size);
      const { dx, dy } = findInkBoundsAndOffset(
        ctx.getImageData(0, 0, size, size),
        size,
      );

      // If the glyph is already centered, skip the re-draw.
      if (dx === 0 && dy === 0) {
        resolve(ctx.getImageData(0, 0, size, size));
        return;
      }

      // Second pass: clear and redraw shifted so the ink bbox center
      // lands at the sprite center. Then the dropline anchored at the
      // sprite's bottom-center lines up with the glyph's visible center.
      ctx.clearRect(0, 0, size, size);
      ctx.drawImage(img, dx, dy, size, size);
      resolve(ctx.getImageData(0, 0, size, size));
    };
    img.onerror = () => resolve(null);
    img.src = url;
  });
}
