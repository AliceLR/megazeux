import resolve from 'rollup-plugin-node-resolve';
import babel from 'rollup-plugin-babel';
import { terser } from 'rollup-plugin-terser';

let plugins = [
    resolve(),
    babel({
      exclude: 'node_modules/**' // only transpile our source code
    })
];

if (process.env.MINIFY) {
    plugins.push(terser());
}

export default {
  input: './src/index.js',
  plugins: plugins,
  output: {
    file: 'mzxrun_web.js',
    format: 'iife', // use browser globals
    sourceMap: true
  }
};
