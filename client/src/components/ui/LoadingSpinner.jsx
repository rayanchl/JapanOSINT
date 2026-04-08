import React from 'react';

export default function LoadingSpinner({ size = 'md', color = 'cyan' }) {
  const sizes = {
    sm: 'w-4 h-4 border-[2px]',
    md: 'w-6 h-6 border-[2px]',
    lg: 'w-10 h-10 border-[3px]',
  };

  const colors = {
    cyan: 'border-neon-cyan/30 border-t-neon-cyan',
    green: 'border-neon-green/30 border-t-neon-green',
    orange: 'border-neon-orange/30 border-t-neon-orange',
    white: 'border-gray-600 border-t-gray-200',
  };

  return (
    <div
      className={`rounded-full animate-spin ${sizes[size] || sizes.md} ${colors[color] || colors.cyan}`}
      role="status"
      aria-label="Loading"
    />
  );
}
