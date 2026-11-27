const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

test('proposal search supports case, whitespace, empty results and reset', () => {
  const rows = ['Example Alpha', 'Example Beta', 'Example Gamma', 'Example Delta', 'Example Epsilon', 'Example Zeta'].map(textContent => ({textContent, hidden: false}));
  let onInput;
  const search = {value: '', addEventListener: (event, fn) => { assert.equal(event, 'input'); onInput = fn; }};
  const count = {textContent: ''};
  const document = {getElementById: id => id === 'proposal-search' ? search : count, querySelectorAll: () => rows};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../web/filter.js'), 'utf8'), {document});
  search.value = '  ALPHA  '; onInput();
  assert.equal(rows.filter(row => !row.hidden).length, 1);
  assert.equal(count.textContent, '1 of 6 proposals shown');
  search.value = 'no matching proposal'; onInput();
  assert.equal(rows.filter(row => !row.hidden).length, 0);
  assert.equal(count.textContent, '0 of 6 proposals shown');
  search.value = ''; onInput();
  assert.equal(rows.filter(row => !row.hidden).length, 6);
});

test('proposal detail pages can load the shared script without a search box', () => {
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../web/filter.js'), 'utf8'), {document: {getElementById: () => null}});
});
