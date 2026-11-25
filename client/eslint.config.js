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
      // Rules at zero findings are errors so they cannot regress.
      "no-unused-vars": ["error", { args: "none", ignoreRestSiblings: true, varsIgnorePattern: "^_", caughtErrors: "none" }],
      "no-empty": ["error", { allowEmptyCatch: true }],
      "react/no-unescaped-entities": "error",
      "react/display-name": "error",
      "react-hooks/exhaustive-deps": "error",
      // eslint-plugin-react-hooks v7 ships React-Compiler rules. The fetch-then-
      // setState / reset-on-prop-change findings are all restructured (0 left).
      "react-hooks/set-state-in-effect": "error",
      "react-hooks/refs": "error",
      "react-hooks/use-memo": "error",
      "react-hooks/static-components": "error",
      "react-hooks/purity": "error",
      "react/jsx-key": "error",
    },
  },
  {
    // vitest globals / node-side files
    files: ["**/*.test.{js,jsx}", "src/test/**", "vite.config.js", "tailwind.config.js", "postcss.config.js"],
    languageOptions: { globals: { ...globals.node, ...globals.browser, vi: "readonly", describe: "readonly", it: "readonly", test: "readonly", expect: "readonly", beforeEach: "readonly", afterEach: "readonly", beforeAll: "readonly", afterAll: "readonly" } },
  },
];
