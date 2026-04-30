import os
import glob

def process_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    if 'mappedInput.consumeTouchEvent(&touchEvent, renderer)' not in content:
        return

    # If the file already has the fix, skip
    if '!mappedInput.isTouchButtonHintTap(touchEvent)' in content:
        return

    lines = content.split('\n')
    new_lines = []
    
    i = 0
    patched = False
    in_tap_block = False
    
    while i < len(lines):
        line = lines[i]
        
        if 'if (touchEvent.isTap()) {' in line:
            in_tap_block = True
            
        if in_tap_block:
            # We are looking for the closing brace of the isTap block, which is followed by `} else if (` or `} else {`
            # or we are looking for the closing brace of the inner `if (clickedIndex >= 0) { ... return; }`
            # It's tricky because the nesting varies.
            pass

        new_lines.append(line)
        i += 1

