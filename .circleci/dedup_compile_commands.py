#!/usr/bin/env python3
# Takes compile_commands.json and removes duplicate entries for the same source file,
# which can cause issues with some tools like Clang Tidy. 
# The first entry for each file is kept, and subsequent duplicates are removed.
import json, sys
db = json.load(open(sys.argv[1]))
seen = set()
deduped = [e for e in db if e['file'] not in seen and not seen.add(e['file'])]
json.dump(deduped, open(sys.argv[1], 'w'), indent=2)
print(f'Deduplicated compile_commands.json: {len(db)} -> {len(deduped)} entries')
