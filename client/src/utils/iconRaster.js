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
      ctx.drawImage(img, 0, 0, size, size);
      resolve(ctx.getImageData(0, 0, size, size));
    };
    img.onerror = () => resolve(null);
    img.src = url;
  });
}
