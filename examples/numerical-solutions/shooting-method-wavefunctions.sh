#! /bin/sh
set -e

cat > s.r << EOF
150 0.2 0.0
100 0.0 0.0
150 0.2 0.0
EOF

find_heterostructure

# Create alloy concentration file
efxv -m 0.067

# Implement shooting method
efshoot -a