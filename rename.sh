#!/bin/sh
find . -type f -name "pivx*" -print0 | while read -r file; do
    mv "$file" "${file//pivx/hemis}"
done
