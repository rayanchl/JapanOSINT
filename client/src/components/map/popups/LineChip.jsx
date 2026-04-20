// Colored pill showing a line's ref (or name fallback). Matches the
// active-tag style used elsewhere in the map (translucent colored
// background + full-color text + matching border).

// NOTE: the prop is `refText`, not `ref` — React reserves `ref` for
// forwardRef / DOM refs, so it never reaches a normal function component
// as a value.
export default function LineChip({ color, refText, name }) {
  const bg = color ? `${color}33` : '#88888833'; // 20% alpha
  const fg = color || '#ccc';
  const border = color || '#888';
  return (
    <span
      className="inline-flex items-center px-1.5 py-0.5 rounded font-mono text-[10px] leading-none whitespace-nowrap"
      style={{ background: bg, color: fg, border: `1px solid ${border}` }}
      title={name || refText || ''}
    >
      {refText || name || '?'}
    </span>
  );
}
