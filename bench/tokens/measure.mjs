import { execFileSync } from 'node:child_process';
import { mkdtempSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { encode as cl100k } from 'gpt-tokenizer/encoding/cl100k_base';
import { encode as o200k } from 'gpt-tokenizer/encoding/o200k_base';
import llama3 from 'llama3-tokenizer-js';
import llama2 from 'llama-tokenizer-js';
import { countTokens as claudeLegacy } from '@anthropic-ai/tokenizer';

const [bylc, ...dirs] = process.argv.slice(2);
if (!bylc || dirs.length === 0) {
  console.error('usage: node measure.mjs PATH_TO_BYLC CORPUS_DIR...');
  process.exit(2);
}

const tokenizers = {
  'openai cl100k': (t) => cl100k(t).length,
  'openai o200k': (t) => o200k(t).length,
  'llama3': (t) => llama3.encode(t, { bos: false, eos: false }).length,
  'llama1/2': (t) => llama2.encode(t, false, false).length,
  'claude legacy': (t) => claudeLegacy(t),
};

const work = mkdtempSync(join(tmpdir(), 'bylink-tokens-'));

function bylcOut(args, input, name) {
  const out = join(work, name);
  execFileSync(bylc, [input, ...args, '-o', out], { stdio: ['ignore', 'ignore', 'inherit'] });
  return out;
}

function indent(text) {
  let level = 0;
  return text.split('\n').map((line) => {
    const head = line.split(' ')[0];
    if (head === 'end' || head === 'else') level -= 1;
    const out = '  '.repeat(Math.max(level, 0)) + line;
    if (head === 'fn' || head === 'if' || head === 'while' || head === 'scope' || head === 'else') level += 1;
    return out;
  }).join('\n');
}

const programs = [];
for (const dir of dirs) {
  for (const f of readdirSync(dir).filter((x) => x.endsWith('.by')).sort()) programs.push(join(dir, f));
}

const representations = {
  'source': (p) => readFileSync(p, 'utf8'),
  'source whitespace-collapsed': (p) => readFileSync(p, 'utf8').replace(/\s+/g, ' ').trim(),
  'byai': (p, n) => readFileSync(bylcOut(['--emit-byai'], p, n + '.byai'), 'utf8'),
  'byai indented': (p, n) => indent(readFileSync(bylcOut(['--emit-byai'], p, n + '.byai'), 'utf8')),
  'ir text -O0': (p, n) => readFileSync(bylcOut(['--emit-ir', '-O0'], p, n + '.o0.byir'), 'utf8'),
  'ir text -O2': (p, n) => readFileSync(bylcOut(['--emit-ir', '-O2'], p, n + '.o2.byir'), 'utf8'),
  'byb as hex': (p, n) => readFileSync(bylcOut(['--emit-byb', '-O0'], p, n + '.byb')).toString('hex'),
  'byb as base64': (p, n) => readFileSync(bylcOut(['--emit-byb', '-O0'], p, n + '.byb')).toString('base64'),
};

const totals = {};
for (const rep of Object.keys(representations)) totals[rep] = { bytes: 0, tokens: Object.fromEntries(Object.keys(tokenizers).map((t) => [t, 0])) };

for (const p of programs) {
  const name = p.split('/').pop().replace('.by', '');
  for (const [rep, make] of Object.entries(representations)) {
    const text = make(p, name);
    totals[rep].bytes += Buffer.byteLength(text);
    for (const [tname, count] of Object.entries(tokenizers)) totals[rep].tokens[tname] += count(text);
  }
}

const names = Object.keys(tokenizers);
console.log(`programs: ${programs.length}`);
console.log(['representation', 'bytes', ...names.map((n) => `${n} (vs source)`)].join(' | '));
for (const [rep, t] of Object.entries(totals)) {
  const cells = names.map((n) => {
    const base = totals['source'].tokens[n];
    const delta = ((t.tokens[n] - base) / base) * 100;
    return `${t.tokens[n]} (${delta >= 0 ? '+' : ''}${delta.toFixed(1)}%)`;
  });
  console.log([rep, t.bytes, ...cells].join(' | '));
}
writeFileSync(join(work, 'totals.json'), JSON.stringify(totals, null, 2));
