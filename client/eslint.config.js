// ESLint flat config (OPS-08): eslint:recommended + React + react-hooks.
//
// Policy: rules that flag real bugs stay at "error". Rules that report
// pre-existing style/intent issues across many files are "warn" so lint can
// gate CI today without a churn-heavy source rewrite; each is listed with why.
// Tighten them to "error" as the warnings are worked down.
import js from "@eslint/js";
import globals from "globals";
import react from "eslint-plugin-react";
import reactHooks from "eslint-plugin-react-hooks";

export default [
  { ignores: ["dist/**", "node_modules/**", "coverage/**"] },
  js.configs.recommended,
  {
    files: ["**/*.{js,jsx}"],
    plugins: { react, "react-hooks": reactHooks },
    languageOptions: {
      ecmaVersion: "latest",
      sourceType: "module",
      parserOptions: { ecmaFeatures: { jsx: true } },
      globals: { ...globals.browser, ...globals.es2021 },
    },
    settings: { react: { version: "18.3" } },
    rules: {
      ...react.configs.recommended.rules,
      ...reactHooks.configs.recommended.rules,
      "react/react-in-jsx-scope": "off",   // automatic JSX runtime
      "react/prop-types": "off",           // the client does not use PropTypes
      // Downgraded: pre-existing, widespread, not behaviour bugs.
      "no-unused-vars": ["warn", { args: "none", ignoreRestSiblings: true, varsIgnorePattern: "^_", caughtErrors: "none" }],
      "no-empty": ["warn", { allowEmptyCatch: true }],
      "react/no-unescaped-entities": "warn",
      "react/display-name": "warn",
      "react-hooks/exhaustive-deps": "warn",
      // eslint-plugin-react-hooks v7 ships React-Compiler rules; they report
      // existing patterns (setState in effects, ref reads in render) that work
      // today. Warn, do not block, until each is reviewed.
      "react-hooks/set-state-in-effect": "warn",
      "react-hooks/refs": "warn",
      "react-hooks/use-memo": "warn",
      "react-hooks/static-components": "warn",
      "react-hooks/purity": "warn",
      "react/jsx-key": "warn",             // 10 existing missing-key sites: worth fixing, not a gate yet
    },
  },
  {
    // vitest globals / node-side files
    files: ["**/*.test.{js,jsx}", "src/test/**", "vite.config.js", "tailwind.config.js", "postcss.config.js"],
    languageOptions: { globals: { ...globals.node, ...globals.browser, vi: "readonly", describe: "readonly", it: "readonly", test: "readonly", expect: "readonly", beforeEach: "readonly", afterEach: "readonly", beforeAll: "readonly", afterAll: "readonly" } },
  },
];
