import resolve from 'rollup-plugin-node-resolve';
import babel from 'rollup-plugin-babel';
import { terser } from 'rollup-plugin-terser';

export default {
  input: './src/index.js',
  plugins: [
    resolve(),
    babel({
      exclude: 'node_modules/**' // only transpile our source code
    })
//    ,terser()
  ],
  output: {
    file: 'mzxrun_web.js',
    format: 'iife', // use browser globals
    sourceMap: true
  }
};
