import os
import glob

def patch_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    lines = content.split('\n')
    new_lines = []
    i = 0
    patched = False
    while i < len(lines):
        line = lines[i]
        new_lines.append(line)
        # Look for the exact pattern of the end of the `if (clickedIndex >= 0) { ... return; }`
        # which is usually `      }\n    } else if (` or `      }\n    } else {\n`
        
        # We want to catch the `}` that closes `if (clickedIndex >= 0)` or `if (tappedCategory >= 0)`
        # Let's search for `mappedInput.suppressTouchButtonFallback();` inside the `isTap()` block.
        i += 1

    with open(filepath, 'w') as f:
        f.write("\n".join(new_lines))

