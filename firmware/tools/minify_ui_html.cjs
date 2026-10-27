const fs = require('node:fs');
const { minify } = require('html-minifier-terser');
const CleanCSS = require('clean-css');

async function main() {
  const html = fs.readFileSync(0, 'utf8');
  const match = html.match(/<style>([\s\S]*?)<\/style>/);
  if (!match) throw new Error('dashboard style not found');
  const result = new CleanCSS({ level: 2 }).minify(match[1]);
  if (result.errors.length) throw new Error(result.errors.join('; '));
  const compact = html.replace(match[0], `<style>${result.styles}</style>`);
  process.stdout.write(await minify(compact, {
    collapseWhitespace: true,
    removeComments: true,
    removeAttributeQuotes: true,
    removeOptionalTags: false,
    removeRedundantAttributes: true,
    removeEmptyAttributes: true,
    collapseBooleanAttributes: true,
    sortAttributes: true,
    sortClassName: true,
    minifyCSS: false,
    minifyJS: false,
  }));
}

main().catch((error) => { console.error(error); process.exitCode = 1; });
