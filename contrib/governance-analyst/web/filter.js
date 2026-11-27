'use strict';
const search = document.getElementById('proposal-search');
if (search) {
  const rows = Array.from(document.querySelectorAll('[data-proposal]'));
  const count = document.getElementById('filter-count');
  search.addEventListener('input', () => {
    const query = search.value.trim().toLocaleLowerCase();
    let shown = 0;
    for (const row of rows) {
      row.hidden = !row.textContent.toLocaleLowerCase().includes(query);
      if (!row.hidden) shown += 1;
    }
    count.textContent = `${shown} of ${rows.length} proposals shown`;
  });
}
