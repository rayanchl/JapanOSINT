/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx}'],
  darkMode: 'class',
  theme: {
    extend: {
      colors: {
        osint: {
          bg: '#0a0e17',
          surface: '#111827',
          panel: '#1a2035',
          border: '#1e293b',
          'border-bright': '#2d3a52',
        },
        neon: {
          cyan: '#00f0ff',
          green: '#00ff88',
          blue: '#3b82f6',
          purple: '#a855f7',
          orange: '#ff8c00',
          red: '#ff4444',
          yellow: '#ffd600',
          pink: '#f06292',
        },
        status: {
          online: '#00ff88',
          degraded: '#ffb74d',
          offline: '#ff4444',
        },
      },
      fontFamily: {
        mono: ['"JetBrains Mono"', '"Fira Code"', 'monospace'],
        sans: ['"Inter"', 'system-ui', 'sans-serif'],
      },
      boxShadow: {
        'neon-cyan': '0 0 8px rgba(0, 240, 255, 0.4), 0 0 20px rgba(0, 240, 255, 0.1)',
        'neon-green': '0 0 8px rgba(0, 255, 136, 0.4), 0 0 20px rgba(0, 255, 136, 0.1)',
        'neon-red': '0 0 8px rgba(255, 68, 68, 0.4), 0 0 20px rgba(255, 68, 68, 0.1)',
      },
      animation: {
        'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
        'glow': 'glow 2s ease-in-out infinite alternate',
      },
      keyframes: {
        glow: {
          '0%': { boxShadow: '0 0 5px rgba(0, 240, 255, 0.2)' },
          '100%': { boxShadow: '0 0 15px rgba(0, 240, 255, 0.6)' },
        },
      },
    },
  },
  plugins: [],
};
