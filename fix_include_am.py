import sys

filepath = 'src/atmosphere_manager.cpp'
with open(filepath, 'r') as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    if line.strip() == '#include "shader.h"':
        new_lines.append('#include "shader.h"\n')
        new_lines.append('#include "constants.h"\n')
    else:
        new_lines.append(line)

with open(filepath, 'w') as f:
    f.writelines(new_lines)
